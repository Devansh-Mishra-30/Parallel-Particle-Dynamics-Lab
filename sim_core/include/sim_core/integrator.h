#pragma once

#include "sim_core/types.h"

namespace sim_core {

/**
 * \brief Semi-implicit Euler integration functions.
 */
class Integrator {
public:
    /**
     * \brief Advance particle velocity under constant acceleration: v_{n+1} = v_n + a * dt
     */
    static inline void integrate_velocity(Particle& p, const Vec2& acceleration, float dt) {
        p.velocity += acceleration * dt;
    }

    /**
     * \brief Advance particle position under current velocity: x_{n+1} = x_n + v_{n+1} * dt
     */
    static inline void integrate_position(Particle& p, float dt) {
        p.position += p.velocity * dt;
    }

    /**
     * \brief Full single-particle Semi-implicit Euler step under constant acceleration.
     */
    static inline void step_semi_implicit_euler(Particle& p, const Vec2& acceleration, float dt) {
        integrate_velocity(p, acceleration, dt);
        integrate_position(p, dt);
    }

    /**
     * \brief Full single-particle Explicit (Forward) Euler step: x_{n+1} = x_n + v_n * dt, v_{n+1} = v_n + a * dt.
     * Note: Non-symplectic; phase-space determinant > 1 causes artificial energy amplification.
     */
    static inline void step_forward_euler(Particle& p, const Vec2& acceleration, float dt) {
        p.position += p.velocity * dt;
        p.velocity += acceleration * dt;
    }
};

} // namespace sim_core
