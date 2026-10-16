#include "sim_core/boundary.h"
#include "sim_core/contact.h"
#include "sim_core/cpu_reference_solver.h"
#include "sim_core/diagnostics.h"
#include "sim_core/types.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace sim_core;

struct CollisionVerificationCase {
    std::string name;
    float m1, m2;
    float r1, r2;
    Vec2 x1, x2;
    Vec2 v1, v2;
    float restitution;
    bool is_oblique{false};
};

struct CollisionAcceptanceResult {
    std::string name;
    bool passed{false};
    float initial_momentum_x{0.0f};
    float final_momentum_x{0.0f};
    float momentum_drift{0.0f};
    float initial_ek{0.0f};
    float final_ek{0.0f};
    float theoretical_dek{0.0f};
    float measured_dek{0.0f};
    float dek_error{0.0f};
    Vec2 v1_exact{0.0f, 0.0f};
    Vec2 v1_sim{0.0f, 0.0f};
    float v1_error{0.0f};
    Vec2 v2_exact{0.0f, 0.0f};
    Vec2 v2_sim{0.0f, 0.0f};
    float v2_error{0.0f};
    std::string details;
};

static CollisionAcceptanceResult run_collision_verification(const CollisionVerificationCase& tc) {
    CollisionAcceptanceResult res;
    res.name = tc.name;

    SimConfig config;
    config.dt = 1.0f / 120.0f;
    config.substeps = 8;
    config.restitution = tc.restitution;
    config.boundary_restitution = 1.0f;
    config.gravity = Vec2(0.0f, 0.0f); // isolated 2-body system
    config.solver_iterations = 2;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 10.0f); // unconstrained by wall
    CpuReferenceSolver solver(config, boundary);

    Particle p1(1, tc.x1, tc.v1, tc.r1);
    p1.set_fixed_mass(tc.m1);
    Particle p2(2, tc.x2, tc.v2, tc.r2);
    p2.set_fixed_mass(tc.m2);

    solver.add_particle(p1);
    solver.add_particle(p2);

    const auto initial_diag = solver.compute_diagnostics();
    res.initial_momentum_x = initial_diag.total_momentum.x;
    res.initial_ek = initial_diag.total_kinetic_energy;

    // Analytical reference prediction
    const float m1 = tc.m1;
    const float m2 = tc.m2;
    const float m_tot = m1 + m2;
    const float mu = (m1 * m2) / m_tot;
    const float r_sum = tc.r1 + tc.r2;
    const float b = tc.x1.y - tc.x2.y; // impact parameter
    Vec2 n;
    if (tc.is_oblique && std::abs(b) < r_sum) {
        const float ny = b / r_sum;
        const float sgn_x = (tc.v1.x > tc.v2.x) ? -1.0f : 1.0f;
        const float nx = sgn_x * std::sqrt(std::max(0.0f, 1.0f - ny * ny));
        n = Vec2(nx, ny);
    } else {
        n = (tc.x1 - tc.x2).normalized();
    }
    const Vec2 v_rel = tc.v1 - tc.v2;
    const float vn = Vec2::dot(v_rel, n);

    // Frictionless normal impulse: Jn = -(1 + e) * mu * vn
    const float Jn = -(1.0f + tc.restitution) * mu * vn;
    res.v1_exact = tc.v1 + (Jn / m1) * n;
    res.v2_exact = tc.v2 - (Jn / m2) * n;
    res.theoretical_dek = -0.5f * mu * (1.0f - tc.restitution * tc.restitution) * (vn * vn);

    // Simulate for 0.4 seconds (48 frames = 384 substeps), sufficient for impact and full separation
    constexpr int TOTAL_FRAMES = 48;
    for (int f = 0; f < TOTAL_FRAMES; ++f) {
        solver.step();
    }

    const auto final_diag = solver.compute_diagnostics();
    const auto& particles = solver.get_particles();

    res.final_momentum_x = final_diag.total_momentum.x;
    res.momentum_drift = (final_diag.total_momentum - initial_diag.total_momentum).length();
    res.final_ek = final_diag.total_kinetic_energy;
    res.measured_dek = res.final_ek - res.initial_ek;
    res.dek_error = std::abs(res.measured_dek - res.theoretical_dek);

    res.v1_sim = particles[0].velocity;
    res.v2_sim = particles[1].velocity;
    res.v1_error = (res.v1_sim - res.v1_exact).length();
    res.v2_error = (res.v2_sim - res.v2_exact).length();

    // Tolerances:
    // Momentum: strict machine precision conservation (drift < 1e-5 kg*m/s)
    // Velocity: within 0.01 m/s for head-on collisions; for oblique impacts, discrete
    //           collision detection samples contact normal at finite penetration delta ~ vn*dt_sub,
    //           inducing ~1 deg normal rotation, leading to ~0.015 m/s O(dt) discretization error.
    const float v_tol = tc.is_oblique ? 0.02f : 0.01f;
    const bool p_ok = (res.momentum_drift < 1e-5f);
    const bool v_ok = (res.v1_error < v_tol && res.v2_error < v_tol);
    const bool e_ok = (res.dek_error < 0.015f);

    res.passed = p_ok && v_ok && e_ok;
    return res;
}

int main() {
    std::cout << "=================================================================================\n";
    std::cout << "  PARALLEL PARTICLE DYNAMICS LAB: COLLISION LABORATORY VERIFICATION (TASK B2)\n";
    std::cout << "=================================================================================\n\n";

    std::vector<CollisionVerificationCase> cases = {
        {"1. Equal-Mass Elastic Collision (m1=m2, e=1.0)",
         2.0f, 2.0f, 0.05f, 0.05f, {-0.25f, 0.0f}, {0.25f, 0.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f}, 1.0f, false},

        {"2. Unequal-Mass Elastic Collision (m2=2*m1, e=1.0)",
         1.5f, 3.0f, 0.04f, 0.06f, {-0.25f, 0.0f}, {0.25f, 0.0f}, {1.2f, 0.0f}, {-0.6f, 0.0f}, 1.0f, false},

        {"3. Inelastic Collision (e=0.528, m1=m2)",
         2.0f, 2.0f, 0.05f, 0.05f, {-0.25f, 0.0f}, {0.25f, 0.0f}, {1.5f, 0.0f}, {-1.5f, 0.0f}, 0.528f, false},

        {"4. Asymmetric Initial Velocities (v1=2.0, v2=-0.5, e=0.8)",
         2.0f, 1.5f, 0.05f, 0.04f, {-0.30f, 0.0f}, {0.20f, 0.0f}, {2.0f, 0.0f}, {-0.5f, 0.0f}, 0.8f, false},

        {"5. Perfectly Plastic Collision (e=0.0, m1=m2)",
         2.0f, 2.0f, 0.05f, 0.05f, {-0.25f, 0.0f}, {0.25f, 0.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f}, 0.0f, false},

        {"6. Perfectly Elastic High-Speed (e=1.0, v_rel=10 m/s)",
         1.0f, 1.0f, 0.05f, 0.05f, {-0.35f, 0.0f}, {0.35f, 0.0f}, {5.0f, 0.0f}, {-5.0f, 0.0f}, 1.0f, false},

        {"7. Oblique Collision (b=0.04m, e=0.85)",
         2.0f, 2.0f, 0.05f, 0.05f, {-0.25f, 0.02f}, {0.25f, -0.02f}, {1.2f, 0.0f}, {-1.2f, 0.0f}, 0.85f, true}
    };

    int passed = 0;
    int failed = 0;

    for (const auto& tc : cases) {
        const auto res = run_collision_verification(tc);
        if (res.passed) passed++;
        else failed++;

        std::cout << (res.passed ? "  [PASS] " : "  [FAIL] ") << res.name << "\n";
        std::cout << "         Momentum Drift: " << std::scientific << std::setprecision(3) << res.momentum_drift << " kg*m/s\n";
        std::cout << "         Ek Initial: " << std::fixed << std::setprecision(4) << res.initial_ek << " J | Ek Final: " << res.final_ek << " J\n";
        std::cout << "         Theoretical dEk: " << res.theoretical_dek << " J | Measured dEk: " << res.measured_dek
                  << " J | Error: " << std::scientific << res.dek_error << " J\n";
        std::cout << "         v1 Exact: (" << std::fixed << std::setprecision(3) << res.v1_exact.x << ", " << res.v1_exact.y << ") m/s"
                  << " | v1 Sim: (" << res.v1_sim.x << ", " << res.v1_sim.y << ") m/s"
                  << " | Error: " << std::scientific << res.v1_error << " m/s\n";
        std::cout << "         v2 Exact: (" << std::fixed << std::setprecision(3) << res.v2_exact.x << ", " << res.v2_exact.y << ") m/s"
                  << " | v2 Sim: (" << res.v2_sim.x << ", " << res.v2_sim.y << ") m/s"
                  << " | Error: " << std::scientific << res.v2_error << " m/s\n\n";
    }

    std::cout << "=================================================================================\n";
    std::cout << "  COLLISION LABORATORY SUMMARY: " << passed << " PASSED, " << failed << " FAILED (TOTAL " << cases.size() << ")\n";
    std::cout << "=================================================================================\n";

    return (failed == 0) ? 0 : 1;
}
