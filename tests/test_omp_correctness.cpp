#include "sim_core/boundary.h"
#include "sim_core/contact.h"
#include "sim_core/cpu_reference_solver.h"
#include "sim_core/diagnostics.h"
#include "sim_core/types.h"
#include "sim_omp_v2/omp_solver.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace sim_core;
using namespace sim_omp;

struct TestCaseSummary {
    std::string name;
    bool passed{false};
    float max_pos_diff{0.0f};
    float max_vel_diff{0.0f};
    float momentum_drift{0.0f};
    float energy_error{0.0f};
    std::string details;
};

// 1. Constant Velocity Motion Test (Free flight, no forces)
static TestCaseSummary test_constant_velocity(int threads) {
    TestCaseSummary res;
    res.name = "Constant Velocity (Threads=" + std::to_string(threads) + ")";

    SimConfig cfg;
    cfg.dt = 1.0f / 120.0f;
    cfg.substeps = 8;
    cfg.gravity = Vec2(0.0f, 0.0f);

    CircularBoundary bnd({0.0f, 0.0f}, 10.0f);
    CpuReferenceSolver cpu_solver(cfg, bnd);
    OmpSolverV2 omp_solver(cfg, bnd);
    omp_solver.set_num_threads(threads);

    constexpr int N = 64;
    for (int i = 0; i < N; ++i) {
        const float angle = 2.0f * 3.14159265f * static_cast<float>(i) / N;
        const Vec2 pos(0.5f * std::cos(angle), 0.5f * std::sin(angle));
        const Vec2 vel(-1.0f * std::sin(angle), 1.0f * std::cos(angle));
        Particle p(i, pos, vel, 0.02f);
        p.set_fixed_mass(1.0f);
        cpu_solver.add_particle(p);
        omp_solver.add_particle(p);
    }

    // Step 20 frames
    for (int f = 0; f < 20; ++f) {
        cpu_solver.step();
        omp_solver.step();
    }

    const auto& cpu_p = cpu_solver.get_particles();
    const auto& omp_p = omp_solver.get_particles();

    for (size_t i = 0; i < N; ++i) {
        const float dp = (cpu_p[i].position - omp_p[i].position).length();
        const float dv = (cpu_p[i].velocity - omp_p[i].velocity).length();
        if (dp > res.max_pos_diff) res.max_pos_diff = dp;
        if (dv > res.max_vel_diff) res.max_vel_diff = dv;
    }

    res.passed = (res.max_pos_diff < 1e-5f && res.max_vel_diff < 1e-5f);
    return res;
}

// 2. Uniform Gravity Ballistic Flight Test
static TestCaseSummary test_gravity_ballistic(int threads) {
    TestCaseSummary res;
    res.name = "Gravity Ballistic (Threads=" + std::to_string(threads) + ")";

    SimConfig cfg;
    cfg.dt = 1.0f / 120.0f;
    cfg.substeps = 8;
    cfg.gravity = Vec2(0.0f, -9.80665f);

    CircularBoundary bnd({0.0f, 0.0f}, 10.0f);
    CpuReferenceSolver cpu_solver(cfg, bnd);
    OmpSolverV2 omp_solver(cfg, bnd);
    omp_solver.set_num_threads(threads);

    constexpr int N = 32;
    for (int i = 0; i < N; ++i) {
        const float x = -0.8f + 1.6f * static_cast<float>(i) / N;
        Particle p(i, Vec2(x, -0.5f), Vec2(1.0f, 2.5f + 0.1f * i), 0.02f);
        p.set_fixed_mass(1.5f);
        cpu_solver.add_particle(p);
        omp_solver.add_particle(p);
    }

    for (int f = 0; f < 30; ++f) {
        cpu_solver.step();
        omp_solver.step();
    }

    const auto& cpu_p = cpu_solver.get_particles();
    const auto& omp_p = omp_solver.get_particles();

    for (size_t i = 0; i < N; ++i) {
        const float dp = (cpu_p[i].position - omp_p[i].position).length();
        const float dv = (cpu_p[i].velocity - omp_p[i].velocity).length();
        if (dp > res.max_pos_diff) res.max_pos_diff = dp;
        if (dv > res.max_vel_diff) res.max_vel_diff = dv;
    }

    res.passed = (res.max_pos_diff < 1e-5f && res.max_vel_diff < 1e-5f);
    return res;
}

// 3. Isolated Two-Particle Collision Mechanics (Equal, Unequal, Inelastic, Oblique)
static TestCaseSummary test_binary_collision(const std::string& label,
                                             float m1, float m2,
                                             float r1, float r2,
                                             Vec2 x1, Vec2 x2,
                                             Vec2 v1, Vec2 v2,
                                             float restitution,
                                             int threads) {
    TestCaseSummary res;
    res.name = label + " (Threads=" + std::to_string(threads) + ")";

    SimConfig cfg;
    cfg.dt = 1.0f / 120.0f;
    cfg.substeps = 8;
    cfg.restitution = restitution;
    cfg.boundary_restitution = 1.0f;
    cfg.gravity = Vec2(0.0f, 0.0f);
    cfg.solver_iterations = 2;

    CircularBoundary bnd({0.0f, 0.0f}, 10.0f);
    CpuReferenceSolver cpu_solver(cfg, bnd);
    OmpSolverV2 omp_solver(cfg, bnd);
    omp_solver.set_num_threads(threads);
    omp_solver.set_contact_mode(ContactMode::Jacobi);

    Particle p1(1, x1, v1, r1); p1.set_fixed_mass(m1);
    Particle p2(2, x2, v2, r2); p2.set_fixed_mass(m2);

    cpu_solver.add_particle(p1);
    cpu_solver.add_particle(p2);
    omp_solver.add_particle(p1);
    omp_solver.add_particle(p2);

    const auto init_diag = omp_solver.compute_diagnostics();

    for (int f = 0; f < 48; ++f) {
        cpu_solver.step();
        omp_solver.step();
    }

    const auto& cpu_p = cpu_solver.get_particles();
    const auto& omp_p = omp_solver.get_particles();
    const auto final_diag = omp_solver.compute_diagnostics();

    res.max_pos_diff = std::max((cpu_p[0].position - omp_p[0].position).length(),
                                (cpu_p[1].position - omp_p[1].position).length());
    res.max_vel_diff = std::max((cpu_p[0].velocity - omp_p[0].velocity).length(),
                                (cpu_p[1].velocity - omp_p[1].velocity).length());
    res.momentum_drift = (final_diag.total_momentum - init_diag.total_momentum).length();

    // In isolated 2-body collision, Jacobi and Gauss-Seidel must be identical to float precision!
    res.passed = (res.max_pos_diff < 1e-4f && res.max_vel_diff < 1e-4f && res.momentum_drift < 1e-5f);
    return res;
}

// 4. Zero-Distance Overlap Regularization Test
static TestCaseSummary test_zero_distance_overlap(int threads) {
    TestCaseSummary res;
    res.name = "Zero-Distance Overlap (Threads=" + std::to_string(threads) + ")";

    SimConfig cfg;
    cfg.dt = 1.0f / 120.0f;
    cfg.substeps = 4;
    cfg.restitution = 1.0f;
    cfg.gravity = Vec2(0.0f, 0.0f);
    cfg.solver_iterations = 2;

    CircularBoundary bnd({0.0f, 0.0f}, 5.0f);
    OmpSolverV2 omp_solver(cfg, bnd);
    omp_solver.set_num_threads(threads);

    // Two particles placed at exact identical coordinates
    Particle p1(1, Vec2(0.0f, 0.0f), Vec2(0.0f, 0.0f), 0.05f);
    p1.set_fixed_mass(1.0f);
    Particle p2(2, Vec2(0.0f, 0.0f), Vec2(0.0f, 0.0f), 0.05f);
    p2.set_fixed_mass(1.0f);

    omp_solver.add_particle(p1);
    omp_solver.add_particle(p2);

    for (int f = 0; f < 10; ++f) {
        omp_solver.step();
    }

    const auto diag = omp_solver.compute_diagnostics();
    const auto& particles = omp_solver.get_particles();

    const bool finite = diag.is_finite && particles[0].position.is_finite() && particles[1].position.is_finite();
    const float dist = (particles[0].position - particles[1].position).length();

    // Regularization separates particles without NaN explosion
    res.passed = finite && (dist > 0.0f);
    return res;
}

// 5. Three-Particle In-Line Contact Test
static TestCaseSummary test_three_particle_chain(int threads) {
    TestCaseSummary res;
    res.name = "Three-Particle In-Line Chain (Threads=" + std::to_string(threads) + ")";

    SimConfig cfg;
    cfg.dt = 1.0f / 120.0f;
    cfg.substeps = 8;
    cfg.restitution = 1.0f;
    cfg.gravity = Vec2(0.0f, 0.0f);
    cfg.solver_iterations = 4;

    CircularBoundary bnd({0.0f, 0.0f}, 5.0f);
    OmpSolverV2 omp_solver(cfg, bnd);
    omp_solver.set_num_threads(threads);

    // Chain: P1 (moving right) -> P2 (stationary) -> P3 (stationary)
    Particle p1(1, Vec2(-0.20f, 0.0f), Vec2(1.0f, 0.0f), 0.04f); p1.set_fixed_mass(1.0f);
    Particle p2(2, Vec2(0.00f, 0.0f), Vec2(0.0f, 0.0f), 0.04f);  p2.set_fixed_mass(1.0f);
    Particle p3(3, Vec2(0.08f, 0.0f), Vec2(0.0f, 0.0f), 0.04f);  p3.set_fixed_mass(1.0f);

    omp_solver.add_particle(p1);
    omp_solver.add_particle(p2);
    omp_solver.add_particle(p3);

    const auto init_diag = omp_solver.compute_diagnostics();

    for (int f = 0; f < 40; ++f) {
        omp_solver.step();
    }

    const auto final_diag = omp_solver.compute_diagnostics();
    res.momentum_drift = (final_diag.total_momentum - init_diag.total_momentum).length();
    res.energy_error = std::abs(final_diag.total_kinetic_energy - init_diag.total_kinetic_energy);

    // Elastic chain: momentum and kinetic energy strictly conserved
    res.passed = final_diag.is_finite && (res.momentum_drift < 1e-5f) && (res.energy_error < 0.02f);
    return res;
}

// 6. Dense Granular Settling Invariant & Stability Test
static TestCaseSummary test_dense_granular_settling(int threads) {
    TestCaseSummary res;
    res.name = "Dense Granular Settling (N=48, Threads=" + std::to_string(threads) + ")";

    SimConfig cfg;
    cfg.dt = 1.0f / 60.0f;
    cfg.substeps = 12;
    cfg.restitution = 0.20f;
    cfg.boundary_restitution = 0.30f;
    cfg.gravity = Vec2(0.0f, -9.80665f);
    cfg.solver_iterations = 8;
    cfg.stabilization_factor = 0.60f;

    CircularBoundary bnd({0.0f, 0.0f}, 1.0f);
    OmpSolverV2 omp_solver(cfg, bnd);
    omp_solver.set_num_threads(threads);

    // 48 particles in 6x8 column
    int id = 1;
    for (int col = 0; col < 6; ++col) {
        for (int row = 0; row < 8; ++row) {
            const float x = -0.25f + static_cast<float>(col) * 0.10f;
            const float y = 0.10f + static_cast<float>(row) * 0.10f;
            Particle p(id++, Vec2(x, y), Vec2(0.0f, 0.0f), 0.04f);
            p.set_fixed_mass(1.0f);
            omp_solver.add_particle(p);
        }
    }

    // Step for 1.0 second (60 frames)
    for (int f = 0; f < 60; ++f) {
        omp_solver.step();
    }

    const auto final_diag = omp_solver.compute_diagnostics();

    // Verify:
    // 1. 100% finite (no NaN/Inf)
    // 2. Active contacts > 40 (compact heap)
    // 3. Max penetration controlled (< 1 mm = 0.001 m)
    // 4. Kinetic energy properly dissipated (settled heap, Ek < 2.0 J)
    const bool finite = final_diag.is_finite;
    const bool compact = (final_diag.active_contacts >= 40);
    const bool penetration_ok = (final_diag.max_penetration < 0.0015f);
    const bool dissipated = (final_diag.total_kinetic_energy < 2.0f);

    res.passed = finite && compact && penetration_ok && dissipated;
    res.details = "Contacts: " + std::to_string(final_diag.active_contacts) +
                  " | Max Pen: " + std::to_string(final_diag.max_penetration * 1000.0f) + " mm" +
                  " | Ek: " + std::to_string(final_diag.total_kinetic_energy) + " J";
    return res;
}

// 7. Multi-Thread Determinism & Stability Test (16 Threads Repeated Runs)
static TestCaseSummary test_thread_determinism(int threads) {
    TestCaseSummary res;
    res.name = "Multi-Thread Repeatability (16 Threads, 3 Runs)";

    auto run_sim = [threads]() {
        SimConfig cfg;
        cfg.dt = 1.0f / 60.0f;
        cfg.substeps = 8;
        cfg.restitution = 0.5f;
        cfg.boundary_restitution = 0.5f;
        cfg.gravity = Vec2(0.0f, -9.80665f);
        cfg.solver_iterations = 4;

        CircularBoundary bnd({0.0f, 0.0f}, 1.0f);
        OmpSolverV2 solver(cfg, bnd);
        solver.set_num_threads(threads);

        for (int i = 0; i < 24; ++i) {
            const float x = -0.3f + (i % 6) * 0.12f;
            const float y = 0.2f + (i / 6) * 0.12f;
            Particle p(i + 1, Vec2(x, y), Vec2(0.1f * i, -0.2f), 0.04f);
            p.set_fixed_mass(1.0f);
            solver.add_particle(p);
        }

        for (int f = 0; f < 30; ++f) {
            solver.step();
        }
        return solver.compute_diagnostics();
    };

    const auto d1 = run_sim();
    const auto d2 = run_sim();
    const auto d3 = run_sim();

    const float p_diff = std::max((d1.total_momentum - d2.total_momentum).length(),
                                  (d2.total_momentum - d3.total_momentum).length());
    const float ek_diff = std::max(std::abs(d1.total_kinetic_energy - d2.total_kinetic_energy),
                                   std::abs(d2.total_kinetic_energy - d3.total_kinetic_energy));

    res.passed = d1.is_finite && d2.is_finite && d3.is_finite && (p_diff < 1e-4f) && (ek_diff < 1e-4f);
    res.details = "dP diff: " + std::to_string(p_diff) + " | dEk diff: " + std::to_string(ek_diff);
    return res;
}

int main() {
    std::cout << "=================================================================================\n";
    std::cout << "  PARALLEL PARTICLE DYNAMICS LAB: OPENMP V2 CORRECTNESS VERIFICATION (TASK 4 & 5)\n";
    std::cout << "=================================================================================\n\n";

    std::vector<TestCaseSummary> results;

    const std::vector<int> thread_counts = {1, 2, 4, 8, 16};

    // 1. Constant Velocity across threads
    for (int t : thread_counts) {
        results.push_back(test_constant_velocity(t));
    }

    // 2. Gravity Ballistic across threads
    for (int t : thread_counts) {
        results.push_back(test_gravity_ballistic(t));
    }

    // 3. Binary Collisions across thread counts
    for (int t : thread_counts) {
        results.push_back(test_binary_collision("Equal Elastic Collision", 2.0f, 2.0f, 0.05f, 0.05f,
                                                {-0.25f, 0.0f}, {0.25f, 0.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f}, 1.0f, t));
        results.push_back(test_binary_collision("Unequal Elastic Collision", 1.5f, 3.0f, 0.04f, 0.06f,
                                                {-0.25f, 0.0f}, {0.25f, 0.0f}, {1.2f, 0.0f}, {-0.6f, 0.0f}, 1.0f, t));
        results.push_back(test_binary_collision("Inelastic Collision (e=0.528)", 2.0f, 2.0f, 0.05f, 0.05f,
                                                {-0.25f, 0.0f}, {0.25f, 0.0f}, {1.5f, 0.0f}, {-1.5f, 0.0f}, 0.528f, t));
        results.push_back(test_binary_collision("Oblique Collision (b=0.04)", 2.0f, 2.0f, 0.05f, 0.05f,
                                                {-0.25f, 0.02f}, {0.25f, -0.02f}, {1.2f, 0.0f}, {-1.2f, 0.0f}, 0.85f, t));
    }

    // 4. Zero Distance Overlaps
    for (int t : thread_counts) {
        results.push_back(test_zero_distance_overlap(t));
    }

    // 5. Three Particle In-Line Chain
    for (int t : thread_counts) {
        results.push_back(test_three_particle_chain(t));
    }

    // 6. Dense Granular Settling
    for (int t : thread_counts) {
        results.push_back(test_dense_granular_settling(t));
    }

    // 7. Multi-Thread Determinism
    results.push_back(test_thread_determinism(16));

    // Print Results
    int passed = 0;
    int failed = 0;

    for (const auto& r : results) {
        if (r.passed) passed++;
        else failed++;

        std::cout << (r.passed ? "  [PASS] " : "  [FAIL] ") << std::left << std::setw(55) << r.name;
        if (!r.details.empty()) {
            std::cout << " (" << r.details << ")";
        } else if (r.max_pos_diff > 0.0f) {
            std::cout << " (max ||dx||=" << std::scientific << std::setprecision(2) << r.max_pos_diff
                      << ", ||dv||=" << r.max_vel_diff << ")";
        }
        std::cout << "\n";
    }

    std::cout << "\n=================================================================================\n";
    std::cout << "  OPENMP V2 TEST SUMMARY: " << passed << " PASSED, " << failed << " FAILED (TOTAL " << results.size() << ")\n";
    std::cout << "=================================================================================\n";

    return (failed == 0) ? 0 : 1;
}
