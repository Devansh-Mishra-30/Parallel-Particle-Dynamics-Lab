#include "sim_core/boundary.h"
#include "sim_core/cpu_reference_solver.h"
#include "sim_core/types.h"
#include "sim_omp_v2/omp_solver.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

using namespace sim_core;
using namespace sim_omp;

static std::vector<Particle> create_benchmark_particles(int n, float domain_radius) {
    std::vector<Particle> particles;
    particles.reserve(n);

    // Uniform pseudo-random or concentric ring layout
    const float particle_radius = std::max(0.005f, 0.4f / std::sqrt(static_cast<float>(n)));
    int current_id = 1;
    int ring = 1;
    float current_r = particle_radius * 2.5f;

    while (static_cast<int>(particles.size()) < n && current_r < domain_radius * 0.85f) {
        const float circumference = 2.0f * 3.14159265f * current_r;
        const int count_in_ring = std::max(6, static_cast<int>(circumference / (particle_radius * 2.5f)));

        for (int i = 0; i < count_in_ring && static_cast<int>(particles.size()) < n; ++i) {
            const float angle = 2.0f * 3.14159265f * static_cast<float>(i) / count_in_ring;
            const Vec2 pos(current_r * std::cos(angle), current_r * std::sin(angle));
            const Vec2 vel(-0.5f * std::sin(angle), 0.5f * std::cos(angle));

            Particle p(current_id++, pos, vel, particle_radius);
            p.set_fixed_mass(1.0f);
            particles.push_back(p);
        }
        current_r += particle_radius * 2.5f;
        ring++;
    }

    return particles;
}

struct ModeResult {
    std::string name;
    double median_ms{0.0};
    double speedup_vs_baseline{1.0};
};

template <typename SolverT>
static double time_solver(SolverT& solver, const std::vector<Particle>& initial_particles, int frames, int reps) {
    std::vector<double> trial_times;
    trial_times.reserve(reps);

    for (int rep = 0; rep < reps; ++rep) {
        solver.set_particles(initial_particles);
        for (int w = 0; w < 3; ++w) solver.step(); // warm-up

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int f = 0; f < frames; ++f) {
            solver.step();
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double step_ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / frames;
        trial_times.push_back(step_ms);
    }

    std::sort(trial_times.begin(), trial_times.end());
    return trial_times[reps / 2];
}

int main(int argc, char** argv) {
    int frames = 25;
    int reps = 3;
    std::string csv_out = "output/benchmarks/broadphase_and_omp_breakdown.csv";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (arg == "--reps" && i + 1 < argc) reps = std::atoi(argv[++i]);
        else if (arg == "--out" && i + 1 < argc) csv_out = argv[++i];
    }

    std::filesystem::create_directories(std::filesystem::path(csv_out).parent_path());

    std::cout << "========================================================================================\n";
    std::cout << "  PARALLEL PARTICLE DYNAMICS LAB: ALGORITHMIC VS PARALLEL SPEEDUP BREAKDOWN\n";
    std::cout << "========================================================================================\n";
    std::cout << "Modes Compared:\n";
    std::cout << "  [A] Sequential CPU All-Pairs (Gauss-Seidel reference baseline)\n";
    std::cout << "  [B] Sequential CPU Spatial Grid (1 thread, UniformSpatialGrid)\n";
    std::cout << "  [C] OpenMP All-Pairs (8 threads, All-Pairs Jacobi)\n";
    std::cout << "  [D] OpenMP Spatial Grid (8 threads, UniformSpatialGrid + Jacobi)\n\n";

    SimConfig cfg;
    cfg.dt = 1.0f / 120.0f;
    cfg.substeps = 8;
    cfg.restitution = 0.8f;
    cfg.boundary_restitution = 0.8f;
    cfg.gravity = Vec2(0.0f, -9.80665f);
    cfg.solver_iterations = 4;
    cfg.stabilization_factor = 0.5f;

    CircularBoundary bnd(Vec2(0.0f, 0.0f), 2.0f);

    const std::vector<int> particle_counts = {64, 128, 256, 512, 1024};

    std::ofstream csv(csv_out);
    csv << "particles,mode_A_seq_allpairs_ms,mode_B_seq_grid_ms,mode_C_omp_allpairs_ms,mode_D_omp_grid_ms,"
        << "speedup_B_algo,speedup_C_omp,speedup_D_combined\n";

    std::cout << std::left << std::setw(8) << "N"
              << std::setw(18) << "A: Seq AllPairs"
              << std::setw(18) << "B: Seq Grid"
              << std::setw(18) << "C: OMP AllPairs"
              << std::setw(18) << "D: OMP Grid"
              << std::setw(14) << "B/A (Algo)"
              << std::setw(14) << "C/A (OMP)"
              << std::setw(14) << "D/A (Combined)"
              << "\n";
    std::cout << "----------------------------------------------------------------------------------------------------------------\n";

    for (int n : particle_counts) {
        const auto initial_particles = create_benchmark_particles(n, bnd.radius);

        // [A] Sequential CPU All-Pairs
        CpuReferenceSolver solver_a(cfg, bnd);
        const double time_a = time_solver(solver_a, initial_particles, frames, reps);

        // [B] Sequential Spatial Grid (1 thread)
        OmpSolverV2 solver_b(cfg, bnd);
        solver_b.set_num_threads(1);
        solver_b.set_broadphase_mode(BroadphaseMode::SpatialGrid);
        const double time_b = time_solver(solver_b, initial_particles, frames, reps);

        // [C] OpenMP All-Pairs (8 threads)
        OmpSolverV2 solver_c(cfg, bnd);
        solver_c.set_num_threads(8);
        solver_c.set_broadphase_mode(BroadphaseMode::AllPairs);
        const double time_c = time_solver(solver_c, initial_particles, frames, reps);

        // [D] OpenMP Spatial Grid (8 threads)
        OmpSolverV2 solver_d(cfg, bnd);
        solver_d.set_num_threads(8);
        solver_d.set_broadphase_mode(BroadphaseMode::SpatialGrid);
        const double time_d = time_solver(solver_d, initial_particles, frames, reps);

        const double sp_b = time_a / time_b;
        const double sp_c = time_a / time_c;
        const double sp_d = time_a / time_d;

        std::cout << std::left << std::setw(8) << n
                  << std::setw(18) << (std::to_string(time_a).substr(0, 6) + " ms")
                  << std::setw(18) << (std::to_string(time_b).substr(0, 6) + " ms")
                  << std::setw(18) << (std::to_string(time_c).substr(0, 6) + " ms")
                  << std::setw(18) << (std::to_string(time_d).substr(0, 6) + " ms")
                  << std::setw(14) << (std::to_string(sp_b).substr(0, 5) + "x")
                  << std::setw(14) << (std::to_string(sp_c).substr(0, 5) + "x")
                  << std::setw(14) << (std::to_string(sp_d).substr(0, 5) + "x")
                  << "\n";

        csv << n << "," << time_a << "," << time_b << "," << time_c << "," << time_d << ","
            << sp_b << "," << sp_c << "," << sp_d << "\n";
    }

    csv.close();
    std::cout << "================================================================================================================\n";
    std::cout << "Wrote breakdown data to " << csv_out << "\n";
    return 0;
}
