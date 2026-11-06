#pragma once

// Pure helpers for deciding when the numerical trajectory stops being an
// unconstrained ballistic flight. These deliberately have no ImGui dependency.
#include "sim_core/integrator.h"
#include "sim_core/types.h"

namespace lab_gui {

inline sim_core::Particle advance_unconstrained_ballistic(
    sim_core::Particle particle, const sim_core::SimConfig& config) {
    const int substeps = config.substeps > 0 ? config.substeps : 1;
    const float h = config.dt / static_cast<float>(substeps);
    for (int i = 0; i < substeps; ++i) {
        if (config.integrator == sim_core::IntegratorType::ExplicitEuler) {
            sim_core::Integrator::step_forward_euler(particle, config.gravity, h);
        } else {
            sim_core::Integrator::integrate_velocity(particle, config.gravity, h);
            sim_core::Integrator::integrate_position(particle, h);
        }
    }
    return particle;
}

inline bool boundary_impulse_occurred(
    const sim_core::Particle& before,
    const sim_core::Particle& after,
    const sim_core::SimConfig& config,
    float position_tolerance_m = 2.0e-5f,
    float velocity_tolerance_m_s = 2.0e-5f) {
    const sim_core::Particle free_flight =
        advance_unconstrained_ballistic(before, config);
    return (after.position - free_flight.position).length() > position_tolerance_m ||
           (after.velocity - free_flight.velocity).length() > velocity_tolerance_m_s;
}

} // namespace lab_gui
