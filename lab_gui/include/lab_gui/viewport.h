#pragma once

#include "lab_gui/app_state.h"
#include <imgui.h>

namespace lab_gui {

class ViewportPanel {
public:
    ViewportPanel() = default;

    /**
     * \brief Render the 2D Simulation Viewport into the current ImGui window.
     */
    void render(AppState& state);

private:
    ImVec2 world_to_screen(const sim_core::Vec2& world_pos,
                           const ImVec2& canvas_center,
                           const sim_core::Vec2& cam_offset,
                           float zoom) const;

    sim_core::Vec2 screen_to_world(const ImVec2& screen_pos,
                                   const ImVec2& canvas_center,
                                   const sim_core::Vec2& cam_offset,
                                   float zoom) const;

    void draw_metric_grid(ImDrawList* draw_list,
                          const ImVec2& canvas_min,
                          const ImVec2& canvas_max,
                          const ImVec2& canvas_center,
                          const sim_core::Vec2& cam_offset,
                          float zoom,
                          float spacing_m) const;

    void draw_boundary(ImDrawList* draw_list,
                       const ImVec2& canvas_center,
                       const sim_core::Vec2& cam_offset,
                       float zoom,
                       const sim_core::CircularBoundary& boundary) const;

    void draw_analytical_reference(ImDrawList* draw_list,
                                  const ImVec2& canvas_center,
                                  const sim_core::Vec2& cam_offset,
                                  float zoom,
                                  const AppState& state) const;

    void draw_contact_normals(ImDrawList* draw_list,
                              const ImVec2& canvas_center,
                              const sim_core::Vec2& cam_offset,
                              float zoom,
                              const AppState& state) const;

    void draw_particles(ImDrawList* draw_list,
                        const ImVec2& canvas_center,
                        const sim_core::Vec2& cam_offset,
                        float zoom,
                        const AppState& state) const;

    void draw_velocity_vector(ImDrawList* draw_list,
                              const ImVec2& origin_screen,
                              const sim_core::Vec2& vel,
                              float zoom,
                              float scale) const;

    ImU32 get_particle_color(const sim_core::Particle& p,
                             const AppState& state,
                             int contact_count) const;

    bool is_dragging_camera_{false};
    ImVec2 last_mouse_pos_{0.0f, 0.0f};
};

} // namespace lab_gui
