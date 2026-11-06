#pragma once
// Stage 4: controlled studies on temporary solvers. Does NOT mutate live simulation.
// Include from controls_panel.cpp after AppState and ExperimentPresets are visible.
#include "lab_gui/app_state.h"
#include "lab_gui/workbench_evidence.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace lab_gui {
namespace evidence_detail {
using Clock = std::chrono::steady_clock;

inline void copy_provenance(WorkbenchEvidence& e, const AppState& state) {
    e.dt_s = state.config.dt;
    e.initial_substeps = state.config.substeps;
    e.initial_iterations = state.config.solver_iterations;
    e.restitution = state.config.restitution;
    e.boundary_restitution = state.config.boundary_restitution;
    e.gravity_x = state.config.gravity.x;
    e.gravity_y = state.config.gravity.y;
    e.boundary_radius_m = state.boundary.radius;
    e.omp_threads = state.omp_threads;
}

inline std::vector<sim_core::Particle> make_cloud(int n, float boundary_radius) {
    constexpr float pi = 3.14159265358979323846f;
    constexpr float golden_angle = 2.39996322972865332f;
    const float spawn_radius = std::max(0.1f, boundary_radius - 0.10f);
    const float radius = std::min(0.035f,
        0.35f * std::sqrt(pi * spawn_radius * spawn_radius / static_cast<float>(n)));
    std::vector<sim_core::Particle> points;
    points.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const float theta = static_cast<float>(i) * golden_angle;
        const float rho = spawn_radius * std::sqrt((static_cast<float>(i) + 0.5f) / static_cast<float>(n));
        const sim_core::Vec2 p{rho * std::cos(theta), rho * std::sin(theta)};
        const sim_core::Vec2 v{0.45f * std::cos(theta + 1.0f),
                               0.45f * std::sin(theta + 1.0f)};
        points.emplace_back(static_cast<uint32_t>(i + 1), p, v, radius);
    }
    return points;
}

inline float max_position_difference(const std::vector<sim_core::Particle>& a,
                                     const std::vector<sim_core::Particle>& b) {
    if (a.size() != b.size()) return std::numeric_limits<float>::quiet_NaN();
    float m = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id) return std::numeric_limits<float>::quiet_NaN();
        m = std::max(m, (a[i].position - b[i].position).length());
    }
    return m;
}

inline float max_velocity_difference(const std::vector<sim_core::Particle>& a,
                                     const std::vector<sim_core::Particle>& b) {
    if (a.size() != b.size()) return std::numeric_limits<float>::quiet_NaN();
    float m = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].id != b[i].id) return std::numeric_limits<float>::quiet_NaN();
        m = std::max(m, (a[i].velocity - b[i].velocity).length());
    }
    return m;
}

inline bool particles_are_finite(const std::vector<sim_core::Particle>& particles) {
    return std::all_of(particles.begin(), particles.end(), [](const sim_core::Particle& p) {
        return std::isfinite(p.position.x) && std::isfinite(p.position.y) &&
               std::isfinite(p.velocity.x) && std::isfinite(p.velocity.y);
    });
}

template <class Solver>
inline double timed_steps(Solver& solver, int steps,
                          std::vector<sim_core::Particle>* final_particles = nullptr) {
    const auto start = Clock::now();
    for (int k = 0; k < steps; ++k) solver.step();
    // Materialize the final state before stopping timing. On CUDA this can
    // synchronize pending device work and includes host-transfer overhead.
    if (final_particles) *final_particles = solver.get_particles();
    const auto stop = Clock::now();
    return std::chrono::duration<double, std::milli>(stop - start).count() / steps;
}
} // namespace evidence_detail

inline WorkbenchEvidence run_contact_sensitivity_study(const AppState& state) {
    WorkbenchEvidence e;
    e.kind = EvidenceStudy::ContactSensitivity;
    e.scenario = "dense_contacts";
    e.protocol = "4 configs from same initial snapshot, fixed T=3 s; CPU GS; per-frame peak and endpoint overlap; timing includes diagnostics sampling";
    e.interpretation = "Compare measured penetration at identical physical time; parameter response does not by itself prove asymptotic convergence or physical validation.";
    evidence_detail::copy_provenance(e, state);
    e.repeats = 1; // deterministic trajectory study; timings are exploratory, not benchmark estimates
    const auto& initial = state.workbench_initial_particles;
    if (initial.empty() || !(state.config.dt > 0.0f)) return e;
    const int frames = std::max(1, static_cast<int>(std::lround(3.0 / state.config.dt)));
    for (int substeps : {4, 12}) {
        for (int iters : {2, 8}) {
            sim_core::SimConfig cfg = state.config;
            cfg.substeps = substeps;
            cfg.solver_iterations = iters;
            sim_core::CpuReferenceSolver cpu(cfg, state.boundary);
            cpu.set_particles(initial);
            float peak = 0.0f;
            bool finite = true;
            const auto t0 = evidence_detail::Clock::now();
            for (int frame = 0; frame < frames; ++frame) {
                cpu.step();
                const auto d = cpu.compute_diagnostics();
                peak = std::max(peak, d.max_penetration);
                finite = finite && d.is_finite;
            }
            const auto t1 = evidence_detail::Clock::now();
            const auto last = cpu.compute_diagnostics();
            EvidenceRow r;
            r.particles = static_cast<int>(initial.size());
            r.requested_particles = 48;
            r.generated_particles = static_cast<int>(initial.size());
            r.physics_steps = frames;
            r.benchmark_samples = 1;
            r.simulated_seconds = frames * static_cast<double>(cfg.dt);
            r.substeps = substeps;
            r.contact_iterations = iters;
            r.cpu_total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            r.cpu_ms_per_step = r.cpu_total_ms / frames;
            r.maximum_penetration_m = last.max_penetration;
            r.peak_penetration_m = peak;
            r.kinetic_energy_j = last.total_kinetic_energy;
            r.active_contacts = last.active_contacts;
            r.finite = finite && last.is_finite &&
                       evidence_detail::particles_are_finite(cpu.get_particles());
            r.omp_ms_per_step = std::numeric_limits<double>::quiet_NaN();
            r.cuda_ms_per_step = std::numeric_limits<double>::quiet_NaN();
            e.rows.push_back(r);
        }
    }
    e.valid = true;
    return e;
}

inline WorkbenchEvidence run_backend_scaling_study(const AppState& state) {
    WorkbenchEvidence e;
    e.kind = EvidenceStudy::BackendScaling;
    e.scenario = "particle_cloud";
    e.protocol = "fresh deterministic golden-angle disk, N=128/256/512/1024; 3 repeated warm CPU/OMP/CUDA solver steps, median ms/step; N>256 uses 10 vs 25 steps";
    e.interpretation = "Mixed algorithm/hardware effects (CPU all-pairs GS; parallel spatial-grid Jacobi). Not an isolated GPU speedup test.";
    evidence_detail::copy_provenance(e, state);
    e.repeats = 3;
    for (int n : {128, 256, 512, 1024}) {
        const int steps = n > 256 ? 10 : 25;
        const auto cloud = evidence_detail::make_cloud(n, state.boundary.radius);
        const sim_core::SimConfig cfg = state.config;
        EvidenceRow r;
        r.particles = n;
        r.requested_particles = n;
        r.generated_particles = static_cast<int>(cloud.size());
        r.physics_steps = steps;
        r.benchmark_samples = e.repeats;
        r.simulated_seconds = static_cast<double>(steps) * cfg.dt;
        r.substeps = cfg.substeps;
        r.contact_iterations = cfg.solver_iterations;
        std::vector<double> cpu_samples, omp_samples, cuda_samples;
        std::vector<sim_core::Particle> cpu_final, omp_final, cuda_final;
        bool finite = true;
        for (int repeat = 0; repeat < e.repeats; ++repeat) {
            sim_core::CpuReferenceSolver cpu(cfg, state.boundary);
            cpu.set_particles(cloud);
            // This warmup runs a separate solver to avoid changing the measured initial state.
            if (repeat == 0) { sim_core::CpuReferenceSolver warm(cfg, state.boundary);
                               warm.set_particles(cloud); warm.step(); }
            cpu_samples.push_back(evidence_detail::timed_steps(cpu, steps, &cpu_final));
            finite = finite && cpu.compute_diagnostics().is_finite &&
                     evidence_detail::particles_are_finite(cpu_final);
            sim_omp::OmpSolverV2 omp(cfg, state.boundary);
            omp.set_num_threads(state.omp_threads);
            omp.set_broadphase_mode(sim_omp::BroadphaseMode::SpatialGrid);
            omp.set_particles(cloud);
            if (repeat == 0) { sim_omp::OmpSolverV2 warm(cfg, state.boundary);
                               warm.set_num_threads(state.omp_threads);
                               warm.set_broadphase_mode(sim_omp::BroadphaseMode::SpatialGrid);
                               warm.set_particles(cloud); warm.step(); }
            omp_samples.push_back(evidence_detail::timed_steps(omp, steps, &omp_final));
            finite = finite && omp.compute_diagnostics().is_finite &&
                     evidence_detail::particles_are_finite(omp_final);
#if HAS_CUDA_BACKEND
            if (sim_cuda::CudaSolverV2::is_cuda_available()) {
                sim_cuda::CudaSolverV2 gpu(cfg, state.boundary);
                gpu.set_broadphase_mode(sim_cuda::CudaBroadphaseMode::SpatialGrid);
                gpu.set_particles(cloud);
                if (repeat == 0) { sim_cuda::CudaSolverV2 warm(cfg, state.boundary);
                                   warm.set_broadphase_mode(sim_cuda::CudaBroadphaseMode::SpatialGrid);
                                   warm.set_particles(cloud); warm.step();
                                   (void)warm.get_particles(); }
                cuda_samples.push_back(evidence_detail::timed_steps(gpu, steps, &cuda_final));
                finite = finite && gpu.compute_diagnostics().is_finite &&
                         evidence_detail::particles_are_finite(cuda_final);
            }
#endif
        }
        r.cpu_ms_per_step = median_measurement(cpu_samples);
        r.omp_ms_per_step = median_measurement(omp_samples);
        r.cuda_ms_per_step = median_measurement(cuda_samples);
        r.cpu_total_ms = r.cpu_ms_per_step * steps;
        r.omp_speedup = safe_speedup(r.cpu_ms_per_step, r.omp_ms_per_step);
        r.cuda_speedup = safe_speedup(r.cpu_ms_per_step, r.cuda_ms_per_step);
        r.cuda_available = !cuda_samples.empty();
        r.omp_max_position_difference_m = evidence_detail::max_position_difference(cpu_final, omp_final);
        r.cuda_max_position_difference_m = cuda_samples.empty() ?
            std::numeric_limits<float>::quiet_NaN() :
            evidence_detail::max_position_difference(cpu_final, cuda_final);
        r.omp_max_velocity_difference_m_s =
            evidence_detail::max_velocity_difference(cpu_final, omp_final);
        r.cuda_max_velocity_difference_m_s = cuda_samples.empty() ?
            std::numeric_limits<float>::quiet_NaN() :
            evidence_detail::max_velocity_difference(cpu_final, cuda_final);
        r.finite = finite;
        e.rows.push_back(r);
    }
    e.valid = true;
    return e;
}

inline std::vector<ConvergenceDataPoint> run_ballistic_convergence_study(
    sim_core::IntegratorType integrator) {
    const std::array<float, 4> dts{{0.04f, 0.02f, 0.01f, 0.005f}};
    constexpr float end_time = 0.40f;
    const sim_core::Vec2 x0{-0.6f, -0.4f};
    const sim_core::Vec2 v0{1.5f, 2.5f};
    const sim_core::Vec2 gravity{0.0f, -9.80665f};
    const sim_core::Vec2 exact =
        x0 + v0 * end_time + 0.5f * gravity * (end_time * end_time);

    std::vector<ConvergenceDataPoint> results;
    results.reserve(dts.size());
    float previous_error = 0.0f;
    float previous_dt = 0.0f;
    for (float dt : dts) {
        sim_core::SimConfig cfg;
        cfg.integrator = integrator;
        cfg.dt = dt;
        cfg.substeps = 1;
        cfg.gravity = gravity;
        sim_core::CpuReferenceSolver solver(
            cfg, sim_core::CircularBoundary({0.0f, 0.0f}, 100.0f));
        solver.add_particle(sim_core::Particle(1, x0, v0, 0.04f));

        const int steps = static_cast<int>(std::lround(end_time / dt));
        const auto start = evidence_detail::Clock::now();
        for (int step = 0; step < steps; ++step) solver.step();
        const auto stop = evidence_detail::Clock::now();

        ConvergenceDataPoint point;
        point.dt = dt;
        point.substeps = 1;
        point.error_l2 = (solver.get_particles().front().position - exact).length();
        point.runtime_us =
            std::chrono::duration<double, std::micro>(stop - start).count();
        if (previous_error > 0.0f) {
            point.reduction_ratio = previous_error / point.error_l2;
            point.empirical_order = std::log(point.reduction_ratio) /
                std::log(previous_dt / dt);
        }
        previous_error = point.error_l2;
        previous_dt = dt;
        results.push_back(point);
    }
    return results;
}
} // namespace lab_gui
