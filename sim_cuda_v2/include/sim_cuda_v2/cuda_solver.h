#pragma once

#include "sim_core/boundary.h"
#include "sim_core/diagnostics.h"
#include "sim_core/types.h"
#include "sim_cuda_v2/cuda_types.h"

#include <memory>
#include <vector>

namespace sim_cuda {

// Forward declaration of internal CUDA device buffers
struct CudaDeviceBuffers;

/**
 * \brief High-Performance NVIDIA CUDA GPU Physics Engine (Phase 3).
 *
 * Implements identical governing equations to CpuReferenceSolver and OmpSolverV2:
 *  - Symplectic Euler numerical integration
 *  - Mass-weighted Baumgarte penetration stabilization
 *  - Frictionless normal collision impulses
 *  - Circular container boundaries
 *  - Real-time diagnostic reductions on GPU
 */
class CudaSolverV2 {
public:
    explicit CudaSolverV2(const sim_core::SimConfig& config = sim_core::SimConfig(),
                          const sim_core::CircularBoundary& boundary = sim_core::CircularBoundary(),
                          int device_id = 0);
    ~CudaSolverV2();

    // Move semantics enabled, non-copyable due to GPU stream/buffer ownership
    CudaSolverV2(const CudaSolverV2&) = delete;
    CudaSolverV2& operator=(const CudaSolverV2&) = delete;
    CudaSolverV2(CudaSolverV2&&) noexcept;
    CudaSolverV2& operator=(CudaSolverV2&&) noexcept;

    // --- Configuration & Boundaries ---
    void set_config(const sim_core::SimConfig& config);
    [[nodiscard]] const sim_core::SimConfig& get_config() const { return config_; }

    void set_boundary(const sim_core::CircularBoundary& boundary);
    [[nodiscard]] const sim_core::CircularBoundary& get_boundary() const { return boundary_; }

    // --- Particle State Management (Host <-> Device) ---
    void set_particles(const std::vector<sim_core::Particle>& particles);
    [[nodiscard]] const std::vector<sim_core::Particle>& get_particles() const;
    [[nodiscard]] size_t particle_count() const { return host_particles_.size(); }

    // --- Numerical Simulation Step ---
    void step();

    // --- Physical Invariant Diagnostics ---
    [[nodiscard]] sim_core::SimulationDiagnostics compute_diagnostics();

    // --- Hardware Query & Configuration ---
    [[nodiscard]] static bool is_cuda_available();
    [[nodiscard]] static CudaDeviceSpecs query_device_specs(int device_id = 0);
    [[nodiscard]] const CudaDeviceSpecs& get_device_specs() const { return device_specs_; }

    void set_broadphase_mode(CudaBroadphaseMode mode) { broadphase_mode_ = mode; }
    [[nodiscard]] CudaBroadphaseMode get_broadphase_mode() const { return broadphase_mode_; }

    void set_collision_method(CudaCollisionMethod method) { collision_method_ = method; }
    [[nodiscard]] CudaCollisionMethod get_collision_method() const { return collision_method_; }

    [[nodiscard]] const CudaPerformanceMetrics& get_last_metrics() const { return metrics_; }

private:
    void allocate_device_memory(size_t capacity);
    void free_device_memory();
    void sync_h2d();
    void sync_d2h() const;

    sim_core::SimConfig config_;
    sim_core::CircularBoundary boundary_;
    CudaDeviceSpecs device_specs_;
    CudaBroadphaseMode broadphase_mode_{CudaBroadphaseMode::AllPairs};
    CudaCollisionMethod collision_method_{CudaCollisionMethod::PerParticleGather};

    mutable std::vector<sim_core::Particle> host_particles_;
    mutable bool host_particles_dirty_{false};

    CudaPerformanceMetrics metrics_;
    std::unique_ptr<CudaDeviceBuffers> device_buffers_;
};

} // namespace sim_cuda
