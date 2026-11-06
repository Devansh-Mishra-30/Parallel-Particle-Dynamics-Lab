#pragma once

#include "lab_gui/workbench_modes.h"
#include "lab_gui/workbench_predictions.h"
#include "lab_gui/workbench_evidence.h"

#include "sim_core/boundary.h"
#include "sim_core/cpu_reference_solver.h"
#include "sim_core/diagnostics.h"
#include "sim_core/types.h"
#include "sim_omp_v2/omp_solver.h"
#if HAS_CUDA_BACKEND
#include "sim_cuda_v2/cuda_solver.h"
#endif

#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace lab_gui {

/**
 * \brief Color coding modes for particle rendering in the 2D viewport.
 */
enum class ParticleColorMode {
    Material = 0,
    VelocityHeatmap = 1,
    ContactState = 2,
    MassSpectrum = 3
};

/**
 * \brief Three-tier UI application mode for progressive scientific disclosure.
 */
enum class LabUiMode {
    Learn = 0,       // Mode 1: Guided Physics Laboratories (EXP_01 to EXP_04)
    Analyze = 1,     // Mode 2: Numerical Verification & Convergence Studies
    Performance = 2  // Mode 3: Multicore OpenMP & GPU Scaling Workbench
};

/**
 * \brief Predefined scientific experiment modes.
 */
enum class ExperimentType {
    Collision = 0,        // Two-Particle Impact (Central & Oblique)
    GravityBallistic = 1, // Ballistic Flight & Free Fall Trajectories
    ConvergenceStudy = 2, // Multi-timestep Numerical Refinement
    DensePacking = 3,     // Granular Column Settling (Dense simultaneous contacts)
    Custom = 4            // User-Configurable Particle Setup
};

/**
 * \brief Rolling buffer of 2D data points for real-time ImPlot visualization.
 */
struct RollingBuffer {
    int max_size{2400};
    int offset{0};
    std::vector<float> x;
    std::vector<float> y;

    explicit RollingBuffer(int size = 2400) : max_size(size) {
        x.reserve(max_size);
        y.reserve(max_size);
    }

    void add_point(float pt_x, float pt_y) {
        if (static_cast<int>(x.size()) < max_size) {
            x.push_back(pt_x);
            y.push_back(pt_y);
        } else {
            x[offset] = pt_x;
            y[offset] = pt_y;
            offset = (offset + 1) % max_size;
        }
    }

    void clear() {
        x.clear();
        y.clear();
        offset = 0;
    }

    [[nodiscard]] int size() const {
        return static_cast<int>(x.size());
    }
};

/**
 * \brief Frame record captured for scientific CSV export.
 */
struct CsvExportRow {
    double time{0.0};
    float ek{0.0f};
    float ep{0.0f};
    float etotal{0.0f};
    float px{0.0f};
    float py{0.0f};
    float momentum_drift{0.0f};
    float cfl{0.0f};
    int active_contacts{0};
    uint64_t step_collision_impulses{0};
    float max_penetration{0.0f};
    float pos_error{0.0f};
    float vel_error{0.0f};
    bool ballistic_freeflight_valid{false};
};

/**
 * \brief Executable cross-backend verification benchmark result.
 */
struct CrossBackendComparisonResult {
    bool executed{false};
    double cpu_time_ms{0.0};
    double omp_time_ms{0.0};
    double cuda_time_ms{0.0};
    float omp_max_pos_diff{0.0f};
    float cuda_max_pos_diff{0.0f};
    float omp_max_vel_diff{0.0f};
    float cuda_max_vel_diff{0.0f};
    size_t particle_count{0};
    int steps_simulated{0};
    double snapshot_time_s{0.0};
    float snapshot_dt_s{0.0f};
    int snapshot_substeps{0};
    std::string summary;
};

/**
 * \brief Results record for EXP_03: Multi-Timestep Convergence Study.
 */
struct ConvergenceDataPoint {
    float dt{0.0f};
    int substeps{0};
    float error_l2{0.0f};
    float reduction_ratio{0.0f};
    float empirical_order{0.0f};
    double runtime_us{0.0};
};

/**
 * \brief Pre-impact initial state record for EXP_01 (Two-Particle Collision).
 */
struct InitialCollisionSetup {
    sim_core::Vec2 x1{-0.25f, 0.0f};
    sim_core::Vec2 x2{0.25f, 0.0f};
    sim_core::Vec2 v1{1.0f, 0.0f};
    sim_core::Vec2 v2{-1.0f, 0.0f};
    float r1{0.06f};
    float r2{0.06f};
    float m1{0.0f};
    float m2{0.0f};
    float rho{1000.0f};
    float restitution{1.0f};
    float impact_parameter{0.0f}; // vertical offset in y (oblique collision)
};

enum class SolverBackend {
    CpuReference = 0,
    OpenMpParallel = 1,
    CudaGpu = 2
};

/**
 * \brief Central application state binding simulation, UI controls, and diagnostics.
 */
struct AppState {
    // Stage 2: independent investigation and physical scenario selections.
    InvestigationMode investigation_mode{InvestigationMode::PhysicsVerification};
    PhysicsScenario physics_scenario{PhysicsScenario::Collision};
    bool custom_settings_enabled{false};
    bool custom_configuration_dirty{false};

    WorkbenchEvidence stage4_evidence;
    std::vector<sim_core::Vec2> simulated_trajectory;
    // Stage 3: scenario's recommended configuration and its initial particles.
    // No raw pointers; snapshots are owned values, independent of live solver state.
    WorkbenchParameters workbench_defaults{};
    bool has_workbench_defaults{false};
    std::vector<sim_core::Particle> workbench_initial_particles;

    [[nodiscard]] WorkbenchParameters current_workbench_parameters() const {
        WorkbenchParameters p;
        p.restitution = config.restitution;
        p.wall_restitution = config.boundary_restitution;
        p.gravity_x = config.gravity.x;
        p.gravity_y = config.gravity.y;
        p.frame_dt = config.dt;
        p.substeps = config.substeps;
        p.contact_iterations = config.solver_iterations;
        p.stabilization = config.stabilization_factor;
        p.boundary_radius = boundary.radius;
        p.particle_count = static_cast<int>(get_active_particle_count());
        p.integrator = config.integrator == sim_core::IntegratorType::ExplicitEuler ? 1 : 0;
        if (physics_scenario == PhysicsScenario::Collision) {
            p.radius_1 = initial_collision.r1;
            p.radius_2 = initial_collision.r2;
            p.mass_1 = initial_collision.m1;
            p.mass_2 = initial_collision.m2;
            p.impact_offset = initial_collision.impact_parameter;
            p.initial_v1_x = initial_collision.v1.x;
            p.initial_v1_y = initial_collision.v1.y;
            p.initial_v2_x = initial_collision.v2.x;
            p.initial_v2_y = initial_collision.v2.y;
        }
        return p;
    }

    void capture_workbench_start_state() {
        // Presets are authored on the CPU reference, before any dynamics steps.
        workbench_initial_particles = solver.get_particles();
    }

    void capture_workbench_defaults() {
        stage4_evidence = WorkbenchEvidence{};
        workbench_defaults = current_workbench_parameters();
        has_workbench_defaults = true;
        custom_configuration_dirty = false;
    }

    void invalidate_parameter_results() {
        // A previous backend comparison or convergence study belongs to a different
        // configuration. Do not silently represent it as a new run's evidence.
        backend_comparison.executed = false;
        convergence_results.clear();
        stage4_evidence = WorkbenchEvidence{};
        last_export_success = false;
    }

    void note_workbench_parameter_edit() {
        if (has_workbench_defaults) {
            custom_configuration_dirty = parameters_differ(
                workbench_defaults, current_workbench_parameters());
        }
        invalidate_parameter_results();
        // Update all solver parameters WITHOUT replacing the live state on the
        // active CUDA/OpenMP backend with potentially stale CPU particles.
        solver.set_config(config);
        solver.set_boundary(boundary);
        omp_solver.set_config(config);
        omp_solver.set_boundary(boundary);
#if HAS_CUDA_BACKEND
        if (cuda_initialized) {
            cuda_solver.set_config(config);
            cuda_solver.set_boundary(boundary);
        }
#endif
        // One run must not mix observations from different configurations.
        // If a physical/numerical edit happens mid-run, restart at t=0 with
        // identical initial particles and the newly selected parameters.
        if (current_time > 0.0 || frame_index > 0 || !recorded_rows.empty()) {
            reset_run_preserving_parameters();
            show_toast("Settings changed: run restarted, old traces cleared");
        }
    }

    void reset_run_preserving_parameters() {
        // Reset RUN restarts the scenario with current custom settings. In
        // contrast, Restore Defaults reloads the recommended scenario preset.
        if (workbench_initial_particles.empty()) {
            show_toast("No initial scenario snapshot; load a scenario first");
            return;
        }
        solver.set_config(config);
        solver.set_boundary(boundary);
        solver.set_particles(workbench_initial_particles);
        current_time = 0.0;
        frame_index = 0;
        total_substeps = 0;
        is_paused = true;
        step_single_frame = false;
        last_physics_step_ms = 0.0;
        current_cfl = 0.0f;
        current_pos_error = 0.0f;
        current_vel_error = 0.0f;
        reset_buffers();
        invalidate_parameter_results();
        current_diagnostics = solver.compute_diagnostics();
        initial_momentum = current_diagnostics.total_momentum;
        initial_energy = current_diagnostics.total_mechanical_energy;
        measured_initial_ek = current_diagnostics.total_kinetic_energy;
        if (!workbench_initial_particles.empty()) {
            initial_pos1 = workbench_initial_particles.front().position;
            initial_vel1 = workbench_initial_particles.front().velocity;
        }
        if (physics_scenario == PhysicsScenario::Collision) {
            initial_collision.restitution = config.restitution;
            recompute_collision_analytical_solution();
        }
        sync_solvers_from_cpu();
        camera_auto_fit = true;
        custom_configuration_dirty = has_workbench_defaults &&
            parameters_differ(workbench_defaults, current_workbench_parameters());
        show_toast("Run restarted with current parameter values");
    }

    // Progressive Scientific UI Disclosure Mode (Learn, Analyze, Performance)
    LabUiMode ui_mode{LabUiMode::Learn};

    // Current Experiment Mode
    ExperimentType active_experiment{ExperimentType::Collision};

    // Physics Engine & Multi-Backend Architecture (Phase 2 & 3)
    SolverBackend active_backend{SolverBackend::CpuReference};
    sim_core::SimConfig config;
    sim_core::CircularBoundary boundary{sim_core::Vec2(0.0f, 0.0f), 1.0f};
    sim_core::CpuReferenceSolver solver;
    sim_omp::OmpSolverV2 omp_solver;
#if HAS_CUDA_BACKEND
    sim_cuda::CudaSolverV2 cuda_solver;
#endif
    int omp_threads{4};
    bool cuda_initialized{false};
    double last_physics_step_ms{0.0};
    bool show_metrics_window{false};
    int requested_particle_count{256};
    bool scaling_workload_active{false};
    std::vector<sim_core::Particle> scaling_initial_particles;
    bool camera_auto_fit{true};

    void sync_solvers_from_cpu() {
        omp_solver.set_config(config);
        omp_solver.set_boundary(boundary);
        omp_solver.set_num_threads(omp_threads);
        omp_solver.set_particles(solver.get_particles());

#if HAS_CUDA_BACKEND
        if (sim_cuda::CudaSolverV2::is_cuda_available()) {
            cuda_solver.set_config(config);
            cuda_solver.set_boundary(boundary);
            cuda_solver.set_particles(solver.get_particles());
            cuda_initialized = true;
        }
#endif
    }

    void step_active_solver() {
        const auto t0 = std::chrono::high_resolution_clock::now();
#if HAS_CUDA_BACKEND
        if (active_backend == SolverBackend::CudaGpu && cuda_initialized) {
            cuda_solver.set_config(config);
            cuda_solver.set_boundary(boundary);
            cuda_solver.step();
            current_diagnostics = cuda_solver.compute_diagnostics();
        } else
#endif
        if (active_backend == SolverBackend::OpenMpParallel) {
            omp_solver.set_config(config);
            omp_solver.set_boundary(boundary);
            omp_solver.set_num_threads(omp_threads);
            omp_solver.step();
            current_diagnostics = omp_solver.compute_diagnostics();
        } else {
            solver.set_config(config);
            solver.set_boundary(boundary);
            solver.step();
            current_diagnostics = solver.compute_diagnostics();
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        last_physics_step_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }

    const std::vector<sim_core::Particle>& get_active_particles() const {
#if HAS_CUDA_BACKEND
        if (active_backend == SolverBackend::CudaGpu && cuda_initialized) {
            return cuda_solver.get_particles();
        }
#endif
        if (active_backend == SolverBackend::OpenMpParallel) {
            return omp_solver.get_particles();
        }
        return solver.get_particles();
    }

    size_t get_active_particle_count() const {
#if HAS_CUDA_BACKEND
        if (active_backend == SolverBackend::CudaGpu && cuda_initialized) {
            return cuda_solver.particle_count();
        }
#endif
        if (active_backend == SolverBackend::OpenMpParallel) {
            return omp_solver.particle_count();
        }
        return solver.particle_count();
    }

    sim_core::SimulationDiagnostics current_diagnostics;

    // Initial State Anchors for Conservation Laws
    sim_core::Vec2 initial_momentum{0.0f, 0.0f};
    float initial_energy{0.0f};
    sim_core::Vec2 initial_pos1{0.0f, 0.0f};
    sim_core::Vec2 initial_vel1{0.0f, 0.0f};

    // EXP_01 Collision Pre-Impact Anchor & Analytical Model
    InitialCollisionSetup initial_collision;
    bool collision_has_occurred{false};
    double collision_time{0.0};
    sim_core::Vec2 measured_post_v1{0.0f, 0.0f};
    sim_core::Vec2 measured_post_v2{0.0f, 0.0f};
    float measured_initial_ek{0.0f};
    float measured_post_ek{0.0f};
    float measured_dek{0.0f};

    // Analytical Targets
    sim_core::Vec2 analytical_v1_exact{0.0f, 0.0f};
    sim_core::Vec2 analytical_v2_exact{0.0f, 0.0f};
    float analytical_theoretical_dek{0.0f};
    float analytical_reduced_mass{0.0f};
    float analytical_relative_normal_vel{0.0f};
    sim_core::Vec2 analytical_contact_normal{1.0f, 0.0f};

    // Playback and Simulation Clock (Loads paused at t=0)
    bool is_paused{true};
    bool step_single_frame{false};
    float sim_speed{1.0f}; // Speed multiplier
    float sim_accumulator{0.0f}; // Glenn Fiedler fixed-timestep accumulator for fractional speeds
    double current_time{0.0};
    uint64_t total_substeps{0};
    uint64_t frame_index{0};

    // Viewport Visualization Settings
    bool show_grid{true};
    float grid_spacing{0.2f}; // meters
    bool show_boundary{true};
    bool show_velocity_vectors{true};
    float velocity_vector_scale{0.15f}; // vector line length per (m/s)
    bool show_contact_normals{true};
    bool show_analytical_trajectory{true};
    ParticleColorMode color_mode{ParticleColorMode::Material};

    // Viewport Camera Coordinates
    sim_core::Vec2 camera_offset{0.0f, 0.0f}; // Center in meters
    float camera_zoom{360.0f};                 // Screen pixels per meter

    // Time-Series Buffers for Live ImPlot Graphs
    RollingBuffer buf_ek{2400};
    RollingBuffer buf_ep{2400};
    RollingBuffer buf_etotal{2400};
    RollingBuffer buf_drift{2400};
    RollingBuffer buf_cfl{2400};
    RollingBuffer buf_pos_error{2400};
    RollingBuffer buf_vel_error{2400};

    // Analytical Verification Metrics
    float current_pos_error{0.0f};
    float current_vel_error{0.0f};
    float current_cfl{0.0f};
    bool wall_contact_occurred{false};
    double wall_contact_time{0.0};

    // EXP_03: Convergence Study Results
    std::vector<ConvergenceDataPoint> convergence_results;
    bool convergence_study_running{false};

    // Cross-Backend Comparison Results
    CrossBackendComparisonResult backend_comparison;

    // CSV Frame Recording
    std::vector<CsvExportRow> recorded_rows;
    std::string last_export_path;
    bool last_export_success{false};

    // Screenshot Capture Request
    std::string pending_screenshot_path;

    // Notification toast
    std::string toast_message;
    float toast_timer{0.0f};

    void show_toast(const std::string& msg, float duration = 3.5f) {
        toast_message = msg;
        toast_timer = duration;
    }

    void reset_buffers() {
        buf_ek.clear();
        buf_ep.clear();
        buf_etotal.clear();
        buf_drift.clear();
        buf_cfl.clear();
        buf_pos_error.clear();
        buf_vel_error.clear();
        recorded_rows.clear();
        simulated_trajectory.clear();
        backend_comparison.executed = false; // results refer to a previous snapshot
        collision_has_occurred = false;
        collision_time = 0.0;
        measured_post_v1 = {0.0f, 0.0f};
        measured_post_v2 = {0.0f, 0.0f};
        measured_post_ek = 0.0f;
        measured_dek = 0.0f;
        wall_contact_occurred = false;
        wall_contact_time = 0.0;
        sim_accumulator = 0.0f;
    }

    /**
     * \brief Recompute analytical binary collision predictions from initial configuration.
     *        Applies 2D momentum conservation & frictionless normal restitution:
     *        Delta Ek = -0.5 * mu * (1 - e^2) * vn^2
     */
    void recompute_collision_analytical_solution() {
        constexpr float PI = 3.14159265358979323846f;
        const float m1 = (initial_collision.m1 > 0.0f) ? initial_collision.m1
                       : (PI * initial_collision.rho * initial_collision.r1 * initial_collision.r1);
        const float m2 = (initial_collision.m2 > 0.0f) ? initial_collision.m2
                       : (PI * initial_collision.rho * initial_collision.r2 * initial_collision.r2);

        initial_collision.m1 = m1;
        initial_collision.m2 = m2;

        const float m_tot = m1 + m2;
        if (m_tot <= 0.0f) return;

        analytical_reduced_mass = (m1 * m2) / m_tot;

        const float r_sum = initial_collision.r1 + initial_collision.r2;
        const float b = initial_collision.impact_parameter;

        if (std::abs(b) >= r_sum) {
            // Trajectories miss completely - no collision occurs
            analytical_contact_normal = sim_core::Vec2(1.0f, 0.0f);
            analytical_relative_normal_vel = 0.0f;
            analytical_v1_exact = initial_collision.v1;
            analytical_v2_exact = initial_collision.v2;
            analytical_theoretical_dek = 0.0f;
            return;
        }

        // True contact normal at collision moment (when ||x1 - x2|| = r1 + r2)
        const float ny = b / r_sum;
        const float sgn_x = (initial_collision.v1.x > initial_collision.v2.x) ? -1.0f : 1.0f;
        const float nx = sgn_x * std::sqrt(std::max(0.0f, 1.0f - ny * ny));
        analytical_contact_normal = sim_core::Vec2(nx, ny);

        const sim_core::Vec2 v_rel = initial_collision.v1 - initial_collision.v2;
        const float vn = sim_core::Vec2::dot(v_rel, analytical_contact_normal);
        analytical_relative_normal_vel = vn;

        const float e = config.restitution;
        initial_collision.restitution = e;

        if (vn < 0.0f) {
            // Approaching along contact normal -> collision occurs
            const float Jn = -(1.0f + e) * analytical_reduced_mass * vn;
            analytical_v1_exact = initial_collision.v1 + (Jn / m1) * analytical_contact_normal;
            analytical_v2_exact = initial_collision.v2 - (Jn / m2) * analytical_contact_normal;
            analytical_theoretical_dek = -0.5f * analytical_reduced_mass * (1.0f - e * e) * (vn * vn);
        } else {
            // Diverging -> no collision occurs
            analytical_v1_exact = initial_collision.v1;
            analytical_v2_exact = initial_collision.v2;
            analytical_theoretical_dek = 0.0f;
        }
    }

    void run_cross_backend_comparison(int steps = 100) {
        backend_comparison.executed = true;
        backend_comparison.steps_simulated = steps;
        backend_comparison.snapshot_time_s = current_time;
        backend_comparison.snapshot_dt_s = config.dt;
        backend_comparison.snapshot_substeps = config.substeps;

        // Compare the SAME live scenario snapshot across all backends.
        // Do not replace 1- or 2-particle experiments with a synthetic cloud.
        std::vector<sim_core::Particle> benchmark_particles = get_active_particles();
        if (benchmark_particles.empty()) {
            backend_comparison.executed = false;
            show_toast("No particles available for backend comparison");
            return;
        }
        backend_comparison.particle_count = benchmark_particles.size();

        sim_core::SimConfig test_cfg = config;
        // Retain the actual scenario timestep and substep count.
        sim_core::CircularBoundary test_bnd = boundary;

        // 1. CPU Reference Solver
        sim_core::CpuReferenceSolver cpu_test(test_cfg, test_bnd);
        cpu_test.set_particles(benchmark_particles);
        const auto t_cpu_0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < steps; ++i) {
            cpu_test.step();
        }
        const auto t_cpu_1 = std::chrono::high_resolution_clock::now();
        backend_comparison.cpu_time_ms = std::chrono::duration<double, std::milli>(t_cpu_1 - t_cpu_0).count();
        const auto& cpu_pts = cpu_test.get_particles();

        // 2. OpenMP Multicore Solver
        sim_omp::OmpSolverV2 omp_test(test_cfg, test_bnd);
        omp_test.set_broadphase_mode(omp_solver.get_broadphase_mode());
        omp_test.set_num_threads(omp_threads);
        omp_test.set_particles(benchmark_particles);
        const auto t_omp_0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < steps; ++i) {
            omp_test.step();
        }
        const auto t_omp_1 = std::chrono::high_resolution_clock::now();
        backend_comparison.omp_time_ms = std::chrono::duration<double, std::milli>(t_omp_1 - t_omp_0).count();
        const auto& omp_pts = omp_test.get_particles();

        // Compute CPU vs OpenMP deviation
        float omp_max_pos = 0.0f;
        float omp_max_vel = 0.0f;
        for (size_t i = 0; i < cpu_pts.size() && i < omp_pts.size(); ++i) {
            omp_max_pos = std::max(omp_max_pos, (cpu_pts[i].position - omp_pts[i].position).length());
            omp_max_vel = std::max(omp_max_vel, (cpu_pts[i].velocity - omp_pts[i].velocity).length());
        }
        backend_comparison.omp_max_pos_diff = omp_max_pos;
        backend_comparison.omp_max_vel_diff = omp_max_vel;

#if HAS_CUDA_BACKEND
        if (sim_cuda::CudaSolverV2::is_cuda_available()) {
            sim_cuda::CudaSolverV2 cuda_test(test_cfg, test_bnd);
            cuda_test.set_broadphase_mode(cuda_solver.get_broadphase_mode());
            cuda_test.set_particles(benchmark_particles);
            const auto t_cuda_0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < steps; ++i) {
                cuda_test.step();
            }
            const auto t_cuda_1 = std::chrono::high_resolution_clock::now();
            backend_comparison.cuda_time_ms = std::chrono::duration<double, std::milli>(t_cuda_1 - t_cuda_0).count();
            const auto& cuda_pts = cuda_test.get_particles();

            float cuda_max_pos = 0.0f;
            float cuda_max_vel = 0.0f;
            for (size_t i = 0; i < cpu_pts.size() && i < cuda_pts.size(); ++i) {
                cuda_max_pos = std::max(cuda_max_pos, (cpu_pts[i].position - cuda_pts[i].position).length());
                cuda_max_vel = std::max(cuda_max_vel, (cpu_pts[i].velocity - cuda_pts[i].velocity).length());
            }
            backend_comparison.cuda_max_pos_diff = cuda_max_pos;
            backend_comparison.cuda_max_vel_diff = cuda_max_vel;
        } else {
            backend_comparison.cuda_time_ms = -1.0;
        }
#else
        backend_comparison.cuda_time_ms = -1.0;
#endif

        std::stringstream ss;
        ss << "Cross-backend comparison completed: " << backend_comparison.particle_count
           << " particles, " << steps << " steps. CPU: " << std::fixed << std::setprecision(2)
           << backend_comparison.cpu_time_ms << "ms, OpenMP: " << backend_comparison.omp_time_ms << "ms";
        if (backend_comparison.cuda_time_ms > 0.0) {
            ss << ", CUDA: " << backend_comparison.cuda_time_ms << "ms";
        }
        backend_comparison.summary = ss.str();
        show_toast(backend_comparison.summary);
    }

    bool export_recorded_csv(const std::string& filepath) {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        std::ofstream file(filepath);
        if (!file.is_open()) {
            last_export_success = false;
            return false;
        }

        const char* exp_name = "EXP_CUSTOM";
        switch (active_experiment) {
            case ExperimentType::Collision:        exp_name = "EXP_01_TWO_PARTICLE_COLLISION"; break;
            case ExperimentType::GravityBallistic: exp_name = "EXP_02_BALLISTIC_GRAVITY"; break;
            case ExperimentType::ConvergenceStudy: exp_name = "EXP_03_TIMESTEP_CONVERGENCE"; break;
            case ExperimentType::DensePacking:     exp_name = "EXP_04_DENSE_GRANULAR_COLUMN"; break;
            case ExperimentType::Custom:           exp_name = "EXP_CUSTOM"; break;
        }

        const char* backend_name = (active_backend == SolverBackend::CudaGpu ? "CUDA GPU" :
                                    active_backend == SolverBackend::OpenMpParallel ? "OpenMP Multicore" : "CPU Reference");
        const char* mode_name = (ui_mode == LabUiMode::Learn ? "LEARN" :
                                 ui_mode == LabUiMode::Analyze ? "ANALYZE" : "PERFORMANCE");
        const char* integ_name = (config.integrator == sim_core::IntegratorType::ExplicitEuler ?
                                  "Explicit Forward Euler (Non-Symplectic)" : "Semi-implicit Symplectic Euler");

        file << "# Parallel Particle Dynamics Lab: Scientific Experiment Export\n";
        file << "# Experiment: " << exp_name << "\n";
        file << "# UI Mode: " << mode_name << "\n";
        file << "# Active Backend: " << backend_name << "\n";
        file << "# Integrator: " << integ_name << "\n";
        file << "# Total Physical Duration: " << current_time << " s\n";
        file << "# Frame Timestep dt: " << config.dt << " s\n";
        file << "# Integration Substeps: " << config.substeps << "\n";
        file << "# Effective dt_sub: " << config.dt_substep() << " s\n";
        file << "# Gauss-Seidel Solver Iterations K: " << config.solver_iterations << "\n";
        file << "# Penetration Stabilization Factor alpha: " << config.stabilization_factor << "\n";
        file << "# Coefficient of Restitution e: " << config.restitution << "\n";
        file << "# Boundary Restitution e_b: " << config.boundary_restitution << "\n";
        file << "# Gravitational Field g: (" << config.gravity.x << ", " << config.gravity.y << ") m/s^2\n";
        file << "# Particle Count: " << solver.particle_count() << "\n";
        file << "# Coordinate Reference: SI Units (meters, seconds, kilograms, Joules)\n";
        file << "# Initial Particle Configuration [id, x_m, y_m, vx_m_s, vy_m_s, radius_m, mass_kg]:\n";
        for (const auto& pt : (workbench_initial_particles.empty() ?
                                 get_active_particles() : workbench_initial_particles)) {
            file << "#   P" << pt.id << ": pos=(" << pt.position.x << ", " << pt.position.y
                 << "), vel=(" << pt.velocity.x << ", " << pt.velocity.y
                 << "), r=" << pt.radius << ", m=" << pt.mass << "\n";
        }
        file << "time_s,kinetic_energy_J,potential_energy_J,total_energy_J,momentum_x_kg_m_s,momentum_y_kg_m_s,momentum_drift_kg_m_s,cfl_displacement_ratio,active_contacts,step_collision_impulses,max_penetration_m,pos_error_m,vel_error_m_s,ballistic_freeflight_valid\n";

        file << std::fixed << std::setprecision(6);
        for (const auto& row : recorded_rows) {
            file << row.time << ","
                 << row.ek << ","
                 << row.ep << ","
                 << row.etotal << ","
                 << row.px << ","
                 << row.py << ","
                 << row.momentum_drift << ","
                 << row.cfl << ","
                 << row.active_contacts << ","
                 << row.step_collision_impulses << ","
                 << row.max_penetration << ","
                 << row.pos_error << ","
                 << row.vel_error << ","
                 << (row.ballistic_freeflight_valid ? 1 : 0) << "\n";
        }

        last_export_path = filepath;
        last_export_success = true;
        show_toast("Exported " + std::to_string(recorded_rows.size()) + " frames to " + filepath);
        return true;
    }
};

} // namespace lab_gui
