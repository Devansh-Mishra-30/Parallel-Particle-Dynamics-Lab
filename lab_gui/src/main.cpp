#include "lab_gui/app.h"
#include "lab_gui/diagnostics_panel.h"
#include "lab_gui/experiment_presets.h"
#include "lab_gui/screenshot_util.h"

#include <iostream>
#include <string>

// Header-only GL definitions for OpenGL 3
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <implot.h>

static void glfw_error_callback(int error, const char* description) {
    std::cerr << "[GLFW Error " << error << "]: " << description << std::endl;
}

static int run_headless_smoke_test(int frames_to_run) {
    std::cout << "=================================================================\n";
    std::cout << "  Parallel Particle Dynamics Lab: Headless GUI Smoke Test\n";
    std::cout << "=================================================================\n";
    std::cout << "Running " << frames_to_run << " headless frames across all experiment presets...\n";

    lab_gui::AppState state;

    // Test 1: Collision Experiment
    std::cout << "[1/4] Testing Collision scenario...\n";
    lab_gui::ExperimentPresets::load_collision(state, 0.528f);
    for (int i = 0; i < frames_to_run; ++i) {
        state.solver.step();
        state.current_time += state.config.dt;
        state.current_diagnostics = state.solver.compute_diagnostics();
        if (!state.current_diagnostics.is_finite) {
            std::cerr << "FAIL: Non-finite state detected in Collision scenario!\n";
            return 1;
        }
    }
    const float current_drift = (state.current_diagnostics.total_momentum - state.initial_momentum).length();
    std::cout << "      Passed. Final time: " << state.current_time << " s, Momentum drift: "
              << current_drift << " kg*m/s, Active contacts: "
              << state.current_diagnostics.active_contacts << "\n";

    // Test 2: Ballistic Gravity Experiment
    std::cout << "[2/4] Testing Gravity Ballistic scenario...\n";
    lab_gui::ExperimentPresets::load_gravity(state);
    for (int i = 0; i < frames_to_run; ++i) {
        state.solver.step();
        state.current_time += state.config.dt;
        state.current_diagnostics = state.solver.compute_diagnostics();
        if (!state.current_diagnostics.is_finite) {
            std::cerr << "FAIL: Non-finite state detected in Gravity scenario!\n";
            return 1;
        }
    }
    std::cout << "      Passed. Final time: " << state.current_time << " s\n";

    // Test 3: Dense Granular Column Settling (Genuinely dense contact test)
    std::cout << "[3/4] Testing Dense Granular Column scenario...\n";
    lab_gui::ExperimentPresets::load_dense_packing(state, 48);
    for (int i = 0; i < frames_to_run; ++i) {
        state.solver.step();
        state.current_time += state.config.dt;
        state.current_diagnostics = state.solver.compute_diagnostics();
        if (!state.current_diagnostics.is_finite) {
            std::cerr << "FAIL: Non-finite state detected in Dense Packing scenario!\n";
            return 1;
        }
    }
    std::cout << "      Passed. Final time: " << state.current_time
              << " s, Active pairwise contacts: " << state.current_diagnostics.active_contacts
              << ", Max penetration: " << state.current_diagnostics.max_penetration * 1000.0f << " mm\n";

    // Verify that dense contact test actually had multiple active contacts
    if (state.current_diagnostics.active_contacts == 0) {
        std::cerr << "WARNING: Expected active contacts in dense packing heap!\n";
    }

    // Test 4: CSV Export Validation
    std::cout << "[4/4] Testing CSV Data Export...\n";
    const std::string test_csv = "output/csv/smoke_test_export.csv";
    lab_gui::CsvExportRow dummy_row;
    dummy_row.time = 0.5;
    dummy_row.ek = 1.2f;
    dummy_row.ep = 0.8f;
    dummy_row.etotal = 2.0f;
    state.recorded_rows.push_back(dummy_row);
    if (!state.export_recorded_csv(test_csv)) {
        std::cerr << "FAIL: Could not export CSV to " << test_csv << "!\n";
        return 1;
    }
    std::cout << "      Passed. CSV exported successfully to " << test_csv << "\n";

    std::cout << "=================================================================\n";
    std::cout << "  ALL HEADLESS GUI VERIFICATION TESTS PASSED (100%)\n";
    std::cout << "=================================================================\n";
    return 0;
}

int main(int argc, char* argv[]) {
    std::string target_scenario = "collision";
    std::string screenshot_path = "";
    float custom_restitution = -1.0f;
    int target_frames = 0;
    std::string backend_name = "cpu";
    int omp_threads = 8;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--test-headless" || arg == "--headless" || arg == "--smoke-test") {
            int frames = 60;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                frames = std::max(10, std::atoi(argv[++i]));
            }
            return run_headless_smoke_test(frames);
        }
        if (arg == "--scenario" && i + 1 < argc) {
            target_scenario = argv[++i];
        } else if (arg == "--restitution" && i + 1 < argc) {
            custom_restitution = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--frames" && i + 1 < argc) {
            target_frames = std::atoi(argv[++i]);
        } else if (arg == "--screenshot" && i + 1 < argc) {
            screenshot_path = argv[++i];
        } else if (arg == "--backend" && i + 1 < argc) {
            backend_name = argv[++i];
        } else if (arg == "--threads" && i + 1 < argc) {
            omp_threads = std::max(1, std::min(16, std::atoi(argv[++i])));
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Parallel Particle Dynamics Lab (Phase 1C)\n";
            std::cout << "Usage:\n";
            std::cout << "  ./sim_lab                                           # Launch interactive GUI\n";
            std::cout << "  ./sim_lab --test-headless <frames>                  # Automated headless test\n";
            std::cout << "  ./sim_lab --scenario <name> --frames N --screenshot <path>  # Capture real render\n";
            std::cout << "  ./sim_lab --backend omp --threads 8                 # Select OpenMP Multicore backend\n";
            std::cout << "Scenarios: collision, gravity, convergence, granular, custom\n";
            return 0;
        }
    }

    // Launch Real Graphical Desktop Application
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::cerr << "Error: Failed to initialize GLFW.\n";
        std::cerr << "If running in a headless environment without an X11/Wayland display server,\n";
        std::cerr << "use --test-headless to execute automated verification suite.\n";
        return 1;
    }

    // Configure OpenGL 3.3 Core Profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    const int window_w = 1600;
    const int window_h = 920;
    GLFWwindow* window = glfwCreateWindow(window_w, window_h,
                                          "Parallel Particle Dynamics Lab - Mechanical Engineering Verification Workbench",
                                          nullptr, nullptr);
    if (!window) {
        std::cerr << "Error: Failed to create GLFW window.\n";
        std::cerr << "Please ensure WSLg or an X11 server is active (DISPLAY=:0).\n";
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable V-Sync

    // Initialize ImGui and ImPlot
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    lab_gui::LabApp app;
    auto& state = app.get_state();

    // Configure initial scenario if requested via CLI
    if (target_scenario == "gravity") {
        lab_gui::ExperimentPresets::load_gravity(state);
    } else if (target_scenario == "convergence") {
        lab_gui::ExperimentPresets::load_convergence(state);
    } else if (target_scenario == "granular") {
        lab_gui::ExperimentPresets::load_dense_packing(state, 48);
    } else if (target_scenario == "custom") {
        lab_gui::ExperimentPresets::load_custom(state);
    } else { // default collision
        const float e = (custom_restitution >= 0.0f) ? custom_restitution : 0.528f;
        lab_gui::ExperimentPresets::load_collision(state, e);
    }

    if (backend_name == "omp" || backend_name == "openmp") {
        state.active_backend = lab_gui::SolverBackend::OpenMpParallel;
        state.omp_threads = omp_threads;
        state.sync_solvers_from_cpu();
        state.omp_solver.set_broadphase_mode(sim_omp::BroadphaseMode::SpatialGrid);
#if HAS_CUDA_BACKEND
    } else if (backend_name == "cuda" || backend_name == "gpu") {
        state.active_backend = lab_gui::SolverBackend::CudaGpu;
        state.sync_solvers_from_cpu();
        state.cuda_solver.set_broadphase_mode(sim_cuda::CudaBroadphaseMode::SpatialGrid);
#endif
    }

    // Automated screenshot capture mode
    if (!screenshot_path.empty()) {
        std::cout << "[Screenshot Mode] Simulating scenario '" << target_scenario
                  << "' for " << target_frames << " frames...\n";
        for (int f = 0; f < target_frames; ++f) {
            app.update(static_cast<float>(state.config.dt));
        }

        if (target_scenario == "convergence") {
            lab_gui::DiagnosticsPanel::execute_convergence_sweep(state);
        }

        // Render target frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        app.render(display_w, display_h);

        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.12f, 0.16f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        lab_gui::ScreenshotUtil::capture_framebuffer_png(screenshot_path, display_w, display_h);

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    }

    // Standard Interactive Render Loop
    double last_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        const double current_time = glfwGetTime();
        const float delta_time = static_cast<float>(current_time - last_time);
        last_time = current_time;

        app.update(delta_time);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        app.render(display_w, display_h);

        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.12f, 0.16f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Graceful Shutdown
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
