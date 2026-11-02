#pragma once

#include "lab_gui/app_state.h"
#include <cmath>
#include <random>

namespace lab_gui {

class ExperimentPresets {
public:
    /**
     * \brief EXP_01: Two-Particle Impact (Central & Oblique)
     */
    static void load_collision(AppState& state,
                               float restitution = 1.0f,
                               float r1 = 0.06f,
                               float r2 = 0.06f,
                               float m1 = 0.0f, // 0 => derive from area
                               float m2 = 0.0f, // 0 => derive from area
                               sim_core::Vec2 v1 = {1.0f, 0.0f},
                               sim_core::Vec2 v2 = {-1.0f, 0.0f},
                               float impact_parameter_b = 0.0f) {
        state.active_experiment = ExperimentType::Collision;
        state.config.dt = 1.0f / 120.0f;
        state.config.substeps = 8;
        state.config.restitution = restitution;
        state.config.boundary_restitution = 1.0f;
        state.config.gravity = {0.0f, 0.0f}; // Isolated 2-body system
        state.config.solver_iterations = 2;
        state.config.stabilization_factor = 0.5f;

        // Container radius large enough to prevent wall interference during collision and separation
        state.boundary = sim_core::CircularBoundary({0.0f, 0.0f}, 1.2f);
        state.solver.set_config(state.config);
        state.solver.set_boundary(state.boundary);
        state.solver.clear_particles();

        // Initial positions with optional vertical impact offset (oblique collision)
        const sim_core::Vec2 x1(-0.30f, +impact_parameter_b * 0.5f);
        const sim_core::Vec2 x2(+0.30f, -impact_parameter_b * 0.5f);

        sim_core::Particle p1(1, x1, v1, r1);
        sim_core::Particle p2(2, x2, v2, r2);

        if (m1 > 0.0f) p1.set_fixed_mass(m1);
        if (m2 > 0.0f) p2.set_fixed_mass(m2);

        state.solver.add_particle(p1);
        state.solver.add_particle(p2);

        state.current_time = 0.0;
        state.total_substeps = 0;
        state.frame_index = 0;
        state.reset_buffers();

        state.current_diagnostics = state.solver.compute_diagnostics();
        state.initial_momentum = state.current_diagnostics.total_momentum;
        state.initial_energy = state.current_diagnostics.total_mechanical_energy;
        state.initial_pos1 = p1.position;
        state.initial_vel1 = p1.velocity;
        state.measured_initial_ek = state.current_diagnostics.total_kinetic_energy;

        // Store immutable initial state for rigorous analytical comparison
        state.initial_collision.x1 = x1;
        state.initial_collision.x2 = x2;
        state.initial_collision.v1 = v1;
        state.initial_collision.v2 = v2;
        state.initial_collision.r1 = r1;
        state.initial_collision.r2 = r2;
        state.initial_collision.m1 = p1.mass;
        state.initial_collision.m2 = p2.mass;
        state.initial_collision.restitution = restitution;
        state.initial_collision.impact_parameter = impact_parameter_b;

        // Recompute analytical predictions
        state.recompute_collision_analytical_solution();

        state.camera_offset = {0.0f, 0.0f};
        state.camera_zoom = 380.0f;
        state.sync_solvers_from_cpu();
    }

    /**
     * \brief EXP_02: Ballistic Flight & Free Fall Trajectories
     */
    static void load_gravity(AppState& state,
                             sim_core::Vec2 g = {0.0f, -9.80665f},
                             sim_core::Vec2 v0 = {1.2f, 2.5f},
                             float dt = 1.0f / 120.0f) {
        state.active_experiment = ExperimentType::GravityBallistic;
        state.config.dt = dt;
        state.config.substeps = 8;
        state.config.restitution = 1.0f;
        state.config.boundary_restitution = 0.85f;
        state.config.gravity = g;
        state.config.solver_iterations = 1;

        state.boundary = sim_core::CircularBoundary({0.0f, 0.0f}, 1.2f);
        state.solver.set_config(state.config);
        state.solver.set_boundary(state.boundary);
        state.solver.clear_particles();

        const sim_core::Vec2 x0(-0.6f, -0.4f);
        sim_core::Particle p1(1, x0, v0, 0.04f);
        state.solver.add_particle(p1);

        state.current_time = 0.0;
        state.total_substeps = 0;
        state.frame_index = 0;
        state.reset_buffers();

        state.current_diagnostics = state.solver.compute_diagnostics();
        state.initial_momentum = state.current_diagnostics.total_momentum;
        state.initial_energy = state.current_diagnostics.total_mechanical_energy;
        state.initial_pos1 = x0;
        state.initial_vel1 = v0;

        state.camera_offset = {0.0f, 0.0f};
        state.camera_zoom = 320.0f;
        state.sync_solvers_from_cpu();
    }

    /**
     * \brief EXP_03: Empirical Timestep Refinement Study
     */
    static void load_convergence(AppState& state) {
        // High-precision projectile flight under constant gravity
        load_gravity(state, {0.0f, -9.80665f}, {1.5f, 2.2f}, 1.0f / 120.0f);
        state.active_experiment = ExperimentType::ConvergenceStudy;
        state.convergence_results.clear();
        state.convergence_study_running = false;
        state.sync_solvers_from_cpu();
    }

    /**
     * \brief EXP_04: Dense Granular Column Settling (Dense simultaneous contacts)
     */
    static void load_dense_packing(AppState& state, int count = 48) {
        state.active_experiment = ExperimentType::DensePacking;
        state.config.dt = 1.0f / 60.0f;
        state.config.substeps = 12;
        state.config.restitution = 0.2f;        // High energy dissipation to promote rapid settling
        state.config.boundary_restitution = 0.3f;
        state.config.gravity = {0.0f, -9.80665f};
        state.config.solver_iterations = 8;     // Multi-pass Gauss-Seidel for stacked contacts
        state.config.stabilization_factor = 0.6f;

        state.boundary = sim_core::CircularBoundary({0.0f, 0.0f}, 0.9f);
        state.solver.set_config(state.config);
        state.solver.set_boundary(state.boundary);
        state.solver.clear_particles();

        // Create stratified vertical columns resting close to the floor for reproducible dense contacts
        const float r = 0.035f;
        int p_idx = 1;
        const int cols = 6;
        const int rows = count / cols;

        for (int r_idx = 0; r_idx < rows; ++r_idx) {
            for (int c_idx = 0; c_idx < cols; ++c_idx) {
                const float offset_x = (r_idx % 2 == 1) ? (r * 0.5f) : 0.0f;
                const float px = -0.22f + c_idx * (2.0f * r + 0.005f) + offset_x;
                const float py = -0.65f + r_idx * (2.0f * r + 0.006f);

                sim_core::Particle p(p_idx++, {px, py}, {0.0f, 0.0f}, r);
                state.solver.add_particle(p);
            }
        }

        state.current_time = 0.0;
        state.total_substeps = 0;
        state.frame_index = 0;
        state.reset_buffers();

        state.current_diagnostics = state.solver.compute_diagnostics();
        state.initial_momentum = state.current_diagnostics.total_momentum;
        state.initial_energy = state.current_diagnostics.total_mechanical_energy;

        state.camera_offset = {0.0f, -0.3f};
        state.camera_zoom = 360.0f;
        state.sync_solvers_from_cpu();
    }

    /**
     * \brief EXP_CUSTOM: User-Configurable Particle Setup
     */
    static void load_custom(AppState& state) {
        state.active_experiment = ExperimentType::Custom;
        state.config.dt = 1.0f / 60.0f;
        state.config.substeps = 8;
        state.config.restitution = 0.85f;
        state.config.boundary_restitution = 0.9f;
        state.config.gravity = {0.0f, -9.80665f};
        state.config.solver_iterations = 4;
        state.config.stabilization_factor = 0.5f;

        state.boundary = sim_core::CircularBoundary({0.0f, 0.0f}, 1.0f);
        state.solver.set_config(state.config);
        state.solver.set_boundary(state.boundary);
        state.solver.clear_particles();

        state.solver.add_particle(sim_core::Particle(1, {-0.4f, 0.3f}, {1.0f, -0.5f}, 0.06f));
        state.solver.add_particle(sim_core::Particle(2, {0.3f, 0.4f}, {-0.8f, -0.2f}, 0.05f));
        state.solver.add_particle(sim_core::Particle(3, {0.0f, 0.6f}, {0.2f, -1.2f}, 0.07f));
        state.solver.add_particle(sim_core::Particle(4, {-0.2f, -0.2f}, {0.5f, 0.8f}, 0.045f));
        state.solver.add_particle(sim_core::Particle(5, {0.4f, -0.3f}, {-0.6f, 0.4f}, 0.055f));

        state.current_time = 0.0;
        state.total_substeps = 0;
        state.frame_index = 0;
        state.reset_buffers();

        state.current_diagnostics = state.solver.compute_diagnostics();
        state.initial_momentum = state.current_diagnostics.total_momentum;
        state.initial_energy = state.current_diagnostics.total_mechanical_energy;

        state.camera_offset = {0.0f, 0.0f};
        state.camera_zoom = 340.0f;
        state.sync_solvers_from_cpu();
    }
};

} // namespace lab_gui
