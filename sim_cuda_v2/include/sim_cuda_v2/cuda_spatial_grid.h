#pragma once

#include "sim_cuda_v2/cuda_types.h"

#include <cuda_runtime.h>
#include <cstdint>
#include <cstddef>

namespace sim_cuda {

/**
 * \brief 2D Uniform Spatial Grid parameters mapped onto the device.
 */
struct GpuGridParams {
    float origin_x{-2.0f};
    float origin_y{-2.0f};
    float cell_size{0.1f};
    float inv_cell_size{10.0f};
    int grid_dim_x{40};
    int grid_dim_y{40};
    int total_cells{1600};
};

/**
 * \brief GPU-accelerated Uniform Spatial Grid broadphase.
 *
 * Utilizes parallel hashing, Thrust radix key-sorting, and cell range pointer tables
 * to reduce collision candidate queries from O(N^2) to O(N) with provably zero false negatives.
 */
class CudaSpatialGrid {
public:
    CudaSpatialGrid();
    ~CudaSpatialGrid();

    // Disable copying, allow move
    CudaSpatialGrid(const CudaSpatialGrid&) = delete;
    CudaSpatialGrid& operator=(const CudaSpatialGrid&) = delete;
    CudaSpatialGrid(CudaSpatialGrid&& other) noexcept;
    CudaSpatialGrid& operator=(CudaSpatialGrid&& other) noexcept;

    void allocate(size_t max_particles, size_t max_cells);
    void free_memory();

    void build(const DeviceParticle* d_particles, size_t n, float cell_size,
               float domain_radius, cudaStream_t stream);

    [[nodiscard]] const GpuGridParams& get_params() const { return params_; }
    [[nodiscard]] const uint32_t* get_cell_start() const { return d_cell_start_; }
    [[nodiscard]] const uint32_t* get_cell_end() const { return d_cell_end_; }
    [[nodiscard]] const uint32_t* get_sorted_indices() const { return d_particle_indices_; }

private:
    GpuGridParams params_;
    uint32_t* d_cell_hashes_{nullptr};
    uint32_t* d_particle_indices_{nullptr};
    uint32_t* d_cell_start_{nullptr};
    uint32_t* d_cell_end_{nullptr};
    size_t particle_capacity_{0};
    size_t cell_capacity_{0};
};

} // namespace sim_cuda
