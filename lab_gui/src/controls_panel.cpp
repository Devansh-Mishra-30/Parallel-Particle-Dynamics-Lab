#include "lab_gui/controls_panel.h"
#include "lab_gui/experiment_presets.h"

#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace lab_gui {

void ControlsPanel::render(AppState& state) {
    ImGui::Begin("Experiment Controls & Parameters", nullptr);

    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "PARALLEL PARTICLE DYNAMICS LAB");
    ImGui::TextDisabled("Phase 1C: Scientific Verification & Student Workbench");
    ImGui::Separator();

    // 1. Playback & Clock Controls (Prominently at the top)
    render_playback_controls(state);
    ImGui::Separator();

    // 2. Predefined Experiment Selector
    render_experiment_selector(state);
    ImGui::Separator();

    // 3. Physical Parameters
    render_physical_parameters(state);
    ImGui::Separator();

    // 4. Numerical Integration Controls
    render_numerical_controls(state);
    ImGui::Separator();

    // 5. Viewport Overlays & Rendering Modes
    render_visualization_options(state);

    ImGui::End();
}

void ControlsPanel::render_playback_controls(AppState& state) {
    ImGui::TextUnformatted("Simulation Playback");

    // Play / Pause
    if (state.is_paused) {
        if (ImGui::Button(" [>] Play ", ImVec2(90.0f, 32.0f))) {
            state.is_paused = false;
        }
    } else {
        if (ImGui::Button(" [||] Pause ", ImVec2(90.0f, 32.0f))) {
            state.is_paused = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(" [>|] Step ", ImVec2(85.0f, 32.0f))) {
        state.is_paused = true;
        state.step_single_frame = true;
    }

    ImGui::SameLine();
    if (ImGui::Button(" [<<] Reset ", ImVec2(85.0f, 32.0f))) {
        switch (state.active_experiment) {
            case ExperimentType::Collision:
                ExperimentPresets::load_collision(state,
                                                  state.initial_collision.restitution,
                                                  state.initial_collision.r1,
                                                  state.initial_collision.r2,
                                                  state.initial_collision.m1,
                                                  state.initial_collision.m2,
                                                  state.initial_collision.v1,
                                                  state.initial_collision.v2,
                                                  state.initial_collision.impact_parameter);
                break;
            case ExperimentType::GravityBallistic:
                ExperimentPresets::load_gravity(state, state.config.gravity, state.initial_vel1, state.config.dt);
                break;
            case ExperimentType::ConvergenceStudy:
                ExperimentPresets::load_convergence(state);
                break;
            case ExperimentType::DensePacking:
                ExperimentPresets::load_dense_packing(state, static_cast<int>(state.solver.particle_count()));
                break;
            case ExperimentType::Custom:
                ExperimentPresets::load_custom(state);
                break;
        }
        state.is_paused = false;
    }

    // Speed Multipliers
    ImGui::Spacing();
    ImGui::Text("Sim Speed:");
    ImGui::SameLine();
    if (ImGui::RadioButton("0.1x", state.sim_speed == 0.1f)) state.sim_speed = 0.1f;
    ImGui::SameLine();
    if (ImGui::RadioButton("0.5x", state.sim_speed == 0.5f)) state.sim_speed = 0.5f;
    ImGui::SameLine();
    if (ImGui::RadioButton("1.0x", state.sim_speed == 1.0f)) state.sim_speed = 1.0f;
    ImGui::SameLine();
    if (ImGui::RadioButton("2.0x", state.sim_speed == 2.0f)) state.sim_speed = 2.0f;

    // Simulation Clock
    ImGui::Text("Physical Time: %.4f s  |  Frames: %llu", state.current_time, static_cast<unsigned long long>(state.frame_index));

    // CSV Export & Screenshot Buttons
    if (ImGui::Button(" Export Run to CSV ", ImVec2(180.0f, 28.0f))) {
        const auto now = std::chrono::system_clock::now();
        const auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "output/csv/lab_export_" << in_time_t << ".csv";
        state.export_recorded_csv(ss.str());
    }

    ImGui::SameLine();
    if (ImGui::Button(" Capture Screenshot ", ImVec2(180.0f, 28.0f))) {
        const auto now = std::chrono::system_clock::now();
        const auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "output/screenshots/screenshot_" << in_time_t << ".png";
        state.pending_screenshot_path = ss.str();
    }

    if (state.toast_timer > 0.0f) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", state.toast_message.c_str());
    }
}

void ControlsPanel::render_experiment_selector(AppState& state) {
    ImGui::TextUnformatted("Select Experiment Scenario");

    const char* experiment_names[] = {
        "EXP_01: Two-Particle Impact (Central & Oblique)",
        "EXP_02: Ballistic Flight & Free Fall Trajectories",
        "EXP_03: Empirical Timestep Refinement Study",
        "EXP_04: Dense Granular Column Settling",
        "EXP_CUSTOM: User-Defined Particle Setup"
    };

    int current_idx = static_cast<int>(state.active_experiment);
    if (ImGui::Combo("##Scenario", &current_idx, experiment_names, IM_ARRAYSIZE(experiment_names))) {
        state.active_experiment = static_cast<ExperimentType>(current_idx);
        switch (state.active_experiment) {
            case ExperimentType::Collision:        ExperimentPresets::load_collision(state); break;
            case ExperimentType::GravityBallistic: ExperimentPresets::load_gravity(state); break;
            case ExperimentType::ConvergenceStudy: ExperimentPresets::load_convergence(state); break;
            case ExperimentType::DensePacking:     ExperimentPresets::load_dense_packing(state); break;
            case ExperimentType::Custom:           ExperimentPresets::load_custom(state); break;
        }
    }
}

void ControlsPanel::render_physical_parameters(AppState& state) {
    if (ImGui::CollapsingHeader("Physical Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Restitution e
        if (ImGui::SliderFloat("Restitution (e)", &state.config.restitution, 0.0f, 1.0f, "%.3f")) {
            state.solver.set_config(state.config);
            state.initial_collision.restitution = state.config.restitution;
            if (state.active_experiment == ExperimentType::Collision) {
                state.recompute_collision_analytical_solution();
            }
        }

        if (state.config.restitution > 1.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "WARNING: e > 1.0 causes non-physical energy creation!");
        }

        // Boundary Restitution e_b
        if (ImGui::SliderFloat("Wall Restitution (e_b)", &state.config.boundary_restitution, 0.0f, 1.0f, "%.3f")) {
            state.solver.set_config(state.config);
        }

        // Gravity Settings
        ImGui::Spacing();
        ImGui::Text("Gravitational Acceleration g (m/s^2):");
        if (ImGui::SmallButton("Earth (-9.81)")) {
            state.config.gravity = {0.0f, -9.80665f};
            state.solver.set_config(state.config);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Moon (-1.62)")) {
            state.config.gravity = {0.0f, -1.62f};
            state.solver.set_config(state.config);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Zero G")) {
            state.config.gravity = {0.0f, 0.0f};
            state.solver.set_config(state.config);
        }

        if (ImGui::SliderFloat2("g_x, g_y", &state.config.gravity.x, -20.0f, 20.0f, "%.2f")) {
            state.solver.set_config(state.config);
        }

        // Scenario Specific Parameters
        if (state.active_experiment == ExperimentType::Collision) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.85f, 0.95f, 0.5f, 1.0f), "Collision Laboratory Presets:");

            if (ImGui::SmallButton("1. Equal Elastic (e=1)")) {
                ExperimentPresets::load_collision(state, 1.0f, 0.06f, 0.06f, 0.0f, 0.0f, {1.0f, 0.0f}, {-1.0f, 0.0f}, 0.0f);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("2. Unequal Elastic (m2=2*m1)")) {
                const float r1 = 0.05f;
                const float r2 = r1 * std::sqrt(2.0f);
                ExperimentPresets::load_collision(state, 1.0f, r1, r2, 0.0f, 0.0f, {1.2f, 0.0f}, {-0.8f, 0.0f}, 0.0f);
            }

            if (ImGui::SmallButton("3. Inelastic (e=0.528)")) {
                ExperimentPresets::load_collision(state, 0.528f, 0.06f, 0.06f, 0.0f, 0.0f, {1.5f, 0.0f}, {-1.5f, 0.0f}, 0.0f);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("4. Plastic (e=0)")) {
                ExperimentPresets::load_collision(state, 0.0f, 0.06f, 0.06f, 0.0f, 0.0f, {1.0f, 0.0f}, {-1.0f, 0.0f}, 0.0f);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("5. Oblique (b=0.04m)")) {
                ExperimentPresets::load_collision(state, 0.9f, 0.06f, 0.06f, 0.0f, 0.0f, {1.2f, 0.0f}, {-1.2f, 0.0f}, 0.04f);
            }

            ImGui::Spacing();
            ImGui::Text("Adjust Collision Setup (Resets initial state):");

            float r1 = state.initial_collision.r1;
            float r2 = state.initial_collision.r2;
            if (ImGui::SliderFloat("Radius 1 (m)", &r1, 0.02f, 0.15f, "%.3f m")) {
                ExperimentPresets::load_collision(state, state.config.restitution, r1, r2, 0.0f, 0.0f,
                                                  state.initial_collision.v1, state.initial_collision.v2,
                                                  state.initial_collision.impact_parameter);
            }
            ImGui::TextDisabled("  -> Mass 1: %.3f kg", state.initial_collision.m1);

            if (ImGui::SliderFloat("Radius 2 (m)", &r2, 0.02f, 0.15f, "%.3f m")) {
                ExperimentPresets::load_collision(state, state.config.restitution, r1, r2, 0.0f, 0.0f,
                                                  state.initial_collision.v1, state.initial_collision.v2,
                                                  state.initial_collision.impact_parameter);
            }
            ImGui::TextDisabled("  -> Mass 2: %.3f kg", state.initial_collision.m2);

            float b = state.initial_collision.impact_parameter;
            if (ImGui::SliderFloat("Impact Offset b (m)", &b, -0.10f, 0.10f, "%.3f m")) {
                ExperimentPresets::load_collision(state, state.config.restitution, r1, r2, 0.0f, 0.0f,
                                                  state.initial_collision.v1, state.initial_collision.v2, b);
            }
        }
    }
}

void ControlsPanel::render_numerical_controls(AppState& state) {
    if (ImGui::CollapsingHeader("Numerical Solver Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Frame Timestep dt
        if (ImGui::SliderFloat("Frame dt (s)", &state.config.dt, 0.001f, 0.050f, "%.4f s")) {
            state.solver.set_config(state.config);
        }

        // Substeps
        if (ImGui::SliderInt("Substeps (N_sub)", &state.config.substeps, 1, 32)) {
            state.solver.set_config(state.config);
        }

        // Effective Substep dt_sub
        const float dt_sub_ms = state.config.dt_substep() * 1000.0f;
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Effective dt_sub: %.3f ms", dt_sub_ms);

        // Gauss-Seidel Contact Iterations
        if (ImGui::SliderInt("Gauss-Seidel Iters", &state.config.solver_iterations, 1, 20)) {
            state.solver.set_config(state.config);
        }

        // Baumgarte Stabilization Factor
        if (ImGui::SliderFloat("Penetration Stabilization (alpha)", &state.config.stabilization_factor, 0.0f, 1.0f, "%.2f")) {
            state.solver.set_config(state.config);
            state.omp_solver.set_config(state.config);
        }

        // Integrator Scheme Selection (Lab 2 Coursework)
        int integ_choice = (state.config.integrator == sim_core::IntegratorType::ExplicitEuler) ? 1 : 0;
        if (ImGui::Combo("Integrator##scheme", &integ_choice, "Semi-implicit Euler (Symplectic)\0Explicit Forward Euler (Non-Symplectic)\0")) {
            state.config.integrator = (integ_choice == 1) ? sim_core::IntegratorType::ExplicitEuler : sim_core::IntegratorType::SemiImplicitEuler;
            state.solver.set_config(state.config);
            state.omp_solver.set_config(state.config);
        }

        // Phase 2 & 3: Multi-Backend Compute Architecture
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Physics Compute Backend:");

        int backend_choice = static_cast<int>(state.active_backend);
        if (ImGui::RadioButton("CPU Sequential", &backend_choice, 0)) {
            if (state.active_backend != SolverBackend::CpuReference) {
                const auto current_pts = state.get_active_particles();
                state.active_backend = SolverBackend::CpuReference;
                state.solver.set_particles(current_pts);
                state.solver.set_config(state.config);
                state.solver.set_boundary(state.boundary);
                state.show_toast("Switched to CPU Reference Backend");
            }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("OpenMP Multicore", &backend_choice, 1)) {
            if (state.active_backend != SolverBackend::OpenMpParallel) {
                const auto current_pts = state.get_active_particles();
                state.active_backend = SolverBackend::OpenMpParallel;
                state.omp_solver.set_particles(current_pts);
                state.omp_solver.set_config(state.config);
                state.omp_solver.set_boundary(state.boundary);
                state.omp_solver.set_num_threads(state.omp_threads);
                state.show_toast("Switched to OpenMP Multicore Backend");
            }
        }
        ImGui::SameLine();
#if HAS_CUDA_BACKEND
        const bool cuda_avail = sim_cuda::CudaSolverV2::is_cuda_available();
        if (ImGui::RadioButton("CUDA GPU (RTX 4060)", &backend_choice, 2)) {
            if (cuda_avail) {
                if (state.active_backend != SolverBackend::CudaGpu) {
                    const auto current_pts = state.get_active_particles();
                    state.active_backend = SolverBackend::CudaGpu;
                    state.cuda_solver.set_particles(current_pts);
                    state.cuda_solver.set_config(state.config);
                    state.cuda_solver.set_boundary(state.boundary);
                    state.cuda_initialized = true;
                    state.show_toast("Switched to NVIDIA CUDA GPU Backend");
                }
            } else {
                state.show_toast("CUDA GPU Hardware Unavailable! Falling back to CPU.");
            }
        }
#else
        ImGui::BeginDisabled();
        ImGui::RadioButton("CUDA GPU (No CUDA Build)", false);
        ImGui::EndDisabled();
#endif

        if (state.active_backend == SolverBackend::OpenMpParallel) {
            if (ImGui::SliderInt("OpenMP Threads", &state.omp_threads, 1, 16)) {
                state.omp_solver.set_num_threads(state.omp_threads);
            }

            int bp_mode = static_cast<int>(state.omp_solver.get_broadphase_mode());
            if (ImGui::RadioButton("All-Pairs O(N^2)##omp", &bp_mode, 0)) {
                state.omp_solver.set_broadphase_mode(sim_omp::BroadphaseMode::AllPairs);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Spatial Grid O(N)##omp", &bp_mode, 1)) {
                state.omp_solver.set_broadphase_mode(sim_omp::BroadphaseMode::SpatialGrid);
            }
#if HAS_CUDA_BACKEND
        } else if (state.active_backend == SolverBackend::CudaGpu) {
            const auto& specs = state.cuda_solver.get_device_specs();
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "GPU: %s (CC %s)",
                               specs.device_name.c_str(), specs.compute_capability_str().c_str());
            ImGui::TextDisabled("VRAM: %.0f MB | Multiprocessors: %d SMs",
                                specs.total_memory_mb(), specs.multiprocessor_count);

            int bp_mode = static_cast<int>(state.cuda_solver.get_broadphase_mode());
            if (ImGui::RadioButton("All-Pairs O(N^2)##cuda", &bp_mode, 0)) {
                state.cuda_solver.set_broadphase_mode(sim_cuda::CudaBroadphaseMode::AllPairs);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Spatial Grid O(N)##cuda", &bp_mode, 1)) {
                state.cuda_solver.set_broadphase_mode(sim_cuda::CudaBroadphaseMode::SpatialGrid);
            }
#endif
        }

        // Live Physics Performance Readout
        ImGui::Spacing();
        ImGui::Text("Step Compute Time: %.3f ms (%.1f FPS)", state.last_physics_step_ms,
                    (state.last_physics_step_ms > 0.0 ? 1000.0 / state.last_physics_step_ms : 0.0));
        ImGui::TextDisabled("Backend: %s | Active Particles: %zu",
                            (state.active_backend == SolverBackend::CudaGpu ? "CUDA GPU" :
                             state.active_backend == SolverBackend::OpenMpParallel ? "OpenMP Multicore" : "CPU Reference"),
                            state.get_active_particle_count());

        // Stage 10: Scientific Comparison Experiment (Educational Investigation)
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Scientific Comparison: CPU vs OpenMP vs CUDA", ImGuiTreeNodeFlags_None)) {
            ImGui::TextColored(ImVec4(0.85f, 0.95f, 0.5f, 1.0f), "Research Question: Does parallel computing alter simulation accuracy?");
            ImGui::BulletText("Physical Model: Identical Symplectic Euler, mass-weighted Baumgarte, frictionless impulses.");
            ImGui::BulletText("Numerical Algorithm: CPU uses Gauss-Seidel (in-place); OpenMP and CUDA use Jacobi (simultaneous).");
            ImGui::BulletText("2-Body Impacts: Mathematically identical across all backends (||e_v|| < 1e-4 m/s).");
            ImGui::BulletText("Dense Packings: Jacobi evaluates neighbor impulses symmetrically; Gauss-Seidel has directional bias.");
            ImGui::BulletText("Floating-Point Rounding: GPU FP32 FMA fusion vs x86-64 AVX SIMD causes minor machine deviations (~1e-6).");
            ImGui::BulletText("Determinism: Verified identical across repeat GPU runs (dP diff = 0.000000 kg*m/s).");
        }
    }
}

void ControlsPanel::render_visualization_options(AppState& state) {
    if (ImGui::CollapsingHeader("Viewport Visual Overlays", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Metric Grid (SI)", &state.show_grid);
        ImGui::SameLine();
        ImGui::Checkbox("Boundary Circle", &state.show_boundary);

        ImGui::Checkbox("Velocity Vectors (v)", &state.show_velocity_vectors);
        if (state.show_velocity_vectors) {
            ImGui::SliderFloat("Vector Scale", &state.velocity_vector_scale, 0.05f, 0.50f, "%.2f");
        }

        ImGui::Checkbox("Contact Normals & Penetration", &state.show_contact_normals);
        ImGui::Checkbox("Analytical Trajectory Ghost", &state.show_analytical_trajectory);

        // Color Mode Selection
        const char* color_modes[] = {
            "Material Palette",
            "Velocity Heatmap (Speed)",
            "Contact State (Clamping)",
            "Mass Spectrum"
        };
        int current_color = static_cast<int>(state.color_mode);
        if (ImGui::Combo("Particle Color", &current_color, color_modes, IM_ARRAYSIZE(color_modes))) {
            state.color_mode = static_cast<ParticleColorMode>(current_color);
        }
    }
}

} // namespace lab_gui
