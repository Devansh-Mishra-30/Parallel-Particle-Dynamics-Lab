#pragma once

#include "sim_core/types.h"

namespace sim_core {

class ContactSolver {
public:
    /**
     * \brief Detect contact between two circular particles.
     * \return true if particles overlap (distance < r_a + r_b).
     */
    static bool detect_contact(const Particle& a, const Particle& b, ContactPair& out_contact);

    /**
     * \brief Applies momentum-conserving frictionless normal collision impulse.
     *        Modifies velocities ONLY if particles are approaching (v_n < 0).
     */
    static void resolve_collision_impulse(Particle& a, Particle& b, const ContactPair& contact, float restitution);

    /**
     * \brief Resolves geometric penetration via mass-weighted position projection.
     *        Modifies positions ONLY; velocity state is untouched.
     */
    static void resolve_penetration_stabilization(Particle& a, Particle& b, const ContactPair& contact, float alpha);
};

} // namespace sim_core
