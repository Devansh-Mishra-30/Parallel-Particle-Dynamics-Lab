#include "sim_core/boundary.h"

namespace sim_core {

void BoundaryHandler::apply_circular_boundary(Particle& p, const CircularBoundary& boundary, float restitution) {
    const Vec2 r_b = p.position - boundary.center;
    const float dist = r_b.length();
    const float max_dist = boundary.radius - p.radius;

    if (dist <= max_dist) {
        return; // Particle completely inside
    }

    constexpr float EPS = 1e-6f;
    // Inward normal pointing from wall toward container center
    const Vec2 n_b = (dist > EPS) ? (-r_b / dist) : Vec2(0.0f, 1.0f);

    // Normal velocity component along inward normal
    const float v_n = Vec2::dot(p.velocity, n_b);

    // If particle is moving outward towards the wall (v_n < 0), reflect it
    if (v_n < 0.0f) {
        p.velocity -= (1.0f + restitution) * v_n * n_b;
    }

    // Clamp particle position to interior boundary perimeter
    p.position = boundary.center - max_dist * n_b;
}

} // namespace sim_core
