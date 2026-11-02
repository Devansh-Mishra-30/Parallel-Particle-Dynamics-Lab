#include "experiments/gravity_experiment.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace lab_experiments {

bool GravityParams::is_valid(std::string& error_message) const {
    if (particle_radius <= 0.0f) {
        error_message = "Particle radius must be strictly positive.";
        return false;
    }
    if (dt <= 0.0f) {
        error_message = "Timestep dt must be strictly positive.";
        return false;
    }
    if (substeps <= 0) {
        error_message = "Substeps must be at least 1.";
        return false;
    }
    if (duration <= 0.0f) {
        error_message = "Duration must be strictly positive.";
        return false;
    }
    return true;
}

GravityFreeMotionExperiment::GravityFreeMotionExperiment(const GravityParams& params)
    : params_(params) {
    init_metadata();
}

void GravityFreeMotionExperiment::init_metadata() {
    metadata_.id = "EXP_02_GRAVITY_AND_FREE_MOTION";
    metadata_.title = "Ballistic Trajectories & Gravitational Free Fall";
    metadata_.description = "Analyzes single-particle ballistic kinematics under uniform gravity and zero-gravity "
                            "regimes. Compares numerical trajectories against continuous analytical equations and "
                            "tracks total mechanical energy conservation.";
    metadata_.physical_objective = "Verify that particle positions follow parabolic trajectory x(t) = x0 + v0*t + 0.5*g*t^2 "
                                  "and evaluate Semi-implicit Euler numerical energy drift.";
    metadata_.governing_equations = "m * d^2x/dt^2 = m * g; v(t) = v0 + g*t; x(t) = x0 + v0*t + 0.5*g*t^2; "
                                   "E_total = 0.5*m*v^2 - m*(g . x) = const.";
    metadata_.assumptions = {
        "Uniform, non-varying gravitational field",
        "Zero aerodynamic drag / vacuum ballistic regime",
        "Point-mass rigid body kinematics"
    };
    metadata_.verification_metrics = {
        "Global position error: ||x_sim(t) - x_exact(t)||",
        "Velocity error: ||v_sim(t) - v_exact(t)||",
        "Mechanical energy drift ratio: |E(t) - E(0)| / E(0)",
        "Comparison against exact discrete Semi-implicit Euler solution"
    };
    metadata_.numerical_limitations = {
        "Semi-implicit Euler possesses a small O(dt_sub) phase displacement in position: 0.5 * g * t * dt_sub",
        "Symplectic integrator bounds energy oscillations without secular growth, but energy is not strictly constant"
    };
}

bool GravityFreeMotionExperiment::initialize() {
    std::string err;
    if (!params_.is_valid(err)) {
        std::cerr << "Initialization error in GravityFreeMotionExperiment: " << err << "\n";
        return false;
    }

    sim_core::SimConfig config;
    config.dt = params_.dt;
    config.substeps = params_.substeps;
    config.gravity = params_.gravity;

    sim_core::CircularBoundary boundary(sim_core::Vec2(0.0f, 0.0f), 100.0f); // unbounded
    solver_.set_config(config);
    solver_.set_boundary(boundary);
    solver_.clear_particles();

    sim_core::Particle p(1, params_.x0, params_.v0, params_.particle_radius);
    solver_.add_particle(p);

    data_.clear();
    current_time_ = 0.0;
    current_frame_ = 0;
    total_frames_ = static_cast<int>(std::ceil(params_.duration / params_.dt));

    latest_diag_ = solver_.compute_diagnostics();
    initial_total_energy_ = latest_diag_.total_mechanical_energy;

    summary_.max_pos_error = 0.0f;
    summary_.max_vel_error = 0.0f;
    summary_.final_pos_error = 0.0f;
    summary_.final_vel_error = 0.0f;
    summary_.energy_drift_ratio = 0.0f;

    record_frame();
    return true;
}

void GravityFreeMotionExperiment::record_frame() {
    const auto& p = solver_.get_particles()[0];
    latest_diag_ = solver_.compute_diagnostics();

    const float t = static_cast<float>(current_time_);
    const sim_core::Vec2 x_exact = params_.x0 + params_.v0 * t + 0.5f * params_.gravity * (t * t);
    const sim_core::Vec2 v_exact = params_.v0 + params_.gravity * t;

    const float pos_err = (p.position - x_exact).length();
    const float vel_err = (p.velocity - v_exact).length();

    GravityDataRow row;
    row.time = current_time_;
    row.x = p.position.x;
    row.y = p.position.y;
    row.vx = p.velocity.x;
    row.vy = p.velocity.y;
    row.x_exact = x_exact.x;
    row.y_exact = x_exact.y;
    row.vx_exact = v_exact.x;
    row.vy_exact = v_exact.y;
    row.pos_error = pos_err;
    row.vel_error = vel_err;
    row.kinetic_energy = latest_diag_.total_kinetic_energy;
    row.potential_energy = latest_diag_.total_potential_energy;
    row.total_energy = latest_diag_.total_mechanical_energy;

    data_.push_back(row);

    if (pos_err > summary_.max_pos_error) summary_.max_pos_error = pos_err;
    if (vel_err > summary_.max_vel_error) summary_.max_vel_error = vel_err;

    if (std::abs(initial_total_energy_) > 1e-6f) {
        const float drift = std::abs(latest_diag_.total_mechanical_energy - initial_total_energy_) / std::abs(initial_total_energy_);
        if (drift > summary_.energy_drift_ratio) summary_.energy_drift_ratio = drift;
    }
}

void GravityFreeMotionExperiment::step() {
    if (is_completed()) return;

    solver_.step();
    current_frame_++;
    current_time_ = current_frame_ * params_.dt;
    record_frame();

    if (is_completed()) {
        const auto& last = data_.back();
        summary_.final_pos_error = last.pos_error;
        summary_.final_vel_error = last.vel_error;
    }
}

bool GravityFreeMotionExperiment::is_completed() const {
    return current_frame_ >= total_frames_;
}

void GravityFreeMotionExperiment::run() {
    if (!initialize()) return;
    while (!is_completed()) {
        step();
    }
}

bool GravityFreeMotionExperiment::export_csv(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out) {
        std::cerr << "Failed to open CSV for writing: " << filepath << "\n";
        return false;
    }

    out << "# Experiment: " << metadata_.id << " (" << metadata_.title << ")\n";
    out << "# Physical Objective: " << metadata_.physical_objective << "\n";
    out << "# Parameters: g=(" << params_.gravity.x << "," << params_.gravity.y << ") m/s^2, x0=("
        << params_.x0.x << "," << params_.x0.y << ") m, v0=(" << params_.v0.x << "," << params_.v0.y << ") m/s, dt="
        << params_.dt << " s, substeps=" << params_.substeps << "\n";
    out << "# Summary: max_pos_error=" << summary_.max_pos_error << " m, max_vel_error=" << summary_.max_vel_error
        << " m/s, energy_drift_ratio=" << summary_.energy_drift_ratio << "\n";
    out << "# Columns: time(s),x(m),y(m),vx(m/s),vy(m/s),x_exact(m),y_exact(m),vx_exact(m/s),vy_exact(m/s),pos_error(m),vel_error(m/s),Ek(J),Ep(J),E_total(J)\n";
    out << "time,x,y,vx,vy,x_exact,y_exact,vx_exact,vy_exact,pos_error,vel_error,Ek,Ep,E_total\n";

    out << std::setprecision(6) << std::fixed;
    for (const auto& r : data_) {
        out << r.time << ","
            << r.x << "," << r.y << "," << r.vx << "," << r.vy << ","
            << r.x_exact << "," << r.y_exact << "," << r.vx_exact << "," << r.vy_exact << ","
            << r.pos_error << "," << r.vel_error << ","
            << r.kinetic_energy << "," << r.potential_energy << "," << r.total_energy << "\n";
    }

    return true;
}

} // namespace lab_experiments
