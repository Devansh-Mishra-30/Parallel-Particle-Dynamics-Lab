#include <limits>
#include "lab_gui/app.h"
#include "lab_gui/ballistic_reference.h"
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
    ImGuiStyle& style = ImGui::GetStyle();
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 9.0f);
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.WindowRounding = 7.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.ScrollbarRounding = 5.0f;
    // Default starting scenario: Two-Particle Impact
    ExperimentPresets::load_workbench_scenario(state_, state_.physics_scenario);
    state_.show_metrics_window = true; // Physics mode opens scientific diagnostics.
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
            sim_core::Particle ballistic_before;
            bool have_ballistic_before = false;
            if (state_.active_experiment == ExperimentType::GravityBallistic) {
                const auto& before = state_.get_active_particles();
                if (before.size() == 1) {
                    ballistic_before = before.front();
                    have_ballistic_before = true;
                }
            }
            state_.step_active_solver();
            state_.current_time += state_.config.dt;
            state_.total_substeps += state_.config.substeps;
            state_.frame_index++;
            // Stage 4: keep a sampled numerical path separately from the dashed
            // free-flight reference. It is bounded to avoid unbounded memory growth.
            if (state_.physics_scenario == PhysicsScenario::Gravity) {
                const auto& pts = state_.get_active_particles();
                if (!pts.empty()) {
                    if (state_.simulated_trajectory.empty())
                        state_.simulated_trajectory.push_back(state_.initial_pos1);
                    state_.simulated_trajectory.push_back(pts.front().position);
                    if (state_.simulated_trajectory.size() > 1200)
                        state_.simulated_trajectory.erase(state_.simulated_trajectory.begin());
                }
            }

            // Analytical ballistic error tracking & boundary collision sentinel
            if (state_.active_experiment == ExperimentType::GravityBallistic) {
                const auto& particles = state_.get_active_particles();
                if (!particles.empty()) {
                    const float t = static_cast<float>(state_.current_time);
                    const sim_core::Vec2 x_exact = state_.initial_pos1 + state_.initial_vel1 * t + 0.5f * state_.config.gravity * (t * t);
                    const sim_core::Vec2 v_exact = state_.initial_vel1 + state_.config.gravity * t;

                    if (!state_.wall_contact_occurred &&
                        have_ballistic_before &&
                        boundary_impulse_occurred(ballistic_before, particles[0], state_.config)) {
                        state_.wall_contact_occurred = true;
                        state_.wall_contact_time = state_.current_time;
                    }
                    // DO NOT interpret post-wall deviation from free flight as
                    // numerical integration error. The reference model has changed.
                    if (!state_.wall_contact_occurred) {
                        state_.current_pos_error = (particles[0].position - x_exact).length();
                        state_.current_vel_error = (particles[0].velocity - v_exact).length();
                    } else {
                        state_.current_pos_error = std::numeric_limits<float>::quiet_NaN();
                        state_.current_vel_error = std::numeric_limits<float>::quiet_NaN();
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
            row.ballistic_freeflight_valid =
                (state_.physics_scenario == PhysicsScenario::Gravity && !state_.wall_contact_occurred);
            state_.recorded_rows.push_back(row);
        }
    } else {
        // Paused: compute diagnostics on static snapshot
#if HAS_CUDA_BACKEND
        if (state_.active_backend == SolverBackend::CudaGpu && state_.cuda_initialized) {
            state_.current_diagnostics = state_.cuda_solver.compute_diagnostics();
        } else
#endif
        if (state_.active_backend == SolverBackend::OpenMpParallel) {
            state_.current_diagnostics = state_.omp_solver.compute_diagnostics();
        } else {
            state_.current_diagnostics = state_.solver.compute_diagnostics();
        }
    }
}

void LabApp::render(int window_width, int window_height) {
    // Stage 1: one root window occupying the actual ImGui main viewport.
    // Use WorkPos/WorkSize, NOT cached window dimensions or FirstUseEver sizes.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 9.0f));
    const ImGuiWindowFlags root_flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##ParticleDynamicsWorkspace", nullptr, root_flags);
    ImGui::PopStyleVar(3);

    // Stage 2: independent investigation and physical-scenario selectors.
    ImGui::TextColored(ImVec4(0.38f, 0.80f, 1.00f, 1.0f),
                       "PARALLEL PARTICLE DYNAMICS LAB");
    ImGui::SameLine();
    ImGui::TextDisabled("/  Numerical Verification & Performance Workbench");

    const char* investigation_items[] = {
        "01  Physics verification", "02  Numerical accuracy", "03  HPC performance"
    };
    const char* scenario_items[] = {
        "01  Two-particle collision", "02  Gravity and ballistics",
        "03  Dense contacts", "04  Particle cloud", "05  Custom sandbox"
    };
    int mode_index = static_cast<int>(state_.investigation_mode);
    int scenario_index = static_cast<int>(state_.physics_scenario);
    const bool narrow_header = ImGui::GetContentRegionAvail().x < 870.0f;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Investigation");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(narrow_header ? 230.0f : 250.0f);
    if (ImGui::Combo("##InvestigationMode", &mode_index,
                     investigation_items, IM_ARRAYSIZE(investigation_items))) {
        state_.investigation_mode = static_cast<InvestigationMode>(mode_index);
        state_.show_metrics_window =
            (state_.investigation_mode != InvestigationMode::HpcPerformance);
        state_.backend_comparison.executed = false;
    }
    if (!narrow_header) ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Scenario");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(narrow_header ? 230.0f : 240.0f);
    if (ImGui::Combo("##PhysicsScenario", &scenario_index,
                     scenario_items, IM_ARRAYSIZE(scenario_items))) {
        const auto chosen = static_cast<PhysicsScenario>(scenario_index);
        ExperimentPresets::load_workbench_scenario(state_, chosen);
        // New scenario means new defaults; opt-in editing starts disabled.
        state_.custom_settings_enabled = false;
        state_.show_toast("Loaded default setup for selected physics scenario");
    }
    if (!narrow_header) ImGui::SameLine();
    ImGui::Checkbox("Custom Settings", &state_.custom_settings_enabled);

    const auto description = describe_investigation(state_.investigation_mode,
                                                     state_.physics_scenario);
    ImGui::TextColored(ImVec4(0.91f, 0.94f, 0.99f, 1.0f), "%s", description.title);
    ImGui::TextWrapped("Question: %s", description.question);
    ImGui::Separator();

    // Reserve one line for a low-noise status footer. The table fills all other space.
    const float footer_height = ImGui::GetFrameHeightWithSpacing() + 7.0f;
    const ImGuiTableFlags table_flags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings;
    if (ImGui::BeginTable("##MainWorkspaceColumns", 3, table_flags,
                          ImVec2(0.0f, -footer_height))) {
        // Compact windows need enough side-panel width for scientific labels.
        // The user can drag the column borders. Widths stretch with the OS window.
        const bool compact_workspace = ImGui::GetContentRegionAvail().x < 1400.0f;
        const float side_weight = compact_workspace ? 0.30f : 0.25f;
        const float simulation_weight = compact_workspace ? 0.40f : 0.50f;
        ImGui::TableSetupColumn("Setup", ImGuiTableColumnFlags_WidthStretch, side_weight);
        ImGui::TableSetupColumn("Simulation", ImGuiTableColumnFlags_WidthStretch, simulation_weight);
        ImGui::TableSetupColumn("Evidence", ImGuiTableColumnFlags_WidthStretch, side_weight);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        controls_panel_.render(state_); // Scrollable child window, not a floating window.

        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(ImVec4(0.38f, 0.80f, 1.00f, 1.0f), "SIMULATION");
        ImGui::SameLine();
        ImGui::TextDisabled("Mouse wheel: zoom   |   Right-drag: pan");
        viewport_panel_.render(state_); // Child fills the remaining central table cell.

        ImGui::TableSetColumnIndex(2);
        // The old fourth metrics window is now an integrated right-hand view.
        // The existing left-side 'Open detailed diagnostics' switch selects it too.
        const float right_width = ImGui::GetContentRegionAvail().x;
        const float half_width = (right_width > 16.0f)
            ? (right_width - ImGui::GetStyle().ItemSpacing.x) * 0.5f : 90.0f;
        if (ImGui::Button("PERFORMANCE", ImVec2(half_width, 0.0f))) {
            state_.show_metrics_window = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("DIAGNOSTICS", ImVec2(half_width, 0.0f))) {
            state_.show_metrics_window = true;
        }
        if (state_.show_metrics_window) {
            diagnostics_panel_.render(state_);
        } else {
            controls_panel_.render_performance(state_);
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextDisabled("%s  |  N = %zu  |  Simulation time %.3f s  |  Substeps %d",
                        state_.is_paused ? "PAUSED" : "RUNNING",
                        state_.get_active_particle_count(), state_.current_time,
                        state_.config.substeps);
    ImGui::End();

    // Handle Pending Screenshot Request
    if (!state_.pending_screenshot_path.empty()) {
        ScreenshotUtil::capture_framebuffer_png(state_.pending_screenshot_path, window_width, window_height);
        state_.show_toast("Screenshot saved: " + state_.pending_screenshot_path);
        state_.pending_screenshot_path.clear();
    }
}

} // namespace lab_gui
