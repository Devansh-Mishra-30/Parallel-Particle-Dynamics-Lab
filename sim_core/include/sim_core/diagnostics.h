#pragma once

#include "sim_core/types.h"
#include <vector>

namespace sim_core {

class DiagnosticsCalculator {
public:
    /**
     * \brief Computes physical invariants and health diagnostics for the particle state.
     */
    static SimulationDiagnostics compute(const std::vector<Particle>& particles, const SimConfig& config);
};

} // namespace sim_core
