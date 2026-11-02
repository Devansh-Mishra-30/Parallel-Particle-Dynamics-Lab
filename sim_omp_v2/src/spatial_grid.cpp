#include "sim_omp_v2/spatial_grid.h"

#include <omp.h>
#include <cmath>

namespace sim_omp {

UniformSpatialGrid::UniformSpatialGrid(float cell_size) {
    set_cell_size(cell_size);
}

void UniformSpatialGrid::set_cell_size(float cell_size) {
    cell_size_ = std::max(1e-4f, cell_size);
    inv_cell_size_ = 1.0f / cell_size_;
}

uint32_t UniformSpatialGrid::hash_coords(int cx, int cy) const {
    // Standard spatial hashing with large prime multipliers
    constexpr uint32_t p1 = 73856093u;
    constexpr uint32_t p2 = 19349663u;
    const uint32_t n = (static_cast<uint32_t>(cx) * p1) ^ (static_cast<uint32_t>(cy) * p2);
    return n % hash_table_size_;
}

void UniformSpatialGrid::build(const std::vector<sim_core::Particle>& particles) {
    const size_t n = particles.size();
    if (n == 0) return;

    // Dynamically size hash table to keep bucket load low (load factor ~ 0.5)
    uint32_t target_size = 256;
    while (target_size < n * 2 && target_size < 65536) {
        target_size <<= 1;
    }
    hash_table_size_ = target_size;

    cell_heads_.assign(hash_table_size_, -1);
    next_particle_.assign(n, -1);

    for (size_t i = 0; i < n; ++i) {
        const int cx = static_cast<int>(std::floor(particles[i].position.x * inv_cell_size_));
        const int cy = static_cast<int>(std::floor(particles[i].position.y * inv_cell_size_));
        const uint32_t h = hash_coords(cx, cy);

        next_particle_[i] = cell_heads_[h];
        cell_heads_[h] = static_cast<int>(i);
    }
}

void UniformSpatialGrid::find_potential_pairs(const std::vector<sim_core::Particle>& particles,
                                             std::vector<PotentialPair>& out_pairs) const {
    out_pairs.clear();
    const size_t n = particles.size();
    if (n < 2) return;

    // Relative offsets for unique neighbor checking:
    // (0,0) handled specially (j > i).
    // Forward neighbors: (1,0), (-1,1), (0,1), (1,1)
    constexpr int neighbor_offsets[4][2] = {
        { 1,  0},
        {-1,  1},
        { 0,  1},
        { 1,  1}
    };

    for (size_t i = 0; i < n; ++i) {
        const int cx = static_cast<int>(std::floor(particles[i].position.x * inv_cell_size_));
        const int cy = static_cast<int>(std::floor(particles[i].position.y * inv_cell_size_));

        // 1. Same cell pairs: check remaining particles in current linked list
        int curr = next_particle_[i];
        while (curr != -1) {
            const size_t j = static_cast<size_t>(curr);
            const int j_cx = static_cast<int>(std::floor(particles[j].position.x * inv_cell_size_));
            const int j_cy = static_cast<int>(std::floor(particles[j].position.y * inv_cell_size_));

            // Verify actual cell coordinates (filters out hash collisions)
            if (cx == j_cx && cy == j_cy) {
                out_pairs.emplace_back(static_cast<uint32_t>(i), static_cast<uint32_t>(j));
            }
            curr = next_particle_[curr];
        }

        // 2. Forward neighbor cells
        for (int k = 0; k < 4; ++k) {
            const int n_cx = cx + neighbor_offsets[k][0];
            const int n_cy = cy + neighbor_offsets[k][1];
            const uint32_t nh = hash_coords(n_cx, n_cy);

            int n_curr = cell_heads_[nh];
            while (n_curr != -1) {
                const size_t j = static_cast<size_t>(n_curr);
                const int j_cx = static_cast<int>(std::floor(particles[j].position.x * inv_cell_size_));
                const int j_cy = static_cast<int>(std::floor(particles[j].position.y * inv_cell_size_));

                if (n_cx == j_cx && n_cy == j_cy) {
                    out_pairs.emplace_back(static_cast<uint32_t>(i), static_cast<uint32_t>(j));
                }
                n_curr = next_particle_[n_curr];
            }
        }
    }
}

void UniformSpatialGrid::find_potential_pairs_omp(const std::vector<sim_core::Particle>& particles,
                                                 std::vector<PotentialPair>& out_pairs,
                                                 int num_threads) const {
    out_pairs.clear();
    const size_t n = particles.size();
    if (n < 2) return;

    constexpr int neighbor_offsets[4][2] = {
        { 1,  0},
        {-1,  1},
        { 0,  1},
        { 1,  1}
    };

    const int t_count = std::max(1, num_threads);
    std::vector<std::vector<PotentialPair>> thread_pairs(t_count);

    #pragma omp parallel num_threads(t_count)
    {
        const int tid = omp_get_thread_num();
        auto& local_pairs = thread_pairs[tid];
        local_pairs.reserve(n * 2);

        #pragma omp for schedule(dynamic, 16)
        for (size_t i = 0; i < n; ++i) {
            const int cx = static_cast<int>(std::floor(particles[i].position.x * inv_cell_size_));
            const int cy = static_cast<int>(std::floor(particles[i].position.y * inv_cell_size_));

            // Same cell
            int curr = next_particle_[i];
            while (curr != -1) {
                const size_t j = static_cast<size_t>(curr);
                const int j_cx = static_cast<int>(std::floor(particles[j].position.x * inv_cell_size_));
                const int j_cy = static_cast<int>(std::floor(particles[j].position.y * inv_cell_size_));

                if (cx == j_cx && cy == j_cy) {
                    local_pairs.emplace_back(static_cast<uint32_t>(i), static_cast<uint32_t>(j));
                }
                curr = next_particle_[curr];
            }

            // Neighbor cells
            for (int k = 0; k < 4; ++k) {
                const int n_cx = cx + neighbor_offsets[k][0];
                const int n_cy = cy + neighbor_offsets[k][1];
                const uint32_t nh = hash_coords(n_cx, n_cy);

                int n_curr = cell_heads_[nh];
                while (n_curr != -1) {
                    const size_t j = static_cast<size_t>(n_curr);
                    const int j_cx = static_cast<int>(std::floor(particles[j].position.x * inv_cell_size_));
                    const int j_cy = static_cast<int>(std::floor(particles[j].position.y * inv_cell_size_));

                    if (n_cx == j_cx && n_cy == j_cy) {
                        local_pairs.emplace_back(static_cast<uint32_t>(i), static_cast<uint32_t>(j));
                    }
                    n_curr = next_particle_[n_curr];
                }
            }
        }
    }

    // Merge thread pairs into output
    size_t total_pairs = 0;
    for (int t = 0; t < t_count; ++t) {
        total_pairs += thread_pairs[t].size();
    }
    out_pairs.reserve(total_pairs);
    for (int t = 0; t < t_count; ++t) {
        out_pairs.insert(out_pairs.end(), thread_pairs[t].begin(), thread_pairs[t].end());
    }
}

} // namespace sim_omp
