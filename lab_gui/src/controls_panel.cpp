#include "lab_gui/controls_panel.h"
#include "lab_gui/diagnostics_panel.h"
#include "lab_gui/experiment_presets.h"
#include "lab_gui/workbench_study_runner.h"

#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <implot.h>

namespace lab_gui {

static void render_stage4_evidence(AppState& state) {
    const auto& e = state.stage4_evidence;
    if (!e.valid) return;
    const bool is_scaling = e.kind == EvidenceStudy::BackendScaling;
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.36f, 0.86f, 0.62f, 1.0f),
                       "%s", is_scaling ? "AUTOMATED BACKEND SCALING" : "CONTROLLED CONTACT STUDY");
    ImGui::TextWrapped("%s", e.protocol.c_str());
    if (ImGui::BeginTable(is_scaling ? "##ScalingStudyResults" : "##ContactStudyResults",
                          is_scaling ? 5 : 5,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        if (is_scaling) {
            ImGui::TableSetupColumn("N");
            ImGui::TableSetupColumn("CPU ms/step");
            ImGui::TableSetupColumn("OMP ms/step");
            ImGui::TableSetupColumn("CUDA ms/step");
            ImGui::TableSetupColumn("OMP x");
        } else {
            ImGui::TableSetupColumn("Sub");
            ImGui::TableSetupColumn("K");
            ImGui::TableSetupColumn("End mm");
            ImGui::TableSetupColumn("Peak mm");
            ImGui::TableSetupColumn("CPU ms/step");
        }
        ImGui::TableHeadersRow();
        for (const auto& row : e.rows) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (is_scaling) {
                ImGui::Text("%d", row.particles); ImGui::TableNextColumn();
                ImGui::Text("%.3f", row.cpu_ms_per_step); ImGui::TableNextColumn();
                ImGui::Text("%.3f", row.omp_ms_per_step); ImGui::TableNextColumn();
                if (std::isfinite(row.cuda_ms_per_step)) ImGui::Text("%.3f", row.cuda_ms_per_step);
                else ImGui::TextUnformatted("N/A");
                ImGui::TableNextColumn();
                ImGui::Text("%.2f", safe_speedup(row.cpu_ms_per_step, row.omp_ms_per_step));
            } else {
                ImGui::Text("%d", row.substeps); ImGui::TableNextColumn();
                ImGui::Text("%d", row.contact_iterations); ImGui::TableNextColumn();
                ImGui::Text("%.3f", row.maximum_penetration_m * 1000.0f); ImGui::TableNextColumn();
                ImGui::Text("%.3f", row.peak_penetration_m * 1000.0f); ImGui::TableNextColumn();
                ImGui::Text("%.4f", row.cpu_ms_per_step);
            }
        }
        ImGui::EndTable();
    }
    if (is_scaling && e.rows.size() == 4) {
        double n[4], cpu[4], omp[4], cuda[4];
        bool has_cuda = true;
        for (int i=0;i<4;++i) {
            n[i] = e.rows[static_cast<size_t>(i)].particles;
            cpu[i] = e.rows[static_cast<size_t>(i)].cpu_ms_per_step;
            omp[i] = e.rows[static_cast<size_t>(i)].omp_ms_per_step;
            cuda[i] = e.rows[static_cast<size_t>(i)].cuda_ms_per_step;
            has_cuda = has_cuda && std::isfinite(cuda[i]);
        }
        if (ImPlot::BeginPlot("##Stage4ScalingPlot", ImVec2(-1.0f, 205.0f))) {
            ImPlot::SetupAxes("Particle count N", "Median ms / step", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
            ImPlot::PlotLine("CPU (all-pairs GS)", n, cpu, 4);
            ImPlot::PlotLine("OpenMP (grid Jacobi)", n, omp, 4);
            if (has_cuda) ImPlot::PlotLine("CUDA (grid Jacobi)", n, cuda, 4);
            ImPlot::EndPlot();
        }
        ImGui::TextDisabled("Three samples per N. CUDA N/A on CPU-only builds or unavailable devices.");
        for (const auto& row : e.rows) {
            ImGui::Text("N=%d | OMP max |dx| = %.2e m, |dv| = %.2e m/s",
                        row.particles, row.omp_max_position_difference_m,
                        row.omp_max_velocity_difference_m_s);
            ImGui::Text("       GPU max |dx| = %.2e m, |dv| = %.2e m/s",
                        row.cuda_max_position_difference_m,
                        row.cuda_max_velocity_difference_m_s);
        }
        ImGui::TextWrapped("Zero trajectory difference for collision-free samples is NOT a dense-contact parity test.");
    }
    if (!std::all_of(e.rows.begin(), e.rows.end(), [](const EvidenceRow& r){return r.finite;})) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.3f, 1.0f),
                           "WARNING: At least one study run produced non-finite particle states");
    }
    if (state.omp_threads != e.omp_threads && is_scaling) {
        ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f),
                           "Results used %d OpenMP threads; current setting is %d. Rerun for new configuration.",
                           e.omp_threads, state.omp_threads);
    }
    ImGui::TextWrapped("Interpretation: %s", e.interpretation.c_str());
    if (ImGui::Button("Export study CSV", ImVec2(-1.0f, 0.0f))) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::filesystem::create_directories("output/studies");
        const std::string path = std::string("output/studies/") +
            (is_scaling ? "scaling_" : "contact_") + std::to_string(ms) + ".csv";
        state.show_toast(export_evidence_csv(e, path) ?
            ("Study exported: " + path) : "Study CSV export failed");
    }
}


void ControlsPanel::render(AppState& state) {
    const WorkbenchParameters previous_workbench = state.current_workbench_parameters();
    ImGui::BeginChild("##ExperimentControlsChild", ImVec2(0.0f, 0.0f), true);
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.58f);
    ImGui::TextColored(ImVec4(0.40f, 0.78f, 1.0f, 1.0f), "EXPERIMENT SETUP");
    const auto description = describe_investigation(state.investigation_mode,
                                                     state.physics_scenario);
    ImGui::TextWrapped("Evidence: %s", description.evidence);
    ImGui::Spacing();
    render_playback_controls(state);
    ImGui::Separator();

    if (!state.custom_settings_enabled) {
        const bool modified = state.has_workbench_defaults &&
            parameters_differ(state.workbench_defaults, state.current_workbench_parameters());
        ImGui::TextColored(modified ? ImVec4(1.0f, 0.76f, 0.30f, 1.0f)
                                    : ImVec4(0.40f, 0.86f, 0.55f, 1.0f),
                           "%s", modified ? "MODIFIED CONFIGURATION" : "RECOMMENDED DEFAULTS");
        ImGui::Text("Particles: %zu", state.get_active_particle_count());
        ImGui::Text("Restitution: %.3f", state.config.restitution);
        ImGui::Text("Frame dt: %.5f s", state.config.dt);
        ImGui::Text("Substeps: %d", state.config.substeps);
        ImGui::Text("Boundary radius: %.2f m", state.boundary.radius);
        ImGui::TextDisabled("Use Custom Settings in the top bar to edit parameters.");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.79f, 0.40f, 1.0f), "CUSTOM SETTINGS ACTIVE");
        if (ImGui::Button("Restore Recommended Defaults", ImVec2(-1.0f, 0.0f))) {
            ExperimentPresets::load_workbench_scenario(state, state.physics_scenario);
            state.custom_settings_enabled = false;
            state.show_toast("Restored recommended scenario defaults");
        } else {
            if (state.physics_scenario == PhysicsScenario::ParticleCloud) {
                ImGui::SliderInt("Cloud particles", &state.requested_particle_count, 16, 1024);
                if (ImGui::Button("Regenerate cloud", ImVec2(-1.0f, 0.0f))) {
                    ExperimentPresets::load_scaling_workload(state, state.requested_particle_count);
                    state.capture_workbench_start_state();
                    state.camera_auto_fit = true;
                    state.is_paused = true;
                    state.note_workbench_parameter_edit();
                }
            }
            float radius = state.boundary.radius;
            if (ImGui::SliderFloat("Boundary radius (m)", &radius, 0.85f, 2.4f, "%.2f m")) {
                state.boundary.radius = radius;
            }
            render_physical_parameters(state);
            render_numerical_controls(state);
            render_visualization_options(state);
        }
    }

    const auto changes = state.has_workbench_defaults
        ? predict_changes(state.workbench_defaults, state.current_workbench_parameters(),
                          state.physics_scenario, state.investigation_mode)
        : std::vector<WorkbenchPrediction>{};
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.42f, 0.82f, 1.0f, 1.0f), "PREDICT  >  RUN  >  OBSERVE");
    if (changes.empty()) {
        ImGui::TextWrapped("Using recommended values. Enable Custom Settings to vary one parameter and predict its effect.");
    } else {
        ImGui::Text("%zu parameter change%s vs recommended defaults", changes.size(),
                    changes.size() == 1 ? "" : "s");
        if (changes.size() > 1) {
            ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.32f, 1.0f), "MULTIPLE VARIABLES CHANGED");
            ImGui::TextWrapped("Observed differences combine these effects. Isolate one parameter to infer causality.");
        }
        for (const auto& change : changes) {
            ImGui::PushID(change.parameter.c_str());
            if (ImGui::TreeNodeEx("##Prediction", ImGuiTreeNodeFlags_DefaultOpen,
                                  "%s: %s", change.parameter.c_str(), change.change.c_str())) {
                ImGui::TextWrapped("Expected: %s", change.expectation.c_str());
                ImGui::TextWrapped("Why: %s", change.explanation.c_str());
                ImGui::TextWrapped("Trade-off: %s", change.tradeoff.c_str());
                ImGui::TextWrapped("Check: %s", change.inspect.c_str());
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    ImGui::Spacing();
    ImGui::TextWrapped("Scope: %s", description.limitation);
    if (state.investigation_mode == InvestigationMode::NumericalAccuracy &&
        state.physics_scenario == PhysicsScenario::Gravity) {
        if (ImGui::Button("Run ballistic timestep convergence sweep", ImVec2(-1.0f, 0.0f))) {
            // Existing sweep is a documented fixed-reference experiment. Its
            // sweep settings do not automatically follow every custom slider.
            ExperimentPresets::load_convergence(state);
            state.capture_workbench_start_state();
            DiagnosticsPanel::execute_convergence_sweep(state);
            state.show_metrics_window = true;
            state.is_paused = true;
            state.show_toast("Reference convergence sweep completed (fixed dt levels)");
        }
        ImGui::TextDisabled("Sweep uses its own fixed reference setup; see diagnostics for measured errors.");
    }
    if (state.investigation_mode == InvestigationMode::HpcPerformance &&
        state.get_active_particle_count() < 16) {
        ImGui::TextWrapped("For scaling, choose Particle cloud. Small workloads mostly measure dispatch overhead.");
    }
    ImGui::Checkbox("Show diagnostics in right panel", &state.show_metrics_window);
    if (state.investigation_mode == InvestigationMode::NumericalAccuracy &&
        state.physics_scenario == PhysicsScenario::DenseContacts) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.40f, 0.78f, 1.0f, 1.0f), "AUTOMATED CONTACT SENSITIVITY");
        ImGui::TextWrapped("Compare 4x2, 4x8, 12x2 and 12x8 at the SAME physical time (3 s) from an identical initial snapshot.");
        if (ImGui::Button("Run controlled contact study", ImVec2(-1.0f, 0.0f))) {
            state.stage4_evidence = run_contact_sensitivity_study(state);
            state.show_toast(state.stage4_evidence.valid ? "Controlled contact study completed" : "Missing initial contact snapshot");
        }
        if (state.stage4_evidence.valid &&
            state.stage4_evidence.kind == EvidenceStudy::ContactSensitivity) {
            render_stage4_evidence(state);
        }
    }
    ImGui::Spacing();

    if (parameters_differ(previous_workbench,
                          state.current_workbench_parameters())) {
        state.note_workbench_parameter_edit();
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();
}

void ControlsPanel::render_performance(AppState& state) {
    const SolverBackend old_backend = state.active_backend;
    const int old_threads = state.omp_threads;
    const auto old_omp_bp = state.omp_solver.get_broadphase_mode();
#if HAS_CUDA_BACKEND
    const auto old_cuda_bp = state.cuda_solver.get_broadphase_mode();
#endif
    ImGui::BeginChild("##PerformanceEvidenceChild", ImVec2(0.0f, 0.0f), true);
    ImGui::TextColored(ImVec4(0.40f, 0.78f, 1.0f, 1.0f), "PERFORMANCE & EVIDENCE");
    ImGui::TextDisabled("Same initial state  |  Different engines");
    ImGui::Separator();
    render_performance_workbench(state);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.40f, 0.78f, 1.0f, 1.0f), "LIVE SIMULATION HEALTH");
    const auto& diag = state.current_diagnostics;
    ImGui::Text("Kinetic energy     %.3f J", diag.total_kinetic_energy);
    ImGui::Text("Total energy       %.3f J", diag.total_mechanical_energy);
    ImGui::Text("Active contacts    %d", diag.active_contacts);
    ImGui::Text("Max penetration    %.3f mm", diag.max_penetration * 1000.0f);
    if (diag.is_finite)
        ImGui::TextColored(ImVec4(0.38f, 0.85f, 0.55f, 1.0f), "NUMERICAL STATE: FINITE");
    else
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "NUMERICAL STATE: NON-FINITE");
    if (state.buf_etotal.size() > 1 &&
        ImPlot::BeginPlot("##LiveEnergyPreview", ImVec2(-1.0f, 180.0f))) {
        ImPlot::SetupAxes("Simulation time (s)", "Total energy (J)");
        ImPlot::PlotLine("E total", state.buf_etotal.x.data(),
                         state.buf_etotal.y.data(), state.buf_etotal.size(),
                         0, state.buf_etotal.offset, sizeof(float));
        ImPlot::EndPlot();
    } else if (state.buf_etotal.size() <= 1) {
        ImGui::TextDisabled("Run the simulation to populate the energy trend.");
    }
    bool backend_settings_changed =
        old_backend != state.active_backend || old_threads != state.omp_threads ||
        old_omp_bp != state.omp_solver.get_broadphase_mode();
#if HAS_CUDA_BACKEND
    backend_settings_changed = backend_settings_changed ||
        old_cuda_bp != state.cuda_solver.get_broadphase_mode();
#endif
    if (backend_settings_changed) state.backend_comparison.executed = false;
    ImGui::EndChild();
}

void ControlsPanel::render_mode_tabs(AppState& state) {
    ImGui::TextUnformatted("Workstation Mode:");

    const bool is_learn = (state.ui_mode == LabUiMode::Learn);
    const bool is_analyze = (state.ui_mode == LabUiMode::Analyze);
    const bool is_perf = (state.ui_mode == LabUiMode::Performance);

    if (is_learn) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.48f, 0.85f, 1.0f));
    if (ImGui::Button(" 1. LEARN ", ImVec2(95.0f, 28.0f))) state.ui_mode = LabUiMode::Learn;
    if (is_learn) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (is_analyze) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.48f, 0.85f, 1.0f));
    if (ImGui::Button(" 2. ANALYZE ", ImVec2(105.0f, 28.0f))) state.ui_mode = LabUiMode::Analyze;
    if (is_analyze) ImGui::PopStyleColor();

    ImGui::SameLine();
    if (is_perf) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.48f, 0.85f, 1.0f));
    if (ImGui::Button(" 3. PERFORMANCE ", ImVec2(130.0f, 28.0f))) state.ui_mode = LabUiMode::Performance;
    if (is_perf) ImGui::PopStyleColor();

    if (state.ui_mode == LabUiMode::Learn) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Mode 1: Guided Physics Laboratories");
        ImGui::TextDisabled("Physics exploration: Conservation laws, elasticity, and impact geometry.");
    } else if (state.ui_mode == LabUiMode::Analyze) {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Mode 2: Numerical Verification & Stability");
        ImGui::TextDisabled("Numerical study: Convergence sweeps, Gauss-Seidel iterations & stabilization.");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "Mode 3: Multicore OpenMP & GPU Scaling");
        ImGui::TextDisabled("HPC workbench: Hardware acceleration & side-by-side backend verification.");
    }
}

void ControlsPanel::render_playback_controls(AppState& state) {
    ImGui::TextUnformatted("Simulation Playback");

    // Play / Pause
    if (state.is_paused) {
        if (ImGui::Button(" [>] Play ", ImVec2(90.0f, 32.0f))) {
            state.is_paused = false;
        }
    } else {
        if (ImGui::Button(" [||] Pause ", ImVec2(90.0f, 32.0f))) {
            state.is_paused = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(" [>|] Step ", ImVec2(85.0f, 32.0f))) {
        state.is_paused = true;
        state.step_single_frame = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset Run", ImVec2(105.0f, 32.0f))) {
        state.reset_run_preserving_parameters();
    }

    // Speed Multipliers
    ImGui::Spacing();
    ImGui::Text("Sim Speed:");
    ImGui::SameLine();
    if (ImGui::RadioButton("0.1x", state.sim_speed == 0.1f)) state.sim_speed = 0.1f;
    ImGui::SameLine();
    if (ImGui::RadioButton("0.5x", state.sim_speed == 0.5f)) state.sim_speed = 0.5f;
    ImGui::SameLine();
    if (ImGui::RadioButton("1.0x", state.sim_speed == 1.0f)) state.sim_speed = 1.0f;
    ImGui::SameLine();
    if (ImGui::RadioButton("2.0x", state.sim_speed == 2.0f)) state.sim_speed = 2.0f;

    // Simulation Clock
    ImGui::Text("Physical Time: %.4f s  |  Frames: %llu", state.current_time, static_cast<unsigned long long>(state.frame_index));

    // Responsive CSV export and screenshot actions.
    const float action_button_width =
        (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button(" Export Run to CSV ", ImVec2(action_button_width, 28.0f))) {
        const auto now = std::chrono::system_clock::now();
        const auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "output/csv/lab_export_" << in_time_t << ".csv";
        state.export_recorded_csv(ss.str());
    }

    ImGui::SameLine();
    if (ImGui::Button(" Capture Screenshot ", ImVec2(action_button_width, 28.0f))) {
        const auto now = std::chrono::system_clock::now();
        const auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << "output/screenshots/screenshot_" << in_time_t << ".png";
        state.pending_screenshot_path = ss.str();
    }

    if (state.toast_timer > 0.0f) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", state.toast_message.c_str());
    }
}

void ControlsPanel::render_experiment_selector(AppState& state) {
    ImGui::TextUnformatted("Select Experiment Scenario");

    const char* experiment_names[] = {
        "EXP_01: Two-Particle Impact (Central & Oblique)",
        "EXP_02: Ballistic Flight & Free Fall Trajectories",
        "EXP_03: Empirical Timestep Refinement Study",
        "EXP_04: Dense Granular Column Settling",
        "EXP_CUSTOM: User-Defined Particle Setup"
    };

    int current_idx = static_cast<int>(state.active_experiment);
    if (ImGui::Combo("##Scenario", &current_idx, experiment_names, IM_ARRAYSIZE(experiment_names))) {
        state.active_experiment = static_cast<ExperimentType>(current_idx);
        switch (state.active_experiment) {
            case ExperimentType::Collision:        ExperimentPresets::load_collision(state); break;
            case ExperimentType::GravityBallistic: ExperimentPresets::load_gravity(state); break;
            case ExperimentType::ConvergenceStudy: ExperimentPresets::load_convergence(state); break;
            case ExperimentType::DensePacking:     ExperimentPresets::load_dense_packing(state); break;
            case ExperimentType::Custom:           ExperimentPresets::load_custom(state); break;
        }
    }

    ImGui::Spacing();
    ImGui::Text("Live particles: %zu", state.get_active_particle_count());
    ImGui::SliderInt("Generate N particles", &state.requested_particle_count, 16, 1024);
    if (ImGui::Button("Generate N-Particle Workload", ImVec2(-1.0f, 30.0f))) {
        ExperimentPresets::load_scaling_workload(state, state.requested_particle_count);
        state.camera_auto_fit = true;
        state.show_toast("Loaded deterministic workload; compare from the right panel");
    }
    ImGui::TextDisabled("Collision/gravity presets retain fixed N for analytical checks.");
    ImGui::TextDisabled("Generated cloud is for performance exploration, not physical validation.");

    ImGui::Spacing();
    ImGui::TextUnformatted("Container shape: Circle (supported by all backends)");
    float new_radius = state.boundary.radius;
    if (ImGui::SliderFloat("Container radius (m)", &new_radius, 0.85f, 2.0f, "%.2f m")) {
        state.boundary.radius = new_radius;
        state.solver.set_boundary(state.boundary);
        state.omp_solver.set_boundary(state.boundary);
#if HAS_CUDA_BACKEND
        if (state.cuda_initialized) state.cuda_solver.set_boundary(state.boundary);
#endif
        state.backend_comparison.executed = false;
    }
    ImGui::TextDisabled("Rectangle boundary requires physics support in all 3 engines.");
}

void ControlsPanel::render_physical_parameters(AppState& state) {
    if (ImGui::CollapsingHeader("Physical Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Restitution e
        if (ImGui::SliderFloat("Restitution (e)", &state.config.restitution, 0.0f, 1.0f, "%.3f")) {
            state.solver.set_config(state.config);
            state.initial_collision.restitution = state.config.restitution;
            if (state.active_experiment == ExperimentType::Collision) {
                state.recompute_collision_analytical_solution();
            }
        }

        if (state.config.restitution > 1.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "WARNING: e > 1.0 causes non-physical energy creation!");
        }

        // Boundary Restitution e_b
        if (ImGui::SliderFloat("Wall Restitution (e_b)", &state.config.boundary_restitution, 0.0f, 1.0f, "%.3f")) {
            state.solver.set_config(state.config);
        }

        // Gravity Settings
        ImGui::Spacing();
        ImGui::Text("Gravitational Acceleration g (m/s^2):");
        if (ImGui::SmallButton("Earth (-9.81)")) {
            state.config.gravity = {0.0f, -9.80665f};
            state.solver.set_config(state.config);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Moon (-1.62)")) {
            state.config.gravity = {0.0f, -1.62f};
            state.solver.set_config(state.config);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Zero G")) {
            state.config.gravity = {0.0f, 0.0f};
            state.solver.set_config(state.config);
        }

        if (ImGui::SliderFloat2("g_x, g_y", &state.config.gravity.x, -20.0f, 20.0f, "%.2f")) {
            state.solver.set_config(state.config);
        }

        // Scenario Specific Parameters
        if (state.active_experiment == ExperimentType::Collision) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.85f, 0.95f, 0.5f, 1.0f), "Collision Laboratory Presets:");

            if (ImGui::SmallButton("Elastic")) {
                ExperimentPresets::load_collision(state, 1.0f, 0.06f, 0.06f, 0.0f, 0.0f, {1.0f, 0.0f}, {-1.0f, 0.0f}, 0.0f);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Unequal Mass")) {
                const float r1 = 0.05f;
                const float r2 = r1 * std::sqrt(2.0f);
                ExperimentPresets::load_collision(state, 1.0f, r1, r2, 0.0f, 0.0f, {1.2f, 0.0f}, {-0.8f, 0.0f}, 0.0f);
            }

            if (ImGui::SmallButton("Inelastic")) {
                ExperimentPresets::load_collision(state, 0.528f, 0.06f, 0.06f, 0.0f, 0.0f, {1.5f, 0.0f}, {-1.5f, 0.0f}, 0.0f);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Plastic")) {
                ExperimentPresets::load_collision(state, 0.0f, 0.06f, 0.06f, 0.0f, 0.0f, {1.0f, 0.0f}, {-1.0f, 0.0f}, 0.0f);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Oblique")) {
                ExperimentPresets::load_collision(state, 0.9f, 0.06f, 0.06f, 0.0f, 0.0f, {1.2f, 0.0f}, {-1.2f, 0.0f}, 0.04f);
            }

            ImGui::Spacing();
            ImGui::Text("Adjust Collision Setup (Resets initial state):");

            float r1 = state.initial_collision.r1;
            float r2 = state.initial_collision.r2;
            if (ImGui::SliderFloat("Radius 1 (m)", &r1, 0.02f, 0.15f, "%.3f m")) {
                ExperimentPresets::load_collision(state, state.config.restitution, r1, r2, 0.0f, 0.0f,
                                                  state.initial_collision.v1, state.initial_collision.v2,
                                                  state.initial_collision.impact_parameter);
            }
            ImGui::TextDisabled("  -> Mass 1: %.3f kg", state.initial_collision.m1);

            if (ImGui::SliderFloat("Radius 2 (m)", &r2, 0.02f, 0.15f, "%.3f m")) {
                ExperimentPresets::load_collision(state, state.config.restitution, r1, r2, 0.0f, 0.0f,
                                                  state.initial_collision.v1, state.initial_collision.v2,
                                                  state.initial_collision.impact_parameter);
            }
            ImGui::TextDisabled("  -> Mass 2: %.3f kg", state.initial_collision.m2);

            float b = state.initial_collision.impact_parameter;
            if (ImGui::SliderFloat("Impact Offset b (m)", &b, -0.10f, 0.10f, "%.3f m")) {
                ExperimentPresets::load_collision(state, state.config.restitution, r1, r2, 0.0f, 0.0f,
                                                  state.initial_collision.v1, state.initial_collision.v2, b);
            }
        }
    }
}

void ControlsPanel::render_numerical_controls(AppState& state) {
    if (ImGui::CollapsingHeader("Numerical Solver Parameters")) {
        // Frame Timestep dt
        if (ImGui::SliderFloat("Frame dt (s)", &state.config.dt, 0.001f, 0.050f, "%.4f s")) {
            state.solver.set_config(state.config);
            state.omp_solver.set_config(state.config);
        }

        // Substeps
        if (ImGui::SliderInt("Substeps (N_sub)", &state.config.substeps, 1, 32)) {
            state.solver.set_config(state.config);
            state.omp_solver.set_config(state.config);
        }

        // Effective Substep dt_sub
        const float dt_sub_ms = state.config.dt_substep() * 1000.0f;
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "Effective dt_sub: %.3f ms (f_sub = %.0f Hz)",
                           dt_sub_ms, 1000.0f / dt_sub_ms);

        // Integrator Scheme Selection (Lab 2 Coursework)
        int integ_choice = (state.config.integrator == sim_core::IntegratorType::ExplicitEuler) ? 1 : 0;
        if (ImGui::Combo("Integrator##scheme", &integ_choice, "Semi-implicit Euler (Symplectic)\0Explicit Forward Euler (Non-Symplectic)\0")) {
            state.config.integrator = (integ_choice == 1) ? sim_core::IntegratorType::ExplicitEuler : sim_core::IntegratorType::SemiImplicitEuler;
            state.solver.set_config(state.config);
            state.omp_solver.set_config(state.config);
        }

        // Gauss-Seidel Contact Iterations
        if (ImGui::SliderInt("Gauss-Seidel Iters (K)", &state.config.solver_iterations, 1, 32)) {
            state.solver.set_config(state.config);
            state.omp_solver.set_config(state.config);
        }
        ImGui::TextDisabled("  -> Note: K in [4, 8] provides sub-millimeter contact error.");

        // Baumgarte Stabilization Factor
        if (ImGui::SliderFloat("Stabilization (alpha)", &state.config.stabilization_factor, 0.0f, 1.0f, "%.2f")) {
            state.solver.set_config(state.config);
            state.omp_solver.set_config(state.config);
        }
        ImGui::TextDisabled("  -> Note: alpha in [0.25, 0.60] balances settling vs micro-jitter.");

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Numerical Verification Actions:");
        if (ImGui::Button(" Run Timestep Convergence Sweep (EXP_03) ", ImVec2(-1.0f, 30.0f))) {
            state.active_experiment = ExperimentType::ConvergenceStudy;
            ExperimentPresets::load_convergence(state);
            DiagnosticsPanel::execute_convergence_sweep(state);
        }
    }
}

void ControlsPanel::render_performance_workbench(AppState& state) {
    if (state.investigation_mode == InvestigationMode::HpcPerformance &&
        state.physics_scenario == PhysicsScenario::ParticleCloud) {
        ImGui::TextColored(ImVec4(0.40f, 0.78f, 1.0f, 1.0f), "AUTOMATED HPC SCALING");
        ImGui::TextWrapped("N = 128, 256, 512, 1024 | 3 repeats | median normalized time/step. Runs on temporary solvers; current simulation is unchanged.");
        if (ImGui::Button("Run scaling study (CPU / OMP / CUDA)", ImVec2(-1.0f, 0.0f))) {
            state.stage4_evidence = run_backend_scaling_study(state);
            state.show_toast(state.stage4_evidence.valid ? "Scaling study complete: inspect time per step" : "Scaling study failed");
        }
        if (state.stage4_evidence.valid &&
            state.stage4_evidence.kind == EvidenceStudy::BackendScaling) {
            render_stage4_evidence(state);
        }
        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Physics Compute Backend & Architecture", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Active Compute Engine:");

        int backend_choice = static_cast<int>(state.active_backend);
        if (ImGui::RadioButton("CPU Sequential", &backend_choice, 0)) {
            if (state.active_backend != SolverBackend::CpuReference) {
                const auto current_pts = state.get_active_particles();
                state.active_backend = SolverBackend::CpuReference;
                state.solver.set_particles(current_pts);
                state.solver.set_config(state.config);
                state.solver.set_boundary(state.boundary);
                state.show_toast("Switched to CPU Reference Backend");
            }
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("OpenMP Multicore", &backend_choice, 1)) {
            if (state.active_backend != SolverBackend::OpenMpParallel) {
                const auto current_pts = state.get_active_particles();
                state.active_backend = SolverBackend::OpenMpParallel;
                state.omp_solver.set_particles(current_pts);
                state.omp_solver.set_config(state.config);
                state.omp_solver.set_boundary(state.boundary);
                state.omp_solver.set_num_threads(state.omp_threads);
                state.show_toast("Switched to OpenMP Multicore Backend");
            }
        }
        ImGui::SameLine();
#if HAS_CUDA_BACKEND
        const bool cuda_avail = sim_cuda::CudaSolverV2::is_cuda_available();
        if (ImGui::RadioButton("CUDA GPU (RTX 4060)", &backend_choice, 2)) {
            if (cuda_avail) {
                if (state.active_backend != SolverBackend::CudaGpu) {
                    const auto current_pts = state.get_active_particles();
                    state.active_backend = SolverBackend::CudaGpu;
                    state.cuda_solver.set_particles(current_pts);
                    state.cuda_solver.set_config(state.config);
                    state.cuda_solver.set_boundary(state.boundary);
                    state.cuda_initialized = true;
                    state.show_toast("Switched to NVIDIA CUDA GPU Backend");
                }
            } else {
                state.show_toast("CUDA GPU Hardware Unavailable! Falling back to CPU.");
            }
        }
#else
        ImGui::BeginDisabled();
        ImGui::RadioButton("CUDA GPU (No CUDA Build)", false);
        ImGui::EndDisabled();
#endif

        if (state.active_backend == SolverBackend::OpenMpParallel) {
            if (ImGui::SliderInt("OpenMP Threads", &state.omp_threads, 1, 16)) {
                state.omp_solver.set_num_threads(state.omp_threads);
            }

            int bp_mode = static_cast<int>(state.omp_solver.get_broadphase_mode());
            if (ImGui::RadioButton("All-Pairs O(N^2)##omp", &bp_mode, 0)) {
                state.omp_solver.set_broadphase_mode(sim_omp::BroadphaseMode::AllPairs);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Spatial Grid O(N)##omp", &bp_mode, 1)) {
                state.omp_solver.set_broadphase_mode(sim_omp::BroadphaseMode::SpatialGrid);
            }
#if HAS_CUDA_BACKEND
        } else if (state.active_backend == SolverBackend::CudaGpu) {
            const auto& specs = state.cuda_solver.get_device_specs();
            ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "GPU: %s (CC %s)",
                               specs.device_name.c_str(), specs.compute_capability_str().c_str());
            ImGui::TextDisabled("VRAM: %.0f MB | Multiprocessors: %d SMs",
                                specs.total_memory_mb(), specs.multiprocessor_count);

            int bp_mode = static_cast<int>(state.cuda_solver.get_broadphase_mode());
            if (ImGui::RadioButton("All-Pairs O(N^2)##cuda", &bp_mode, 0)) {
                state.cuda_solver.set_broadphase_mode(sim_cuda::CudaBroadphaseMode::AllPairs);
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Spatial Grid O(N)##cuda", &bp_mode, 1)) {
                state.cuda_solver.set_broadphase_mode(sim_cuda::CudaBroadphaseMode::SpatialGrid);
            }
#endif
        }

        // Live Performance & Compute Metrics
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "Live HPC Performance:");
        const double step_time_ms = state.last_physics_step_ms;
        const double sim_rate_hz = (step_time_ms > 0.001) ? (1000.0 / step_time_ms) : 0.0;
        const size_t n_particles = state.get_active_particle_count();
        const double throughput = n_particles * sim_rate_hz;

        ImGui::BulletText("Step Compute Time: %.3f ms (%.1f us)", step_time_ms, step_time_ms * 1000.0);
        ImGui::BulletText("Physics Simulation Rate: %.1f steps/s", sim_rate_hz);
        ImGui::BulletText("Workload Throughput: %.2f M particles*steps/s", throughput / 1e6);
        ImGui::BulletText("Active Particles: %zu | Substeps: %d", n_particles, state.config.substeps);

        // Executable Side-by-Side Multi-Backend Verification
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "Cross-Backend Numerical Verification:");
        ImGui::TextWrapped("Runs the selected scenario from an identical particle snapshot "
                           "on each backend; reports timing and numerical differences. "
                           "A comparison is not an automatic correctness pass.");

        const size_t comparison_n = state.get_active_particle_count();
        const int comparison_steps = comparison_n > 512 ? 10 : (comparison_n > 128 ? 25 : 100);
        ImGui::TextWrapped("The benchmark starts from the CURRENT particle snapshot. "
                           "N > 128 uses fewer steps to keep the UI responsive.");
        if (ImGui::Button("Compare CPU / OMP / CUDA", ImVec2(-1.0f, 32.0f))) {
            state.run_cross_backend_comparison(comparison_steps);
        }

        if (state.backend_comparison.executed) {
            ImGui::Spacing();
            ImGui::TextDisabled("Measured from t=%.3f s; dt=%.5f s; substeps=%d",
                                state.backend_comparison.snapshot_time_s,
                                state.backend_comparison.snapshot_dt_s,
                                state.backend_comparison.snapshot_substeps);
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "[SNAPSHOT COMPARISON: %d STEPS, %zu PARTICLES]",
                               state.backend_comparison.steps_simulated,
                               state.backend_comparison.particle_count);

            ImGui::Columns(4, "BackendTable", true);
            ImGui::Text("Backend"); ImGui::NextColumn();
            ImGui::Text("Time (ms)"); ImGui::NextColumn();
            ImGui::Text("Speedup"); ImGui::NextColumn();
            ImGui::Text("Max ||dx||"); ImGui::NextColumn();
            ImGui::Separator();

            // CPU Row
            ImGui::Text("CPU Ref"); ImGui::NextColumn();
            ImGui::Text("%.2f ms", state.backend_comparison.cpu_time_ms); ImGui::NextColumn();
            ImGui::Text("1.00x"); ImGui::NextColumn();
            ImGui::Text("Baseline"); ImGui::NextColumn();

            // OpenMP Row
            const double omp_speedup = (state.backend_comparison.omp_time_ms > 0.0) ?
                (state.backend_comparison.cpu_time_ms / state.backend_comparison.omp_time_ms) : 0.0;
            ImGui::Text("OpenMP (%dT)", state.omp_threads); ImGui::NextColumn();
            ImGui::Text("%.2f ms", state.backend_comparison.omp_time_ms); ImGui::NextColumn();
            ImGui::Text("%.2fx", omp_speedup); ImGui::NextColumn();
            ImGui::Text("%.2e m", state.backend_comparison.omp_max_pos_diff); ImGui::NextColumn();

            // CUDA Row
            if (state.backend_comparison.cuda_time_ms > 0.0) {
                const double cuda_speedup = (state.backend_comparison.cuda_time_ms > 0.0) ?
                    (state.backend_comparison.cpu_time_ms / state.backend_comparison.cuda_time_ms) : 0.0;
                ImGui::Text("CUDA GPU"); ImGui::NextColumn();
                ImGui::Text("%.2f ms", state.backend_comparison.cuda_time_ms); ImGui::NextColumn();
                ImGui::Text("%.2fx", cuda_speedup); ImGui::NextColumn();
                ImGui::Text("%.2e m", state.backend_comparison.cuda_max_pos_diff); ImGui::NextColumn();
            }
            ImGui::Columns(1);
            ImGui::Separator();

            ImGui::TextWrapped("Interpretation: CPU reference uses all-pairs Gauss-Seidel; "
                           "OpenMP / CUDA use their selected broadphase and parallel Jacobi. "
                           "Speedup can mix algorithmic and hardware effects; trajectories need not match exactly.");
        }
    }
}

void ControlsPanel::render_visualization_options(AppState& state) {
    if (ImGui::CollapsingHeader("Viewport Visual Overlays")) {
        ImGui::Checkbox("Auto-fit container in viewport", &state.camera_auto_fit);
        ImGui::Checkbox("Metric Grid (SI)", &state.show_grid);
        ImGui::SameLine();
        ImGui::Checkbox("Boundary Circle", &state.show_boundary);

        ImGui::Checkbox("Velocity Vectors (v)", &state.show_velocity_vectors);
        if (state.show_velocity_vectors) {
            ImGui::SliderFloat("Vector Scale", &state.velocity_vector_scale, 0.05f, 0.50f, "%.2f");
        }

        ImGui::Checkbox("Contact Normals & Penetration", &state.show_contact_normals);
        ImGui::Checkbox("Analytical Trajectory Ghost", &state.show_analytical_trajectory);

        // Color Mode Selection
        const char* color_modes[] = {
            "Material Palette",
            "Velocity Heatmap (Speed)",
            "Contact State (Clamping)",
            "Mass Spectrum"
        };
        int current_color = static_cast<int>(state.color_mode);
        if (ImGui::Combo("Particle Color", &current_color, color_modes, IM_ARRAYSIZE(color_modes))) {
            state.color_mode = static_cast<ParticleColorMode>(current_color);
        }
    }
}


void ControlsPanel::render_experiment_coach(AppState& state) {
    if (!ImGui::CollapsingHeader("Predict -> Run -> Observe", ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::TextColored(ImVec4(0.85f, 0.90f, 0.45f, 1.0f), "WHAT CHANGES WHEN YOU TURN THE DIAL?");

    if (state.active_experiment == ExperimentType::Collision) {
        if (state.config.restitution > 0.95f) {
            ImGui::TextWrapped("Prediction: near-elastic central impacts should retain nearly all kinetic energy, absent external wall impulses.");
        } else if (state.config.restitution < 0.10f) {
            ImGui::TextWrapped("Prediction: strongly inelastic impacts lose more kinetic energy; total momentum remains conserved for isolated pairwise impulses.");
        } else {
            ImGui::TextWrapped("Prediction: decreasing restitution increases impact energy loss. Compare measured vs analytical Delta Ek after contact.");
        }
    } else if (state.active_experiment == ExperimentType::GravityBallistic) {
        ImGui::TextWrapped("Prediction: stronger gravity bends the trajectory more. Analytical ballistic error is meaningful BEFORE wall contact.");
    } else if (state.active_experiment == ExperimentType::ConvergenceStudy) {
        ImGui::TextWrapped("Prediction: for smooth free flight, halving the integration timestep should approximately halve first-order position error.");
    } else if (state.active_experiment == ExperimentType::DensePacking) {
        ImGui::TextWrapped("Prediction: denser contact networks are more sensitive to update order and may require more contact iterations.");
    } else {
        ImGui::TextWrapped("Prediction: higher particle count increases collision candidates and can change which backend is faster.");
    }

    ImGui::Text("Effective integration step: %.4f ms", 1000.0f * state.config.dt_substep());
    ImGui::TextWrapped("More substeps reduce distance traveled per discrete check, but increase computation. This is not a no-tunneling guarantee.");
    ImGui::TextWrapped("Try: (1) predict (2) step or run (3) open live metrics (4) compare backends using the right panel.");
}

} // namespace lab_gui
