#pragma once

#include "sim_core/types.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace sim_cuda {

/**
 * \brief Hardware capabilities and telemetry for NVIDIA GPU devices.
 */
struct CudaDeviceSpecs {
    bool available{false};
    int device_id{0};
    std::string device_name{"None"};
    int compute_major{0};
    int compute_minor{0};
    size_t total_memory_bytes{0};
    int multiprocessor_count{0};
    int warp_size{32};
    int max_threads_per_block{1024};
    int max_threads_per_sm{1536};
    size_t shared_mem_per_block{0};
    int clock_rate_khz{0};
    int memory_bus_width_bits{0};
    size_t l2_cache_bytes{0};

    [[nodiscard]] std::string compute_capability_str() const {
        return std::to_string(compute_major) + "." + std::to_string(compute_minor);
    }

    [[nodiscard]] double total_memory_mb() const {
        return static_cast<double>(total_memory_bytes) / (1024.0 * 1024.0);
    }
};

/**
 * \brief Broadphase collision candidate discovery strategy on GPU.
 */
enum class CudaBroadphaseMode {
    AllPairs = 0,     // O(N^2) GPU brute-force all-pairs
    SpatialGrid = 1   // O(N) GPU uniform spatial grid
};

/**
 * \brief Contact resolution impulse accumulation strategy on GPU.
 */
enum class CudaCollisionMethod {
    PerParticleGather = 0, // Thread per particle gathering impulses from neighbors (zero write races)
    PairwiseAtomic = 1     // Thread per pair scattering impulses via atomicAdd
};

/**
 * \brief Fine-grained kernel timing telemetry measured via CUDA events.
 */
struct CudaPerformanceMetrics {
    double integrate_time_ms{0.0};
    double broadphase_time_ms{0.0};
    double narrowphase_time_ms{0.0};
    double resolve_time_ms{0.0};
    double boundary_time_ms{0.0};
    double d2h_transfer_ms{0.0};
    double h2d_transfer_ms{0.0};
    double total_step_gpu_ms{0.0};
    size_t collision_pairs_checked{0};
    size_t active_contacts_found{0};
};

/**
 * \brief Compact POD structure for GPU device memory representation.
 */
struct DeviceParticle {
    float pos_x;
    float pos_y;
    float vel_x;
    float vel_y;
    float radius;
    float mass;
    float inv_mass;
    uint32_t id;
};

} // namespace sim_cuda
