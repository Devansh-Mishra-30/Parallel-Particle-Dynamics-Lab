#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace sim_core {

/**
 * \brief 2D Cartesian vector using standard SI units (meters, m/s, N, etc.).
 */
struct Vec2 {
    float x{0.0f};
    float y{0.0f};

    constexpr Vec2() = default;
    constexpr Vec2(float in_x, float in_y) : x(in_x), y(in_y) {}

    constexpr Vec2 operator+(const Vec2& rhs) const { return {x + rhs.x, y + rhs.y}; }
    constexpr Vec2 operator-(const Vec2& rhs) const { return {x - rhs.x, y - rhs.y}; }
    constexpr Vec2 operator*(float scalar) const { return {x * scalar, y * scalar}; }
    constexpr Vec2 operator/(float scalar) const { return {x / scalar, y / scalar}; }

    Vec2& operator+=(const Vec2& rhs) { x += rhs.x; y += rhs.y; return *this; }
    Vec2& operator-=(const Vec2& rhs) { x -= rhs.x; y -= rhs.y; return *this; }
    Vec2& operator*=(float scalar) { x *= scalar; y *= scalar; return *this; }
    Vec2& operator/=(float scalar) { x /= scalar; y /= scalar; return *this; }

    constexpr Vec2 operator-() const { return {-x, -y}; }

    [[nodiscard]] float length_squared() const { return x * x + y * y; }
    [[nodiscard]] float length() const { return std::sqrt(length_squared()); }

    [[nodiscard]] bool is_finite() const {
        return std::isfinite(x) && std::isfinite(y);
    }

    /**
     * \brief Return normalized unit vector. Falls back to (1, 0) if length <= epsilon.
     */
    [[nodiscard]] Vec2 normalized(float eps = 1e-6f) const {
        const float len = length();
        if (len > eps) {
            return {x / len, y / len};
        }
        return {1.0f, 0.0f};
    }

    static constexpr float dot(const Vec2& a, const Vec2& b) {
        return a.x * b.x + a.y * b.y;
    }

    static constexpr float cross(const Vec2& a, const Vec2& b) {
        return a.x * b.y - a.y * b.x;
    }

    static float distance(const Vec2& a, const Vec2& b) {
        return (a - b).length();
    }

    static float distance_squared(const Vec2& a, const Vec2& b) {
        return (a - b).length_squared();
    }
};

inline constexpr Vec2 operator*(float scalar, const Vec2& v) {
    return {scalar * v.x, scalar * v.y};
}

/**
 * \brief Circular particle representation in 2D SI space.
 */
struct Particle {
    uint32_t id{0};
    Vec2 position{0.0f, 0.0f};     // SI: meters (m)
    Vec2 velocity{0.0f, 0.0f};     // SI: meters per second (m/s)
    float radius{0.01f};           // SI: meters (m), e.g. 0.01m = 1cm
    float mass{0.0f};             // SI: kilograms (kg)
    float inv_mass{0.0f};         // SI: 1/kg (0 for fixed/infinite mass)

    Particle() = default;

    Particle(uint32_t in_id, const Vec2& pos, const Vec2& vel, float r, float rho = 1000.0f)
        : id(in_id), position(pos), velocity(vel), radius(r) {
        set_radius_and_density(r, rho);
    }

    /**
     * \brief Sets mass based on 2D disk area: m = pi * rho * r^2.
     */
    void set_radius_and_density(float r, float rho = 1000.0f) {
        radius = r;
        constexpr float PI = 3.14159265358979323846f;
        mass = PI * rho * radius * radius;
        inv_mass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
    }

    void set_fixed_mass(float in_mass) {
        mass = in_mass;
        inv_mass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
    }
};

/**
 * \brief Integration algorithms supported by the physical engine.
 */
enum class IntegratorType {
    SemiImplicitEuler = 0, // Symplectic (v_{n+1} = v_n + a*dt, x_{n+1} = x_n + v_{n+1}*dt)
    ExplicitEuler = 1      // Non-symplectic Forward Euler (x_{n+1} = x_n + v_n*dt, v_{n+1} = v_n + a*dt)
};

/**
 * \brief Physical simulation parameters in SI units.
 */
struct SimConfig {
    float dt{1.0f / 60.0f};              // Frame timestep: seconds (s)
    int substeps{8};                     // Number of integration sub-steps per frame
    Vec2 gravity{0.0f, -9.80665f};       // Uniform gravitational field: m/s^2
    float restitution{1.0f};             // Coefficient of restitution e in [0, 1]
    float boundary_restitution{1.0f};    // Boundary coefficient of restitution e_b in [0, 1]
    float stabilization_factor{0.5f};    // Baumgarte position stabilization alpha in [0.2, 0.8]
    int solver_iterations{1};            // Number of Gauss-Seidel contact iterations per substep
    IntegratorType integrator{IntegratorType::SemiImplicitEuler}; // Selected integration scheme

    [[nodiscard]] float dt_substep() const {
        return (substeps > 0) ? (dt / static_cast<float>(substeps)) : dt;
    }
};

/**
 * \brief Circular boundary constraint in SI units.
 */
struct CircularBoundary {
    Vec2 center{0.0f, 0.0f};  // Center coordinates (m)
    float radius{1.0f};       // Container radius (m)

    CircularBoundary() = default;
    CircularBoundary(const Vec2& c, float r) : center(c), radius(r) {}
};

/**
 * \brief Geometric and kinematic contact information between two particles.
 */
struct ContactPair {
    size_t i{0};
    size_t j{0};
    Vec2 normal{1.0f, 0.0f};      // Unit contact normal pointing from j to i
    float distance{0.0f};         // Center-to-center distance ||x_i - x_j|| (m)
    float penetration{0.0f};      // Overlap depth delta = (r_i + r_j) - distance (m)
    float normal_velocity{0.0f};  // Relative normal velocity v_rel . normal (m/s)
};

/**
 * \brief Physical conservation and stability diagnostics.
 */
struct SimulationDiagnostics {
    Vec2 total_momentum{0.0f, 0.0f};       // SI: kg*m/s
    float total_kinetic_energy{0.0f};      // SI: Joules (J)
    float total_potential_energy{0.0f};    // SI: Joules (J)
    float total_mechanical_energy{0.0f};   // SI: Joules (J)
    Vec2 center_of_mass{0.0f, 0.0f};       // SI: meters (m)
    float max_penetration{0.0f};           // SI: meters (m)
    int active_contacts{0};
    int nan_count{0};
    bool is_finite{true};
};

} // namespace sim_core
