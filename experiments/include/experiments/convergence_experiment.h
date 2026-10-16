#pragma once

#include "experiments/experiment.h"
#include <string>
#include <vector>

namespace lab_experiments {

enum class ConvergenceScenario {
    BallisticGravity,
    TwoParticleContact
};

struct ConvergenceParams {
    std::vector<float> timesteps{0.04f, 0.02f, 0.01f, 0.005f, 0.0025f};
    float duration{0.2f};
    ConvergenceScenario scenario{ConvergenceScenario::BallisticGravity};

    [[nodiscard]] bool is_valid(std::string& error_message) const;
};

struct ConvergenceLevelResult {
    float dt{0.0f};
    int total_steps{0};
    float l2_error{0.0f};
    float linf_error{0.0f};
    float error_reduction_ratio{0.0f};
    float empirical_order{0.0f};
    double compute_time_ms{0.0};
};

class TimestepConvergenceExperiment : public IExperiment {
public:
    explicit TimestepConvergenceExperiment(const ConvergenceParams& params = ConvergenceParams());

    [[nodiscard]] const ExperimentMetadata& get_metadata() const override { return metadata_; }
    bool initialize() override;
    void step() override;
    [[nodiscard]] bool is_completed() const override { return completed_; }
    [[nodiscard]] double current_time() const override { return current_time_; }
    void run() override;
    bool export_csv(const std::string& filepath) const override;
    [[nodiscard]] const sim_core::SimulationDiagnostics& get_diagnostics() const override { return latest_diag_; }

    [[nodiscard]] const ConvergenceParams& get_params() const { return params_; }
    [[nodiscard]] const std::vector<ConvergenceLevelResult>& get_results() const { return results_; }

private:
    ConvergenceParams params_;
    ExperimentMetadata metadata_;
    std::vector<ConvergenceLevelResult> results_;
    sim_core::SimulationDiagnostics latest_diag_;
    double current_time_{0.0};
    bool completed_{false};

    void init_metadata();
};

} // namespace lab_experiments
