#pragma once

#include "sim_core/types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace sim_omp {

/**
 * \brief Candidate collision pair identified by broadphase culling.
 */
struct PotentialPair {
    uint32_t i{0};
    uint32_t j{0};

    PotentialPair() = default;
    PotentialPair(uint32_t a, uint32_t b)
        : i(std::min(a, b)), j(std::max(a, b)) {}

    bool operator==(const PotentialPair& other) const {
        return i == other.i && j == other.j;
    }
    bool operator<(const PotentialPair& other) const {
        return (i < other.i) || (i == other.i && j < other.j);
    }
};

/**
 * \brief 2D Uniform Spatial Hash Grid for Broadphase Collision Detection.
 *        Culls distant particles, reducing pair tests from O(N^2) toward O(N).
 */
class UniformSpatialGrid {
public:
    explicit UniformSpatialGrid(float cell_size = 0.1f);

    /**
     * \brief Configure grid cell size based on maximum particle diameter.
     */
    void set_cell_size(float cell_size);
    float get_cell_size() const { return cell_size_; }

    /**
     * \brief Build spatial grid binning for a particle set.
     */
    void build(const std::vector<sim_core::Particle>& particles);

    /**
     * \brief Query all potential collision pairs across the grid.
     *        Guarantees zero false negatives if cell_size >= 2 * max_radius.
     */
    void find_potential_pairs(const std::vector<sim_core::Particle>& particles,
                              std::vector<PotentialPair>& out_pairs) const;

    /**
     * \brief Multi-threaded candidate pair discovery with OpenMP.
     */
    void find_potential_pairs_omp(const std::vector<sim_core::Particle>& particles,
                                  std::vector<PotentialPair>& out_pairs,
                                  int num_threads) const;

    size_t cell_count() const { return cell_heads_.size(); }

private:
    uint32_t hash_coords(int cx, int cy) const;

    float cell_size_{0.1f};
    float inv_cell_size_{10.0f};

    // Linked-cell list representation (Zero heap allocation per step)
    // cell_heads_[hash] points to the first particle in that bucket
    // next_particle_[i] points to the next particle in the same bucket
    mutable std::vector<int> cell_heads_;
    mutable std::vector<int> next_particle_;
    uint32_t hash_table_size_{4096};
};

} // namespace sim_omp
