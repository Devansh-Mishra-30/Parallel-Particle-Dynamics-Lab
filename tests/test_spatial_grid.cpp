#include "sim_core/contact.h"
#include "sim_core/types.h"
#include "sim_omp_v2/spatial_grid.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <vector>

using namespace sim_core;
using namespace sim_omp;

static std::vector<Particle> generate_test_particles(int n, float box_size, float min_r, float max_r, unsigned int seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> pos_dist(-box_size * 0.5f, box_size * 0.5f);
    std::uniform_real_distribution<float> r_dist(min_r, max_r);

    std::vector<Particle> particles;
    particles.reserve(n);

    for (int i = 0; i < n; ++i) {
        const Vec2 pos(pos_dist(rng), pos_dist(rng));
        const float r = r_dist(rng);
        Particle p(i + 1, pos, Vec2(0.0f, 0.0f), r);
        p.set_fixed_mass(1.0f);
        particles.push_back(p);
    }
    return particles;
}

// Ground truth all-pairs brute-force contact discovery
static std::vector<PotentialPair> find_exact_ground_truth_contacts(const std::vector<Particle>& particles) {
    std::vector<PotentialPair> exact_contacts;
    const size_t n = particles.size();

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            ContactPair contact;
            if (ContactSolver::detect_contact(particles[i], particles[j], contact)) {
                exact_contacts.emplace_back(static_cast<uint32_t>(i), static_cast<uint32_t>(j));
            }
        }
    }
    std::sort(exact_contacts.begin(), exact_contacts.end());
    return exact_contacts;
}

static bool verify_zero_false_negatives(int n, float box_size, float min_r, float max_r, unsigned int seed) {
    const auto particles = generate_test_particles(n, box_size, min_r, max_r, seed);
    const auto ground_truth = find_exact_ground_truth_contacts(particles);

    // Cell size must be >= 2 * max_radius to guarantee zero false negatives
    UniformSpatialGrid grid(2.0f * max_r);
    grid.build(particles);

    std::vector<PotentialPair> candidates;
    grid.find_potential_pairs(particles, candidates);

    std::set<PotentialPair> candidate_set(candidates.begin(), candidates.end());

    // Verify: Every true overlapping pair MUST be in candidate_set
    int missed = 0;
    for (const auto& pair : ground_truth) {
        if (candidate_set.find(pair) == candidate_set.end()) {
            missed++;
            std::cerr << "FAIL: Missed true overlapping pair (" << pair.i << ", " << pair.j << ")\n";
        }
    }

    const size_t total_possible_pairs = (static_cast<size_t>(n) * (n - 1)) / 2;
    const double culling_efficiency = 100.0 * (1.0 - static_cast<double>(candidates.size()) / total_possible_pairs);

    std::cout << "  [PASS] N=" << std::setw(5) << n
              << " | True Contacts: " << std::setw(4) << ground_truth.size()
              << " | Candidates: " << std::setw(6) << candidates.size()
              << " | Total Pairs: " << std::setw(8) << total_possible_pairs
              << " | Culled: " << std::fixed << std::setprecision(1) << culling_efficiency << "%\n";

    return (missed == 0);
}

static bool verify_omp_grid_equivalence(int n) {
    const auto particles = generate_test_particles(n, 2.0f, 0.02f, 0.04f, 42);

    UniformSpatialGrid grid(0.08f);
    grid.build(particles);

    std::vector<PotentialPair> seq_pairs;
    grid.find_potential_pairs(particles, seq_pairs);
    std::sort(seq_pairs.begin(), seq_pairs.end());

    for (int threads : {1, 2, 4, 8, 16}) {
        std::vector<PotentialPair> omp_pairs;
        grid.find_potential_pairs_omp(particles, omp_pairs, threads);
        std::sort(omp_pairs.begin(), omp_pairs.end());

        if (seq_pairs.size() != omp_pairs.size()) {
            std::cerr << "FAIL: OpenMP candidate count mismatch for threads=" << threads
                      << " (seq=" << seq_pairs.size() << ", omp=" << omp_pairs.size() << ")\n";
            return false;
        }

        for (size_t i = 0; i < seq_pairs.size(); ++i) {
            if (seq_pairs[i].i != omp_pairs[i].i || seq_pairs[i].j != omp_pairs[i].j) {
                std::cerr << "FAIL: OpenMP pair mismatch at index " << i << " for threads=" << threads << "\n";
                return false;
            }
        }
    }

    std::cout << "  [PASS] OpenMP Multi-Threaded Grid Equivalence (1, 2, 4, 8, 16 threads verified)\n";
    return true;
}

int main() {
    std::cout << "=================================================================================\n";
    std::cout << "  PARALLEL PARTICLE DYNAMICS LAB: SPATIAL GRID BROADPHASE AUDIT (TASK 7)\n";
    std::cout << "=================================================================================\n\n";

    bool all_ok = true;

    std::cout << "1. Zero False-Negative Verification against Brute-Force Ground Truth:\n";
    all_ok &= verify_zero_false_negatives(50,   1.0f, 0.02f, 0.04f, 101);
    all_ok &= verify_zero_false_negatives(200,  1.5f, 0.015f, 0.03f, 202);
    all_ok &= verify_zero_false_negatives(500,  2.0f, 0.01f, 0.025f, 303);
    all_ok &= verify_zero_false_negatives(1000, 2.5f, 0.008f, 0.02f, 404);

    std::cout << "\n2. OpenMP Multi-Threaded Grid Parallelization Equivalence:\n";
    all_ok &= verify_omp_grid_equivalence(400);

    std::cout << "\n=================================================================================\n";
    std::cout << "  SPATIAL GRID BROADPHASE AUDIT: " << (all_ok ? "ALL TESTS PASSED" : "FAILED") << "\n";
    std::cout << "=================================================================================\n";

    return all_ok ? 0 : 1;
}
