#pragma once

#include "sim_core/boundary.h"
#include "sim_core/contact.h"
#include "sim_core/cpu_reference_solver.h"
#include "sim_core/diagnostics.h"
#include "sim_core/types.h"

#include <vector>
#include <chrono>

namespace sim_omp {

/**
 * \brief Contact update resolution mode.
 */
enum class ContactMode {
    Jacobi,      ///< Thread-safe parallel Jacobi delta accumulation (Default)
    GaussSeidel  ///< Sequential Gauss-Seidel (Available for single-thread baseline comparison)
};

/**
 * \brief Broadphase collision detection algorithm.
 */
enum class BroadphaseMode {
    AllPairs,   ///< O(N^2) parallel pair testing (Exact reference baseline)
    SpatialGrid ///< O(N) uniform spatial hash grid (Task 7 optimization)
};

/**
 * \brief HPC profiling breakdown for a single simulation step.
 */
struct OmpPerformanceMetrics {
    double integrate_time_ms{0.0};
    double broadphase_time_ms{0.0};
    double narrowphase_time_ms{0.0};
    double resolve_time_ms{0.0};
    double boundary_time_ms{0.0};
    double total_step_time_ms{0.0};
    int active_threads{1};
    size_t collision_pairs_checked{0};
    size_t contacts_resolved{0};
};

/**
 * \brief High-Performance Thread-Safe OpenMP Particle Dynamics Solver (Phase 2).
 */
class OmpSolverV2 {
public:
    explicit OmpSolverV2(const sim_core::SimConfig& config = sim_core::SimConfig(),
                         const sim_core::CircularBoundary& boundary = sim_core::CircularBoundary(sim_core::Vec2(0.0f, 0.0f), 1.0f));

    ~OmpSolverV2() = default;

    // Simulation Lifecycle & Particle Management
    void step();
    void reset();
    void add_particle(const sim_core::Particle& p);
    const std::vector<sim_core::Particle>& get_particles() const;
    std::vector<sim_core::Particle>& get_particles();
    void set_particles(const std::vector<sim_core::Particle>& particles);
    void clear_particles() { reset(); }
    void set_config(const sim_core::SimConfig& config);
    const sim_core::SimConfig& get_config() const { return config_; }
    void set_boundary(const sim_core::CircularBoundary& boundary) { boundary_ = boundary; }
    const sim_core::CircularBoundary& get_boundary() const { return boundary_; }
    size_t particle_count() const { return particles_.size(); }
    sim_core::SimulationDiagnostics compute_diagnostics() const;

    // Threading & Mode Controls
    void set_num_threads(int threads);
    int get_num_threads() const { return num_threads_; }

    void set_contact_mode(ContactMode mode) { contact_mode_ = mode; }
    ContactMode get_contact_mode() const { return contact_mode_; }

    void set_broadphase_mode(BroadphaseMode mode) { broadphase_mode_ = mode; }
    BroadphaseMode get_broadphase_mode() const { return broadphase_mode_; }

    // HPC Profiling Telemetry
    const OmpPerformanceMetrics& get_last_metrics() const { return metrics_; }

private:
    void substep(float dt_sub);
    void integrate_particles_omp(float dt_sub);
    void handle_boundaries_omp();
    void resolve_contacts_all_pairs_jacobi();
    void resolve_contacts_all_pairs_gauss_seidel();
    void resolve_contacts_spatial_grid_jacobi();

    void resize_thread_buffers(size_t n, int threads);

    sim_core::SimConfig config_;
    sim_core::CircularBoundary boundary_;
    std::vector<sim_core::Particle> particles_;

    int num_threads_{1};
    ContactMode contact_mode_{ContactMode::Jacobi};
    BroadphaseMode broadphase_mode_{BroadphaseMode::AllPairs};
    OmpPerformanceMetrics metrics_;

    // Thread-local delta buffers for race-free Jacobi accumulation
    // Outer dimension: Thread ID [0 .. num_threads - 1]
    // Inner dimension: Particle index [0 .. N - 1]
    std::vector<std::vector<sim_core::Vec2>> thread_dv_;
    std::vector<std::vector<sim_core::Vec2>> thread_dx_;
    std::vector<std::vector<int>> thread_contact_counts_;
};

} // namespace sim_omp
