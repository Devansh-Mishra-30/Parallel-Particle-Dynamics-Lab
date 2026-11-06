#pragma once

// Stage 3 prediction rules are deterministic and independent of Dear ImGui.
// They describe expected trends; they do NOT certify measured correctness.
#include "lab_gui/workbench_modes.h"
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace lab_gui {

struct WorkbenchParameters {
    float restitution{1.0f};
    float wall_restitution{1.0f};
    float gravity_x{0.0f};
    float gravity_y{0.0f};
    float frame_dt{1.0f / 120.0f};
    int substeps{8};
    int contact_iterations{2};
    float stabilization{0.5f};
    float boundary_radius{1.2f};
    int particle_count{2};
    int integrator{0}; // 0: semi-implicit Euler; 1: explicit Euler
    // Relevant for the isolated collision scenario only.
    float radius_1{0.0f};
    float radius_2{0.0f};
    float mass_1{0.0f};
    float mass_2{0.0f};
    float impact_offset{0.0f};
    float initial_v1_x{0.0f};
    float initial_v1_y{0.0f};
    float initial_v2_x{0.0f};
    float initial_v2_y{0.0f};
};

inline bool different(float a, float b) {
    return std::fabs(a - b) > 1e-6f;
}

inline bool parameters_differ(const WorkbenchParameters& a,
                              const WorkbenchParameters& b) {
    return different(a.restitution, b.restitution) ||
           different(a.wall_restitution, b.wall_restitution) ||
           different(a.gravity_x, b.gravity_x) ||
           different(a.gravity_y, b.gravity_y) ||
           different(a.frame_dt, b.frame_dt) ||
           a.substeps != b.substeps ||
           a.contact_iterations != b.contact_iterations ||
           different(a.stabilization, b.stabilization) ||
           different(a.boundary_radius, b.boundary_radius) ||
           a.particle_count != b.particle_count ||
           a.integrator != b.integrator ||
           different(a.radius_1, b.radius_1) ||
           different(a.radius_2, b.radius_2) ||
           different(a.mass_1, b.mass_1) ||
           different(a.mass_2, b.mass_2) ||
           different(a.impact_offset, b.impact_offset) ||
           different(a.initial_v1_x, b.initial_v1_x) ||
           different(a.initial_v1_y, b.initial_v1_y) ||
           different(a.initial_v2_x, b.initial_v2_x) ||
           different(a.initial_v2_y, b.initial_v2_y);
}

struct WorkbenchPrediction {
    std::string parameter;
    std::string change;
    std::string expectation;
    std::string explanation;
    std::string tradeoff;
    std::string inspect;
};

inline std::string format_value(float x, int precision = 3) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << x;
    return out.str();
}

inline std::string format_int(int x) {
    return std::to_string(x);
}

inline std::vector<WorkbenchPrediction> predict_changes(
    const WorkbenchParameters& baseline,
    const WorkbenchParameters& current,
    PhysicsScenario scenario,
    InvestigationMode investigation) {

    std::vector<WorkbenchPrediction> notes;
    auto add = [&](std::string parameter, std::string change,
                   std::string expectation, std::string explanation,
                   std::string tradeoff, std::string inspect) {
        notes.push_back({parameter, change, expectation, explanation,
                         tradeoff, inspect});
    };

    const bool isolated_collision = scenario == PhysicsScenario::Collision;
    const bool gravity_test = scenario == PhysicsScenario::Gravity;
    const bool hpc = investigation == InvestigationMode::HpcPerformance;

    if (different(baseline.restitution, current.restitution)) {
        const bool increased = current.restitution > baseline.restitution;
        add("Particle restitution (e)",
            format_value(baseline.restitution) + " -> " + format_value(current.restitution),
            increased ? "Less kinetic energy lost during individual impacts."
                      : "More kinetic energy lost during individual impacts.",
            "For an isolated frictionless binary collision, impact energy loss scales with 1 - e^2.",
            "In a many-particle system, changing rebound strength also changes later collisions; "
            "total energy is influenced by wall impacts and other events.",
            isolated_collision ? "Compare measured and analytical Delta Ek after the first impact."
                               : "Inspect the energy history and contact events; no universal analytic energy curve exists.");
    }
    if (different(baseline.wall_restitution, current.wall_restitution)) {
        const bool increased = current.wall_restitution > baseline.wall_restitution;
        add("Wall restitution (e_b)",
            format_value(baseline.wall_restitution) + " -> " + format_value(current.wall_restitution),
            increased ? "Less energy lost at wall impacts."
                      : "More energy lost at wall impacts.",
            "Wall restitution controls normal rebound at the circular boundary.",
            "Changing wall restitution has no effect until a particle contacts the boundary.",
            "Inspect energy changes around boundary collisions, not before them.");
    }
    if (different(baseline.gravity_x, current.gravity_x) ||
        different(baseline.gravity_y, current.gravity_y)) {
        add("Gravity (m/s^2)",
            "(" + format_value(baseline.gravity_x, 2) + ", " +
                 format_value(baseline.gravity_y, 2) + ") -> (" +
                 format_value(current.gravity_x, 2) + ", " +
                 format_value(current.gravity_y, 2) + ")",
            "Acceleration and particle trajectories will change.",
            "For free flight, displacement due to gravity depends on 0.5 * g * t^2.",
            isolated_collision ? "Gravity breaks the isolated zero-external-force collision assumption."
                               : "Wall contacts and dense collisions complicate analytical prediction.",
            gravity_test ? "Compare trajectory error against the ballistic solution BEFORE wall contact."
                         : "Inspect particle paths and energy/momentum changes caused by external forces.");
    }
    if (different(baseline.frame_dt, current.frame_dt)) {
        const bool increased = current.frame_dt > baseline.frame_dt;
        add("Frame timestep (dt)",
            format_value(baseline.frame_dt, 5) + " -> " + format_value(current.frame_dt, 5) + " s",
            increased ? "Coarser time resolution per physical frame."
                      : "Finer time resolution per physical frame.",
            "The effective integration step is dt / substeps; global smooth-flight error is first order for Euler.",
            "Changing frame dt can also change collision timing and physical time advanced per frame.",
            gravity_test ? "Run the timestep convergence sweep and inspect position error."
                         : "Inspect displacement ratio, contact detection and step cost.");
    }
    if (baseline.substeps != current.substeps) {
        const bool increased = current.substeps > baseline.substeps;
        add("Integration substeps", format_int(baseline.substeps) + " -> " + format_int(current.substeps),
            increased ? "Smaller displacement between discrete collision checks."
                      : "Larger displacement between discrete collision checks.",
            "Each physics frame is integrated in substeps of dt / N_sub.",
            increased ? "More integration and collision work per frame; tunneling is still possible."
                      : "Lower compute cost may increase missed-contact/tunneling risk.",
            "Compare displacement-to-radius heuristic, contacts, penetration and runtime.");
    }
    if (baseline.contact_iterations != current.contact_iterations) {
        const bool increased = current.contact_iterations > baseline.contact_iterations;
        add("Contact iterations (K)",
            format_int(baseline.contact_iterations) + " -> " + format_int(current.contact_iterations),
            increased ? "Contact overlap may decrease in dense interactions."
                      : "Residual overlap may increase in dense interactions.",
            "Additional relaxation passes refine simultaneous-contact corrections.",
            "More iterations cost more compute; monotonic penetration improvement is not guaranteed.",
            "Inspect peak penetration, active contacts, and frame time in a dense-contact scenario.");
    }
    if (different(baseline.stabilization, current.stabilization)) {
        const bool increased = current.stabilization > baseline.stabilization;
        add("Penetration stabilization (alpha)",
            format_value(baseline.stabilization) + " -> " + format_value(current.stabilization),
            increased ? "Stronger position correction per relaxation pass."
                      : "Weaker position correction per relaxation pass.",
            "Stabilization adjusts how aggressively overlapping particles are separated.",
            "Aggressive correction can introduce jitter or nonphysical motion; effects depend on contact layout.",
            "Inspect penetration and energy behavior in a dense-contact experiment.");
    }
    if (different(baseline.boundary_radius, current.boundary_radius)) {
        const bool increased = current.boundary_radius > baseline.boundary_radius;
        add("Container radius", format_value(baseline.boundary_radius, 2) + " -> " +
            format_value(current.boundary_radius, 2) + " m",
            increased ? "Particles have more room before wall contact."
                      : "Particles encounter a closer wall; initial containment may need checking.",
            "The circular boundary changes the available physical domain, not the particle collision law.",
            "Wall impulses affect total particle momentum, so momentum conservation requires context.",
            "Inspect time of first wall contact and momentum/energy changes.");
    }
    if (baseline.particle_count != current.particle_count) {
        add("Particle count (N)",
            format_int(baseline.particle_count) + " -> " + format_int(current.particle_count),
            "The collision workload and computational cost will change.",
            "An all-pairs search checks N(N-1)/2 candidate pairs per sweep.",
            "Spatial-grid cost depends on density and distribution; higher N does not guarantee GPU wins.",
            hpc ? "Compare CPU/OpenMP/CUDA timings over repeated workload sizes."
                : "Inspect contacts and compute time; this is not an analytical validation.");
    }
    if (baseline.integrator != current.integrator) {
        add("Integrator",
            std::string(baseline.integrator == 1 ? "Explicit Euler" : "Semi-implicit Euler") + " -> " +
            (current.integrator == 1 ? "Explicit Euler" : "Semi-implicit Euler"),
            "Long-term trajectory and energy behavior may change.",
            "The two first-order Euler variants use different position/velocity update ordering.",
            "Neither scheme is universally more accurate for every contact-rich problem.",
            gravity_test ? "Inspect smooth-flight position error and convergence order."
                         : "Compare energy trends at the same physical duration.");
    }
    if (isolated_collision &&
        (different(baseline.radius_1, current.radius_1) ||
         different(baseline.radius_2, current.radius_2) ||
         different(baseline.mass_1, current.mass_1) ||
         different(baseline.mass_2, current.mass_2))) {
        add("Collision particle size / mass",
            "r1 " + format_value(baseline.radius_1) + " -> " + format_value(current.radius_1) +
            " m; r2 " + format_value(baseline.radius_2) + " -> " + format_value(current.radius_2) + " m",
            "Contact geometry and the post-impact impulse distribution can change.",
            "In this 2D disc model, automatically derived mass scales with radius squared.",
            "Changing size and mass together does not isolate either effect; track both.",
            "Compare expected vs measured post-impact velocities and Delta Ek.");
    }
    if (isolated_collision && different(baseline.impact_offset, current.impact_offset)) {
        add("Impact offset (b)",
            format_value(baseline.impact_offset) + " -> " +
                format_value(current.impact_offset) + " m",
            "The contact normal changes; the collision can become oblique or miss.",
            "Impulse depends on relative speed projected onto the contact normal.",
            "A glancing impact generally changes the normal impulse and energy loss.",
            "Inspect post-impact velocity components and collision detection.");
    }
    if (isolated_collision &&
        (different(baseline.initial_v1_x, current.initial_v1_x) ||
         different(baseline.initial_v1_y, current.initial_v1_y) ||
         different(baseline.initial_v2_x, current.initial_v2_x) ||
         different(baseline.initial_v2_y, current.initial_v2_y))) {
        add("Initial collision velocities", "Initial particle velocity vectors changed",
            "Impact timing and normal relative collision speed can change.",
            "Normal impulse scales with approach speed along the contact normal.",
            "If particles miss or separate, the isolated impact formula does not apply.",
            "Compare the first-contact event with the analytical pre-impact setup.");
    }
    return notes;
}

} // namespace lab_gui
