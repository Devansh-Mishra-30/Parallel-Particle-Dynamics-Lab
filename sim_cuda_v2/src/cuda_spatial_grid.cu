#include "sim_cuda_v2/cuda_spatial_grid.h"

#include <cuda_runtime.h>
#include <thrust/device_ptr.h>
#include <thrust/execution_policy.h>
#include <thrust/sort.h>

#include <algorithm>
#include <cmath>
#include <iostream>

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = (call); \
        if (err != cudaSuccess) { \
            std::cerr << "[CUDA Error] " << __FILE__ << ":" << __LINE__ \
                      << " code: " << err << " (" << cudaGetErrorString(err) << ")\n"; \
        } \
    } while (0)

namespace sim_cuda {

__global__ void k_calc_particle_hashes(const DeviceParticle* particles, size_t n,
                                      uint32_t* cell_hashes, uint32_t* particle_indices,
                                      GpuGridParams grid) {
    const size_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    const float px = particles[i].pos_x;
    const float py = particles[i].pos_y;

    int cx = static_cast<int>(floorf((px - grid.origin_x) * grid.inv_cell_size));
    int cy = static_cast<int>(floorf((py - grid.origin_y) * grid.inv_cell_size));

    cx = max(0, min(grid.grid_dim_x - 1, cx));
    cy = max(0, min(grid.grid_dim_y - 1, cy));

    const uint32_t hash = static_cast<uint32_t>(cy * grid.grid_dim_x + cx);
    cell_hashes[i] = hash;
    particle_indices[i] = static_cast<uint32_t>(i);
}

__global__ void k_find_cell_bounds(const uint32_t* sorted_hashes, size_t n,
                                  uint32_t* cell_start, uint32_t* cell_end) {
    const size_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    const uint32_t h = sorted_hashes[i];
    const uint32_t h_prev = (i > 0) ? sorted_hashes[i - 1] : 0xFFFFFFFF;

    if (i == 0 || h != h_prev) {
        cell_start[h] = static_cast<uint32_t>(i);
        if (i > 0) {
            cell_end[h_prev] = static_cast<uint32_t>(i);
        }
    }

    if (i == n - 1) {
        cell_end[h] = static_cast<uint32_t>(n);
    }
}

CudaSpatialGrid::CudaSpatialGrid() = default;

CudaSpatialGrid::~CudaSpatialGrid() {
    free_memory();
}

CudaSpatialGrid::CudaSpatialGrid(CudaSpatialGrid&& other) noexcept
    : params_(other.params_),
      d_cell_hashes_(other.d_cell_hashes_),
      d_particle_indices_(other.d_particle_indices_),
      d_cell_start_(other.d_cell_start_),
      d_cell_end_(other.d_cell_end_),
      particle_capacity_(other.particle_capacity_),
      cell_capacity_(other.cell_capacity_) {
    other.d_cell_hashes_ = nullptr;
    other.d_particle_indices_ = nullptr;
    other.d_cell_start_ = nullptr;
    other.d_cell_end_ = nullptr;
    other.particle_capacity_ = 0;
    other.cell_capacity_ = 0;
}

CudaSpatialGrid& CudaSpatialGrid::operator=(CudaSpatialGrid&& other) noexcept {
    if (this != &other) {
        free_memory();
        params_ = other.params_;
        d_cell_hashes_ = other.d_cell_hashes_;
        d_particle_indices_ = other.d_particle_indices_;
        d_cell_start_ = other.d_cell_start_;
        d_cell_end_ = other.d_cell_end_;
        particle_capacity_ = other.particle_capacity_;
        cell_capacity_ = other.cell_capacity_;

        other.d_cell_hashes_ = nullptr;
        other.d_particle_indices_ = nullptr;
        other.d_cell_start_ = nullptr;
        other.d_cell_end_ = nullptr;
        other.particle_capacity_ = 0;
        other.cell_capacity_ = 0;
    }
    return *this;
}

void CudaSpatialGrid::allocate(size_t max_particles, size_t max_cells) {
    if (max_particles > particle_capacity_) {
        if (d_cell_hashes_) cudaFree(d_cell_hashes_);
        if (d_particle_indices_) cudaFree(d_particle_indices_);

        particle_capacity_ = std::max(max_particles, static_cast<size_t>(64));
        CUDA_CHECK(cudaMalloc(&d_cell_hashes_, particle_capacity_ * sizeof(uint32_t)));
        CUDA_CHECK(cudaMalloc(&d_particle_indices_, particle_capacity_ * sizeof(uint32_t)));
    }

    if (max_cells > cell_capacity_) {
        if (d_cell_start_) cudaFree(d_cell_start_);
        if (d_cell_end_) cudaFree(d_cell_end_);

        cell_capacity_ = std::max(max_cells, static_cast<size_t>(256));
        CUDA_CHECK(cudaMalloc(&d_cell_start_, cell_capacity_ * sizeof(uint32_t)));
        CUDA_CHECK(cudaMalloc(&d_cell_end_, cell_capacity_ * sizeof(uint32_t)));
    }
}

void CudaSpatialGrid::free_memory() {
    if (d_cell_hashes_) { cudaFree(d_cell_hashes_); d_cell_hashes_ = nullptr; }
    if (d_particle_indices_) { cudaFree(d_particle_indices_); d_particle_indices_ = nullptr; }
    if (d_cell_start_) { cudaFree(d_cell_start_); d_cell_start_ = nullptr; }
    if (d_cell_end_) { cudaFree(d_cell_end_); d_cell_end_ = nullptr; }
    particle_capacity_ = 0;
    cell_capacity_ = 0;
}

void CudaSpatialGrid::build(const DeviceParticle* d_particles, size_t n, float cell_size,
                            float domain_radius, cudaStream_t stream) {
    if (n == 0) return;

    // Grid dimension calculation
    const float extent = 2.2f * domain_radius;
    params_.origin_x = -1.1f * domain_radius;
    params_.origin_y = -1.1f * domain_radius;
    params_.cell_size = std::max(0.001f, cell_size);
    params_.inv_cell_size = 1.0f / params_.cell_size;
    params_.grid_dim_x = std::max(1, static_cast<int>(ceilf(extent / params_.cell_size)));
    params_.grid_dim_y = std::max(1, static_cast<int>(ceilf(extent / params_.cell_size)));
    params_.total_cells = params_.grid_dim_x * params_.grid_dim_y;

    allocate(n, params_.total_cells);

    // Initialize cell start/end tables to 0xFFFFFFFF
    CUDA_CHECK(cudaMemsetAsync(d_cell_start_, 0xFF, params_.total_cells * sizeof(uint32_t), stream));
    CUDA_CHECK(cudaMemsetAsync(d_cell_end_, 0xFF, params_.total_cells * sizeof(uint32_t), stream));

    constexpr int BLOCK_SIZE = 256;
    const int num_blocks = static_cast<int>((n + BLOCK_SIZE - 1) / BLOCK_SIZE);

    // 1. Calculate hashes
    k_calc_particle_hashes<<<num_blocks, BLOCK_SIZE, 0, stream>>>(
        d_particles, n, d_cell_hashes_, d_particle_indices_, params_);

    // 2. Parallel Radix Sort by Cell Hash using Thrust
    thrust::device_ptr<uint32_t> keys(d_cell_hashes_);
    thrust::device_ptr<uint32_t> vals(d_particle_indices_);
    thrust::sort_by_key(thrust::cuda::par.on(stream), keys, keys + n, vals);

    // 3. Mark cell start and end offsets
    k_find_cell_bounds<<<num_blocks, BLOCK_SIZE, 0, stream>>>(
        d_cell_hashes_, n, d_cell_start_, d_cell_end_);
}

} // namespace sim_cuda
