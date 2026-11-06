#pragma once

// Stage 2: pure, testable metadata for investigation and scenario selection.
// This header deliberately has no ImGui or solver dependencies.
namespace lab_gui {

enum class InvestigationMode {
    PhysicsVerification = 0,
    NumericalAccuracy = 1,
    HpcPerformance = 2
};

enum class PhysicsScenario {
    Collision = 0,
    Gravity = 1,
    DenseContacts = 2,
    ParticleCloud = 3,
    CustomSandbox = 4
};

constexpr int investigation_count = 3;
constexpr int scenario_count = 5;

inline const char* investigation_name(InvestigationMode mode) {
    switch (mode) {
        case InvestigationMode::PhysicsVerification: return "Physics verification";
        case InvestigationMode::NumericalAccuracy:    return "Numerical accuracy";
        case InvestigationMode::HpcPerformance:       return "HPC performance";
    }
    return "Unknown investigation";
}

inline const char* scenario_name(PhysicsScenario scenario) {
    switch (scenario) {
        case PhysicsScenario::Collision:     return "Two-particle collision";
        case PhysicsScenario::Gravity:       return "Gravity and ballistics";
        case PhysicsScenario::DenseContacts: return "Dense contacts";
        case PhysicsScenario::ParticleCloud: return "Particle cloud";
        case PhysicsScenario::CustomSandbox: return "Custom sandbox";
    }
    return "Unknown scenario";
}

struct InvestigationDescription {
    const char* title;
    const char* question;
    const char* evidence;
    const char* limitation;
    bool has_analytical_reference;
    bool has_builtin_convergence_sweep;
};

inline InvestigationDescription describe_investigation(InvestigationMode mode,
                                                       PhysicsScenario scenario) {
    switch (mode) {
        case InvestigationMode::PhysicsVerification:
            switch (scenario) {
                case PhysicsScenario::Collision:
                    return {"Collision: conservation and energy loss",
                            "Do simulated impact velocities and energy loss match analytical mechanics?",
                            "Post-impact velocity, impulse energy loss, momentum drift",
                            "Analytical model assumes an isolated frictionless binary collision.",
                            true, false};
                case PhysicsScenario::Gravity:
                    return {"Ballistic motion: analytical trajectory",
                            "How closely does numerical motion follow the ballistic solution?",
                            "Position and velocity error, energy trend",
                            "Ballistic analytical reference applies only before boundary contact.",
                            true, false};
                case PhysicsScenario::DenseContacts:
                    return {"Dense contacts: physical diagnostics",
                            "How do simultaneous contacts redistribute energy and momentum?",
                            "Energy, contact count, penetration, wall impulses",
                            "No analytical many-body reference or real-material validation is claimed.",
                            false, false};
                case PhysicsScenario::ParticleCloud:
                    return {"Particle cloud: system behavior",
                            "How does a larger interacting population evolve in confinement?",
                            "Energy, active contacts, finite-state and wall diagnostics",
                            "This synthetic cloud has no independent analytical trajectory reference.",
                            false, false};
                case PhysicsScenario::CustomSandbox:
                    return {"Custom sandbox: qualitative exploration",
                            "What happens to the chosen five-particle demonstration?",
                            "Energy, momentum, contacts and numerical health",
                            "Current sandbox is a five-particle preset; arbitrary particle editing is not yet available.",
                            false, false};
            }
            break;
        case InvestigationMode::NumericalAccuracy:
            switch (scenario) {
                case PhysicsScenario::Collision:
                    return {"Collision: discrete timestep sensitivity",
                            "Do substep count and contact iterations affect detected impacts?",
                            "Displacement ratio, contact response, penetration",
                            "No automated collision timestep sweep is provided yet.",
                            false, false};
                case PhysicsScenario::Gravity:
                    return {"Gravity: timestep convergence study",
                            "Does halving the timestep reduce integration error as expected?",
                            "Four-step refinement sweep, position error, observed convergence order",
                            "Convergence sweep evaluates smooth flight before wall impacts.",
                            true, true};
                case PhysicsScenario::DenseContacts:
                    return {"Dense contacts: penetration sensitivity",
                            "How do substeps, iterations and stabilization affect contacts?",
                            "Maximum overlap, contact count, finite-state checks",
                            "Contact diagnostics alone do not validate a real granular material.",
                            false, false};
                case PhysicsScenario::ParticleCloud:
                    return {"Particle cloud: discrete-step health",
                            "How do workload size and substep size affect collision robustness?",
                            "Displacement ratio, contacts, penetration, execution cost",
                            "The displacement ratio is a tunneling heuristic, not a guarantee.",
                            false, false};
                case PhysicsScenario::CustomSandbox:
                    return {"Custom sandbox: numerical health",
                            "Which numerical settings change the observed trajectory?",
                            "Finite-state diagnostics, contact overlap, energy trend",
                            "An independent accuracy reference is not supplied for this setup.",
                            false, false};
            }
            break;
        case InvestigationMode::HpcPerformance:
            switch (scenario) {
                case PhysicsScenario::Collision:
                    return {"Collision: small-workload overhead",
                            "Are CPU, OpenMP and CUDA competitive on just two particles?",
                            "Same-snapshot timing, speedup, position differences",
                            "Very small workloads are overhead-dominated; solvers use different update orders.",
                            false, false};
                case PhysicsScenario::Gravity:
                    return {"Gravity: single-particle overhead",
                            "What does parallel dispatch cost for one moving particle?",
                            "Compute time and cross-backend differences",
                            "Single-particle latency is not a GPU throughput benchmark.",
                            false, false};
                case PhysicsScenario::DenseContacts:
                    return {"Dense contacts: parallel contact processing",
                            "How do different contact-update strategies scale on dense interactions?",
                            "CPU/OpenMP/CUDA time, speedup, position differences",
                            "Gauss-Seidel and Jacobi may produce different dense-contact trajectories.",
                            false, false};
                case PhysicsScenario::ParticleCloud:
                    return {"Particle cloud: backend scaling",
                            "At which workload sizes do multicore and GPU execution help?",
                            "CPU/OpenMP/CUDA time, speedup, throughput and differences",
                            "CPU all-pairs and parallel spatial-grid comparisons mix algorithm and hardware effects.",
                            false, false};
                case PhysicsScenario::CustomSandbox:
                    return {"Custom sandbox: backend comparison",
                            "How much overhead does each backend add to a small custom setup?",
                            "Same-snapshot compute time and numerical differences",
                            "This small preset is not representative of high-particle-count throughput.",
                            false, false};
            }
            break;
    }
    return {"Unsupported selection", "Choose a valid investigation and scenario.",
            "No measurements", "Unsupported selection.", false, false};
}

} // namespace lab_gui
