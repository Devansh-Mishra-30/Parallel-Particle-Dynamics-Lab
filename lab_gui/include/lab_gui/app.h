#pragma once

#include "lab_gui/app_state.h"
#include "lab_gui/controls_panel.h"
#include "lab_gui/diagnostics_panel.h"
#include "lab_gui/viewport.h"

#include <imgui.h>

namespace lab_gui {

class LabApp {
public:
    LabApp();
    ~LabApp() = default;

    /**
     * \brief Initialize default theme, layout, and starting experiment.
     */
    void initialize();

    /**
     * \brief Advances simulation physics and updates diagnostic rolling buffers.
     */
    void update(float delta_wall_seconds);

    /**
     * \brief Renders all UI panels across the 3-panel layout.
     */
    void render(int window_width, int window_height);

    [[nodiscard]] AppState& get_state() { return state_; }
    [[nodiscard]] const AppState& get_state() const { return state_; }

private:
    void setup_scientific_theme();

    AppState state_;
    ControlsPanel controls_panel_;
    ViewportPanel viewport_panel_;
    DiagnosticsPanel diagnostics_panel_;
};

} // namespace lab_gui
