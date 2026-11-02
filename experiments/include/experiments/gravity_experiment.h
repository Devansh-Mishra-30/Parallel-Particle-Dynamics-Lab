#pragma once

#include "experiments/experiment.h"
#include "sim_core/cpu_reference_solver.h"
#include <string>
#include <vector>

namespace lab_experiments {

struct GravityParams {
    sim_core::Vec2 gravity{0.0f, -9.80665f}; // Gravitational acceleration (m/s^2)
    sim_core::Vec2 x0{0.0f, 2.0f};            // Initial position (m)
    sim_core::Vec2 v0{1.0f, 0.0f};            // Initial velocity (m/s)
    float particle_radius{0.05f};             // Particle radius (m)
    float dt{1.0f / 60.0f};                   // Timestep (s)
    int substeps{8};                          // Substeps per frame
    float duration{1.0f};                     // Duration (s)

    [[nodiscard]] bool is_valid(std::string& error_message) const;
};

struct GravityDataRow {
    double time{0.0};
    float x{0.0f}, y{0.0f}, vx{0.0f}, vy{0.0f};
    float x_exact{0.0f}, y_exact{0.0f}, vx_exact{0.0f}, vy_exact{0.0f};
    float pos_error{0.0f}, vel_error{0.0f};
    float kinetic_energy{0.0f}, potential_energy{0.0f}, total_energy{0.0f};
};

struct GravityAnalyticalSummary {
    float max_pos_error{0.0f};
    float max_vel_error{0.0f};
    float final_pos_error{0.0f};
    float final_vel_error{0.0f};
    float energy_drift_ratio{0.0f};
};

class GravityFreeMotionExperiment : public IExperiment {
public:
    explicit GravityFreeMotionExperiment(const GravityParams& params = GravityParams());

    [[nodiscard]] const ExperimentMetadata& get_metadata() const override { return metadata_; }
    bool initialize() override;
    void step() override;
    [[nodiscard]] bool is_completed() const override;
    [[nodiscard]] double current_time() const override { return current_time_; }
    void run() override;
    bool export_csv(const std::string& filepath) const override;
    [[nodiscard]] const sim_core::SimulationDiagnostics& get_diagnostics() const override { return latest_diag_; }

    [[nodiscard]] const GravityParams& get_params() const { return params_; }
    [[nodiscard]] const GravityAnalyticalSummary& get_summary() const { return summary_; }
    [[nodiscard]] const std::vector<GravityDataRow>& get_data() const { return data_; }

private:
    GravityParams params_;
    ExperimentMetadata metadata_;
    sim_core::CpuReferenceSolver solver_;
    std::vector<GravityDataRow> data_;
    GravityAnalyticalSummary summary_;
    sim_core::SimulationDiagnostics latest_diag_;
    double current_time_{0.0};
    int current_frame_{0};
    int total_frames_{0};
    float initial_total_energy_{0.0f};

    void init_metadata();
    void record_frame();
};

} // namespace lab_experiments
