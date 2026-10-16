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

struct BenchmarkRunData {
    int particle_count{0};
    int threads{0}; // 0 = Sequential CPU reference
    double median_total_ms{0.0};
    double min_total_ms{0.0};
    double max_total_ms{0.0};
    double stddev_ms{0.0};
    double integrate_ms{0.0};
    double resolve_ms{0.0};
    double boundary_ms{0.0};
    double speedup{1.0};
    double efficiency{1.0};
};

static std::vector<Particle> create_benchmark_particles(int n, float domain_radius) {
    std::vector<Particle> particles;
    particles.reserve(n);

    // Seeded deterministic layout: concentric rings
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

static BenchmarkRunData run_benchmark_cpu(int n, const SimConfig& cfg, const CircularBoundary& bnd, int frames, int reps) {
    BenchmarkRunData result;
    result.particle_count = n;
    result.threads = 0; // Sequential reference

    const auto initial_particles = create_benchmark_particles(n, bnd.radius);
    std::vector<double> trial_times;
    trial_times.reserve(reps);

    for (int rep = 0; rep < reps; ++rep) {
        CpuReferenceSolver solver(cfg, bnd);
        solver.set_particles(initial_particles);

        // Warm-up 5 frames
        for (int w = 0; w < 5; ++w) solver.step();

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int f = 0; f < frames; ++f) {
            solver.step();
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / frames;
        trial_times.push_back(total_ms);
    }

    std::sort(trial_times.begin(), trial_times.end());
    result.median_total_ms = trial_times[reps / 2];
    result.min_total_ms = trial_times.front();
    result.max_total_ms = trial_times.back();

    double sum = 0.0;
    for (double v : trial_times) sum += v;
    const double mean = sum / reps;
    double sq_diff = 0.0;
    for (double v : trial_times) sq_diff += (v - mean) * (v - mean);
    result.stddev_ms = std::sqrt(sq_diff / reps);

    result.speedup = 1.0;
    result.efficiency = 1.0;
    return result;
}

static BenchmarkRunData run_benchmark_omp(int n, int threads, const SimConfig& cfg, const CircularBoundary& bnd,
                                         int frames, int reps, double seq_baseline_ms) {
    BenchmarkRunData result;
    result.particle_count = n;
    result.threads = threads;

    const auto initial_particles = create_benchmark_particles(n, bnd.radius);
    std::vector<double> trial_times;
    trial_times.reserve(reps);

    double sum_int = 0.0, sum_res = 0.0, sum_bnd = 0.0;

    for (int rep = 0; rep < reps; ++rep) {
        OmpSolverV2 solver(cfg, bnd);
        solver.set_num_threads(threads);
        solver.set_contact_mode(ContactMode::Jacobi);
        solver.set_particles(initial_particles);

        // Warm-up 5 frames
        for (int w = 0; w < 5; ++w) solver.step();

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int f = 0; f < frames; ++f) {
            solver.step();
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        const double total_ms = std::chrono::duration<double, std::milli>(t1 - t0).count() / frames;
        trial_times.push_back(total_ms);

        const auto m = solver.get_last_metrics();
        sum_int += m.integrate_time_ms;
        sum_res += m.resolve_time_ms;
        sum_bnd += m.boundary_time_ms;
    }

    std::sort(trial_times.begin(), trial_times.end());
    result.median_total_ms = trial_times[reps / 2];
    result.min_total_ms = trial_times.front();
    result.max_total_ms = trial_times.back();

    double sum = 0.0;
    for (double v : trial_times) sum += v;
    const double mean = sum / reps;
    double sq_diff = 0.0;
    for (double v : trial_times) sq_diff += (v - mean) * (v - mean);
    result.stddev_ms = std::sqrt(sq_diff / reps);

    result.integrate_ms = sum_int / (reps * frames);
    result.resolve_ms = sum_res / (reps * frames);
    result.boundary_ms = sum_bnd / (reps * frames);

    result.speedup = (result.median_total_ms > 0.0) ? (seq_baseline_ms / result.median_total_ms) : 1.0;
    result.efficiency = (threads > 0) ? (result.speedup / threads) : 1.0;
    return result;
}

int main(int argc, char** argv) {
    int frames = 30;
    int reps = 3;
    std::string csv_out = "output/benchmarks/openmp_scaling_results.csv";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) frames = std::atoi(argv[++i]);
        else if (arg == "--reps" && i + 1 < argc) reps = std::atoi(argv[++i]);
        else if (arg == "--out" && i + 1 < argc) csv_out = argv[++i];
    }

    std::filesystem::create_directories(std::filesystem::path(csv_out).parent_path());

    std::cout << "========================================================================================\n";
    std::cout << "  PARALLEL PARTICLE DYNAMICS LAB: OPENMP MULTICORE SCALING BENCHMARK (TASK 6)\n";
    std::cout << "========================================================================================\n";
    std::cout << "Hardware: 16 Logical Threads (Intel Core i7-13620H) | Compiler: GCC OpenMP 4.5\n";
    std::cout << "Frames per Trial: " << frames << " | Repetitions: " << reps << " | Output CSV: " << csv_out << "\n\n";

    SimConfig cfg;
    cfg.dt = 1.0f / 120.0f;
    cfg.substeps = 8;
    cfg.restitution = 0.8f;
    cfg.boundary_restitution = 0.8f;
    cfg.gravity = Vec2(0.0f, -9.80665f);
    cfg.solver_iterations = 4;
    cfg.stabilization_factor = 0.5f;

    CircularBoundary bnd(Vec2(0.0f, 0.0f), 2.0f);

    const std::vector<int> particle_counts = {32, 64, 128, 256, 512, 1024};
    const std::vector<int> thread_counts = {1, 2, 4, 8, 16};

    std::vector<BenchmarkRunData> all_results;

    std::ofstream csv(csv_out);
    csv << "particles,threads,median_time_ms,min_time_ms,max_time_ms,stddev_ms,speedup,efficiency\n";

    for (int n : particle_counts) {
        std::cout << "\n----------------------------------------------------------------------------------------\n";
        std::cout << "  PARTICLE COUNT N = " << n << " (" << (n * (n - 1)) / 2 << " pair checks per iteration)\n";
        std::cout << "----------------------------------------------------------------------------------------\n";
        std::cout << std::left << std::setw(14) << "Backend"
                  << std::setw(10) << "Threads"
                  << std::setw(18) << "Median Time (ms)"
                  << std::setw(18) << "Min / Max (ms)"
                  << std::setw(14) << "Speedup"
                  << std::setw(14) << "Efficiency"
                  << "\n";

        // 1. Sequential Reference Baseline
        const auto seq_res = run_benchmark_cpu(n, cfg, bnd, frames, reps);
        all_results.push_back(seq_res);

        std::cout << std::left << std::setw(14) << "CPU Ref"
                  << std::setw(10) << "1 (seq)"
                  << std::setw(18) << std::fixed << std::setprecision(4) << seq_res.median_total_ms
                  << (std::to_string(seq_res.min_total_ms).substr(0, 6) + " / " + std::to_string(seq_res.max_total_ms).substr(0, 6))
                  << std::setw(6) << ""
                  << std::setw(14) << "1.00x"
                  << std::setw(14) << "100.0%"
                  << "\n";

        csv << n << ",0," << seq_res.median_total_ms << "," << seq_res.min_total_ms << ","
            << seq_res.max_total_ms << "," << seq_res.stddev_ms << ",1.0,1.0\n";

        // 2. OpenMP Thread Scaling
        for (int t : thread_counts) {
            const auto omp_res = run_benchmark_omp(n, t, cfg, bnd, frames, reps, seq_res.median_total_ms);
            all_results.push_back(omp_res);

            std::cout << std::left << std::setw(14) << "OpenMP V2"
                      << std::setw(10) << t
                      << std::setw(18) << std::fixed << std::setprecision(4) << omp_res.median_total_ms
                      << (std::to_string(omp_res.min_total_ms).substr(0, 6) + " / " + std::to_string(omp_res.max_total_ms).substr(0, 6))
                      << std::setw(6) << ""
                      << std::setw(14) << (std::to_string(omp_res.speedup).substr(0, 4) + "x")
                      << std::setw(14) << (std::to_string(omp_res.efficiency * 100.0).substr(0, 5) + "%")
                      << "\n";

            csv << n << "," << t << "," << omp_res.median_total_ms << "," << omp_res.min_total_ms << ","
                << omp_res.max_total_ms << "," << omp_res.stddev_ms << "," << omp_res.speedup << ","
                << omp_res.efficiency << "\n";
        }
    }

    csv.close();
    std::cout << "\n========================================================================================\n";
    std::cout << "  BENCHMARK COMPLETE: Wrote raw scaling data to " << csv_out << "\n";
    std::cout << "========================================================================================\n";

    return 0;
}
