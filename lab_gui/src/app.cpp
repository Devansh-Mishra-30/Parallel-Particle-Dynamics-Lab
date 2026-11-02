#include "lab_gui/app.h"
#include "lab_gui/experiment_presets.h"
#include "lab_gui/screenshot_util.h"

#include <algorithm>
#include <cmath>

namespace lab_gui {

LabApp::LabApp() {
    initialize();
}

void LabApp::initialize() {
    setup_scientific_theme();
    // Default starting scenario: Two-Particle Impact
    ExperimentPresets::load_collision(state_);
}

void LabApp::setup_scientific_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.5f;
    style.PopupBorderSize = 1.0f;

    // Dark slate scientific palette
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.94f, 0.96f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ImVec4(0.55f, 0.60f, 0.68f, 1.00f);
    colors[ImGuiCol_WindowBg]              = ImVec4(0.11f, 0.13f, 0.17f, 1.00f);
    colors[ImGuiCol_ChildBg]               = ImVec4(0.09f, 0.11f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg]               = ImVec4(0.12f, 0.15f, 0.20f, 0.98f);
    colors[ImGuiCol_Border]                = ImVec4(0.24f, 0.28f, 0.36f, 0.85f);
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = ImVec4(0.16f, 0.20f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.22f, 0.27f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.28f, 0.34f, 0.44f, 1.00f);
    colors[ImGuiCol_TitleBg]               = ImVec4(0.14f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBgActive]         = ImVec4(0.18f, 0.22f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.10f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_MenuBarBg]             = ImVec4(0.14f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.08f, 0.10f, 0.12f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.28f, 0.34f, 0.44f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.35f, 0.42f, 0.54f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.42f, 0.50f, 0.64f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(0.35f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = ImVec4(0.35f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.50f, 0.85f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                = ImVec4(0.20f, 0.25f, 0.33f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.35f, 0.46f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.35f, 0.44f, 0.58f, 1.00f);
    colors[ImGuiCol_Header]                = ImVec4(0.20f, 0.25f, 0.33f, 1.00f);
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.26f, 0.32f, 0.42f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.32f, 0.40f, 0.52f, 1.00f);
    colors[ImGuiCol_Separator]             = ImVec4(0.22f, 0.26f, 0.33f, 0.80f);
    colors[ImGuiCol_SeparatorHovered]      = ImVec4(0.35f, 0.75f, 1.00f, 0.80f);
    colors[ImGuiCol_SeparatorActive]       = ImVec4(0.35f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.24f, 0.30f, 0.38f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.35f, 0.75f, 1.00f, 0.70f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.35f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_Tab]                   = ImVec4(0.14f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.26f, 0.33f, 0.44f, 1.00f);
    colors[ImGuiCol_TabActive]             = ImVec4(0.22f, 0.28f, 0.38f, 1.00f);
}

void LabApp::update(float delta_wall_seconds) {
    if (state_.toast_timer > 0.0f) {
        state_.toast_timer -= delta_wall_seconds;
    }

    if (!state_.is_paused || state_.step_single_frame) {
        int steps_to_run = 0;
        if (state_.step_single_frame) {
            steps_to_run = 1;
            state_.step_single_frame = false;
            state_.sim_accumulator = 0.0f;
        } else {
            // Nominal frame time if delta_wall_seconds is 0 or abnormal
            const float dt_wall = (delta_wall_seconds > 0.001f && delta_wall_seconds < 0.2f) ? delta_wall_seconds : (1.0f / 60.0f);
            state_.sim_accumulator += dt_wall * state_.sim_speed;

            // Prevent accumulator spiral of death if frame freezes
            const float max_accum = state_.config.dt * 5.0f;
            if (state_.sim_accumulator > max_accum) {
                state_.sim_accumulator = max_accum;
            }

            while (state_.sim_accumulator >= state_.config.dt && steps_to_run < 5) {
                steps_to_run++;
                state_.sim_accumulator -= state_.config.dt;
            }
        }

        for (int s = 0; s < steps_to_run; ++s) {
            state_.step_active_solver();
            state_.current_time += state_.config.dt;
            state_.total_substeps += state_.config.substeps;
            state_.frame_index++;

            // Analytical ballistic error tracking & boundary collision sentinel
            if (state_.active_experiment == ExperimentType::GravityBallistic) {
                const auto& particles = state_.get_active_particles();
                if (!particles.empty()) {
                    const float t = static_cast<float>(state_.current_time);
                    const sim_core::Vec2 x_exact = state_.initial_pos1 + state_.initial_vel1 * t + 0.5f * state_.config.gravity * (t * t);
                    const sim_core::Vec2 v_exact = state_.initial_vel1 + state_.config.gravity * t;

                    state_.current_pos_error = (particles[0].position - x_exact).length();
                    state_.current_vel_error = (particles[0].velocity - v_exact).length();

                    const float dist_from_center = (particles[0].position - state_.boundary.center).length();
                    if (!state_.wall_contact_occurred && (dist_from_center + particles[0].radius >= state_.boundary.radius - 1e-3f)) {
                        state_.wall_contact_occurred = true;
                        state_.wall_contact_time = state_.current_time;
                    }
                }
            } else {
                state_.current_pos_error = 0.0f;
                state_.current_vel_error = 0.0f;
            }

            const float current_drift = (state_.current_diagnostics.total_momentum - state_.initial_momentum).length();

            // Append live time-series
            const float t = static_cast<float>(state_.current_time);
            state_.buf_ek.add_point(t, state_.current_diagnostics.total_kinetic_energy);
            state_.buf_ep.add_point(t, state_.current_diagnostics.total_potential_energy);
            state_.buf_etotal.add_point(t, state_.current_diagnostics.total_mechanical_energy);
            state_.buf_drift.add_point(t, current_drift);
            state_.buf_cfl.add_point(t, state_.current_cfl);

            // EXP_01 Collision Event Detection & Post-Impact Capture
            if (state_.active_experiment == ExperimentType::Collision && !state_.collision_has_occurred) {
                const auto& particles = state_.get_active_particles();
                if (particles.size() >= 2) {
                    const sim_core::Vec2 delta_pos = particles[0].position - particles[1].position;
                    const float dist = delta_pos.length();
                    const float r_sum = particles[0].radius + particles[1].radius;
                    const sim_core::Vec2 v_rel = particles[0].velocity - particles[1].velocity;
                    const sim_core::Vec2 n = delta_pos.normalized();
                    const float vn = sim_core::Vec2::dot(v_rel, n);

                    // If particles touched and are now moving apart (vn > 0):
                    if (dist <= r_sum + 0.02f && vn > 0.01f) {
                        state_.collision_has_occurred = true;
                        state_.collision_time = state_.current_time;
                        state_.measured_post_v1 = particles[0].velocity;
                        state_.measured_post_v2 = particles[1].velocity;
                        state_.measured_post_ek = state_.current_diagnostics.total_kinetic_energy;
                        state_.measured_dek = state_.measured_post_ek - state_.measured_initial_ek;
                    }
                }
            }

            // Frame record for scientific CSV export
            CsvExportRow row;
            row.time = state_.current_time;
            row.ek = state_.current_diagnostics.total_kinetic_energy;
            row.ep = state_.current_diagnostics.total_potential_energy;
            row.etotal = state_.current_diagnostics.total_mechanical_energy;
            row.px = state_.current_diagnostics.total_momentum.x;
            row.py = state_.current_diagnostics.total_momentum.y;
            row.momentum_drift = current_drift;
            row.cfl = state_.current_cfl;
            row.active_contacts = state_.current_diagnostics.active_contacts;
            row.step_collision_impulses = state_.current_diagnostics.step_collision_impulses;
            row.max_penetration = state_.current_diagnostics.max_penetration;
            row.pos_error = state_.current_pos_error;
            row.vel_error = state_.current_vel_error;
            state_.recorded_rows.push_back(row);
        }
    } else {
        // Paused: compute diagnostics on static snapshot
        if (state_.active_backend == SolverBackend::OpenMpParallel) {
            state_.current_diagnostics = state_.omp_solver.compute_diagnostics();
        } else {
            state_.current_diagnostics = state_.solver.compute_diagnostics();
        }
    }
}

void LabApp::render(int window_width, int window_height) {
    const float w = static_cast<float>(window_width);
    const float h = static_cast<float>(window_height);

    // Responsive 3-Panel Split
    const float left_w = std::clamp(w * 0.26f, 320.0f, 420.0f);
    const float right_w = std::clamp(w * 0.32f, 380.0f, 500.0f);
    const float center_w = std::max(200.0f, w - left_w - right_w);

    // Left Panel: Controls
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(left_w, h), ImGuiCond_FirstUseEver);
    controls_panel_.render(state_);

    // Center Panel: Viewport
    ImGui::SetNextWindowPos(ImVec2(left_w, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(center_w, h), ImGuiCond_FirstUseEver);
    viewport_panel_.render(state_);

    // Right Panel: Diagnostics
    ImGui::SetNextWindowPos(ImVec2(left_w + center_w, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(right_w, h), ImGuiCond_FirstUseEver);
    diagnostics_panel_.render(state_);

    // Handle Pending Screenshot Request
    if (!state_.pending_screenshot_path.empty()) {
        ScreenshotUtil::capture_framebuffer_png(state_.pending_screenshot_path, window_width, window_height);
        state_.show_toast("Screenshot saved: " + state_.pending_screenshot_path);
        state_.pending_screenshot_path.clear();
    }
}

} // namespace lab_gui
