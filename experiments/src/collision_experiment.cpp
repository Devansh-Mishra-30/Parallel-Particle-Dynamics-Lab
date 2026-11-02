#include "experiments/collision_experiment.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace lab_experiments {

bool CollisionParams::is_valid(std::string& error_message) const {
    if (r1 <= 0.0f || r2 <= 0.0f) {
        error_message = "Particle radii must be strictly positive (r > 0).";
        return false;
    }
    if (rho <= 0.0f) {
        error_message = "Areal mass density must be strictly positive (rho > 0).";
        return false;
    }
    if (m1_override < 0.0f || m2_override < 0.0f) {
        error_message = "Mass overrides cannot be negative.";
        return false;
    }
    if (restitution < 0.0f || restitution > 1.0f) {
        error_message = "Restitution coefficient e must be within [0.0, 1.0].";
        return false;
    }
    if (dt <= 0.0f) {
        error_message = "Timestep dt must be strictly positive.";
        return false;
    }
    if (substeps <= 0) {
        error_message = "Substeps count must be at least 1.";
        return false;
    }
    if (duration <= 0.0f) {
        error_message = "Experiment duration must be strictly positive.";
        return false;
    }
    return true;
}

TwoParticleCollisionExperiment::TwoParticleCollisionExperiment(const CollisionParams& params)
    : params_(params) {
    init_metadata();
}

void TwoParticleCollisionExperiment::init_metadata() {
    metadata_.id = "EXP_01_TWO_PARTICLE_COLLISION";
    metadata_.title = "Two-Particle Central Collision Mechanics";
    metadata_.description = "Investigates binary particle collisions under varying mass ratios and coefficients "
                            "of restitution. Verifies linear momentum conservation and kinetic energy dissipation.";
    metadata_.physical_objective = "Quantify post-collision velocities and energy changes against 1D analytical "
                                  "impact mechanics: v1' = (m1 - e*m2)/(m1+m2)*v1 + (1+e)*m2/(m1+m2)*v2.";
    metadata_.governing_equations = "P = m1*v1 + m2*v2 = const; Delta Ek = -0.5 * m_eff * (1 - e^2) * (v1 - v2)^2; "
                                   "Jn = -(1 + e) * m_eff * (v_rel . n).";
    metadata_.assumptions = {
        "Frictionless contact (zero tangential friction)",
        "Zero rotational degrees of freedom (non-spinning disks)",
        "Zero external forces (isolated 2-body system)",
        "Instantaneous impulse response during contact"
    };
    metadata_.verification_metrics = {
        "Linear momentum conservation error: |P(t) - P(0)|",
        "Relative normal separation velocity ratio: |v_rel' / v_rel| vs e",
        "Kinetic energy change Delta Ek vs theoretical analytical dissipation",
        "Post-collision velocity errors against exact kinematic equations"
    };
    metadata_.numerical_limitations = {
        "Discrete collision detection requires v_max * dt_sub <= 0.5 * r_min to prevent tunneling",
        "Finite float32 precision introduces ~10^-7 relative roundoff error across integration steps"
    };
}

bool TwoParticleCollisionExperiment::initialize() {
    std::string err;
    if (!params_.is_valid(err)) {
        std::cerr << "Initialization error in TwoParticleCollisionExperiment: " << err << "\n";
        return false;
    }

    sim_core::SimConfig config;
    config.dt = params_.dt;
    config.substeps = params_.substeps;
    config.restitution = params_.restitution;
    config.boundary_restitution = 1.0f;
    config.gravity = sim_core::Vec2(0.0f, 0.0f); // isolated 2-body system

    sim_core::CircularBoundary boundary(sim_core::Vec2(0.0f, 0.0f), 10.0f); // large domain
    solver_.set_config(config);
    solver_.set_boundary(boundary);
    solver_.clear_particles();

    sim_core::Particle p1(1, params_.x1, params_.v1, params_.r1, params_.rho);
    sim_core::Particle p2(2, params_.x2, params_.v2, params_.r2, params_.rho);

    if (params_.m1_override > 0.0f) p1.set_fixed_mass(params_.m1_override);
    if (params_.m2_override > 0.0f) p2.set_fixed_mass(params_.m2_override);

    solver_.add_particle(p1);
    solver_.add_particle(p2);

    data_.clear();
    current_time_ = 0.0;
    current_frame_ = 0;
    total_frames_ = static_cast<int>(std::ceil(params_.duration / params_.dt));

    latest_diag_ = solver_.compute_diagnostics();
    initial_momentum_ = latest_diag_.total_momentum;
    initial_kinetic_energy_ = latest_diag_.total_kinetic_energy;

    compute_analytical_reference();
    record_frame();

    return true;
}

void TwoParticleCollisionExperiment::compute_analytical_reference() {
    const auto& particles = solver_.get_particles();
    summary_.m1 = particles[0].mass;
    summary_.m2 = particles[1].mass;
    summary_.v1_initial_x = particles[0].velocity.x;
    summary_.v2_initial_x = particles[1].velocity.x;

    const float m1 = summary_.m1;
    const float m2 = summary_.m2;
    const float u1 = summary_.v1_initial_x;
    const float u2 = summary_.v2_initial_x;
    const float e = params_.restitution;

    // 1D kinematic collision formulas with restitution e:
    summary_.v1_final_exact_x = ((m1 - e * m2) / (m1 + m2)) * u1 + (((1.0f + e) * m2) / (m1 + m2)) * u2;
    summary_.v2_final_exact_x = (((1.0f + e) * m1) / (m1 + m2)) * u1 + ((m2 - e * m1) / (m1 + m2)) * u2;

    const float m_eff = (m1 * m2) / (m1 + m2);
    const float v_rel = u1 - u2;
    summary_.theoretical_dEk = -0.5f * m_eff * (1.0f - e * e) * (v_rel * v_rel);

    summary_.max_momentum_error = 0.0f;
    summary_.final_v1_error = 0.0f;
    summary_.final_v2_error = 0.0f;
    summary_.actual_dEk = 0.0f;
}

void TwoParticleCollisionExperiment::record_frame() {
    const auto& particles = solver_.get_particles();
    latest_diag_ = solver_.compute_diagnostics();

    CollisionDataRow row;
    row.time = current_time_;
    row.x1 = particles[0].position.x;
    row.y1 = particles[0].position.y;
    row.vx1 = particles[0].velocity.x;
    row.vy1 = particles[0].velocity.y;

    row.x2 = particles[1].position.x;
    row.y2 = particles[1].position.y;
    row.vx2 = particles[1].velocity.x;
    row.vy2 = particles[1].velocity.y;

    row.momentum_x = latest_diag_.total_momentum.x;
    row.momentum_y = latest_diag_.total_momentum.y;
    row.kinetic_energy = latest_diag_.total_kinetic_energy;
    row.momentum_error = (latest_diag_.total_momentum - initial_momentum_).length();
    row.distance = (particles[0].position - particles[1].position).length();

    data_.push_back(row);

    if (row.momentum_error > summary_.max_momentum_error) {
        summary_.max_momentum_error = row.momentum_error;
    }
}

void TwoParticleCollisionExperiment::step() {
    if (is_completed()) return;

    solver_.step();
    current_frame_++;
    current_time_ = current_frame_ * params_.dt;
    record_frame();

    if (is_completed()) {
        const auto& particles = solver_.get_particles();
        summary_.final_v1_error = std::abs(particles[0].velocity.x - summary_.v1_final_exact_x);
        summary_.final_v2_error = std::abs(particles[1].velocity.x - summary_.v2_final_exact_x);
        summary_.actual_dEk = latest_diag_.total_kinetic_energy - initial_kinetic_energy_;
    }
}

bool TwoParticleCollisionExperiment::is_completed() const {
    return current_frame_ >= total_frames_;
}

void TwoParticleCollisionExperiment::run() {
    if (!initialize()) return;
    while (!is_completed()) {
        step();
    }
}

bool TwoParticleCollisionExperiment::export_csv(const std::string& filepath) const {
    std::ofstream out(filepath);
    if (!out) {
        std::cerr << "Failed to open CSV for writing: " << filepath << "\n";
        return false;
    }

    // Metadata header
    out << "# Experiment: " << metadata_.id << " (" << metadata_.title << ")\n";
    out << "# Physical Objective: " << metadata_.physical_objective << "\n";
    out << "# Parameters: m1=" << summary_.m1 << " kg, m2=" << summary_.m2 << " kg, e=" << params_.restitution
        << ", dt=" << params_.dt << " s, substeps=" << params_.substeps << "\n";
    out << "# Theoretical Post-Collision: v1'=" << summary_.v1_final_exact_x << " m/s, v2'="
        << summary_.v2_final_exact_x << " m/s, dEk=" << summary_.theoretical_dEk << " J\n";
    out << "# Columns: time(s),x1(m),y1(m),vx1(m/s),vy1(m/s),x2(m),y2(m),vx2(m/s),vy2(m/s),Px(kg*m/s),Py(kg*m/s),Ek(J),momentum_error(kg*m/s),distance(m)\n";
    out << "time,x1,y1,vx1,vy1,x2,y2,vx2,vy2,Px,Py,Ek,momentum_error,distance\n";

    out << std::setprecision(6) << std::fixed;
    for (const auto& r : data_) {
        out << r.time << ","
            << r.x1 << "," << r.y1 << "," << r.vx1 << "," << r.vy1 << ","
            << r.x2 << "," << r.y2 << "," << r.vx2 << "," << r.vy2 << ","
            << r.momentum_x << "," << r.momentum_y << ","
            << r.kinetic_energy << "," << r.momentum_error << "," << r.distance << "\n";
    }

    return true;
}

} // namespace lab_experiments
