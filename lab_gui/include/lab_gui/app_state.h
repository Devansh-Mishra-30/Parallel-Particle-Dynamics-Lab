#pragma once

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
    float max_penetration{0.0f};
    float pos_error{0.0f};
    float vel_error{0.0f};
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

    // Playback and Simulation Clock
    bool is_paused{false};
    bool step_single_frame{false};
    float sim_speed{1.0f}; // Speed multiplier
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

    // EXP_03: Convergence Study Results
    std::vector<ConvergenceDataPoint> convergence_results;
    bool convergence_study_running{false};

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
        collision_has_occurred = false;
        collision_time = 0.0;
        measured_post_v1 = {0.0f, 0.0f};
        measured_post_v2 = {0.0f, 0.0f};
        measured_post_ek = 0.0f;
        measured_dek = 0.0f;
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

        file << "# Parallel Particle Dynamics Lab: Scientific Experiment Export\n";
        file << "# Experiment: " << exp_name << "\n";
        file << "# Integrator: Semi-implicit Symplectic Euler\n";
        file << "# Total Physical Duration: " << current_time << " s\n";
        file << "# Frame Timestep dt: " << config.dt << " s\n";
        file << "# Integration Substeps: " << config.substeps << "\n";
        file << "# Effective dt_sub: " << config.dt_substep() << " s\n";
        file << "# Coefficient of Restitution e: " << config.restitution << "\n";
        file << "# Boundary Restitution e_b: " << config.boundary_restitution << "\n";
        file << "# Gravitational Field g: (" << config.gravity.x << ", " << config.gravity.y << ") m/s^2\n";
        file << "# Particle Count: " << solver.particle_count() << "\n";
        file << "# Coordinate Reference: SI Units (meters, seconds, kilograms, Joules)\n";
        file << "time_s,kinetic_energy_J,potential_energy_J,total_energy_J,momentum_x_kg_m_s,momentum_y_kg_m_s,momentum_drift_kg_m_s,cfl_displacement_ratio,active_contacts,max_penetration_m,pos_error_m,vel_error_m_s\n";

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
                 << row.max_penetration << ","
                 << row.pos_error << ","
                 << row.vel_error << "\n";
        }

        last_export_path = filepath;
        last_export_success = true;
        show_toast("Exported " + std::to_string(recorded_rows.size()) + " frames to " + filepath);
        return true;
    }
};

} // namespace lab_gui
