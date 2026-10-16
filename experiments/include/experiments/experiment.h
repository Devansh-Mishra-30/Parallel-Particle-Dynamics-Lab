#pragma once

#include "sim_core/types.h"
#include <string>
#include <vector>

namespace lab_experiments {

/**
 * \brief Metadata describing the scientific context, governing equations,
 *        and educational objectives of an experiment.
 */
struct ExperimentMetadata {
    std::string id;
    std::string title;
    std::string description;
    std::string physical_objective;
    std::string governing_equations;
    std::vector<std::string> assumptions;
    std::vector<std::string> verification_metrics;
    std::vector<std::string> numerical_limitations;
};

/**
 * \brief Common interface for reproducible numerical experiments.
 */
class IExperiment {
public:
    virtual ~IExperiment() = default;

    [[nodiscard]] virtual const ExperimentMetadata& get_metadata() const = 0;
    virtual bool initialize() = 0;
    virtual void step() = 0;
    [[nodiscard]] virtual bool is_completed() const = 0;
    [[nodiscard]] virtual double current_time() const = 0;
    virtual void run() = 0;
    virtual bool export_csv(const std::string& filepath) const = 0;
    [[nodiscard]] virtual const sim_core::SimulationDiagnostics& get_diagnostics() const = 0;
};

} // namespace lab_experiments
