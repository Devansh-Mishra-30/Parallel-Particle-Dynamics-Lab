#pragma once

#include "sim_core/types.h"

namespace sim_core {

class BoundaryHandler {
public:
    /**
     * \brief Applies circular boundary reflection and positional clamping.
     */
    static void apply_circular_boundary(Particle& p, const CircularBoundary& boundary, float restitution);
};

} // namespace sim_core
