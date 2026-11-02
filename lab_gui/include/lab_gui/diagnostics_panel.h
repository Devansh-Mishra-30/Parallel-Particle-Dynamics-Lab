#pragma once

#include "lab_gui/app_state.h"
#include <imgui.h>
#include <implot.h>

namespace lab_gui {

class DiagnosticsPanel {
public:
    DiagnosticsPanel() = default;

    /**
     * \brief Render the Diagnostics Dashboard (Right Panel) in the current ImGui window.
     */
    void render(AppState& state);

    /**
     * \brief Execute multi-timestep convergence study (dt, dt/2, dt/4, dt/8)
     */
    static void execute_convergence_sweep(AppState& state);

private:
    void render_energy_plot(AppState& state);
    void render_momentum_monitor(AppState& state);
    void render_analytical_verification(AppState& state);
    void render_stability_sentinel(AppState& state);
    void render_educational_modules(AppState& state);
};

} // namespace lab_gui
