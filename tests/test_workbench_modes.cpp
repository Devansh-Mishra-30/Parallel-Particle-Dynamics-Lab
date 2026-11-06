#include "lab_gui/workbench_modes.h"

#include <cstring>
#include <iostream>

int main() {
    using namespace lab_gui;
    static_assert(investigation_count == 3, "Expected three investigations");
    static_assert(scenario_count == 5, "Expected five physics scenarios");

    int count = 0;
    for (int mode = 0; mode < investigation_count; ++mode) {
        for (int scenario = 0; scenario < scenario_count; ++scenario) {
            const auto m = static_cast<InvestigationMode>(mode);
            const auto s = static_cast<PhysicsScenario>(scenario);
            const auto d = describe_investigation(m, s);
            const bool valid = std::strlen(investigation_name(m)) > 0 &&
                               std::strlen(scenario_name(s)) > 0 &&
                               std::strlen(d.title) > 0 &&
                               std::strlen(d.question) > 0 &&
                               std::strlen(d.evidence) > 0 &&
                               std::strlen(d.limitation) > 0;
            if (!valid) {
                std::cerr << "Missing description at mode " << mode
                          << ", scenario " << scenario << "\n";
                return 1;
            }
            if (d.has_builtin_convergence_sweep &&
                !(m == InvestigationMode::NumericalAccuracy && s == PhysicsScenario::Gravity)) {
                std::cerr << "Convergence sweep exposed on unsupported combination\n";
                return 1;
            }
            ++count;
        }
    }
    if (count != 15 ||
        !describe_investigation(InvestigationMode::PhysicsVerification,
                                 PhysicsScenario::Collision).has_analytical_reference ||
        describe_investigation(InvestigationMode::PhysicsVerification,
                                PhysicsScenario::DenseContacts).has_analytical_reference) {
        std::cerr << "Invalid mode/scenario reference metadata\n";
        return 1;
    }
    std::cout << "Stage 2: " << count << " investigation/scenario combinations verified.\n";
    return 0;
}
