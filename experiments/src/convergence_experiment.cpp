#include "experiments/convergence_experiment.h"
#include "sim_core/cpu_reference_solver.h"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace lab_experiments {

bool ConvergenceParams::is_valid(std::string& error_message) const {
    if (timesteps.empty()) {
        error_message = "Timesteps vector cannot be empty.";
        return false;
    }
    for (float dt : timesteps) {
        if (dt <= 0.0f) {
            error_message = "All timesteps must be strictly positive.";
            return false;
        }
    }
    if (duration <= 0.0f) {
        error_message = "Duration must be strictly positive.";
        return false;
    }
    return true;
}

TimestepConvergenceExperiment::TimestepConvergenceExperiment(const ConvergenceParams& params)
    : params_(params) {
    init_metadata();
}

void TimestepConvergenceExperiment::init_metadata() {
    metadata_.id = "EXP_03_TIMESTEP_CONVERGENCE";
    metadata_.title = "Empirical Order of Convergence & Timestep Refinement";
    metadata_.description = "Evaluates global numerical error reduction across diminishing timestep sizes. "
                            "Measures empirical order of convergence p = ln(E1/E2) / ln(dt1/dt2) and computation time.";
    metadata_.physical_objective = "Determine whether the numerical integrator achieves theoretical first-order "
                                  "convergence in smooth ballistic motion, and analyze convergence behavior across "
                                  "discontinuous collision events.";
    metadata_.governing_equations = "E(dt) = C * dt^p; p = ln(E_{k-1} / E_k) / ln(dt_{k-1} / dt_k); "
                                   "L2_error = sqrt( (1/N) * sum ||x_n - x_exact||^2 ); Linf_error = max ||x_n - x_exact||.";
    metadata_.assumptions = {
        "Continuous reference trajectory known analytically for ballistic fall",
        "Deterministic initial conditions identical across all refinement levels",
        "Fixed-step integration without adaptive step control"
    };
    metadata_.verification_metrics = {
        "Global L2 and Linf position error norms",
        "Error reduction ratio between successive halving of timestep",
        "Empirical order of convergence (EOC)",
        "Wall-clock computation time scaling with N_steps"
    };
    metadata_.numerical_limitations = {
        "Smooth ballistic trajectories exhibit clean first-order convergence (p approx 1.0)",
        "Collision-dominated scenarios produce non-smooth temporal discontinuities where discrete collision detection "
        "shifts impact times by up to O(dt), breaking uniform monotonic asymptotic convergence."
    };
}

bool TimestepConvergenceExperiment::initialize() {
    std::string err;
    if (!params_.is_valid(err)) {
        std::cerr << "Initialization error in TimestepConvergenceExperiment: " << err << "\n";
        return false;
    }
    results_.clear();
    completed_ = false;
    current_time_ = 0.0;
    return true;
}

void TimestepConvergenceExperiment::step() {
    // Timestep convergence runs a sweep across timesteps in run()
    run();
}

void TimestepConvergenceExperiment::run() {
    if (!initialize()) return;

    for (size_t k = 0; k < params_.timesteps.size(); ++k) {
        const float dt = params_.timesteps[k];
        const int total_steps = static_cast<int>(std::ceil(params_.duration / dt));

        sim_core::SimConfig config;
        config.dt = dt;
        config.substeps = 1; // 1 substep so dt_sub = dt
        config.gravity = sim_core::Vec2(0.0f, -9.80665f);

        sim_core::CircularBoundary boundary(sim_core::Vec2(0.0f, 0.0f), 100.0f);
        sim_core::CpuReferenceSolver solver(config, boundary);

        const sim_core::Vec2 x0(0.0f, 2.0f);
        const sim_core::Vec2 v0(1.0f, 0.0f);
        solver.add_particle(sim_core::Particle(1, x0, v0, 0.02f));

        float sum_sq_err = 0.0f;
        float max_err = 0.0f;

        using clock = std::chrono::steady_clock;
        const auto t_start = clock::now();

        for (int step_idx = 1; step_idx <= total_steps; ++step_idx) {
            solver.step();
            const float t = step_idx * dt;

            const sim_core::Vec2 x_exact = x0 + v0 * t + 0.5f * config.gravity * (t * t);
            const sim_core::Vec2 x_sim = solver.get_particles()[0].position;

            const float err = (x_sim - x_exact).length();
            sum_sq_err += err * err;
            if (err > max_err) max_err = err;
        }

        const auto t_end = clock::now();
        const std::chrono::duration<double, std::milli> comp_time = t_end - t_start;

        ConvergenceLevelResult res;
        res.dt = dt;
        res.total_steps = total_steps;
        res.l2_error = std::sqrt(sum_sq_err / total_steps);
        res.linf_error = max_err;
        res.compute_time_ms = comp_time.count();

        if (k > 0) {
            const float prev_err = results_[k - 1].linf_error;
            const float prev_dt = results_[k - 1].dt;
            res.error_reduction_ratio = (res.linf_error > 0.0f) ? (prev_err / res.linf_error) : 0.0f;
            res.empirical_order = std::log(prev_err / res.linf_error) / std::log(prev_dt / dt);
        } else {
            res.error_reduction_ratio = 1.0f;
            res.empirical_order = 0.0f;
        }

        results_.push_back(res);
        latest_diag_ = solver.compute_diagnostics();
    }

    current_time_ = params_.duration;
    completed_ = true;
}

bool TimestepConvergenceExperiment::export_csv(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out) {
        std::cerr << "Failed to open CSV for writing: " << filepath << "\n";
        return false;
    }

    out << "# Experiment: " << metadata_.id << " (" << metadata_.title << ")\n";
    out << "# Physical Objective: " << metadata_.physical_objective << "\n";
    out << "# Duration: " << params_.duration << " s\n";
    out << "# Columns: dt(s),total_steps,l2_error(m),linf_error(m),error_reduction_ratio,empirical_order,compute_time(ms)\n";
    out << "dt,total_steps,l2_error,linf_error,error_reduction_ratio,empirical_order,compute_time_ms\n";

    out << std::setprecision(6) << std::fixed;
    for (const auto& r : results_) {
        out << r.dt << ","
            << r.total_steps << ","
            << std::scientific << r.l2_error << ","
            << r.linf_error << ","
            << std::fixed << r.error_reduction_ratio << ","
            << r.empirical_order << ","
            << r.compute_time_ms << "\n";
    }

    return true;
}

} // namespace lab_experiments
