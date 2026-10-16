#include "sim_core/boundary.h"
#include "sim_core/cpu_reference_solver.h"
#include "sim_core/types.h"
#include "sim_omp_v2/omp_solver.h"
#include "sim_cuda_v2/cuda_solver.h"

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
using namespace sim_cuda;

static std::vector<Particle> create_benchmark_particles(int n, float domain_radius) {
    std::vector<Particle> particles;
    particles.reserve(n);

    const float particle_radius = std::max(0.002f, 0.4f / std::sqrt(static_cast<float>(n)));
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

struct TrialStats {
    double median_ms{0.0};
    double min_ms{0.0};
    double max_ms{0.0};
    double stddev_ms{0.0};
};

template <typename SolverT>
static TrialStats benchmark_solver(SolverT& solver, const std::vector<Particle>& initial, int frames, int reps) {
    std::vector<double> runs;
    runs.reserve(reps);

    for (int r = 0; r < reps; ++r) {
        solver.set_particles(initial);
        for (int w = 0; w < 3; ++w) solver.step(); // warm-up

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int f = 0; f < frames; ++f) {
            solver.step();
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double step_ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / frames;
        runs.push_back(step_ms);
    }

    std::sort(runs.begin(), runs.end());
    TrialStats stats;
    stats.median_ms = runs[reps / 2];
    stats.min_ms = runs.front();
    stats.max_ms = runs.back();

    double sum = 0.0;
    for (double v : runs) sum += v;
    const double mean = sum / reps;
    double sq = 0.0;
    for (double v : runs) sq += (v - mean) * (v - mean);
    stats.stddev_ms = std::sqrt(sq / reps);

    return stats;
}

int main(int argc, char** argv) {
    int frames = 20;
    int reps = 3;
    std::string csv_out = "output/benchmarks/cross_backend_scaling_results.csv";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (arg == "--reps" && i + 1 < argc) reps = std::atoi(argv[++i]);
        else if (arg == "--out" && i + 1 < argc) csv_out = argv[++i];
    }

    std::filesystem::create_directories(std::filesystem::path(csv_out).parent_path());

    std::cout << "========================================================================================\n";
    std::cout << "  PARALLEL PARTICLE DYNAMICS LAB: CROSS-BACKEND FAIR HPC BENCHMARK (STAGE 8)\n";
    std::cout << "========================================================================================\n";
    std::cout << "Hardware: Intel Core i7-13620H (16 Threads) + NVIDIA GeForce RTX 4060 Laptop GPU\n";
    std::cout << "Frames per Trial: " << frames << " | Repetitions: " << reps << " | Output: " << csv_out << "\n\n";

    SimConfig cfg;
    cfg.dt = 1.0f / 120.0f;
    cfg.substeps = 8;
    cfg.restitution = 0.8f;
    cfg.boundary_restitution = 0.8f;
    cfg.gravity = Vec2(0.0f, -9.80665f);
    cfg.solver_iterations = 4;
    cfg.stabilization_factor = 0.5f;

    CircularBoundary bnd(Vec2(0.0f, 0.0f), 2.0f);

    const std::vector<int> particle_counts = {100, 500, 1000, 5000, 10000};

    std::ofstream csv(csv_out);
    csv << "particles,backend,broadphase,median_ms,min_ms,max_ms,stddev_ms,speedup_vs_cpu_grid,throughput_p_per_sec\n";

    std::cout << std::left << std::setw(8) << "N"
              << std::setw(24) << "Backend & Mode"
              << std::setw(18) << "Median (ms)"
              << std::setw(18) << "Min / Max (ms)"
              << std::setw(16) << "Speedup vs CPU"
              << std::setw(18) << "Throughput (p/s)"
              << "\n";
    std::cout << "--------------------------------------------------------------------------------------------------------\n";

    for (int n : particle_counts) {
        const auto initial_particles = create_benchmark_particles(n, bnd.radius);

        // 1. Sequential CPU Spatial Grid (Baseline for algorithmic comparison)
        OmpSolverV2 cpu_grid(cfg, bnd);
        cpu_grid.set_num_threads(1);
        cpu_grid.set_broadphase_mode(BroadphaseMode::SpatialGrid);
        const auto s_cpu = benchmark_solver(cpu_grid, initial_particles, frames, reps);
        const double base_ms = s_cpu.median_ms;
        const double tp_cpu = (n * 1000.0) / base_ms;

        std::cout << std::left << std::setw(8) << n
                  << std::setw(24) << "CPU Spatial Grid (1T)"
                  << std::setw(18) << (std::to_string(s_cpu.median_ms).substr(0, 6) + " ms")
                  << (std::to_string(s_cpu.min_ms).substr(0, 5) + " / " + std::to_string(s_cpu.max_ms).substr(0, 5))
                  << std::setw(7) << ""
                  << std::setw(16) << "1.00x"
                  << std::setw(18) << static_cast<uint64_t>(tp_cpu)
                  << "\n";

        csv << n << ",CPU_Serial,SpatialGrid," << s_cpu.median_ms << "," << s_cpu.min_ms << ","
            << s_cpu.max_ms << "," << s_cpu.stddev_ms << ",1.0," << tp_cpu << "\n";

        // 2. OpenMP Multicore Spatial Grid (8 Threads)
        OmpSolverV2 omp_grid(cfg, bnd);
        omp_grid.set_num_threads(8);
        omp_grid.set_broadphase_mode(BroadphaseMode::SpatialGrid);
        const auto s_omp = benchmark_solver(omp_grid, initial_particles, frames, reps);
        const double sp_omp = base_ms / s_omp.median_ms;
        const double tp_omp = (n * 1000.0) / s_omp.median_ms;

        std::cout << std::left << std::setw(8) << n
                  << std::setw(24) << "OpenMP Grid (8T)"
                  << std::setw(18) << (std::to_string(s_omp.median_ms).substr(0, 6) + " ms")
                  << (std::to_string(s_omp.min_ms).substr(0, 5) + " / " + std::to_string(s_omp.max_ms).substr(0, 5))
                  << std::setw(7) << ""
                  << std::setw(16) << (std::to_string(sp_omp).substr(0, 5) + "x")
                  << std::setw(18) << static_cast<uint64_t>(tp_omp)
                  << "\n";

        csv << n << ",OpenMP_8T,SpatialGrid," << s_omp.median_ms << "," << s_omp.min_ms << ","
            << s_omp.max_ms << "," << s_omp.stddev_ms << "," << sp_omp << "," << tp_omp << "\n";

        // 3. NVIDIA CUDA GPU Spatial Grid (RTX 4060)
        CudaSolverV2 cuda_grid(cfg, bnd);
        cuda_grid.set_broadphase_mode(CudaBroadphaseMode::SpatialGrid);
        const auto s_cuda = benchmark_solver(cuda_grid, initial_particles, frames, reps);
        const double sp_cuda = base_ms / s_cuda.median_ms;
        const double tp_cuda = (n * 1000.0) / s_cuda.median_ms;

        std::cout << std::left << std::setw(8) << n
                  << std::setw(24) << "CUDA GPU Grid (RTX 4060)"
                  << std::setw(18) << (std::to_string(s_cuda.median_ms).substr(0, 6) + " ms")
                  << (std::to_string(s_cuda.min_ms).substr(0, 5) + " / " + std::to_string(s_cuda.max_ms).substr(0, 5))
                  << std::setw(7) << ""
                  << std::setw(16) << (std::to_string(sp_cuda).substr(0, 5) + "x")
                  << std::setw(18) << static_cast<uint64_t>(tp_cuda)
                  << "\n";

        csv << n << ",CUDA_RTX4060,SpatialGrid," << s_cuda.median_ms << "," << s_cuda.min_ms << ","
            << s_cuda.max_ms << "," << s_cuda.stddev_ms << "," << sp_cuda << "," << tp_cuda << "\n";

        std::cout << "--------------------------------------------------------------------------------------------------------\n";
    }

    csv.close();
    std::cout << "\n========================================================================================\n";
    std::cout << "  FAIR HPC BENCHMARK COMPLETE: Results written to " << csv_out << "\n";
    std::cout << "========================================================================================\n";

    return 0;
}
