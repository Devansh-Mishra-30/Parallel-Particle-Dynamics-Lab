#pragma once

#include "lab_gui/app_state.h"
#include <imgui.h>

namespace lab_gui {

class ControlsPanel {
public:
    ControlsPanel() = default;

    /**
     * \brief Render the Controls Panel (Left Panel) in the current ImGui window.
     */
    void render(AppState& state);

private:
    void render_experiment_selector(AppState& state);
    void render_physical_parameters(AppState& state);
    void render_numerical_controls(AppState& state);
    void render_playback_controls(AppState& state);
    void render_visualization_options(AppState& state);
};

} // namespace lab_gui
