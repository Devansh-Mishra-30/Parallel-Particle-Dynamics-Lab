#include "sim_core/boundary.h"
#include "sim_core/cpu_reference_solver.h"
#include "sim_core/types.h"
#include "sim_cuda_v2/cuda_solver.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace sim_core;
using namespace sim_cuda;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

static void report_test(const std::string& name, bool pass, const std::string& details = "") {
    std::cout << "  " << (pass ? "[PASS] " : "[FAIL] ")
              << std::left << std::setw(54) << name;
    if (!details.empty()) {
        std::cout << " (" << details << ")";
    }
    std::cout << "\n";
    if (pass) g_tests_passed++;
    else g_tests_failed++;
}

// -----------------------------------------------------------------------------
// Test 1: Constant Velocity (Zero Gravity, Free Flight)
// -----------------------------------------------------------------------------
static void test_constant_velocity(CudaSolverV2& solver) {
    SimConfig cfg;
    cfg.dt = 0.01f;
    cfg.substeps = 1;
    cfg.gravity = Vec2(0.0f, 0.0f);
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 10.0f);

    solver.set_config(cfg);
    solver.set_boundary(bnd);

    Particle p(1, Vec2(0.0f, 0.0f), Vec2(1.5f, -0.8f), 0.05f);
    p.set_fixed_mass(1.0f);
    solver.set_particles({p});

    for (int i = 0; i < 100; ++i) solver.step();

    const auto& particles = solver.get_particles();
    const Vec2 expected_pos(1.5f * 1.0f, -0.8f * 1.0f);
    const float pos_err = (particles[0].position - expected_pos).length();
    const float vel_err = (particles[0].velocity - Vec2(1.5f, -0.8f)).length();

    report_test("Constant Velocity (Zero Gravity)",
                pos_err < 1e-4f && vel_err < 1e-5f,
                "pos_err: " + std::to_string(pos_err) + " m");
}

// -----------------------------------------------------------------------------
// Test 2: Gravitational Free-Fall & Parabolic Ballistics
// -----------------------------------------------------------------------------
static void test_gravitational_ballistics(CudaSolverV2& solver) {
    SimConfig cfg;
    cfg.dt = 0.01f;
    cfg.substeps = 10;
    cfg.gravity = Vec2(0.0f, -9.80665f);
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 100.0f);

    solver.set_config(cfg);
    solver.set_boundary(bnd);

    Particle p(1, Vec2(0.0f, 0.0f), Vec2(2.0f, 5.0f), 0.05f);
    p.set_fixed_mass(1.0f);

    // Compute CPU reference baseline
    CpuReferenceSolver cpu_solver(cfg, bnd);
    cpu_solver.set_particles({p});
    solver.set_particles({p});

    for (int i = 0; i < 50; ++i) {
        cpu_solver.step();
        solver.step();
    }

    const auto& p_gpu = solver.get_particles()[0];
    const auto& p_cpu = cpu_solver.get_particles()[0];
    const float pos_err = (p_gpu.position - p_cpu.position).length();
    const float vel_err = (p_gpu.velocity - p_cpu.velocity).length();

    report_test("Gravitational Ballistic Flight vs CPU Ref",
                pos_err < 1e-4f && vel_err < 1e-4f,
                "pos_err: " + std::to_string(pos_err) + " m");
}

// -----------------------------------------------------------------------------
// Test 3: Equal-Mass Elastic Central Collision (e = 1.0)
// -----------------------------------------------------------------------------
static void test_equal_elastic_collision(CudaSolverV2& solver) {
    SimConfig cfg;
    cfg.dt = 0.005f;
    cfg.substeps = 8;
    cfg.gravity = Vec2(0.0f, 0.0f);
    cfg.restitution = 1.0f;
    cfg.solver_iterations = 4;
    cfg.stabilization_factor = 0.5f;
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 5.0f);

    solver.set_config(cfg);
    solver.set_boundary(bnd);

    // Particles approaching head-on
    Particle p1(1, Vec2(-0.20f, 0.0f), Vec2(1.0f, 0.0f), 0.05f);
    Particle p2(2, Vec2(0.20f, 0.0f), Vec2(-1.0f, 0.0f), 0.05f);
    p1.set_fixed_mass(1.0f);
    p2.set_fixed_mass(1.0f);

    solver.set_particles({p1, p2});

    // Step until collision occurs and particles separate
    for (int i = 0; i < 50; ++i) solver.step();

    const auto& res = solver.get_particles();
    // After equal-mass elastic head-on collision: v1' = -1.0, v2' = +1.0
    const float err1 = std::abs(res[0].velocity.x - (-1.0f));
    const float err2 = std::abs(res[1].velocity.x - (1.0f));
    const float total_momentum_x = res[0].mass * res[0].velocity.x + res[1].mass * res[1].velocity.x;

    report_test("Equal-Mass Elastic Collision (e=1.0)",
                err1 < 1e-3f && err2 < 1e-3f && std::abs(total_momentum_x) < 1e-5f,
                "v1=" + std::to_string(res[0].velocity.x).substr(0, 6) + " v2=" + std::to_string(res[1].velocity.x).substr(0, 6));
}

// -----------------------------------------------------------------------------
// Test 4: Unequal-Mass Elastic Central Collision (m2 = 2 * m1, e = 1.0)
// -----------------------------------------------------------------------------
static void test_unequal_elastic_collision(CudaSolverV2& solver) {
    SimConfig cfg;
    cfg.dt = 0.005f;
    cfg.substeps = 8;
    cfg.gravity = Vec2(0.0f, 0.0f);
    cfg.restitution = 1.0f;
    cfg.solver_iterations = 4;
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 5.0f);

    solver.set_config(cfg);
    solver.set_boundary(bnd);

    const float m1 = 1.0f;
    const float m2 = 2.0f;
    const float u1 = 1.0f;
    const float u2 = -0.5f;

    // Analytical 1D elastic targets:
    // v1' = ((m1-m2)u1 + 2m2 u2) / (m1+m2) = ((-1)(1) + 2(2)(-0.5)) / 3 = (-1 - 2)/3 = -1.0 m/s
    // v2' = ((m2-m1)u2 + 2m1 u1) / (m1+m2) = ((1)(-0.5) + 2(1)(1)) / 3 = (-0.5 + 2)/3 = +0.5 m/s
    const float v1_target = -1.0f;
    const float v2_target = 0.5f;

    Particle p1(1, Vec2(-0.20f, 0.0f), Vec2(u1, 0.0f), 0.05f);
    Particle p2(2, Vec2(0.20f, 0.0f), Vec2(u2, 0.0f), 0.05f);
    p1.set_fixed_mass(m1);
    p2.set_fixed_mass(m2);

    solver.set_particles({p1, p2});
    for (int i = 0; i < 50; ++i) solver.step();

    const auto& res = solver.get_particles();
    const float err1 = std::abs(res[0].velocity.x - v1_target);
    const float err2 = std::abs(res[1].velocity.x - v2_target);

    report_test("Unequal-Mass Elastic Collision (m2=2*m1)",
                err1 < 1e-3f && err2 < 1e-3f,
                "v1=" + std::to_string(res[0].velocity.x).substr(0, 6) + " v2=" + std::to_string(res[1].velocity.x).substr(0, 6));
}

// -----------------------------------------------------------------------------
// Test 5: Inelastic Collision (e = 0.528, Theoretical dEk Verification)
// -----------------------------------------------------------------------------
static void test_inelastic_collision(CudaSolverV2& solver) {
    SimConfig cfg;
    cfg.dt = 0.005f;
    cfg.substeps = 8;
    cfg.gravity = Vec2(0.0f, 0.0f);
    cfg.restitution = 0.528f;
    cfg.solver_iterations = 4;
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 5.0f);

    solver.set_config(cfg);
    solver.set_boundary(bnd);

    Particle p1(1, Vec2(-0.20f, 0.0f), Vec2(1.0f, 0.0f), 0.05f);
    Particle p2(2, Vec2(0.20f, 0.0f), Vec2(-1.0f, 0.0f), 0.05f);
    p1.set_fixed_mass(1.0f);
    p2.set_fixed_mass(1.0f);

    const float initial_ek = 0.5f * p1.mass * 1.0f + 0.5f * p2.mass * 1.0f; // 1.0 J

    // Analytical dEk = -0.5 * mu * (1 - e^2) * vn^2
    // mu = (1*1)/(1+1) = 0.5 kg, vn = -2.0 m/s
    // dEk = -0.5 * 0.5 * (1 - 0.528^2) * 4.0 = -0.25 * (1 - 0.278784) * 4 = -0.721216 J
    const float mu = 0.5f;
    const float exact_dek = -0.5f * mu * (1.0f - cfg.restitution * cfg.restitution) * (4.0f);

    solver.set_particles({p1, p2});
    for (int i = 0; i < 50; ++i) solver.step();

    const auto& res = solver.get_particles();
    const float post_ek = 0.5f * res[0].mass * res[0].velocity.length_squared()
                        + 0.5f * res[1].mass * res[1].velocity.length_squared();
    const float measured_dek = post_ek - initial_ek;
    const float err = std::abs(measured_dek - exact_dek);

    report_test("Inelastic Collision (e=0.528, dEk error)",
                err < 1e-3f,
                "measured dEk=" + std::to_string(measured_dek).substr(0, 6) + " J, exact=" + std::to_string(exact_dek).substr(0, 6) + " J");
}

// -----------------------------------------------------------------------------
// Test 6: Oblique Collision (b = 0.04m, e = 0.90)
// -----------------------------------------------------------------------------
static void test_oblique_collision(CudaSolverV2& solver) {
    SimConfig cfg;
    cfg.dt = 0.005f;
    cfg.substeps = 8;
    cfg.gravity = Vec2(0.0f, 0.0f);
    cfg.restitution = 0.90f;
    cfg.solver_iterations = 4;
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 5.0f);

    solver.set_config(cfg);
    solver.set_boundary(bnd);

    const float b = 0.04f;
    Particle p1(1, Vec2(-0.25f,  b * 0.5f), Vec2( 1.2f, 0.0f), 0.06f);
    Particle p2(2, Vec2( 0.25f, -b * 0.5f), Vec2(-1.2f, 0.0f), 0.06f);
    p1.set_fixed_mass(1.0f);
    p2.set_fixed_mass(1.0f);

    solver.set_particles({p1, p2});
    for (int i = 0; i < 60; ++i) solver.step();

    const auto& res = solver.get_particles();
    // After oblique impact, particles should deflect in y
    const bool deflected_y = (std::abs(res[0].velocity.y) > 0.05f) && (std::abs(res[1].velocity.y) > 0.05f);
    const float total_py = res[0].mass * res[0].velocity.y + res[1].mass * res[1].velocity.y;

    report_test("Oblique Collision (b=0.04m, e=0.90)",
                deflected_y && std::abs(total_py) < 1e-4f,
                "v1.y=" + std::to_string(res[0].velocity.y).substr(0, 6) + " v2.y=" + std::to_string(res[1].velocity.y).substr(0, 6));
}

// -----------------------------------------------------------------------------
// Test 7: Circular Boundary Projection & Reflection
// -----------------------------------------------------------------------------
static void test_boundary_reflection(CudaSolverV2& solver) {
    SimConfig cfg;
    cfg.dt = 0.01f;
    cfg.substeps = 10;
    cfg.gravity = Vec2(0.0f, 0.0f);
    cfg.boundary_restitution = 0.80f;
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 1.0f);

    solver.set_config(cfg);
    solver.set_boundary(bnd);

    // Particle moving right toward wall at x=1.0m
    Particle p(1, Vec2(0.80f, 0.0f), Vec2(2.0f, 0.0f), 0.05f);
    p.set_fixed_mass(1.0f);

    solver.set_particles({p});
    for (int i = 0; i < 25; ++i) solver.step();

    const auto& res = solver.get_particles();
    const float dist = res[0].position.length();
    const bool contained = (dist <= bnd.radius - res[0].radius + 1e-4f);
    const bool bounced_back = (res[0].velocity.x < -0.5f);

    report_test("Circular Boundary Reflection (e_b=0.80)",
                contained && bounced_back,
                "dist=" + std::to_string(dist).substr(0, 6) + " m, vx=" + std::to_string(res[0].velocity.x).substr(0, 6));
}

// -----------------------------------------------------------------------------
// Test 8: Zero-Distance Overlap Singularity Handling
// -----------------------------------------------------------------------------
static void test_zero_distance_overlap(CudaSolverV2& solver) {
    SimConfig cfg;
    cfg.dt = 0.01f;
    cfg.substeps = 4;
    cfg.gravity = Vec2(0.0f, 0.0f);
    cfg.restitution = 1.0f;
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 5.0f);

    solver.set_config(cfg);
    solver.set_boundary(bnd);

    // Exactly coincident positions
    Particle p1(1, Vec2(0.0f, 0.0f), Vec2(0.0f, 0.0f), 0.05f);
    Particle p2(2, Vec2(0.0f, 0.0f), Vec2(0.0f, 0.0f), 0.05f);
    p1.set_fixed_mass(1.0f);
    p2.set_fixed_mass(1.0f);

    solver.set_particles({p1, p2});
    solver.step();

    const auto diag = solver.compute_diagnostics();
    const auto& res = solver.get_particles();
    const bool is_finite = res[0].position.is_finite() && res[1].position.is_finite()
                        && res[0].velocity.is_finite() && res[1].velocity.is_finite()
                        && diag.is_finite;

    report_test("Zero-Distance Overlap Fallback (Finite Check)",
                is_finite,
                diag.is_finite ? "100% Finite" : "NaN/Inf Detected");
}

// -----------------------------------------------------------------------------
// Test 9: Dense Granular Settling (N = 48 Particles under Gravity)
// -----------------------------------------------------------------------------
static void test_dense_granular_settling(CudaSolverV2& solver) {
    SimConfig cfg;
    cfg.dt = 1.0f / 60.0f;
    cfg.substeps = 12;
    cfg.gravity = Vec2(0.0f, -9.80665f);
    cfg.restitution = 0.20f;
    cfg.boundary_restitution = 0.30f;
    cfg.solver_iterations = 8;
    cfg.stabilization_factor = 0.60f;
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 1.0f);

    solver.set_config(cfg);
    solver.set_boundary(bnd);

    // Generate 48-particle vertical column
    std::vector<Particle> particles;
    const float r = 0.035f;
    int id = 1;
    for (int row = 0; row < 12; ++row) {
        for (int col = 0; col < 4; ++col) {
            const float x = -0.15f + col * (2.1f * r);
            const float y = -0.20f + row * (2.1f * r);
            Particle p(id++, Vec2(x, y), Vec2(0.0f, 0.0f), r);
            p.set_fixed_mass(1.0f);
            particles.push_back(p);
        }
    }

    solver.set_particles(particles);

    // Simulate 60 frames (1.0 second physical time)
    for (int f = 0; f < 60; ++f) {
        solver.step();
    }

    const auto diag = solver.compute_diagnostics();
    const bool finite_state = diag.is_finite;
    const bool bounded_penetration = diag.max_penetration < 0.002f; // < 2.0 mm
    const bool active_contacts = diag.active_contacts >= 30;

    report_test("Dense Granular Settling (N=48, 60 frames)",
                finite_state && bounded_penetration && active_contacts,
                "Contacts: " + std::to_string(diag.active_contacts)
                + " | Max Pen: " + std::to_string(diag.max_penetration * 1000.0f).substr(0, 5) + " mm"
                + " | Ek: " + std::to_string(diag.total_kinetic_energy).substr(0, 5) + " J");
}

// -----------------------------------------------------------------------------
// Test 10: Multi-Run Determinism (Identical Initial Conditions)
// -----------------------------------------------------------------------------
static void test_cuda_determinism(CudaSolverV2& solver) {
    SimConfig cfg;
    cfg.dt = 1.0f / 60.0f;
    cfg.substeps = 8;
    cfg.gravity = Vec2(0.0f, -9.80665f);
    cfg.restitution = 0.4f;
    cfg.solver_iterations = 4;
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 1.0f);

    solver.set_config(cfg);
    solver.set_boundary(bnd);

    std::vector<Particle> initial;
    for (int i = 0; i < 20; ++i) {
        Particle p(i + 1, Vec2(-0.2f + 0.02f * i, 0.1f + 0.03f * (i % 4)), Vec2(0.0f, 0.0f), 0.03f);
        p.set_fixed_mass(1.0f);
        initial.push_back(p);
    }

    auto run_sim = [&]() {
        solver.set_particles(initial);
        for (int f = 0; f < 40; ++f) solver.step();
        return solver.compute_diagnostics();
    };

    const auto run1 = run_sim();
    const auto run2 = run_sim();
    const auto run3 = run_sim();

    const float dP = (run1.total_momentum - run2.total_momentum).length()
                   + (run2.total_momentum - run3.total_momentum).length();
    const float dEk = std::abs(run1.total_kinetic_energy - run2.total_kinetic_energy)
                    + std::abs(run2.total_kinetic_energy - run3.total_kinetic_energy);

    report_test("CUDA Multi-Run Determinism (3 Runs)",
                dP < 1e-4f && dEk < 1e-4f,
                "dP diff: " + std::to_string(dP) + " | dEk diff: " + std::to_string(dEk));
}

// -----------------------------------------------------------------------------
// Main Test Runner
// -----------------------------------------------------------------------------
int main() {
    std::cout << "=================================================================================\n";
    std::cout << "  PARALLEL PARTICLE DYNAMICS LAB: CUDA V2 GPU SOLVER VERIFICATION (STAGE 3 & 4)\n";
    std::cout << "=================================================================================\n\n";

    if (!CudaSolverV2::is_cuda_available()) {
        std::cerr << "CRITICAL ERROR: No CUDA devices found or driver initialization failed!\n";
        return 1;
    }

    const auto specs = CudaSolverV2::query_device_specs(0);
    std::cout << "Hardware Device: " << specs.device_name
              << " | Compute " << specs.compute_capability_str()
              << " | " << specs.multiprocessor_count << " SMs ("
              << specs.total_memory_mb() << " MB VRAM)\n\n";

    CudaSolverV2 solver;

    test_constant_velocity(solver);
    test_gravitational_ballistics(solver);
    test_equal_elastic_collision(solver);
    test_unequal_elastic_collision(solver);
    test_inelastic_collision(solver);
    test_oblique_collision(solver);
    test_boundary_reflection(solver);
    test_zero_distance_overlap(solver);
    test_dense_granular_settling(solver);
    test_cuda_determinism(solver);

    std::cout << "\n=================================================================================\n";
    std::cout << "  CUDA V2 TEST SUMMARY: " << g_tests_passed << " PASSED, "
              << g_tests_failed << " FAILED (TOTAL " << (g_tests_passed + g_tests_failed) << ")\n";
    std::cout << "=================================================================================\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
