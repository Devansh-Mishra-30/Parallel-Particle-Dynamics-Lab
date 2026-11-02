#pragma once

#include "experiments/experiment.h"
#include "sim_core/cpu_reference_solver.h"
#include <string>
#include <vector>

namespace lab_experiments {

struct CollisionParams {
    float r1{0.05f};                       // Radius 1 (m)
    float r2{0.05f};                       // Radius 2 (m)
    float m1_override{0.0f};               // If > 0, explicit mass (kg), else pi*rho*r1^2
    float m2_override{0.0f};               // If > 0, explicit mass (kg), else pi*rho*r2^2
    float rho{1000.0f};                    // Areal density (kg/m^2)
    sim_core::Vec2 x1{-0.2f, 0.0f};        // Position 1 (m)
    sim_core::Vec2 x2{0.2f, 0.0f};         // Position 2 (m)
    sim_core::Vec2 v1{1.0f, 0.0f};         // Velocity 1 (m/s)
    sim_core::Vec2 v2{-1.0f, 0.0f};        // Velocity 2 (m/s)
    float restitution{1.0f};               // Restitution coefficient e in [0, 1]
    float dt{1.0f / 120.0f};               // Timestep (s)
    int substeps{8};                       // Substeps per frame
    float duration{0.4f};                  // Total duration (s)

    [[nodiscard]] bool is_valid(std::string& error_message) const;
};

struct CollisionDataRow {
    double time{0.0};
    float x1{0.0f}, y1{0.0f}, vx1{0.0f}, vy1{0.0f};
    float x2{0.0f}, y2{0.0f}, vx2{0.0f}, vy2{0.0f};
    float momentum_x{0.0f}, momentum_y{0.0f};
    float kinetic_energy{0.0f};
    float momentum_error{0.0f};
    float distance{0.0f};
};

struct CollisionAnalyticalSummary {
    float m1{0.0f};
    float m2{0.0f};
    float v1_initial_x{0.0f};
    float v2_initial_x{0.0f};
    float v1_final_exact_x{0.0f};
    float v2_final_exact_x{0.0f};
    float theoretical_dEk{0.0f};
    float actual_dEk{0.0f};
    float max_momentum_error{0.0f};
    float final_v1_error{0.0f};
    float final_v2_error{0.0f};
};

class TwoParticleCollisionExperiment : public IExperiment {
public:
    explicit TwoParticleCollisionExperiment(const CollisionParams& params = CollisionParams());

    [[nodiscard]] const ExperimentMetadata& get_metadata() const override { return metadata_; }
    bool initialize() override;
    void step() override;
    [[nodiscard]] bool is_completed() const override;
    [[nodiscard]] double current_time() const override { return current_time_; }
    void run() override;
    bool export_csv(const std::string& filepath) const override;
    [[nodiscard]] const sim_core::SimulationDiagnostics& get_diagnostics() const override { return latest_diag_; }

    [[nodiscard]] const CollisionParams& get_params() const { return params_; }
    [[nodiscard]] const CollisionAnalyticalSummary& get_summary() const { return summary_; }
    [[nodiscard]] const std::vector<CollisionDataRow>& get_data() const { return data_; }

private:
    CollisionParams params_;
    ExperimentMetadata metadata_;
    sim_core::CpuReferenceSolver solver_;
    std::vector<CollisionDataRow> data_;
    CollisionAnalyticalSummary summary_;
    sim_core::SimulationDiagnostics latest_diag_;
    double current_time_{0.0};
    int current_frame_{0};
    int total_frames_{0};
    sim_core::Vec2 initial_momentum_{0.0f, 0.0f};
    float initial_kinetic_energy_{0.0f};

    void init_metadata();
    void compute_analytical_reference();
    void record_frame();
};

} // namespace lab_experiments
