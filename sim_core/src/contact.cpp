#include "sim_core/contact.h"

namespace sim_core {

bool ContactSolver::detect_contact(const Particle& a, const Particle& b, ContactPair& out_contact) {
    const Vec2 r_ab = a.position - b.position;
    const float min_dist = a.radius + b.radius;
    const float dist_sq = r_ab.length_squared();

    if (dist_sq >= min_dist * min_dist) {
        return false;
    }

    const float dist = std::sqrt(dist_sq);
    out_contact.distance = dist;
    out_contact.penetration = min_dist - dist;

    // Deterministic regularization when particles coincide
    constexpr float EPS = 1e-6f;
    if (dist > EPS) {
        out_contact.normal = r_ab / dist;
    } else {
        out_contact.normal = Vec2(1.0f, 0.0f);
    }

    out_contact.normal_velocity = Vec2::dot(a.velocity - b.velocity, out_contact.normal);
    return true;
}

void ContactSolver::resolve_collision_impulse(Particle& a, Particle& b, const ContactPair& contact, float restitution) {
    // Re-evaluate current relative normal velocity with latest velocities
    const float v_n = Vec2::dot(a.velocity - b.velocity, contact.normal);

    // Physical impulse applies if and only if particles are approaching
    if (v_n >= 0.0f) {
        return;
    }

    const float inv_m_sum = a.inv_mass + b.inv_mass;
    if (inv_m_sum <= 0.0f) {
        return; // Two fixed particles
    }

    const float j_n = -(1.0f + restitution) * v_n / inv_m_sum;

    a.velocity += (j_n * a.inv_mass) * contact.normal;
    b.velocity -= (j_n * b.inv_mass) * contact.normal;
}

void ContactSolver::resolve_penetration_stabilization(Particle& a, Particle& b, const ContactPair& contact, float alpha) {
    // Recompute current geometric separation
    const Vec2 r_ab = a.position - b.position;
    const float min_dist = a.radius + b.radius;
    const float dist = r_ab.length();
    const float delta = min_dist - dist;

    if (delta <= 0.0f) {
        return; // No longer penetrating
    }

    constexpr float EPS = 1e-6f;
    const Vec2 n = (dist > EPS) ? (r_ab / dist) : contact.normal;

    const float inv_m_sum = a.inv_mass + b.inv_mass;
    if (inv_m_sum <= 0.0f) {
        return;
    }

    const float correction = alpha * delta / inv_m_sum;

    // Apply directly to positions. Velocity is strictly untouched.
    a.position += (correction * a.inv_mass) * n;
    b.position -= (correction * b.inv_mass) * n;
}

} // namespace sim_core
