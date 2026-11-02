#include "sim_core/cpu_reference_solver.h"

namespace sim_core {

CpuReferenceSolver::CpuReferenceSolver(const SimConfig& config, const CircularBoundary& boundary)
    : config_(config), boundary_(boundary) {}

void CpuReferenceSolver::substep(float dt_sub) {
    const size_t n = particles_.size();
    if (n == 0) return;

    // 1 & 2. Numerical Integration under external forces (gravity)
    if (config_.integrator == IntegratorType::ExplicitEuler) {
        for (size_t i = 0; i < n; ++i) {
            Integrator::step_forward_euler(particles_[i], config_.gravity, dt_sub);
        }
    } else {
        // Default: Semi-implicit Euler (Symplectic)
        for (size_t i = 0; i < n; ++i) {
            Integrator::integrate_velocity(particles_[i], config_.gravity, dt_sub);
        }
        for (size_t i = 0; i < n; ++i) {
            Integrator::integrate_position(particles_[i], dt_sub);
        }
    }

    // 3. Contact detection & pairwise resolution (sequential Gauss-Seidel)
    const int iterations = (config_.solver_iterations > 0) ? config_.solver_iterations : 1;
    for (int iter = 0; iter < iterations; ++iter) {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                ContactPair contact;
                if (ContactSolver::detect_contact(particles_[i], particles_[j], contact)) {
                    if (contact.normal_velocity < 0.0f) {
                        step_collision_impulses_++;
                        cumulative_collisions_++;
                    }
                    // Physical velocity impulse (momentum conserving)
                    ContactSolver::resolve_collision_impulse(particles_[i], particles_[j], contact, config_.restitution);

                    // Positional penetration stabilization (mass-weighted, does NOT inject velocity)
                    ContactSolver::resolve_penetration_stabilization(particles_[i], particles_[j], contact, config_.stabilization_factor);
                }
            }
        }
    }

    // 4. Circular boundary constraints
    for (size_t i = 0; i < n; ++i) {
        BoundaryHandler::apply_circular_boundary(particles_[i], boundary_, config_.boundary_restitution);
    }
}

void CpuReferenceSolver::step() {
    step_collision_impulses_ = 0;
    const int s = (config_.substeps > 0) ? config_.substeps : 1;
    const float dt_sub = config_.dt_substep();

    for (int step_idx = 0; step_idx < s; ++step_idx) {
        substep(dt_sub);
    }
}

SimulationDiagnostics CpuReferenceSolver::compute_diagnostics() const {
    SimulationDiagnostics diag = DiagnosticsCalculator::compute(particles_, config_);
    diag.step_collision_impulses = step_collision_impulses_;
    diag.cumulative_collisions = cumulative_collisions_;
    return diag;
}

} // namespace sim_core
