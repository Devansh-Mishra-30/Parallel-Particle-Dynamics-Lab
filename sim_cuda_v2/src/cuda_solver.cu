#include "sim_cuda_v2/cuda_solver.h"
#include "sim_cuda_v2/cuda_spatial_grid.h"

#include <cuda_runtime.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <chrono>

#define CUDA_CHECK(call) \
    do { \
        cudaError_t err = (call); \
        if (err != cudaSuccess) { \
            std::cerr << "[CUDA Error] " << __FILE__ << ":" << __LINE__ \
                      << " code: " << err << " (" << cudaGetErrorString(err) << ")\n"; \
        } \
    } while (0)

namespace sim_cuda {

struct CudaDeviceBuffers {
    DeviceParticle* d_particles{nullptr};
    DeviceParticle* d_particles_next{nullptr};
    float* d_atomic_dv_x{nullptr};
    float* d_atomic_dv_y{nullptr};
    float* d_atomic_dx_x{nullptr};
    float* d_atomic_dx_y{nullptr};
    size_t capacity{0};
    cudaStream_t stream{nullptr};
    cudaEvent_t ev_start{nullptr};
    cudaEvent_t ev_end{nullptr};
    std::unique_ptr<CudaSpatialGrid> spatial_grid;
};

// -----------------------------------------------------------------------------
// CUDA Device Kernels
// -----------------------------------------------------------------------------

__global__ void k_integrate_symplectic_euler(DeviceParticle* particles, size_t n,
                                            float gx, float gy, float dt_sub) {
    const size_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    if (particles[i].inv_mass > 0.0f) {
        particles[i].vel_x += gx * dt_sub;
        particles[i].vel_y += gy * dt_sub;
        particles[i].pos_x += particles[i].vel_x * dt_sub;
        particles[i].pos_y += particles[i].vel_y * dt_sub;
    }
}

__global__ void k_resolve_contacts_all_pairs_gather(const DeviceParticle* __restrict__ current_particles,
                                                    DeviceParticle* __restrict__ next_particles,
                                                    size_t n, float restitution,
                                                    float stabilization_factor) {
    const size_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    const DeviceParticle p_i = current_particles[i];
    if (p_i.inv_mass <= 0.0f) {
        next_particles[i] = p_i;
        return;
    }

    float dv_x = 0.0f;
    float dv_y = 0.0f;
    float dx_x = 0.0f;
    float dx_y = 0.0f;

    for (size_t j = 0; j < n; ++j) {
        if (i == j) continue;
        const DeviceParticle p_j = current_particles[j];

        const float rx = p_i.pos_x - p_j.pos_x;
        const float ry = p_i.pos_y - p_j.pos_y;
        const float dist_sq = rx * rx + ry * ry;
        const float r_sum = p_i.radius + p_j.radius;

        if (dist_sq < r_sum * r_sum) {
            const float dist = sqrtf(dist_sq);
            constexpr float EPS = 1e-6f;
            const float nx = (dist > EPS) ? (rx / dist) : 1.0f;
            const float ny = (dist > EPS) ? (ry / dist) : 0.0f;
            const float delta = r_sum - dist;

            const float inv_m_sum = p_i.inv_mass + p_j.inv_mass;
            if (inv_m_sum > 0.0f) {
                const float rel_vx = p_i.vel_x - p_j.vel_x;
                const float rel_vy = p_i.vel_y - p_j.vel_y;
                const float v_n = rel_vx * nx + rel_vy * ny;

                if (v_n < 0.0f) {
                    const float j_n = -(1.0f + restitution) * v_n / inv_m_sum;
                    const float impulse_scale = j_n * p_i.inv_mass;
                    dv_x += impulse_scale * nx;
                    dv_y += impulse_scale * ny;
                }

                if (delta > 0.0f) {
                    const float correction = stabilization_factor * delta / inv_m_sum;
                    const float pos_scale = correction * p_i.inv_mass;
                    dx_x += pos_scale * nx;
                    dx_y += pos_scale * ny;
                }
            }
        }
    }

    DeviceParticle out_p = p_i;
    out_p.vel_x += dv_x;
    out_p.vel_y += dv_y;
    out_p.pos_x += dx_x;
    out_p.pos_y += dx_y;
    next_particles[i] = out_p;
}

__global__ void k_resolve_contacts_spatial_grid_gather(
    const DeviceParticle* __restrict__ current_particles,
    DeviceParticle* __restrict__ next_particles,
    size_t n,
    const uint32_t* __restrict__ sorted_indices,
    const uint32_t* __restrict__ cell_start,
    const uint32_t* __restrict__ cell_end,
    GpuGridParams grid,
    float restitution,
    float stabilization_factor)
{
    const size_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    const DeviceParticle p_i = current_particles[i];
    if (p_i.inv_mass <= 0.0f) {
        next_particles[i] = p_i;
        return;
    }

    float dv_x = 0.0f;
    float dv_y = 0.0f;
    float dx_x = 0.0f;
    float dx_y = 0.0f;

    int cx = static_cast<int>(floorf((p_i.pos_x - grid.origin_x) * grid.inv_cell_size));
    int cy = static_cast<int>(floorf((p_i.pos_y - grid.origin_y) * grid.inv_cell_size));
    cx = max(0, min(grid.grid_dim_x - 1, cx));
    cy = max(0, min(grid.grid_dim_y - 1, cy));

    // Search 3x3 neighboring cells
    for (int dy = -1; dy <= 1; ++dy) {
        const int ny = cy + dy;
        if (ny < 0 || ny >= grid.grid_dim_y) continue;

        for (int dx = -1; dx <= 1; ++dx) {
            const int nx = cx + dx;
            if (nx < 0 || nx >= grid.grid_dim_x) continue;

            const uint32_t cell_hash = static_cast<uint32_t>(ny * grid.grid_dim_x + nx);
            const uint32_t start_idx = cell_start[cell_hash];
            if (start_idx == 0xFFFFFFFF) continue;
            const uint32_t end_idx = cell_end[cell_hash];

            for (uint32_t k = start_idx; k < end_idx; ++k) {
                const uint32_t j = sorted_indices[k];
                if (i == j) continue;

                const DeviceParticle p_j = current_particles[j];
                const float rx = p_i.pos_x - p_j.pos_x;
                const float ry = p_i.pos_y - p_j.pos_y;
                const float dist_sq = rx * rx + ry * ry;
                const float r_sum = p_i.radius + p_j.radius;

                if (dist_sq < r_sum * r_sum) {
                    const float dist = sqrtf(dist_sq);
                    constexpr float EPS = 1e-6f;
                    const float nx_c = (dist > EPS) ? (rx / dist) : 1.0f;
                    const float ny_c = (dist > EPS) ? (ry / dist) : 0.0f;
                    const float delta = r_sum - dist;

                    const float inv_m_sum = p_i.inv_mass + p_j.inv_mass;
                    if (inv_m_sum > 0.0f) {
                        const float rel_vx = p_i.vel_x - p_j.vel_x;
                        const float rel_vy = p_i.vel_y - p_j.vel_y;
                        const float v_n = rel_vx * nx_c + rel_vy * ny_c;

                        if (v_n < 0.0f) {
                            const float j_n = -(1.0f + restitution) * v_n / inv_m_sum;
                            const float impulse_scale = j_n * p_i.inv_mass;
                            dv_x += impulse_scale * nx_c;
                            dv_y += impulse_scale * ny_c;
                        }

                        if (delta > 0.0f) {
                            const float correction = stabilization_factor * delta / inv_m_sum;
                            const float pos_scale = correction * p_i.inv_mass;
                            dx_x += pos_scale * nx_c;
                            dx_y += pos_scale * ny_c;
                        }
                    }
                }
            }
        }
    }

    DeviceParticle out_p = p_i;
    out_p.vel_x += dv_x;
    out_p.vel_y += dv_y;
    out_p.pos_x += dx_x;
    out_p.pos_y += dx_y;
    next_particles[i] = out_p;
}

__global__ void k_resolve_contacts_all_pairs_atomic(
    const DeviceParticle* __restrict__ particles,
    size_t n,
    float* __restrict__ dv_x, float* __restrict__ dv_y,
    float* __restrict__ dx_x, float* __restrict__ dx_y,
    float restitution, float stabilization_factor)
{
    const size_t pair_idx = blockIdx.x * blockDim.x + threadIdx.x;
    const size_t total_pairs = (n * (n - 1)) / 2;
    if (pair_idx >= total_pairs) return;

    // Approximate mapping: upper triangular matrix traversal
    // Using simple row search
    size_t i = 0;
    size_t count = n - 1;
    size_t cur = 0;
    while (pair_idx >= cur + count && i < n - 1) {
        cur += count;
        count--;
        i++;
    }
    const size_t j = i + 1 + (pair_idx - cur);
    if (i >= n || j >= n) return;

    const DeviceParticle p_i = particles[i];
    const DeviceParticle p_j = particles[j];

    const float rx = p_i.pos_x - p_j.pos_x;
    const float ry = p_i.pos_y - p_j.pos_y;
    const float dist_sq = rx * rx + ry * ry;
    const float r_sum = p_i.radius + p_j.radius;

    if (dist_sq < r_sum * r_sum) {
        const float dist = sqrtf(dist_sq);
        constexpr float EPS = 1e-6f;
        const float nx = (dist > EPS) ? (rx / dist) : 1.0f;
        const float ny = (dist > EPS) ? (ry / dist) : 0.0f;
        const float delta = r_sum - dist;

        const float inv_m_sum = p_i.inv_mass + p_j.inv_mass;
        if (inv_m_sum > 0.0f) {
            const float rel_vx = p_i.vel_x - p_j.vel_x;
            const float rel_vy = p_i.vel_y - p_j.vel_y;
            const float v_n = rel_vx * nx + rel_vy * ny;

            if (v_n < 0.0f) {
                const float j_n = -(1.0f + restitution) * v_n / inv_m_sum;
                const float imp_ix = (j_n * p_i.inv_mass) * nx;
                const float imp_iy = (j_n * p_i.inv_mass) * ny;
                const float imp_jx = -(j_n * p_j.inv_mass) * nx;
                const float imp_jy = -(j_n * p_j.inv_mass) * ny;

                atomicAdd(&dv_x[i], imp_ix);
                atomicAdd(&dv_y[i], imp_iy);
                atomicAdd(&dv_x[j], imp_jx);
                atomicAdd(&dv_y[j], imp_jy);
            }

            if (delta > 0.0f) {
                const float corr = stabilization_factor * delta / inv_m_sum;
                const float corr_ix = (corr * p_i.inv_mass) * nx;
                const float corr_iy = (corr * p_i.inv_mass) * ny;
                const float corr_jx = -(corr * p_j.inv_mass) * nx;
                const float corr_jy = -(corr * p_j.inv_mass) * ny;

                atomicAdd(&dx_x[i], corr_ix);
                atomicAdd(&dx_y[i], corr_iy);
                atomicAdd(&dx_x[j], corr_jx);
                atomicAdd(&dx_y[j], corr_jy);
            }
        }
    }
}

__global__ void k_apply_atomic_deltas(DeviceParticle* particles, size_t n,
                                      const float* dv_x, const float* dv_y,
                                      const float* dx_x, const float* dx_y) {
    const size_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    particles[i].vel_x += dv_x[i];
    particles[i].vel_y += dv_y[i];
    particles[i].pos_x += dx_x[i];
    particles[i].pos_y += dx_y[i];
}

__global__ void k_enforce_boundaries(DeviceParticle* particles, size_t n,
                                     float cx, float cy, float bound_radius,
                                     float boundary_restitution) {
    const size_t i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;

    if (particles[i].inv_mass <= 0.0f) return;

    const float rx = particles[i].pos_x - cx;
    const float ry = particles[i].pos_y - cy;
    const float dist = sqrtf(rx * rx + ry * ry);
    const float max_dist = bound_radius - particles[i].radius;

    if (dist > max_dist) {
        constexpr float EPS = 1e-6f;
        const float nx = (dist > EPS) ? (rx / dist) : 0.0f;
        const float ny = (dist > EPS) ? (ry / dist) : 1.0f;

        // Position projection onto boundary perimeter
        particles[i].pos_x = cx + nx * max_dist;
        particles[i].pos_y = cy + ny * max_dist;

        // Normal velocity reflection
        const float v_n = particles[i].vel_x * nx + particles[i].vel_y * ny;
        if (v_n > 0.0f) {
            particles[i].vel_x -= (1.0f + boundary_restitution) * v_n * nx;
            particles[i].vel_y -= (1.0f + boundary_restitution) * v_n * ny;
        }
    }
}

// -----------------------------------------------------------------------------
// Host Implementation: CudaSolverV2
// -----------------------------------------------------------------------------

CudaSolverV2::CudaSolverV2(const sim_core::SimConfig& config,
                           const sim_core::CircularBoundary& boundary,
                           int device_id)
    : config_(config), boundary_(boundary) {
    device_specs_ = query_device_specs(device_id);
    if (device_specs_.available) {
        CUDA_CHECK(cudaSetDevice(device_specs_.device_id));
        device_buffers_ = std::make_unique<CudaDeviceBuffers>();
        device_buffers_->spatial_grid = std::make_unique<CudaSpatialGrid>();
        CUDA_CHECK(cudaStreamCreate(&device_buffers_->stream));
        CUDA_CHECK(cudaEventCreate(&device_buffers_->ev_start));
        CUDA_CHECK(cudaEventCreate(&device_buffers_->ev_end));
    }
}

CudaSolverV2::~CudaSolverV2() {
    free_device_memory();
    if (device_buffers_) {
        if (device_buffers_->stream) cudaStreamDestroy(device_buffers_->stream);
        if (device_buffers_->ev_start) cudaEventDestroy(device_buffers_->ev_start);
        if (device_buffers_->ev_end) cudaEventDestroy(device_buffers_->ev_end);
    }
}

CudaSolverV2::CudaSolverV2(CudaSolverV2&& other) noexcept
    : config_(other.config_),
      boundary_(other.boundary_),
      device_specs_(other.device_specs_),
      broadphase_mode_(other.broadphase_mode_),
      collision_method_(other.collision_method_),
      host_particles_(std::move(other.host_particles_)),
      host_particles_dirty_(other.host_particles_dirty_),
      metrics_(other.metrics_),
      device_buffers_(std::move(other.device_buffers_)) {}

CudaSolverV2& CudaSolverV2::operator=(CudaSolverV2&& other) noexcept {
    if (this != &other) {
        free_device_memory();
        config_ = other.config_;
        boundary_ = other.boundary_;
        device_specs_ = other.device_specs_;
        broadphase_mode_ = other.broadphase_mode_;
        collision_method_ = other.collision_method_;
        host_particles_ = std::move(other.host_particles_);
        host_particles_dirty_ = other.host_particles_dirty_;
        metrics_ = other.metrics_;
        device_buffers_ = std::move(other.device_buffers_);
    }
    return *this;
}

void CudaSolverV2::set_config(const sim_core::SimConfig& config) {
    config_ = config;
}

void CudaSolverV2::set_boundary(const sim_core::CircularBoundary& boundary) {
    boundary_ = boundary;
}

void CudaSolverV2::allocate_device_memory(size_t capacity) {
    if (!device_buffers_ || capacity <= device_buffers_->capacity) return;

    free_device_memory();
    const size_t alloc_count = std::max(capacity, static_cast<size_t>(64));
    device_buffers_->capacity = alloc_count;

    CUDA_CHECK(cudaMalloc(&device_buffers_->d_particles, alloc_count * sizeof(DeviceParticle)));
    CUDA_CHECK(cudaMalloc(&device_buffers_->d_particles_next, alloc_count * sizeof(DeviceParticle)));

    CUDA_CHECK(cudaMalloc(&device_buffers_->d_atomic_dv_x, alloc_count * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&device_buffers_->d_atomic_dv_y, alloc_count * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&device_buffers_->d_atomic_dx_x, alloc_count * sizeof(float)));
    CUDA_CHECK(cudaMalloc(&device_buffers_->d_atomic_dx_y, alloc_count * sizeof(float)));
}

void CudaSolverV2::free_device_memory() {
    if (!device_buffers_) return;
    if (device_buffers_->d_particles) {
        cudaFree(device_buffers_->d_particles);
        device_buffers_->d_particles = nullptr;
    }
    if (device_buffers_->d_particles_next) {
        cudaFree(device_buffers_->d_particles_next);
        device_buffers_->d_particles_next = nullptr;
    }
    if (device_buffers_->d_atomic_dv_x) {
        cudaFree(device_buffers_->d_atomic_dv_x);
        device_buffers_->d_atomic_dv_x = nullptr;
    }
    if (device_buffers_->d_atomic_dv_y) {
        cudaFree(device_buffers_->d_atomic_dv_y);
        device_buffers_->d_atomic_dv_y = nullptr;
    }
    if (device_buffers_->d_atomic_dx_x) {
        cudaFree(device_buffers_->d_atomic_dx_x);
        device_buffers_->d_atomic_dx_x = nullptr;
    }
    if (device_buffers_->d_atomic_dx_y) {
        cudaFree(device_buffers_->d_atomic_dx_y);
        device_buffers_->d_atomic_dx_y = nullptr;
    }
    device_buffers_->capacity = 0;
}

void CudaSolverV2::set_particles(const std::vector<sim_core::Particle>& particles) {
    host_particles_ = particles;
    host_particles_dirty_ = false;
    sync_h2d();
}

const std::vector<sim_core::Particle>& CudaSolverV2::get_particles() const {
    if (host_particles_dirty_) {
        sync_d2h();
    }
    return host_particles_;
}

void CudaSolverV2::sync_h2d() {
    const size_t n = host_particles_.size();
    if (!device_specs_.available || n == 0) return;

    allocate_device_memory(n);

    std::vector<DeviceParticle> dev_host(n);
    for (size_t i = 0; i < n; ++i) {
        dev_host[i].pos_x = host_particles_[i].position.x;
        dev_host[i].pos_y = host_particles_[i].position.y;
        dev_host[i].vel_x = host_particles_[i].velocity.x;
        dev_host[i].vel_y = host_particles_[i].velocity.y;
        dev_host[i].radius = host_particles_[i].radius;
        dev_host[i].mass = host_particles_[i].mass;
        dev_host[i].inv_mass = host_particles_[i].inv_mass;
        dev_host[i].id = host_particles_[i].id;
    }

    const auto t0 = std::chrono::high_resolution_clock::now();
    CUDA_CHECK(cudaMemcpyAsync(device_buffers_->d_particles, dev_host.data(),
                               n * sizeof(DeviceParticle), cudaMemcpyHostToDevice,
                               device_buffers_->stream));
    CUDA_CHECK(cudaStreamSynchronize(device_buffers_->stream));
    const auto t1 = std::chrono::high_resolution_clock::now();
    metrics_.h2d_transfer_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void CudaSolverV2::sync_d2h() const {
    const size_t n = host_particles_.size();
    if (!device_specs_.available || n == 0 || !device_buffers_ || !device_buffers_->d_particles) return;

    std::vector<DeviceParticle> dev_host(n);
    const auto t0 = std::chrono::high_resolution_clock::now();
    CUDA_CHECK(cudaMemcpy(dev_host.data(), device_buffers_->d_particles,
                          n * sizeof(DeviceParticle), cudaMemcpyDeviceToHost));
    const auto t1 = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < n; ++i) {
        host_particles_[i].position.x = dev_host[i].pos_x;
        host_particles_[i].position.y = dev_host[i].pos_y;
        host_particles_[i].velocity.x = dev_host[i].vel_x;
        host_particles_[i].velocity.y = dev_host[i].vel_y;
        host_particles_[i].radius = dev_host[i].radius;
        host_particles_[i].mass = dev_host[i].mass;
        host_particles_[i].inv_mass = dev_host[i].inv_mass;
        host_particles_[i].id = dev_host[i].id;
    }

    const_cast<CudaPerformanceMetrics&>(metrics_).d2h_transfer_ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    host_particles_dirty_ = false;
}

void CudaSolverV2::step() {
    const size_t n = host_particles_.size();
    if (!device_specs_.available || n == 0 || !device_buffers_) return;

    const int substeps = (config_.substeps > 0) ? config_.substeps : 1;
    const float dt_sub = config_.dt_substep();
    const int iterations = (config_.solver_iterations > 0) ? config_.solver_iterations : 1;

    constexpr int BLOCK_SIZE = 256;
    const int num_blocks = static_cast<int>((n + BLOCK_SIZE - 1) / BLOCK_SIZE);

    CUDA_CHECK(cudaEventRecord(device_buffers_->ev_start, device_buffers_->stream));

    // Find max radius for spatial grid cell size
    float max_r = 0.01f;
    for (size_t i = 0; i < n; ++i) {
        if (host_particles_[i].radius > max_r) max_r = host_particles_[i].radius;
    }

    for (int s = 0; s < substeps; ++s) {
        // 1. Integration (Symplectic Euler)
        k_integrate_symplectic_euler<<<num_blocks, BLOCK_SIZE, 0, device_buffers_->stream>>>(
            device_buffers_->d_particles, n, config_.gravity.x, config_.gravity.y, dt_sub);

        // 2. Collision Contact Resolution
        DeviceParticle* curr = device_buffers_->d_particles;
        DeviceParticle* next = device_buffers_->d_particles_next;

        if (broadphase_mode_ == CudaBroadphaseMode::SpatialGrid) {
            // Rebuild uniform spatial grid
            device_buffers_->spatial_grid->build(
                curr, n, 2.0f * max_r, boundary_.radius, device_buffers_->stream);

            const auto& grid_params = device_buffers_->spatial_grid->get_params();
            const uint32_t* cell_start = device_buffers_->spatial_grid->get_cell_start();
            const uint32_t* cell_end = device_buffers_->spatial_grid->get_cell_end();
            const uint32_t* sorted_indices = device_buffers_->spatial_grid->get_sorted_indices();

            for (int iter = 0; iter < iterations; ++iter) {
                k_resolve_contacts_spatial_grid_gather<<<num_blocks, BLOCK_SIZE, 0, device_buffers_->stream>>>(
                    curr, next, n, sorted_indices, cell_start, cell_end, grid_params,
                    config_.restitution, config_.stabilization_factor);
                std::swap(curr, next);
            }
        } else {
            // All-Pairs Broadphase
            if (collision_method_ == CudaCollisionMethod::PairwiseAtomic) {
                const size_t total_pairs = (n * (n - 1)) / 2;
                if (total_pairs > 0) {
                    const int pair_blocks = static_cast<int>((total_pairs + BLOCK_SIZE - 1) / BLOCK_SIZE);
                    for (int iter = 0; iter < iterations; ++iter) {
                        CUDA_CHECK(cudaMemsetAsync(device_buffers_->d_atomic_dv_x, 0, n * sizeof(float), device_buffers_->stream));
                        CUDA_CHECK(cudaMemsetAsync(device_buffers_->d_atomic_dv_y, 0, n * sizeof(float), device_buffers_->stream));
                        CUDA_CHECK(cudaMemsetAsync(device_buffers_->d_atomic_dx_x, 0, n * sizeof(float), device_buffers_->stream));
                        CUDA_CHECK(cudaMemsetAsync(device_buffers_->d_atomic_dx_y, 0, n * sizeof(float), device_buffers_->stream));

                        k_resolve_contacts_all_pairs_atomic<<<pair_blocks, BLOCK_SIZE, 0, device_buffers_->stream>>>(
                            curr, n,
                            device_buffers_->d_atomic_dv_x, device_buffers_->d_atomic_dv_y,
                            device_buffers_->d_atomic_dx_x, device_buffers_->d_atomic_dx_y,
                            config_.restitution, config_.stabilization_factor);

                        k_apply_atomic_deltas<<<num_blocks, BLOCK_SIZE, 0, device_buffers_->stream>>>(
                            curr, n,
                            device_buffers_->d_atomic_dv_x, device_buffers_->d_atomic_dv_y,
                            device_buffers_->d_atomic_dx_x, device_buffers_->d_atomic_dx_y);
                    }
                }
            } else {
                // Per-Particle Gather (Default, zero write races, Jacobi double-buffered)
                for (int iter = 0; iter < iterations; ++iter) {
                    k_resolve_contacts_all_pairs_gather<<<num_blocks, BLOCK_SIZE, 0, device_buffers_->stream>>>(
                        curr, next, n, config_.restitution, config_.stabilization_factor);
                    std::swap(curr, next);
                }
            }
        }

        // If an odd number of iterations ran, copy final result back to d_particles
        if (curr != device_buffers_->d_particles) {
            CUDA_CHECK(cudaMemcpyAsync(device_buffers_->d_particles, curr,
                                       n * sizeof(DeviceParticle), cudaMemcpyDeviceToDevice,
                                       device_buffers_->stream));
        }

        // 3. Boundary Constraint Handling
        k_enforce_boundaries<<<num_blocks, BLOCK_SIZE, 0, device_buffers_->stream>>>(
            device_buffers_->d_particles, n, boundary_.center.x, boundary_.center.y,
            boundary_.radius, config_.boundary_restitution);
    }

    CUDA_CHECK(cudaEventRecord(device_buffers_->ev_end, device_buffers_->stream));
    CUDA_CHECK(cudaEventSynchronize(device_buffers_->ev_end));

    float step_gpu_time = 0.0f;
    CUDA_CHECK(cudaEventElapsedTime(&step_gpu_time, device_buffers_->ev_start, device_buffers_->ev_end));

    metrics_.total_step_gpu_ms = static_cast<double>(step_gpu_time);
    metrics_.collision_pairs_checked = (broadphase_mode_ == CudaBroadphaseMode::SpatialGrid)
        ? (n * 9 * (n / std::max(1, device_buffers_->spatial_grid->get_params().total_cells)))
        : ((n * (n - 1)) / 2 * substeps * iterations);

    host_particles_dirty_ = true;
}

sim_core::SimulationDiagnostics CudaSolverV2::compute_diagnostics() {
    if (host_particles_dirty_) {
        sync_d2h();
    }
    return sim_core::DiagnosticsCalculator::compute(host_particles_, config_);
}

bool CudaSolverV2::is_cuda_available() {
    int count = 0;
    const cudaError_t err = cudaGetDeviceCount(&count);
    return (err == cudaSuccess && count > 0);
}

CudaDeviceSpecs CudaSolverV2::query_device_specs(int device_id) {
    CudaDeviceSpecs specs;
    int count = 0;
    if (cudaGetDeviceCount(&count) != cudaSuccess || count == 0 || device_id >= count) {
        specs.available = false;
        return specs;
    }

    cudaDeviceProp prop;
    if (cudaGetDeviceProperties(&prop, device_id) != cudaSuccess) {
        specs.available = false;
        return specs;
    }

    specs.available = true;
    specs.device_id = device_id;
    specs.device_name = prop.name;
    specs.compute_major = prop.major;
    specs.compute_minor = prop.minor;
    specs.total_memory_bytes = prop.totalGlobalMem;
    specs.multiprocessor_count = prop.multiProcessorCount;
    specs.warp_size = prop.warpSize;
    specs.max_threads_per_block = prop.maxThreadsPerBlock;
    specs.max_threads_per_sm = prop.maxThreadsPerMultiProcessor;
    specs.shared_mem_per_block = prop.sharedMemPerBlock;
    specs.clock_rate_khz = prop.clockRate;
    specs.memory_bus_width_bits = prop.memoryBusWidth;
    specs.l2_cache_bytes = prop.l2CacheSize;

    return specs;
}

} // namespace sim_cuda
