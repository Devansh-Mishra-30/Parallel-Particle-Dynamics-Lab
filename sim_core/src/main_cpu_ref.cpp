#include "sim_core/cpu_reference_solver.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>

using namespace sim_core;

int main(int argc, char** argv) {
    int n_particles = 1000;
    int substeps = 8;
    float dt = 1.0f / 60.0f;
    float seconds = 5.0f;
    float restitution = 1.0f;
    float boundary_radius = 1.0f; // 1 meter
    unsigned seed = 42;
    std::string csv_path = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--n" && i + 1 < argc) n_particles = std::stoi(argv[++i]);
        else if (arg == "--substeps" && i + 1 < argc) substeps = std::stoi(argv[++i]);
        else if (arg == "--dt" && i + 1 < argc) dt = std::stof(argv[++i]);
        else if (arg == "--seconds" && i + 1 < argc) seconds = std::stof(argv[++i]);
        else if (arg == "--restitution" && i + 1 < argc) restitution = std::stof(argv[++i]);
        else if (arg == "--boundary" && i + 1 < argc) boundary_radius = std::stof(argv[++i]);
        else if (arg == "--seed" && i + 1 < argc) seed = static_cast<unsigned>(std::stoul(argv[++i]));
        else if (arg == "--csv" && i + 1 < argc) csv_path = argv[++i];
    }

    std::cout << "========================================================\n";
    std::cout << "  SIMULATOR V2: SEQUENTIAL CPU REFERENCE SOLVER\n";
    std::cout << "========================================================\n";
    std::cout << "Particles: " << n_particles << " | Substeps: " << substeps
              << " | dt: " << dt << " s | Restitution: " << restitution << "\n";
    std::cout << "Boundary Radius: " << boundary_radius << " m | Seed: " << seed << "\n\n";

    SimConfig config;
    config.dt = dt;
    config.substeps = substeps;
    config.restitution = restitution;
    config.boundary_restitution = 1.0f;
    config.gravity = Vec2(0.0f, -9.80665f);

    CircularBoundary boundary(Vec2(0.0f, 0.0f), boundary_radius);
    CpuReferenceSolver solver(config, boundary);

    // Deterministic particle generation
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<float> angle_dist(0.0f, 2.0f * 3.14159265f);
    std::uniform_real_distribution<float> unit_dist(0.0f, 1.0f);
    std::uniform_real_distribution<float> vel_dist(-1.0f, 1.0f); // -1 to 1 m/s

    const float p_radius = 0.01f; // 1cm
    const float max_r = boundary_radius - p_radius - 0.02f;

    for (int i = 0; i < n_particles; ++i) {
        const float theta = angle_dist(rng);
        const float r = max_r * std::sqrt(unit_dist(rng));
        const Vec2 pos(r * std::cos(theta), r * std::sin(theta));
        const Vec2 vel(vel_dist(rng), vel_dist(rng));
        solver.add_particle(Particle(static_cast<uint32_t>(i + 1), pos, vel, p_radius));
    }

    const int total_frames = static_cast<int>(seconds / dt);
    std::cout << "Running simulation for " << total_frames << " frames ("
              << total_frames * substeps << " total substeps)...\n";

    using clock = std::chrono::steady_clock;
    const auto t_start = clock::now();

    for (int f = 0; f < total_frames; ++f) {
        solver.step();
    }

    const auto t_end = clock::now();
    const std::chrono::duration<double, std::milli> duration_ms = t_end - t_start;

    const double avg_ms_frame = duration_ms.count() / total_frames;
    const double avg_ms_substep = avg_ms_frame / substeps;
    const double fps = 1000.0 / avg_ms_frame;

    const auto diag = solver.compute_diagnostics();

    std::cout << "\nResults:\n";
    std::cout << "Total Wall Time: " << duration_ms.count() << " ms\n";
    std::cout << "Average ms / frame: " << avg_ms_frame << " ms (" << fps << " FPS)\n";
    std::cout << "Average ms / substep: " << avg_ms_substep << " ms\n";
    std::cout << "Final Total Kinetic Energy: " << diag.total_kinetic_energy << " J\n";
    std::cout << "Final Total Momentum: (" << diag.total_momentum.x << ", " << diag.total_momentum.y << ") kg*m/s\n";
    std::cout << "NaNs detected: " << diag.nan_count << "\n";

    if (!csv_path.empty()) {
        std::ofstream out(csv_path, std::ios::app);
        if (out) {
            out << "cpu_ref," << n_particles << "," << substeps << "," << config.dt_substep()
                << "," << dt << "," << avg_ms_substep << "," << avg_ms_frame << "," << fps
                << "," << diag.nan_count << "\n";
            std::cout << "Appended timing to " << csv_path << "\n";
        }
    }

    return (diag.nan_count == 0) ? 0 : 1;
}
