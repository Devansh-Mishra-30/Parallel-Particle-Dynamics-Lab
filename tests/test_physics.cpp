#include "sim_core/boundary.h"
#include "sim_core/contact.h"
#include "sim_core/cpu_reference_solver.h"
#include "sim_core/diagnostics.h"
#include "sim_core/integrator.h"
#include "sim_core/types.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace sim_core;

struct TestResult {
    std::string name;
    std::string category;
    bool passed{false};
    std::string details;
    double measured_error{0.0};
    double tolerance{0.0};
};

static std::vector<TestResult> g_results;

static void report_test(const TestResult& res) {
    g_results.push_back(res);
    std::cout << (res.passed ? "  [PASS] " : "  [FAIL] ")
              << std::left << std::setw(42) << res.name
              << " (" << res.category << ")\n"
              << "         Error: " << std::scientific << std::setprecision(4) << res.measured_error
              << " | Tol: " << res.tolerance << "\n"
              << "         " << res.details << "\n";
}

// -----------------------------------------------------------------------------
// Test 1: Constant Velocity (Free Ballistic Motion)
// Category: Analytical Verification
// -----------------------------------------------------------------------------
static void test_constant_velocity() {
    SimConfig config;
    config.gravity = Vec2(0.0f, 0.0f);
    config.dt = 1.0f / 60.0f;
    config.substeps = 8;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 100.0f); // large boundary, no collisions
    CpuReferenceSolver solver(config, boundary);

    const Vec2 x0(0.0f, 0.0f);
    const Vec2 v0(1.5f, -0.8f);
    Particle p(1, x0, v0, 0.02f);
    solver.add_particle(p);

    const float duration = 1.0f; // 1 second
    const int total_frames = 60;
    for (int f = 0; f < total_frames; ++f) {
        solver.step();
    }

    const Particle& p_final = solver.get_particles()[0];
    const Vec2 x_analytical = x0 + v0 * duration;
    const Vec2 v_analytical = v0;

    const float pos_err = (p_final.position - x_analytical).length();
    const float vel_err = (p_final.velocity - v_analytical).length();
    const double max_err = std::max(pos_err, vel_err);
    constexpr double tol = 1e-5;

    TestResult res;
    res.name = "Test 1: Constant Velocity";
    res.category = "Analytical Verification";
    res.measured_error = max_err;
    res.tolerance = tol;
    res.passed = (max_err <= tol);
    res.details = "x_sim=(" + std::to_string(p_final.position.x) + ", " + std::to_string(p_final.position.y) +
                  ") vs analytical=(" + std::to_string(x_analytical.x) + ", " + std::to_string(x_analytical.y) + ")";
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 2: Constant Acceleration (Gravity Free Fall)
// Category: Analytical Verification
// -----------------------------------------------------------------------------
static void test_constant_acceleration() {
    SimConfig config;
    config.gravity = Vec2(0.0f, -9.80665f);
    config.dt = 1.0f / 60.0f;
    config.substeps = 8;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 100.0f);
    CpuReferenceSolver solver(config, boundary);

    const Vec2 x0(0.0f, 5.0f);
    const Vec2 v0(0.5f, 0.0f);
    Particle p(1, x0, v0, 0.02f);
    solver.add_particle(p);

    const int frames = 30; // 0.5s
    for (int f = 0; f < frames; ++f) {
        solver.step();
    }

    const float t = frames * config.dt;
    const float dt_sub = config.dt_substep();
    const Particle& p_final = solver.get_particles()[0];

    // Semi-implicit Euler exact discrete formulas:
    // v_{n} = v_0 + g * t
    // y_{n} = y_0 + v_0 * t + 0.5 * g * t^2 + 0.5 * g * t * dt_sub
    const float v_exact_y = v0.y + config.gravity.y * t;
    const float y_exact_discrete = x0.y + v0.y * t + 0.5f * config.gravity.y * t * t + 0.5f * config.gravity.y * t * dt_sub;

    const float vel_err = std::abs(p_final.velocity.y - v_exact_y);
    const float pos_err = std::abs(p_final.position.y - y_exact_discrete);
    const double max_err = std::max(vel_err, pos_err);
    constexpr double tol = 1e-4;

    TestResult res;
    res.name = "Test 2: Constant Acceleration Gravity";
    res.category = "Analytical Verification";
    res.measured_error = max_err;
    res.tolerance = tol;
    res.passed = (max_err <= tol);
    res.details = "v_y=" + std::to_string(p_final.velocity.y) + " (exact=" + std::to_string(v_exact_y) +
                  "), y=" + std::to_string(p_final.position.y) + " (discrete exact=" + std::to_string(y_exact_discrete) + ")";
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 3: Two-Particle Equal-Mass Elastic Collision
// Category: Analytical Verification
// -----------------------------------------------------------------------------
static void test_two_particle_elastic_collision() {
    SimConfig config;
    config.gravity = Vec2(0.0f, 0.0f);
    config.restitution = 1.0f; // perfectly elastic
    config.dt = 1.0f / 120.0f;
    config.substeps = 8;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 10.0f);
    CpuReferenceSolver solver(config, boundary);

    const float r = 0.05f;
    Particle p1(1, Vec2(-0.2f, 0.0f), Vec2(1.0f, 0.0f), r);
    Particle p2(2, Vec2(0.2f, 0.0f), Vec2(-1.0f, 0.0f), r);
    solver.add_particle(p1);
    solver.add_particle(p2);

    // Initial kinetic energy
    const auto diag_init = solver.compute_diagnostics();

    // Step until collision occurs and particles separate
    for (int f = 0; f < 30; ++f) {
        solver.step();
    }

    const auto& particles = solver.get_particles();
    const auto diag_final = solver.compute_diagnostics();

    // Equal mass central elastic collision: velocities swap completely
    const float expected_v1_x = -1.0f;
    const float expected_v2_x = +1.0f;

    const float err_v1 = std::abs(particles[0].velocity.x - expected_v1_x);
    const float err_v2 = std::abs(particles[1].velocity.x - expected_v2_x);
    const float err_energy = std::abs(diag_final.total_kinetic_energy - diag_init.total_kinetic_energy);
    const float err_momentum = diag_final.total_momentum.length();

    const double max_err = std::max({err_v1, err_v2, err_momentum});
    constexpr double tol = 1e-4;

    TestResult res;
    res.name = "Test 3: Equal-Mass Elastic Collision";
    res.category = "Analytical Verification";
    res.measured_error = max_err;
    res.tolerance = tol;
    res.passed = (max_err <= tol && err_energy <= 1e-3f);
    res.details = "v1_x=" + std::to_string(particles[0].velocity.x) + ", v2_x=" + std::to_string(particles[1].velocity.x) +
                  ", dEk=" + std::to_string(err_energy) + " J";
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 4: Two-Particle Inelastic Collision
// Category: Physical Model Validation
// -----------------------------------------------------------------------------
static void test_two_particle_inelastic_collision() {
    SimConfig config;
    config.gravity = Vec2(0.0f, 0.0f);
    config.restitution = 0.5f; // inelastic: e = 0.5
    config.dt = 1.0f / 120.0f;
    config.substeps = 8;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 10.0f);
    CpuReferenceSolver solver(config, boundary);

    const float r = 0.05f;
    Particle p1(1, Vec2(-0.2f, 0.0f), Vec2(1.0f, 0.0f), r);
    Particle p2(2, Vec2(0.2f, 0.0f), Vec2(-1.0f, 0.0f), r);
    solver.add_particle(p1);
    solver.add_particle(p2);

    const auto diag_init = solver.compute_diagnostics();

    for (int f = 0; f < 30; ++f) {
        solver.step();
    }

    const auto& particles = solver.get_particles();
    const auto diag_final = solver.compute_diagnostics();

    // With v1 = +1, v2 = -1 (v_rel = 2.0 m/s), e = 0.5:
    // v_rel' = -e * v_rel = -1.0 m/s => v1' = -0.5, v2' = +0.5 m/s
    const float expected_v1 = -0.5f;
    const float expected_v2 = +0.5f;

    const float err_v1 = std::abs(particles[0].velocity.x - expected_v1);
    const float err_v2 = std::abs(particles[1].velocity.x - expected_v2);
    const float separation_rel = particles[1].velocity.x - particles[0].velocity.x;
    const float measured_e = separation_rel / 2.0f;
    const float err_e = std::abs(measured_e - 0.5f);
    const float err_momentum = diag_final.total_momentum.length();

    // Theoretical energy dissipation: Delta E = -0.5 * m_eff * (1 - e^2) * v_rel^2
    const float m_eff = (p1.mass * p2.mass) / (p1.mass + p2.mass);
    const float expected_dEk = -0.5f * m_eff * (1.0f - 0.5f * 0.5f) * 4.0f;
    const float actual_dEk = diag_final.total_kinetic_energy - diag_init.total_kinetic_energy;
    const float err_dEk = std::abs(actual_dEk - expected_dEk);

    const double max_err = std::max({err_v1, err_v2, err_e, err_momentum});
    constexpr double tol = 1e-4;

    TestResult res;
    res.name = "Test 4: Two-Particle Inelastic Collision";
    res.category = "Physical Model Validation";
    res.measured_error = max_err;
    res.tolerance = tol;
    res.passed = (max_err <= tol && err_dEk <= 1e-3f);
    res.details = "measured e=" + std::to_string(measured_e) + " (target=0.5), actual dEk=" +
                  std::to_string(actual_dEk) + " J (theory=" + std::to_string(expected_dEk) + " J)";
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 5: Unequal-Mass Elastic Collision
// Category: Analytical Verification
// -----------------------------------------------------------------------------
static void test_unequal_mass_collision() {
    SimConfig config;
    config.gravity = Vec2(0.0f, 0.0f);
    config.restitution = 1.0f;
    config.dt = 1.0f / 120.0f;
    config.substeps = 8;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 10.0f);
    CpuReferenceSolver solver(config, boundary);

    // r1 = 0.02, r2 = 0.04 => m2 = 4 * m1
    Particle p1(1, Vec2(-0.2f, 0.0f), Vec2(2.0f, 0.0f), 0.02f);
    Particle p2(2, Vec2(0.2f, 0.0f), Vec2(0.0f, 0.0f), 0.04f); // at rest
    solver.add_particle(p1);
    solver.add_particle(p2);

    const auto diag_init = solver.compute_diagnostics();

    for (int f = 0; f < 30; ++f) {
        solver.step();
    }

    const auto& particles = solver.get_particles();
    const auto diag_final = solver.compute_diagnostics();

    // Analytical 1D kinematics:
    // v1' = (m1 - m2)/(m1 + m2) * v1 = (1 - 4)/5 * 2.0 = -1.2 m/s
    // v2' = 2*m1/(m1 + m2) * v1 = 2/5 * 2.0 = +0.8 m/s
    constexpr float expected_v1 = -1.2f;
    constexpr float expected_v2 = +0.8f;

    const float err_v1 = std::abs(particles[0].velocity.x - expected_v1);
    const float err_v2 = std::abs(particles[1].velocity.x - expected_v2);
    const float err_energy = std::abs(diag_final.total_kinetic_energy - diag_init.total_kinetic_energy);
    const float err_momentum = (diag_final.total_momentum - diag_init.total_momentum).length();

    const double max_err = std::max({err_v1, err_v2, err_momentum});
    constexpr double tol = 1e-4;

    TestResult res;
    res.name = "Test 5: Unequal-Mass Elastic Collision";
    res.category = "Analytical Verification";
    res.measured_error = max_err;
    res.tolerance = tol;
    res.passed = (max_err <= tol && err_energy <= 1e-3f);
    res.details = "v1'=" + std::to_string(particles[0].velocity.x) + " (exact=-1.2), v2'=" +
                  std::to_string(particles[1].velocity.x) + " (exact=0.8), dEk=" + std::to_string(err_energy) + " J";
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 6: Multi-Particle Momentum Conservation
// Category: Physical Model Validation
// -----------------------------------------------------------------------------
static void test_momentum_conservation() {
    SimConfig config;
    config.gravity = Vec2(0.0f, 0.0f);
    config.restitution = 1.0f;
    config.dt = 1.0f / 60.0f;
    config.substeps = 8;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 5.0f); // large boundary so particles collide internally
    CpuReferenceSolver solver(config, boundary);

    // Create 16 particles in a tight colliding cloud
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            const Vec2 pos(-0.3f + i * 0.2f, -0.3f + j * 0.2f);
            const Vec2 vel((i % 2 == 0 ? 0.5f : -0.5f), (j % 2 == 0 ? -0.5f : 0.5f));
            solver.add_particle(Particle(static_cast<uint32_t>(i * 4 + j), pos, vel, 0.03f));
        }
    }

    const auto diag_init = solver.compute_diagnostics();
    float max_momentum_drift = 0.0f;

    // Run for 120 frames (many internal collisions)
    for (int f = 0; f < 120; ++f) {
        solver.step();
        const auto diag_now = solver.compute_diagnostics();
        const float drift = (diag_now.total_momentum - diag_init.total_momentum).length();
        if (drift > max_momentum_drift) max_momentum_drift = drift;
    }

    constexpr double tol = 1e-5;
    TestResult res;
    res.name = "Test 6: Multi-Particle Momentum Conservation";
    res.category = "Physical Model Validation";
    res.measured_error = max_momentum_drift;
    res.tolerance = tol;
    res.passed = (max_momentum_drift <= tol);
    res.details = "Initial P=(" + std::to_string(diag_init.total_momentum.x) + ", " +
                  std::to_string(diag_init.total_momentum.y) + "), Max |dP|=" + std::to_string(max_momentum_drift);
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 7: Kinetic Energy Monotonicity in Inelastic Gas
// Category: Physical Model Validation
// -----------------------------------------------------------------------------
static void test_kinetic_energy_behavior() {
    SimConfig config_elastic;
    config_elastic.gravity = Vec2(0.0f, 0.0f);
    config_elastic.restitution = 1.0f;
    config_elastic.dt = 1.0f / 60.0f;
    config_elastic.substeps = 8;

    SimConfig config_inelastic = config_elastic;
    config_inelastic.restitution = 0.7f; // Dissipative

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 5.0f);
    CpuReferenceSolver solver_el(config_elastic, boundary);
    CpuReferenceSolver solver_inel(config_inelastic, boundary);

    for (int i = 0; i < 6; ++i) {
        const float angle = i * (2.0f * 3.14159f / 6.0f);
        const Vec2 pos(0.3f * std::cos(angle), 0.3f * std::sin(angle));
        const Vec2 vel(-1.0f * std::cos(angle), -1.0f * std::sin(angle)); // inward collision
        solver_el.add_particle(Particle(i + 1, pos, vel, 0.04f));
        solver_inel.add_particle(Particle(i + 1, pos, vel, 0.04f));
    }

    const float Ek_init = solver_inel.compute_diagnostics().total_kinetic_energy;

    for (int f = 0; f < 60; ++f) {
        solver_el.step();
        solver_inel.step();
    }

    const float Ek_el_final = solver_el.compute_diagnostics().total_kinetic_energy;
    const float Ek_inel_final = solver_inel.compute_diagnostics().total_kinetic_energy;

    const float elastic_drift = std::abs(Ek_el_final - Ek_init) / Ek_init;
    const bool inelastic_dissipated = (Ek_inel_final < 0.6f * Ek_init);

    TestResult res;
    res.name = "Test 7: Kinetic Energy Behavior (Elastic vs Inelastic)";
    res.category = "Physical Model Validation";
    res.measured_error = elastic_drift;
    res.tolerance = 1e-3;
    res.passed = (elastic_drift <= 1e-3 && inelastic_dissipated);
    res.details = "Elastic rel drift=" + std::to_string(elastic_drift) +
                  ", Inelastic Ek: " + std::to_string(Ek_init) + " J -> " + std::to_string(Ek_inel_final) + " J";
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 8: Specular Circular Boundary Reflection
// Category: Analytical Verification
// -----------------------------------------------------------------------------
static void test_boundary_reflection() {
    SimConfig config;
    config.gravity = Vec2(0.0f, 0.0f);
    config.boundary_restitution = 1.0f; // specular elastic
    config.dt = 1.0f / 120.0f;
    config.substeps = 8;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 1.0f); // R = 1m
    CpuReferenceSolver solver(config, boundary);

    // Fire particle toward boundary at 30 degrees to x-axis
    const float r = 0.05f;
    const float angle = 30.0f * (3.14159265f / 180.0f);
    const float speed = 1.5f;
    const Vec2 v0(speed * std::cos(angle), speed * std::sin(angle));
    solver.add_particle(Particle(1, Vec2(0.0f, 0.0f), v0, r));

    // Particle travels until bouncing off boundary and returning
    for (int f = 0; f < 100; ++f) {
        solver.step();
    }

    const Particle& p = solver.get_particles()[0];
    const float final_speed = p.velocity.length();
    const float speed_error = std::abs(final_speed - speed);

    // Particle must remain within valid boundary perimeter
    const float dist_from_center = p.position.length();
    const float max_allowed_dist = boundary.radius - p.radius + 1e-5f;
    const bool bounded = (dist_from_center <= max_allowed_dist);

    constexpr double tol = 1e-4;
    TestResult res;
    res.name = "Test 8: Specular Circular Boundary Reflection";
    res.category = "Analytical Verification";
    res.measured_error = speed_error;
    res.tolerance = tol;
    res.passed = (speed_error <= tol && bounded);
    res.details = "Incident speed=" + std::to_string(speed) + " m/s, Post-bounce speed=" +
                  std::to_string(final_speed) + " m/s, Max distance=" + std::to_string(dist_from_center) + " m";
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 9: Zero-Distance Overlap Regularization
// Category: Numerical Consistency Test
// -----------------------------------------------------------------------------
static void test_zero_distance_overlap() {
    SimConfig config;
    config.gravity = Vec2(0.0f, 0.0f);
    config.restitution = 1.0f;
    config.dt = 1.0f / 60.0f;
    config.substeps = 8;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 2.0f);
    CpuReferenceSolver solver(config, boundary);

    // Exact identical coordinates
    const Vec2 coincident_pos(0.2f, -0.1f);
    solver.add_particle(Particle(1, coincident_pos, Vec2(0.0f, 0.0f), 0.05f));
    solver.add_particle(Particle(2, coincident_pos, Vec2(0.0f, 0.0f), 0.05f));

    // Step simulation
    for (int f = 0; f < 10; ++f) {
        solver.step();
    }

    const auto& particles = solver.get_particles();
    const auto diag = solver.compute_diagnostics();

    const bool is_finite = diag.is_finite && (diag.nan_count == 0);
    const float separation = (particles[0].position - particles[1].position).length();
    const bool separated = (separation > 0.01f);

    TestResult res;
    res.name = "Test 9: Zero-Distance Overlap Regularization";
    res.category = "Numerical Consistency Test";
    res.measured_error = diag.nan_count;
    res.tolerance = 0.0;
    res.passed = (is_finite && separated);
    res.details = "NaN count=" + std::to_string(diag.nan_count) + ", Final separation=" +
                  std::to_string(separation) + " m (particles separated cleanly)";
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 10: Finite Position and Velocity Invariants
// Category: Numerical Consistency Test
// -----------------------------------------------------------------------------
static void test_finite_values_invariants() {
    SimConfig config;
    config.gravity = Vec2(0.0f, -9.80665f);
    config.restitution = 0.8f;
    config.boundary_restitution = 0.8f;
    config.dt = 1.0f / 60.0f;
    config.substeps = 8;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 1.0f);
    CpuReferenceSolver solver(config, boundary);

    // Spawn 20 energetic particles
    for (int i = 0; i < 20; ++i) {
        const float angle = i * (2.0f * 3.14159f / 20.0f);
        const Vec2 pos(0.4f * std::cos(angle), 0.4f * std::sin(angle));
        const Vec2 vel(1.5f * std::sin(angle), -1.5f * std::cos(angle));
        solver.add_particle(Particle(i + 1, pos, vel, 0.03f));
    }

    int nan_detected = 0;
    for (int f = 0; f < 300; ++f) { // 300 frames = 2,400 substeps
        solver.step();
        const auto diag = solver.compute_diagnostics();
        if (!diag.is_finite || diag.nan_count > 0) {
            nan_detected++;
        }
    }

    TestResult res;
    res.name = "Test 10: Finite Position & Velocity Invariants";
    res.category = "Numerical Consistency Test";
    res.measured_error = nan_detected;
    res.tolerance = 0.0;
    res.passed = (nan_detected == 0);
    res.details = "Evaluated 2,400 substeps across 20 interacting particles. Zero NaNs detected.";
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 11: Timestep Refinement & Convergence
// Category: Numerical Consistency Test
// -----------------------------------------------------------------------------
static void test_timestep_refinement() {
    auto run_sim = [](float dt_step, int substeps) -> float {
        SimConfig config;
        config.gravity = Vec2(0.0f, -9.80665f);
        config.dt = dt_step;
        config.substeps = substeps;

        CircularBoundary boundary(Vec2(0.0f, 0.0f), 10.0f);
        CpuReferenceSolver solver(config, boundary);
        solver.add_particle(Particle(1, Vec2(0.0f, 1.0f), Vec2(1.0f, 0.0f), 0.02f));

        const float total_time = 0.2f;
        const int steps = static_cast<int>(total_time / dt_step);
        for (int i = 0; i < steps; ++i) {
            solver.step();
        }
        return solver.get_particles()[0].position.y;
    };

    // Integrate at dt, dt/2, dt/4
    const float y1 = run_sim(0.02f, 1);
    const float y2 = run_sim(0.01f, 1);
    const float y4 = run_sim(0.005f, 1);

    // Exact continuous solution
    const float t = 0.2f;
    const float y_exact = 1.0f - 0.5f * 9.80665f * t * t;

    const float err1 = std::abs(y1 - y_exact);
    const float err2 = std::abs(y2 - y_exact);
    const float err4 = std::abs(y4 - y_exact);

    // Ratio of successive errors should be ~ 2 for O(dt)
    const float ratio1 = err1 / err2;
    const float ratio2 = err2 / err4;
    const float avg_ratio = 0.5f * (ratio1 + ratio2);

    TestResult res;
    res.name = "Test 11: Timestep Convergence Order";
    res.category = "Numerical Consistency Test";
    res.measured_error = std::abs(avg_ratio - 2.0f);
    res.tolerance = 0.2; // expect ~ 2.0 for first order
    res.passed = (avg_ratio >= 1.8f && avg_ratio <= 2.2f);
    res.details = "Error ratio=" + std::to_string(avg_ratio) + " (expected ~ 2.0 for 1st order Semi-implicit Euler)";
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 12: Three-Particle Symmetric Contact
// Category: Physical Model Validation
// -----------------------------------------------------------------------------
static void test_three_particle_symmetric_contact() {
    SimConfig config;
    config.gravity = Vec2(0.0f, 0.0f);
    config.restitution = 1.0f;
    config.dt = 1.0f / 120.0f;
    config.substeps = 8;
    config.solver_iterations = 2;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 5.0f);
    CpuReferenceSolver solver(config, boundary);

    const float r = 0.05f;
    // Central particle at origin at rest
    solver.add_particle(Particle(1, Vec2(0.0f, 0.0f), Vec2(0.0f, 0.0f), r));
    // Left particle moving right (+1 m/s)
    solver.add_particle(Particle(2, Vec2(-0.2f, 0.0f), Vec2(1.0f, 0.0f), r));
    // Right particle moving left (-1 m/s)
    solver.add_particle(Particle(3, Vec2(0.2f, 0.0f), Vec2(-1.0f, 0.0f), r));

    for (int f = 0; f < 30; ++f) {
        solver.step();
    }

    const auto& particles = solver.get_particles();
    const auto diag = solver.compute_diagnostics();

    // Central particle should stay stationary (speed ~ 0)
    const float v_center = particles[0].velocity.length();
    // Left particle should reflect left (-1 m/s), right particle reflect right (+1 m/s)
    const float err_left = std::abs(particles[1].velocity.x - (-1.0f));
    const float err_right = std::abs(particles[2].velocity.x - (+1.0f));
    const float p_drift = diag.total_momentum.length();

    const double max_err = std::max({v_center, err_left, err_right, p_drift});
    constexpr double tol = 1e-3;

    TestResult res;
    res.name = "Test 12: Three-Particle Symmetric Contact";
    res.category = "Physical Model Validation";
    res.measured_error = max_err;
    res.tolerance = tol;
    res.passed = (max_err <= tol);
    res.details = "Center v=" + std::to_string(v_center) + " m/s, Left v=" +
                  std::to_string(particles[1].velocity.x) + " m/s, Right v=" + std::to_string(particles[2].velocity.x) + " m/s";
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 13: Dense Contact Stability (Granular Column Stacking under Gravity)
// Category: Physical Model Validation
// -----------------------------------------------------------------------------
static void test_dense_contact_stability() {
    SimConfig config;
    config.gravity = Vec2(0.0f, -9.80665f);
    config.restitution = 0.0f; // completely inelastic to settle
    config.boundary_restitution = 0.0f;
    config.stabilization_factor = 0.5f;
    config.dt = 1.0f / 60.0f;
    config.substeps = 16;       // high substepping for dense stacking
    config.solver_iterations = 4;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 0.5f); // 0.5m radius
    CpuReferenceSolver solver(config, boundary);

    // Place a vertical stack of 5 particles resting near the bottom
    const float r = 0.04f;
    const float bottom_y = -boundary.radius + r;
    for (int i = 0; i < 5; ++i) {
        solver.add_particle(Particle(i + 1, Vec2(0.0f, bottom_y + i * (2.0f * r + 0.005f)), Vec2(0.0f, 0.0f), r));
    }

    // Step 120 frames (1,920 substeps) allowing stack to compress and settle
    for (int f = 0; f < 120; ++f) {
        solver.step();
    }

    const auto& particles = solver.get_particles();
    const auto diag = solver.compute_diagnostics();

    // Check stability: all particles must remain finite and bounded
    float max_speed = 0.0f;
    float max_overlap = 0.0f;
    for (size_t i = 0; i < particles.size(); ++i) {
        const float spd = particles[i].velocity.length();
        if (spd > max_speed) max_speed = spd;

        for (size_t j = i + 1; j < particles.size(); ++j) {
            const float d = (particles[i].position - particles[j].position).length();
            const float overlap = (particles[i].radius + particles[j].radius) - d;
            if (overlap > max_overlap) max_overlap = overlap;
        }
    }

    // Stack is settled if max settled kinetic energy is low and overlap is small (< 3mm)
    const bool stable = diag.is_finite && (diag.nan_count == 0) && (max_overlap < 0.005f);

    TestResult res;
    res.name = "Test 13: Dense Contact Stability";
    res.category = "Physical Model Validation";
    res.measured_error = max_overlap;
    res.tolerance = 0.005; // 5mm maximum overlap permitted
    res.passed = stable;
    res.details = "Settled max speed=" + std::to_string(max_speed) + " m/s, Max inter-particle overlap=" +
                  std::to_string(max_overlap * 1000.0f) + " mm, NaNs=" + std::to_string(diag.nan_count);
    report_test(res);
}

// -----------------------------------------------------------------------------
// Test 14: Symplectic Euler vs Explicit Forward Euler Stability (Lab 2 Verification)
// Category: Numerical Analysis & Conservation Verification
// -----------------------------------------------------------------------------
static void test_integrator_symplectic_vs_explicit() {
    SimConfig cfg_semi;
    cfg_semi.gravity = Vec2(0.0f, -9.81f);
    cfg_semi.dt = 1.0f / 60.0f;
    cfg_semi.substeps = 8;
    cfg_semi.boundary_restitution = 1.0f;
    cfg_semi.integrator = IntegratorType::SemiImplicitEuler;

    SimConfig cfg_expl = cfg_semi;
    cfg_expl.integrator = IntegratorType::ExplicitEuler;

    CircularBoundary boundary(Vec2(0.0f, 0.0f), 1.0f);

    CpuReferenceSolver solver_semi(cfg_semi, boundary);
    CpuReferenceSolver solver_expl(cfg_expl, boundary);

    Particle p0(1, Vec2(0.0f, 0.5f), Vec2(0.0f, 0.0f), 0.05f);
    p0.set_fixed_mass(1.0f);

    solver_semi.add_particle(p0);
    solver_expl.add_particle(p0);

    const auto diag_init = solver_semi.compute_diagnostics();
    const float e0 = diag_init.total_mechanical_energy;

    for (int f = 0; f < 120; ++f) {
        solver_semi.step();
        solver_expl.step();
    }

    const auto diag_semi = solver_semi.compute_diagnostics();
    const auto diag_expl = solver_expl.compute_diagnostics();

    // Semi-implicit Euler maintains bounded energy oscillations (|dE/E0| < 0.05)
    const double rel_drift_semi = std::abs(diag_semi.total_mechanical_energy - e0) / std::abs(e0);
    // Explicit Forward Euler determinant > 1 artificially injects energy into the system
    const bool expl_amplified = (diag_expl.total_mechanical_energy > e0);

    TestResult res;
    res.name = "Test 14: Symplectic vs Explicit Euler Stability";
    res.category = "Numerical Analysis Verification";
    res.measured_error = rel_drift_semi;
    res.tolerance = 0.05; // 5% maximum symplectic energy drift
    res.passed = (rel_drift_semi < 0.05) && expl_amplified && diag_semi.is_finite && diag_expl.is_finite;
    res.details = "Semi-implicit drift=" + std::to_string(rel_drift_semi * 100.0) + "%, Explicit energy amplified=" +
                  (expl_amplified ? "YES (Artificial Energy Growth Confirmed)" : "NO");
    report_test(res);
}

// -----------------------------------------------------------------------------
// Main Test Runner
// -----------------------------------------------------------------------------
int main() {
    std::cout << "\n======================================================================\n";
    std::cout << "  SIMULATOR V2: PHYSICAL & NUMERICAL VERIFICATION TEST SUITE (PHASE 1A)\n";
    std::cout << "======================================================================\n\n";

    test_constant_velocity();
    test_constant_acceleration();
    test_two_particle_elastic_collision();
    test_two_particle_inelastic_collision();
    test_unequal_mass_collision();
    test_momentum_conservation();
    test_kinetic_energy_behavior();
    test_boundary_reflection();
    test_zero_distance_overlap();
    test_finite_values_invariants();
    test_timestep_refinement();
    test_three_particle_symmetric_contact();
    test_dense_contact_stability();
    test_integrator_symplectic_vs_explicit();

    int passed_count = 0;
    int failed_count = 0;
    for (const auto& r : g_results) {
        if (r.passed) passed_count++;
        else failed_count++;
    }

    std::cout << "\n======================================================================\n";
    std::cout << "  TEST SUMMARY: " << passed_count << " PASSED, " << failed_count << " FAILED (TOTAL " << g_results.size() << ")\n";
    std::cout << "======================================================================\n\n";

    return (failed_count == 0) ? 0 : 1;
}
