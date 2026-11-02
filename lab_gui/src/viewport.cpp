#include "lab_gui/viewport.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace lab_gui {

ImVec2 ViewportPanel::world_to_screen(const sim_core::Vec2& world_pos,
                                      const ImVec2& canvas_center,
                                      const sim_core::Vec2& cam_offset,
                                      float zoom) const {
    const float sx = canvas_center.x + (world_pos.x - cam_offset.x) * zoom;
    const float sy = canvas_center.y - (world_pos.y - cam_offset.y) * zoom; // y-up
    return ImVec2(sx, sy);
}

sim_core::Vec2 ViewportPanel::screen_to_world(const ImVec2& screen_pos,
                                              const ImVec2& canvas_center,
                                              const sim_core::Vec2& cam_offset,
                                              float zoom) const {
    const float wx = cam_offset.x + (screen_pos.x - canvas_center.x) / zoom;
    const float wy = cam_offset.y - (screen_pos.y - canvas_center.y) / zoom;
    return sim_core::Vec2(wx, wy);
}

void ViewportPanel::render(AppState& state) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("2D Simulation Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    const ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    if (canvas_size.x < 50.0f || canvas_size.y < 50.0f) {
        ImGui::End();
        ImGui::PopStyleVar();
        return;
    }

    const ImVec2 canvas_center(canvas_pos.x + canvas_size.x * 0.5f,
                               canvas_pos.y + canvas_size.y * 0.5f);
    const ImVec2 canvas_min = canvas_pos;
    const ImVec2 canvas_max(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(canvas_min, canvas_max, true);

    // Dark background fill
    draw_list->AddRectFilled(canvas_min, canvas_max, IM_COL32(18, 22, 28, 255));

    // Handle Pan & Zoom User Interaction
    const ImGuiIO& io = ImGui::GetIO();
    const bool is_hovered = ImGui::IsWindowHovered();

    if (is_hovered) {
        // Zoom via mouse wheel (zoom centered on cursor)
        if (io.MouseWheel != 0.0f) {
            const float zoom_factor = (io.MouseWheel > 0.0f) ? 1.15f : (1.0f / 1.15f);
            const sim_core::Vec2 mouse_world_before = screen_to_world(io.MousePos, canvas_center, state.camera_offset, state.camera_zoom);
            state.camera_zoom = std::clamp(state.camera_zoom * zoom_factor, 60.0f, 2500.0f);
            const sim_core::Vec2 mouse_world_after = screen_to_world(io.MousePos, canvas_center, state.camera_offset, state.camera_zoom);
            state.camera_offset.x += (mouse_world_before.x - mouse_world_after.x);
            state.camera_offset.y += (mouse_world_before.y - mouse_world_after.y);
        }

        // Pan via Right Mouse Button or Middle Mouse Button drag
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            const ImVec2 drag_delta = io.MouseDelta;
            state.camera_offset.x -= drag_delta.x / state.camera_zoom;
            state.camera_offset.y += drag_delta.y / state.camera_zoom; // y-up
        }
    }

    // 1. Metric Grid & Coordinate Axes
    if (state.show_grid) {
        draw_metric_grid(draw_list, canvas_min, canvas_max, canvas_center, state.camera_offset, state.camera_zoom, state.grid_spacing);
    }

    // 2. Circular Boundary Container
    if (state.show_boundary) {
        draw_boundary(draw_list, canvas_center, state.camera_offset, state.camera_zoom, state.boundary);
    }

    // 3. Analytical Reference Ghost (Exact theoretical solution)
    if (state.show_analytical_trajectory) {
        draw_analytical_reference(draw_list, canvas_center, state.camera_offset, state.camera_zoom, state);
    }

    // 4. Contact Normal Vectors & Penetration Depths
    if (state.show_contact_normals) {
        draw_contact_normals(draw_list, canvas_center, state.camera_offset, state.camera_zoom, state);
    }

    // 5. Particles
    draw_particles(draw_list, canvas_center, state.camera_offset, state.camera_zoom, state);

    // 6. Viewport HUD Controls Overlay
    {
        const ImVec2 hud_pos(canvas_min.x + 16.0f, canvas_min.y + 16.0f);
        ImGui::SetCursorScreenPos(hud_pos);
        ImGui::BeginGroup();

        char hud_buf[256];
        std::snprintf(hud_buf, sizeof(hud_buf),
                      "Time: %.3f s | Substeps: %d | N: %zu | CFL: %.2f",
                      state.current_time,
                      state.config.substeps,
                      state.solver.particle_count(),
                      state.current_cfl);

        // Semi-transparent background pill for HUD text
        const ImVec2 text_size = ImGui::CalcTextSize(hud_buf);
        draw_list->AddRectFilled(ImVec2(hud_pos.x - 6.0f, hud_pos.y - 4.0f),
                                 ImVec2(hud_pos.x + text_size.x + 6.0f, hud_pos.y + text_size.y + 4.0f),
                                 IM_COL32(10, 14, 20, 200), 4.0f);

        ImU32 cfl_color = IM_COL32(100, 230, 100, 255);
        if (state.current_cfl >= 1.0f) {
            cfl_color = IM_COL32(255, 75, 75, 255);
        } else if (state.current_cfl >= 0.5f) {
            cfl_color = IM_COL32(255, 200, 60, 255);
        }

        draw_list->AddText(hud_pos, cfl_color, hud_buf);

        // Quick Navigation Buttons
        ImGui::SetCursorScreenPos(ImVec2(hud_pos.x, hud_pos.y + 26.0f));
        if (ImGui::SmallButton("Reset View")) {
            state.camera_offset = {0.0f, 0.0f};
            state.camera_zoom = 360.0f;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("+ Zoom")) {
            state.camera_zoom = std::min(state.camera_zoom * 1.25f, 2500.0f);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("- Zoom")) {
            state.camera_zoom = std::max(state.camera_zoom / 1.25f, 60.0f);
        }

        ImGui::EndGroup();
    }

    draw_list->PopClipRect();
    ImGui::End();
    ImGui::PopStyleVar();
}

void ViewportPanel::draw_metric_grid(ImDrawList* draw_list,
                                     const ImVec2& canvas_min,
                                     const ImVec2& canvas_max,
                                     const ImVec2& canvas_center,
                                     const sim_core::Vec2& cam_offset,
                                     float zoom,
                                     float spacing_m) const {
    const sim_core::Vec2 world_min = screen_to_world(canvas_min, canvas_center, cam_offset, zoom);
    const sim_core::Vec2 world_max = screen_to_world(canvas_max, canvas_center, cam_offset, zoom);

    const float start_x = std::floor(world_min.x / spacing_m) * spacing_m;
    const float end_x   = std::ceil(world_max.x / spacing_m) * spacing_m;
    const float start_y = std::floor(world_max.y / spacing_m) * spacing_m;
    const float end_y   = std::ceil(world_min.y / spacing_m) * spacing_m;

    const ImU32 col_grid = IM_COL32(40, 50, 65, 120);
    const ImU32 col_axis = IM_COL32(80, 105, 140, 200);
    const ImU32 col_text = IM_COL32(110, 130, 160, 180);

    // Vertical grid lines
    for (float x = start_x; x <= end_x; x += spacing_m) {
        const float sx = canvas_center.x + (x - cam_offset.x) * zoom;
        if (sx < canvas_min.x || sx > canvas_max.x) continue;

        const bool is_axis = (std::abs(x) < 1e-4f);
        draw_list->AddLine(ImVec2(sx, canvas_min.y), ImVec2(sx, canvas_max.y),
                           is_axis ? col_axis : col_grid, is_axis ? 1.5f : 1.0f);

        char label[32];
        std::snprintf(label, sizeof(label), "%.1fm", x);
        draw_list->AddText(ImVec2(sx + 3.0f, canvas_max.y - 18.0f), col_text, label);
    }

    // Horizontal grid lines
    for (float y = start_y; y <= end_y; y += spacing_m) {
        const float sy = canvas_center.y - (y - cam_offset.y) * zoom;
        if (sy < canvas_min.y || sy > canvas_max.y) continue;

        const bool is_axis = (std::abs(y) < 1e-4f);
        draw_list->AddLine(ImVec2(canvas_min.x, sy), ImVec2(canvas_max.x, sy),
                           is_axis ? col_axis : col_grid, is_axis ? 1.5f : 1.0f);

        char label[32];
        std::snprintf(label, sizeof(label), "%.1fm", y);
        draw_list->AddText(ImVec2(canvas_min.x + 4.0f, sy - 15.0f), col_text, label);
    }
}

void ViewportPanel::draw_boundary(ImDrawList* draw_list,
                                  const ImVec2& canvas_center,
                                  const sim_core::Vec2& cam_offset,
                                  float zoom,
                                  const sim_core::CircularBoundary& boundary) const {
    const ImVec2 b_center = world_to_screen(boundary.center, canvas_center, cam_offset, zoom);
    const float b_radius_px = boundary.radius * zoom;

    // Faint boundary interior fill
    draw_list->AddCircleFilled(b_center, b_radius_px, IM_COL32(25, 32, 42, 100), 96);

    // Solid outer perimeter
    draw_list->AddCircle(b_center, b_radius_px, IM_COL32(110, 140, 180, 220), 96, 2.0f);

    // Radial tick marks every 30 degrees
    constexpr float PI = 3.14159265358979323846f;
    for (int deg = 0; deg < 360; deg += 30) {
        const float rad = static_cast<float>(deg) * (PI / 180.0f);
        const float cos_a = std::cos(rad);
        const float sin_a = std::sin(rad);

        const ImVec2 p_inner(b_center.x + cos_a * (b_radius_px - 6.0f),
                             b_center.y - sin_a * (b_radius_px - 6.0f));
        const ImVec2 p_outer(b_center.x + cos_a * (b_radius_px + 6.0f),
                             b_center.y - sin_a * (b_radius_px + 6.0f));

        draw_list->AddLine(p_inner, p_outer, IM_COL32(110, 140, 180, 180), 1.0f);
    }
}

void ViewportPanel::draw_analytical_reference(ImDrawList* draw_list,
                                             const ImVec2& canvas_center,
                                             const sim_core::Vec2& cam_offset,
                                             float zoom,
                                             const AppState& state) const {
    if (state.active_experiment == ExperimentType::GravityBallistic) {
        // Analytical trajectory: x(t) = x0 + v0*t + 0.5*g*t^2
        constexpr float T_MAX = 0.8f;
        constexpr int SEGMENTS = 64;
        const sim_core::Vec2 x0 = state.initial_pos1;
        const sim_core::Vec2 v0 = state.initial_vel1;
        const sim_core::Vec2 g  = state.config.gravity;

        ImVec2 prev_pt;
        for (int i = 0; i <= SEGMENTS; ++i) {
            const float t = (static_cast<float>(i) / static_cast<float>(SEGMENTS)) * T_MAX;
            const sim_core::Vec2 xt = x0 + v0 * t + 0.5f * g * (t * t);
            const ImVec2 pt = world_to_screen(xt, canvas_center, cam_offset, zoom);

            if (i > 0) {
                // Dashed cyan line for analytical truth
                if (i % 2 == 0) {
                    draw_list->AddLine(prev_pt, pt, IM_COL32(70, 220, 240, 160), 1.5f);
                }
            }
            prev_pt = pt;
        }

        // Label on analytical path
        const sim_core::Vec2 peak_world = x0 + v0 * 0.25f + 0.5f * g * (0.25f * 0.25f);
        const ImVec2 peak_screen = world_to_screen(peak_world, canvas_center, cam_offset, zoom);
        draw_list->AddText(ImVec2(peak_screen.x + 8.0f, peak_screen.y - 12.0f),
                           IM_COL32(70, 220, 240, 200), "x_exact(t)");
    }
}

void ViewportPanel::draw_contact_normals(ImDrawList* draw_list,
                                         const ImVec2& canvas_center,
                                         const sim_core::Vec2& cam_offset,
                                         float zoom,
                                         const AppState& state) const {
    const auto& particles = state.get_active_particles();
    const size_t n = particles.size();

    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const sim_core::Vec2 delta = particles[i].position - particles[j].position;
            const float dist = delta.length();
            const float r_sum = particles[i].radius + particles[j].radius;

            if (dist < r_sum) {
                const float penetration = r_sum - dist;
                const ImVec2 pi_screen = world_to_screen(particles[i].position, canvas_center, cam_offset, zoom);
                const ImVec2 pj_screen = world_to_screen(particles[j].position, canvas_center, cam_offset, zoom);

                // Draw contact segment line in bright coral
                draw_list->AddLine(pj_screen, pi_screen, IM_COL32(255, 60, 60, 240), 2.5f);

                // Contact midpoint
                const ImVec2 mid((pi_screen.x + pj_screen.x) * 0.5f, (pi_screen.y + pj_screen.y) * 0.5f);
                draw_list->AddCircleFilled(mid, 3.5f, IM_COL32(255, 230, 80, 255));

                char pen_buf[32];
                std::snprintf(pen_buf, sizeof(pen_buf), "d=%.2fmm", penetration * 1000.0f);
                draw_list->AddText(ImVec2(mid.x + 6.0f, mid.y - 6.0f), IM_COL32(255, 100, 100, 240), pen_buf);
            }
        }
    }
}

void ViewportPanel::draw_particles(ImDrawList* draw_list,
                                   const ImVec2& canvas_center,
                                   const sim_core::Vec2& cam_offset,
                                   float zoom,
                                   const AppState& state) const {
    const auto& particles = state.get_active_particles();
    const size_t n = particles.size();

    // Determine contact counts for color coding
    std::vector<int> contacts(n, 0);
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const float d = (particles[i].position - particles[j].position).length();
            if (d < particles[i].radius + particles[j].radius) {
                contacts[i]++;
                contacts[j]++;
            }
        }
    }

    for (size_t i = 0; i < n; ++i) {
        const auto& p = particles[i];
        const ImVec2 screen_center = world_to_screen(p.position, canvas_center, cam_offset, zoom);
        const float screen_r = p.radius * zoom;

        const ImU32 fill_color = get_particle_color(p, state, contacts[i]);
        const ImU32 border_color = (contacts[i] > 0) ? IM_COL32(255, 60, 60, 255) : IM_COL32(230, 240, 255, 240);

        // Particle interior disk
        draw_list->AddCircleFilled(screen_center, screen_r, fill_color, 48);

        // Anti-aliased outer ring
        draw_list->AddCircle(screen_center, screen_r, border_color, 48, (contacts[i] > 0) ? 2.5f : 1.5f);

        // Center dot
        draw_list->AddCircleFilled(screen_center, 2.0f, IM_COL32(255, 255, 255, 200));

        // Particle ID or mass label if reasonably large on screen
        if (screen_r > 16.0f && n <= 10) {
            char id_buf[32];
            std::snprintf(id_buf, sizeof(id_buf), "P%u (%.1fkg)", p.id, p.mass);
            const ImVec2 label_size = ImGui::CalcTextSize(id_buf);
            draw_list->AddText(ImVec2(screen_center.x - label_size.x * 0.5f, screen_center.y - label_size.y * 0.5f),
                               IM_COL32(255, 255, 255, 220), id_buf);
        }

        // Velocity Vector Arrow
        if (state.show_velocity_vectors) {
            draw_velocity_vector(draw_list, screen_center, p.velocity, zoom, state.velocity_vector_scale);
        }
    }
}

void ViewportPanel::draw_velocity_vector(ImDrawList* draw_list,
                                        const ImVec2& origin_screen,
                                        const sim_core::Vec2& vel,
                                        float zoom,
                                        float scale) const {
    const float speed = vel.length();
    if (speed < 0.01f) return;

    // Vector tip in screen space (y-inverted)
    const ImVec2 tip(origin_screen.x + vel.x * scale * zoom,
                     origin_screen.y - vel.y * scale * zoom);

    const ImU32 col_vector = IM_COL32(255, 220, 50, 240);
    draw_list->AddLine(origin_screen, tip, col_vector, 2.0f);

    // Arrowhead calculations
    const float dx = tip.x - origin_screen.x;
    const float dy = tip.y - origin_screen.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len > 8.0f) {
        const float udx = dx / len;
        const float udy = dy / len;
        constexpr float HEAD_LEN = 8.0f;
        constexpr float HEAD_WIDTH = 4.0f;

        const ImVec2 p1 = tip;
        const ImVec2 p2(tip.x - udx * HEAD_LEN - udy * HEAD_WIDTH,
                        tip.y - udy * HEAD_LEN + udx * HEAD_WIDTH);
        const ImVec2 p3(tip.x - udx * HEAD_LEN + udy * HEAD_WIDTH,
                        tip.y - udy * HEAD_LEN - udx * HEAD_WIDTH);

        draw_list->AddTriangleFilled(p1, p2, p3, col_vector);
    }
}

ImU32 ViewportPanel::get_particle_color(const sim_core::Particle& p,
                                       const AppState& state,
                                       int contact_count) const {
    switch (state.color_mode) {
        case ParticleColorMode::VelocityHeatmap: {
            const float speed = p.velocity.length();
            constexpr float MAX_SPEED = 3.5f;
            const float t = std::clamp(speed / MAX_SPEED, 0.0f, 1.0f);

            // Interpolate Blue -> Cyan -> Green -> Yellow -> Red
            uint8_t r, g, b;
            if (t < 0.25f) {
                const float s = t / 0.25f;
                r = 0;
                g = static_cast<uint8_t>(255 * s);
                b = 255;
            } else if (t < 0.5f) {
                const float s = (t - 0.25f) / 0.25f;
                r = 0;
                g = 255;
                b = static_cast<uint8_t>(255 * (1.0f - s));
            } else if (t < 0.75f) {
                const float s = (t - 0.5f) / 0.25f;
                r = static_cast<uint8_t>(255 * s);
                g = 255;
                b = 0;
            } else {
                const float s = (t - 0.75f) / 0.25f;
                r = 255;
                g = static_cast<uint8_t>(255 * (1.0f - s));
                b = 0;
            }
            return IM_COL32(r, g, b, 230);
        }

        case ParticleColorMode::ContactState: {
            if (contact_count == 0) {
                return IM_COL32(40, 180, 100, 220); // Free: Green
            } else if (contact_count == 1) {
                return IM_COL32(240, 150, 40, 230); // 1 contact: Orange
            } else {
                return IM_COL32(230, 50, 50, 240);  // 2+ contacts: Clamped Red
            }
        }

        case ParticleColorMode::MassSpectrum: {
            // Dark to bright cyan based on mass
            const float norm_mass = std::clamp(p.mass / 15.0f, 0.15f, 1.0f);
            const uint8_t val = static_cast<uint8_t>(255 * norm_mass);
            return IM_COL32(val / 2, val, 255, 230);
        }

        case ParticleColorMode::Material:
        default: {
            // Deterministic palette based on particle ID
            static const ImU32 PALETTE[] = {
                IM_COL32(52, 152, 219, 230),  // Light blue
                IM_COL32(231, 76, 60, 230),   // Coral red
                IM_COL32(46, 204, 113, 230),  // Emerald green
                IM_COL32(241, 196, 15, 230),  // Sun yellow
                IM_COL32(155, 89, 182, 230),  // Amethyst purple
                IM_COL32(230, 126, 34, 230),  // Carrot orange
                IM_COL32(26, 188, 156, 230),  // Turquoise
            };
            return PALETTE[p.id % 7];
        }
    }
}

} // namespace lab_gui
