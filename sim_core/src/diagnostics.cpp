#include "sim_core/diagnostics.h"

namespace sim_core {

SimulationDiagnostics DiagnosticsCalculator::compute(const std::vector<Particle>& particles, const SimConfig& config) {
    SimulationDiagnostics diag;

    float total_mass = 0.0f;
    Vec2 mass_weighted_pos(0.0f, 0.0f);

    const size_t n = particles.size();

    // 1. Single-particle integrals: Momentum, Kinetic Energy, Potential Energy, CoM
    for (size_t i = 0; i < n; ++i) {
        const auto& p = particles[i];
        if (!p.position.is_finite() || !p.velocity.is_finite()) {
            diag.nan_count++;
            diag.is_finite = false;
            continue;
        }

        // Linear momentum: p = m * v
        diag.total_momentum += p.mass * p.velocity;

        // Kinetic energy: 0.5 * m * v^2
        diag.total_kinetic_energy += 0.5f * p.mass * p.velocity.length_squared();

        // Gravitational potential energy: -m * (g . x)
        // With g = (0, -g_world), Ep = -m * (-g_world * y) = m * g_world * y (datum at y = 0)
        diag.total_potential_energy -= p.mass * Vec2::dot(config.gravity, p.position);

        total_mass += p.mass;
        mass_weighted_pos += p.mass * p.position;
    }

    diag.total_mechanical_energy = diag.total_kinetic_energy + diag.total_potential_energy;

    if (total_mass > 0.0f) {
        diag.center_of_mass = mass_weighted_pos / total_mass;
    }

    // 2. Pairwise contact detection & maximum penetration depth
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const float dist = (particles[i].position - particles[j].position).length();
            const float r_sum = particles[i].radius + particles[j].radius;
            if (dist < r_sum) {
                diag.active_contacts++;
                const float pen = r_sum - dist;
                if (pen > diag.max_penetration) {
                    diag.max_penetration = pen;
                }
            }
        }
    }

    return diag;
}

} // namespace sim_core
