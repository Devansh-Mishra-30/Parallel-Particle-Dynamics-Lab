#pragma once

#include "sim_core/boundary.h"
#include "sim_core/contact.h"
#include "sim_core/diagnostics.h"
#include "sim_core/integrator.h"
#include "sim_core/types.h"
#include <vector>

namespace sim_core {

/**
 * \brief Sequential CPU Particle-Dynamics Reference Solver.
 *        Serves as the golden physical and numerical truth for Simulator V2.
 */
class CpuReferenceSolver {
public:
    explicit CpuReferenceSolver(const SimConfig& config = SimConfig(),
                               const CircularBoundary& boundary = CircularBoundary(Vec2(0.f, 0.f), 1.0f));

    void set_config(const SimConfig& config) { config_ = config; }
    [[nodiscard]] const SimConfig& get_config() const { return config_; }

    void set_boundary(const CircularBoundary& boundary) { boundary_ = boundary; }
    [[nodiscard]] const CircularBoundary& get_boundary() const { return boundary_; }

    void add_particle(const Particle& p) { particles_.push_back(p); }
    void set_particles(const std::vector<Particle>& p) { particles_ = p; }
    [[nodiscard]] const std::vector<Particle>& get_particles() const { return particles_; }
    [[nodiscard]] std::vector<Particle>& get_particles() { return particles_; }
    void clear_particles() { particles_.clear(); }
    [[nodiscard]] size_t particle_count() const { return particles_.size(); }

    /**
     * \brief Advance the simulation by exactly one substep of duration dt_sub.
     */
    void substep(float dt_sub);

    /**
     * \brief Advance the simulation by one full frame (executes config.substeps substeps).
     */
    void step();

    /**
     * \brief Compute current physical conservation and integrity diagnostics.
     */
    [[nodiscard]] SimulationDiagnostics compute_diagnostics() const;

private:
    SimConfig config_;
    CircularBoundary boundary_;
    std::vector<Particle> particles_;
};

} // namespace sim_core
