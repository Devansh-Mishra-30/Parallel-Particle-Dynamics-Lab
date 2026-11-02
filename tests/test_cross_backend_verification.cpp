#include "sim_core/boundary.h"
#include "sim_core/cpu_reference_solver.h"
#include "sim_core/types.h"
#include "sim_omp_v2/omp_solver.h"
#include "sim_cuda_v2/cuda_solver.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace sim_core;
using namespace sim_omp;
using namespace sim_cuda;

static int g_tests_passed = 0;
static int g_tests_failed = 0;

static void report_test(const std::string& name, bool pass, const std::string& details = "") {
    std::cout << "  " << (pass ? "[PASS] " : "[FAIL] ")
              << std::left << std::setw(52) << name;
    if (!details.empty()) {
        std::cout << " (" << details << ")";
    }
    std::cout << "\n";
    if (pass) g_tests_passed++;
    else g_tests_failed++;
}

// -----------------------------------------------------------------------------
// Test 1: Cross-Backend Constant Velocity & Free Fall
// -----------------------------------------------------------------------------
static void test_cross_kinematics() {
    SimConfig cfg;
    cfg.dt = 0.01f;
    cfg.substeps = 10;
    cfg.gravity = Vec2(0.0f, -9.80665f);
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 50.0f);

    Particle p(1, Vec2(-2.0f, 1.0f), Vec2(3.0f, 4.0f), 0.05f);
    p.set_fixed_mass(1.0f);

    CpuReferenceSolver cpu(cfg, bnd);
    OmpSolverV2 omp(cfg, bnd);
    omp.set_num_threads(8);
    CudaSolverV2 cuda(cfg, bnd);

    cpu.set_particles({p});
    omp.set_particles({p});
    cuda.set_particles({p});

    for (int f = 0; f < 50; ++f) {
        cpu.step();
        omp.step();
        cuda.step();
    }

    const auto p_cpu = cpu.get_particles()[0];
    const auto p_omp = omp.get_particles()[0];
    const auto p_cuda = cuda.get_particles()[0];

    const float err_omp_pos = (p_cpu.position - p_omp.position).length();
    const float err_cuda_pos = (p_cpu.position - p_cuda.position).length();

    report_test("Cross-Backend Ballistics Trajectory",
                err_omp_pos < 1e-4f && err_cuda_pos < 1e-4f,
                "diff(CPU,OMP)=" + std::to_string(err_omp_pos) + " m, diff(CPU,CUDA)=" + std::to_string(err_cuda_pos) + " m");
}

// -----------------------------------------------------------------------------
// Test 2: Cross-Backend Binary Collision (e = 1.0 Elastic)
// -----------------------------------------------------------------------------
static void test_cross_elastic_collision() {
    SimConfig cfg;
    cfg.dt = 0.005f;
    cfg.substeps = 8;
    cfg.gravity = Vec2(0.0f, 0.0f);
    cfg.restitution = 1.0f;
    cfg.solver_iterations = 4;
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 5.0f);

    Particle p1(1, Vec2(-0.20f, 0.0f), Vec2(1.0f, 0.0f), 0.05f);
    Particle p2(2, Vec2(0.20f, 0.0f), Vec2(-1.0f, 0.0f), 0.05f);
    p1.set_fixed_mass(1.0f);
    p2.set_fixed_mass(1.0f);

    CpuReferenceSolver cpu(cfg, bnd);
    OmpSolverV2 omp(cfg, bnd);
    omp.set_num_threads(8);
    CudaSolverV2 cuda(cfg, bnd);

    cpu.set_particles({p1, p2});
    omp.set_particles({p1, p2});
    cuda.set_particles({p1, p2});

    for (int f = 0; f < 50; ++f) {
        cpu.step();
        omp.step();
        cuda.step();
    }

    const auto res_cpu = cpu.get_particles();
    const auto res_omp = omp.get_particles();
    const auto res_cuda = cuda.get_particles();

    const float v1_cpu = res_cpu[0].velocity.x;
    const float v1_omp = res_omp[0].velocity.x;
    const float v1_cuda = res_cuda[0].velocity.x;

    const bool pass = (std::abs(v1_cpu - (-1.0f)) < 1e-3f) &&
                      (std::abs(v1_omp - (-1.0f)) < 1e-3f) &&
                      (std::abs(v1_cuda - (-1.0f)) < 1e-3f);

    report_test("Cross-Backend Elastic Collision (v1' target = -1.0)",
                pass,
                "CPU=" + std::to_string(v1_cpu).substr(0, 6) + " OMP=" + std::to_string(v1_omp).substr(0, 6) + " CUDA=" + std::to_string(v1_cuda).substr(0, 6));
}

// -----------------------------------------------------------------------------
// Test 3: Cross-Backend Inelastic Collision (e = 0.528, dEk error)
// -----------------------------------------------------------------------------
static void test_cross_inelastic_collision() {
    SimConfig cfg;
    cfg.dt = 0.005f;
    cfg.substeps = 8;
    cfg.gravity = Vec2(0.0f, 0.0f);
    cfg.restitution = 0.528f;
    cfg.solver_iterations = 4;
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 5.0f);

    Particle p1(1, Vec2(-0.20f, 0.0f), Vec2(1.0f, 0.0f), 0.05f);
    Particle p2(2, Vec2(0.20f, 0.0f), Vec2(-1.0f, 0.0f), 0.05f);
    p1.set_fixed_mass(1.0f);
    p2.set_fixed_mass(1.0f);

    const float initial_ek = 1.0f; // 0.5*1*1 + 0.5*1*1
    const float exact_dek = -0.5f * 0.5f * (1.0f - cfg.restitution * cfg.restitution) * 4.0f; // -0.721216 J

    CpuReferenceSolver cpu(cfg, bnd);
    OmpSolverV2 omp(cfg, bnd);
    omp.set_num_threads(8);
    CudaSolverV2 cuda(cfg, bnd);

    cpu.set_particles({p1, p2});
    omp.set_particles({p1, p2});
    cuda.set_particles({p1, p2});

    for (int f = 0; f < 50; ++f) {
        cpu.step();
        omp.step();
        cuda.step();
    }

    auto calc_dek = [initial_ek](const std::vector<Particle>& pts) {
        return (0.5f * pts[0].mass * pts[0].velocity.length_squared() +
                0.5f * pts[1].mass * pts[1].velocity.length_squared()) - initial_ek;
    };

    const float dek_cpu = calc_dek(cpu.get_particles());
    const float dek_omp = calc_dek(omp.get_particles());
    const float dek_cuda = calc_dek(cuda.get_particles());

    const bool pass = (std::abs(dek_cpu - exact_dek) < 1e-3f) &&
                      (std::abs(dek_omp - exact_dek) < 1e-3f) &&
                      (std::abs(dek_cuda - exact_dek) < 1e-3f);

    report_test("Cross-Backend Inelastic Dissipation dEk",
                pass,
                "exact=" + std::to_string(exact_dek).substr(0, 6) + " CPU=" + std::to_string(dek_cpu).substr(0, 6)
                + " OMP=" + std::to_string(dek_omp).substr(0, 6) + " CUDA=" + std::to_string(dek_cuda).substr(0, 6));
}

// -----------------------------------------------------------------------------
// Test 4: Cross-Backend Dense Granular Column Settling (N = 48)
// -----------------------------------------------------------------------------
static void test_cross_granular_settling() {
    SimConfig cfg;
    cfg.dt = 1.0f / 60.0f;
    cfg.substeps = 12;
    cfg.gravity = Vec2(0.0f, -9.80665f);
    cfg.restitution = 0.20f;
    cfg.boundary_restitution = 0.30f;
    cfg.solver_iterations = 8;
    cfg.stabilization_factor = 0.60f;
    CircularBoundary bnd(Vec2(0.0f, 0.0f), 1.0f);

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

    CpuReferenceSolver cpu(cfg, bnd);
    OmpSolverV2 omp(cfg, bnd);
    omp.set_num_threads(8);
    CudaSolverV2 cuda(cfg, bnd);

    cpu.set_particles(particles);
    omp.set_particles(particles);
    cuda.set_particles(particles);

    for (int f = 0; f < 60; ++f) {
        cpu.step();
        omp.step();
        cuda.step();
    }

    const auto d_cpu = cpu.compute_diagnostics();
    const auto d_omp = omp.compute_diagnostics();
    const auto d_cuda = cuda.compute_diagnostics();

    const bool all_finite = d_cpu.is_finite && d_omp.is_finite && d_cuda.is_finite;
    const bool bounded_pen = (d_cpu.max_penetration < 0.002f) &&
                             (d_omp.max_penetration < 0.002f) &&
                             (d_cuda.max_penetration < 0.002f);
    const bool active_contacts = (d_cpu.active_contacts >= 30) &&
                                 (d_omp.active_contacts >= 30) &&
                                 (d_cuda.active_contacts >= 30);

    report_test("Cross-Backend Dense Granular Settling (N=48)",
                all_finite && bounded_pen && active_contacts,
                "Contacts: CPU=" + std::to_string(d_cpu.active_contacts)
                + " OMP=" + std::to_string(d_omp.active_contacts)
                + " CUDA=" + std::to_string(d_cuda.active_contacts));
}

int main() {
    std::cout << "=================================================================================\n";
    std::cout << "  PARALLEL PARTICLE DYNAMICS LAB: CROSS-BACKEND SCIENTIFIC HARNESS (STAGE 7)\n";
    std::cout << "=================================================================================\n\n";

    test_cross_kinematics();
    test_cross_elastic_collision();
    test_cross_inelastic_collision();
    test_cross_granular_settling();

    std::cout << "\n=================================================================================\n";
    std::cout << "  CROSS-BACKEND SUMMARY: " << g_tests_passed << " PASSED, "
              << g_tests_failed << " FAILED (TOTAL " << (g_tests_passed + g_tests_failed) << ")\n";
    std::cout << "=================================================================================\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
