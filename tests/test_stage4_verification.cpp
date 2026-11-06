#include "lab_gui/ballistic_reference.h"
#include "lab_gui/experiment_presets.h"
#include "lab_gui/workbench_study_runner.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using lab_gui::AppState;

static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

static bool close_to(double actual, double expected, double tolerance) {
    return std::abs(actual - expected) <= tolerance;
}

static std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> values;
    std::stringstream input(line);
    std::string value;
    while (std::getline(input, value, ',')) values.push_back(value);
    return values;
}

static void validate_numeric_csv(const fs::path& path, size_t expected_rows) {
    std::ifstream input(path);
    require(input.good(), "Unable to open CSV: " + path.string());
    std::string line;
    std::vector<std::string> header;
    size_t rows = 0;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() == '#') continue;
        if (header.empty()) {
            header = split_csv(line);
            require(!header.empty(), "CSV header is empty: " + path.string());
            continue;
        }
        const auto values = split_csv(line);
        require(values.size() == header.size(), "CSV column count mismatch: " + path.string());
        for (const auto& value : values) {
            size_t parsed = 0;
            (void)std::stod(value, &parsed);
            require(parsed == value.size(), "Non-numeric CSV value: " + value);
        }
        ++rows;
    }
    require(rows == expected_rows, "Unexpected CSV data-row count: " + path.string());
}

struct CollisionMeasurement {
    float restitution{0.0f};
    float v1_error{0.0f};
    float v2_error{0.0f};
    float measured_energy_change{0.0f};
    float theoretical_energy_change{0.0f};
    float energy_error{0.0f};
    float momentum_drift{0.0f};
    int frames{0};
};

static CollisionMeasurement verify_collision(float restitution) {
    AppState state;
    lab_gui::ExperimentPresets::load_collision(state, restitution);
    const auto initial_momentum = state.current_diagnostics.total_momentum;
    const float initial_energy = state.current_diagnostics.total_kinetic_energy;
    bool impulse = false;
    int frame = 0;
    for (; frame < 120; ++frame) {
        state.solver.step();
        state.current_diagnostics = state.solver.compute_diagnostics();
        require(state.current_diagnostics.is_finite, "Collision produced non-finite state");
        if (state.current_diagnostics.step_collision_impulses > 0) {
            impulse = true;
            break;
        }
    }
    require(impulse, "Collision impulse was not observed");
    const auto& particles = state.solver.get_particles();
    require(particles.size() == 2, "Collision particle count changed");
    for (const auto& particle : particles) {
        require((particle.position - state.boundary.center).length() + particle.radius <
                    state.boundary.radius - 0.1f,
                "Boundary invalidated isolated collision assumptions");
    }

    CollisionMeasurement result;
    result.restitution = restitution;
    result.v1_error = (particles[0].velocity - state.analytical_v1_exact).length();
    result.v2_error = (particles[1].velocity - state.analytical_v2_exact).length();
    result.measured_energy_change =
        state.current_diagnostics.total_kinetic_energy - initial_energy;
    result.theoretical_energy_change = state.analytical_theoretical_dek;
    result.energy_error = std::abs(result.measured_energy_change -
                                   result.theoretical_energy_change);
    result.momentum_drift =
        (state.current_diagnostics.total_momentum - initial_momentum).length();
    result.frames = frame + 1;
    require(result.v1_error < 2.0e-5f && result.v2_error < 2.0e-5f,
            "Collision velocity error exceeds tolerance");
    require(result.energy_error < 2.0e-4f, "Collision energy-loss error exceeds tolerance");
    require(result.momentum_drift < 2.0e-5f, "Collision momentum drift exceeds tolerance");
    return result;
}

struct GravityMeasurement {
    float gravity_y{0.0f};
    float time_s{0.0f};
    float position_error_m{0.0f};
    float expected_position_error_m{0.0f};
    float velocity_error_m_s{0.0f};
    float wall_contact_time_s{0.0f};
};

static GravityMeasurement verify_gravity(float gravity_y) {
    AppState state;
    lab_gui::ExperimentPresets::load_gravity(state, {0.0f, gravity_y});
    constexpr int precontact_frames = 36;
    for (int frame = 0; frame < precontact_frames; ++frame) {
        const auto before = state.solver.get_particles().front();
        state.solver.step();
        const auto& after = state.solver.get_particles().front();
        require(!lab_gui::boundary_impulse_occurred(before, after, state.config),
                "Unexpected wall impulse in pre-contact gravity interval");
    }
    const float t = precontact_frames * state.config.dt;
    const auto& particle = state.solver.get_particles().front();
    const sim_core::Vec2 exact_position = state.initial_pos1 + state.initial_vel1 * t +
        0.5f * state.config.gravity * (t * t);
    const sim_core::Vec2 exact_velocity = state.initial_vel1 + state.config.gravity * t;
    const float position_error = (particle.position - exact_position).length();
    const float velocity_error = (particle.velocity - exact_velocity).length();
    const float effective_h = state.config.dt / static_cast<float>(state.config.substeps);
    const float expected_error = 0.5f * std::abs(gravity_y) * t * effective_h;
    require(std::abs(position_error - expected_error) < 3.0e-5f,
            "Gravity position error is inconsistent with semi-implicit Euler");
    require(velocity_error < 2.0e-4f, "Gravity velocity error exceeds roundoff tolerance");

    bool wall_impulse = false;
    float contact_time = t;
    for (int frame = precontact_frames; frame < 600; ++frame) {
        const auto before = state.solver.get_particles().front();
        state.solver.step();
        contact_time += state.config.dt;
        const auto& after = state.solver.get_particles().front();
        if (lab_gui::boundary_impulse_occurred(before, after, state.config)) {
            wall_impulse = true;
            break;
        }
    }
    require(wall_impulse, "Actual boundary impulse was not detected");

    GravityMeasurement result;
    result.gravity_y = gravity_y;
    result.time_s = t;
    result.position_error_m = position_error;
    result.expected_position_error_m = expected_error;
    result.velocity_error_m_s = velocity_error;
    result.wall_contact_time_s = contact_time;
    return result;
}

static void verify_selection_and_configuration() {
    using namespace lab_gui;
    AppState state;
    require(!state.custom_settings_enabled, "Custom Settings must default to disabled");
    const std::array<size_t, 5> counts{{2, 1, 48, 256, 5}};
    int combinations = 0;
    for (int mode_index = 0; mode_index < investigation_count; ++mode_index) {
        for (int scenario_index = 0; scenario_index < scenario_count; ++scenario_index) {
            const auto mode = static_cast<InvestigationMode>(mode_index);
            const auto scenario = static_cast<PhysicsScenario>(scenario_index);
            state.convergence_results.push_back({});
            state.simulated_trajectory.push_back({1.0f, 2.0f});
            ExperimentPresets::load_workbench_scenario(state, scenario);
            require(state.physics_scenario == scenario, "Scenario selection mismatch");
            require(state.get_active_particle_count() == counts[scenario_index],
                    "Scenario default particle count mismatch");
            require(state.requested_particle_count == 256 ||
                    scenario != PhysicsScenario::ParticleCloud,
                    "Particle-cloud requested count mismatch");
            require(state.convergence_results.empty(), "Convergence evidence leaked across scenarios");
            require(state.simulated_trajectory.empty(), "Trajectory leaked across scenarios");
            require(state.has_workbench_defaults &&
                    !parameters_differ(state.workbench_defaults,
                                       state.current_workbench_parameters()),
                    "Scenario defaults were not captured consistently");
            const auto snapshot = state.get_active_particles();
            const auto config = state.config;
            state.investigation_mode = mode;
            require(state.get_active_particles().size() == snapshot.size() &&
                    state.config.dt == config.dt && state.config.substeps == config.substeps,
                    "Changing investigation mode corrupted simulation state");
            const auto description = describe_investigation(mode, scenario);
            require(description.limitation && description.limitation[0] != '\0',
                    "Unsupported/limited comparison lacks a label");
            ++combinations;
        }
    }
    require(combinations == 15, "Did not exercise all investigation/scenario combinations");

    ExperimentPresets::load_workbench_scenario(state, PhysicsScenario::Collision);
    const auto initial = state.workbench_initial_particles;
    state.stage4_evidence.valid = true;
    state.convergence_results.push_back({});
    state.config.restitution = 0.8f;
    state.config.substeps = 16;
    state.note_workbench_parameter_edit();
    require(state.custom_configuration_dirty, "Parameter edit was not marked dirty");
    require(!state.stage4_evidence.valid && state.convergence_results.empty(),
            "Parameter edit did not invalidate outdated results");
    require(!predict_changes(state.workbench_defaults,
                            state.current_workbench_parameters(),
                            state.physics_scenario,
                            state.investigation_mode).empty(),
            "Parameter edit did not update predictions");
    state.solver.step();
    state.current_time = state.config.dt;
    state.reset_run_preserving_parameters();
    require(state.config.restitution == 0.8f && state.config.substeps == 16,
            "Reset Run did not preserve custom configuration");
    require((state.solver.get_particles().front().position - initial.front().position).length() < 1.0e-7f,
            "Reset Run did not restore the initial particle snapshot");
    ExperimentPresets::load_workbench_scenario(state, PhysicsScenario::Collision);
    require(close_to(state.config.restitution, 0.528, 1.0e-6) &&
            !state.custom_configuration_dirty,
            "Restore Defaults did not restore recommended collision settings");

    for (int count : {128, 256, 512, 1024}) {
        ExperimentPresets::load_scaling_workload(state, count);
        require(state.requested_particle_count == count,
                "Requested scaling particle count was not retained");
        require(static_cast<int>(state.get_active_particle_count()) == count &&
                static_cast<int>(state.workbench_initial_particles.size()) == count,
                "Generated scaling workload does not equal requested count");
    }
}

static void write_collision_csv(const fs::path& path,
                                const std::vector<CollisionMeasurement>& rows) {
    std::ofstream out(path);
    out << "restitution,v1_error_m_s,v2_error_m_s,measured_energy_change_J,"
           "theoretical_energy_change_J,energy_error_J,momentum_drift_kg_m_s,frames\n";
    out << std::setprecision(10);
    for (const auto& row : rows) {
        out << row.restitution << ',' << row.v1_error << ',' << row.v2_error << ','
            << row.measured_energy_change << ',' << row.theoretical_energy_change << ','
            << row.energy_error << ',' << row.momentum_drift << ',' << row.frames << '\n';
    }
}

static void write_gravity_csv(const fs::path& path,
                              const std::vector<GravityMeasurement>& rows) {
    std::ofstream out(path);
    out << "gravity_y_m_s2,precontact_time_s,position_error_m,expected_euler_error_m,"
           "velocity_error_m_s,first_boundary_impulse_time_s\n";
    out << std::setprecision(10);
    for (const auto& row : rows) {
        out << row.gravity_y << ',' << row.time_s << ',' << row.position_error_m << ','
            << row.expected_position_error_m << ',' << row.velocity_error_m_s << ','
            << row.wall_contact_time_s << '\n';
    }
}

static void write_convergence_csv(
    const fs::path& path,
    const std::vector<lab_gui::ConvergenceDataPoint>& rows) {
    std::ofstream out(path);
    out << "dt_s,substeps,position_error_m,reduction_ratio,empirical_order,runtime_us\n";
    out << std::setprecision(10);
    for (const auto& row : rows) {
        out << row.dt << ',' << row.substeps << ',' << row.error_l2 << ','
            << row.reduction_ratio << ',' << row.empirical_order << ','
            << row.runtime_us << '\n';
    }
}

int main(int argc, char** argv) {
    try {
        fs::path output = "output/audit/stage4_verification_2026-11-06";
        for (int i = 1; i + 1 < argc; ++i) {
            if (std::string(argv[i]) == "--out") output = argv[++i];
        }
        fs::create_directories(output);

        verify_selection_and_configuration();
        std::cout << "Selection/configuration: 15 combinations and reset/invalidation checks passed\n";

        const std::vector<CollisionMeasurement> collisions{
            verify_collision(0.528f), verify_collision(0.800f)};
        const fs::path collision_csv = output / "collision_verification.csv";
        write_collision_csv(collision_csv, collisions);
        validate_numeric_csv(collision_csv, collisions.size());
        for (const auto& row : collisions) {
            std::cout << "Collision e=" << row.restitution
                      << " v-errors=" << row.v1_error << ',' << row.v2_error
                      << " energy-error=" << row.energy_error
                      << " momentum-drift=" << row.momentum_drift << '\n';
        }

        const std::vector<GravityMeasurement> gravity{
            verify_gravity(-9.80665f), verify_gravity(-3.7f)};
        const fs::path gravity_csv = output / "gravity_verification.csv";
        write_gravity_csv(gravity_csv, gravity);
        validate_numeric_csv(gravity_csv, gravity.size());
        for (const auto& row : gravity) {
            std::cout << "Gravity g_y=" << row.gravity_y
                      << " position-error=" << row.position_error_m
                      << " expected=" << row.expected_position_error_m
                      << " velocity-error=" << row.velocity_error_m_s
                      << " wall-impulse-t=" << row.wall_contact_time_s << '\n';
        }

        const auto convergence = lab_gui::run_ballistic_convergence_study(
            sim_core::IntegratorType::SemiImplicitEuler);
        const std::array<double, 4> references{{0.078453, 0.039227, 0.019613, 0.009806}};
        require(convergence.size() == references.size(), "Convergence level count mismatch");
        for (size_t i = 0; i < convergence.size(); ++i) {
            require(std::abs(convergence[i].error_l2 - references[i]) < 2.5e-4,
                    "Convergence error differs from analytical reference expectation");
            if (i > 0) {
                require(convergence[i].error_l2 < convergence[i - 1].error_l2,
                        "Convergence error did not decrease under refinement");
                require(convergence[i].reduction_ratio > 1.95f &&
                        convergence[i].reduction_ratio < 2.05f,
                        "Convergence reduction ratio is inconsistent with first order");
                require(convergence[i].empirical_order > 0.96f &&
                        convergence[i].empirical_order < 1.04f,
                        "Empirical convergence order is not approximately one");
            }
        }
        const fs::path convergence_csv = output / "convergence_verification.csv";
        write_convergence_csv(convergence_csv, convergence);
        validate_numeric_csv(convergence_csv, convergence.size());

        AppState contact_state;
        lab_gui::ExperimentPresets::load_workbench_scenario(
            contact_state, lab_gui::PhysicsScenario::DenseContacts);
        const auto contact = lab_gui::run_contact_sensitivity_study(contact_state);
        require(contact.valid && contact.rows.size() == 4, "Contact study did not return four rows");
        for (const auto& row : contact.rows) {
            require(row.requested_particles == 48 && row.generated_particles == 48,
                    "Contact study particle-count provenance mismatch");
            require(row.physics_steps == 180 && close_to(row.simulated_seconds, 3.0, 1.0e-5),
                    "Contact study physical duration mismatch");
            require(row.finite && row.cpu_total_ms > 0.0 && row.cpu_ms_per_step > 0.0,
                    "Contact study produced invalid state or timing");
            require(row.maximum_penetration_m >= 0.0f && row.peak_penetration_m >= 0.0f,
                    "Contact penetration must be non-negative");
        }
        const double substeps_4 = contact.rows[0].cpu_ms_per_step + contact.rows[1].cpu_ms_per_step;
        const double substeps_12 = contact.rows[2].cpu_ms_per_step + contact.rows[3].cpu_ms_per_step;
        const double iterations_2 = contact.rows[0].cpu_ms_per_step + contact.rows[2].cpu_ms_per_step;
        const double iterations_8 = contact.rows[1].cpu_ms_per_step + contact.rows[3].cpu_ms_per_step;
        require(substeps_12 > substeps_4, "More substeps did not increase average contact cost");
        require(iterations_8 > iterations_2, "More contact iterations did not increase average cost");
        const fs::path contact_csv = output / "contact_sensitivity.csv";
        require(lab_gui::export_evidence_csv(contact, contact_csv.string()),
                "Contact CSV export failed");
        validate_numeric_csv(contact_csv, contact.rows.size());

        AppState scaling_state;
        lab_gui::ExperimentPresets::load_workbench_scenario(
            scaling_state, lab_gui::PhysicsScenario::ParticleCloud);
        scaling_state.omp_threads = 4;
        const auto scaling = lab_gui::run_backend_scaling_study(scaling_state);
        require(scaling.valid && scaling.rows.size() == 4, "Scaling study did not return four rows");
        for (size_t i = 0; i < scaling.rows.size(); ++i) {
            const int expected_n = 128 << static_cast<int>(i);
            const auto& row = scaling.rows[i];
            require(row.requested_particles == expected_n &&
                    row.generated_particles == expected_n && row.particles == expected_n,
                    "Scaling requested/generated particle count mismatch");
            require(row.benchmark_samples == 3 && row.physics_steps > 0 &&
                    row.simulated_seconds > 0.0, "Scaling protocol metadata mismatch");
            require(row.finite && std::isfinite(row.cpu_ms_per_step) &&
                    std::isfinite(row.omp_ms_per_step) && row.cpu_ms_per_step > 0.0 &&
                    row.omp_ms_per_step > 0.0, "CPU/OpenMP scaling measurement invalid");
            require(close_to(row.omp_speedup,
                             row.cpu_ms_per_step / row.omp_ms_per_step, 1.0e-9),
                    "OpenMP speedup calculation mismatch");
            require(std::isfinite(row.omp_max_position_difference_m) &&
                    std::isfinite(row.omp_max_velocity_difference_m_s),
                    "OpenMP difference metrics are not finite");
            if (row.cuda_available) {
                require(std::isfinite(row.cuda_ms_per_step) && row.cuda_ms_per_step > 0.0 &&
                        std::isfinite(row.cuda_max_position_difference_m) &&
                        std::isfinite(row.cuda_max_velocity_difference_m_s),
                        "CUDA was available but its evidence is invalid");
            } else {
                require(std::isnan(row.cuda_ms_per_step),
                        "Unavailable CUDA must be represented as N/A, not failure timing");
            }
        }
        const fs::path scaling_csv = output / "backend_scaling.csv";
        require(lab_gui::export_evidence_csv(scaling, scaling_csv.string()),
                "Scaling CSV export failed");
        validate_numeric_csv(scaling_csv, scaling.rows.size());

        AppState regular;
        lab_gui::ExperimentPresets::load_workbench_scenario(
            regular, lab_gui::PhysicsScenario::Collision);
        for (int i = 0; i < 12; ++i) regular.solver.step();
        regular.current_time = 12.0 * regular.config.dt;
        regular.current_diagnostics = regular.solver.compute_diagnostics();
        lab_gui::CsvExportRow csv_row;
        csv_row.time = regular.current_time;
        csv_row.ek = regular.current_diagnostics.total_kinetic_energy;
        csv_row.ep = regular.current_diagnostics.total_potential_energy;
        csv_row.etotal = regular.current_diagnostics.total_mechanical_energy;
        csv_row.px = regular.current_diagnostics.total_momentum.x;
        csv_row.py = regular.current_diagnostics.total_momentum.y;
        csv_row.active_contacts = regular.current_diagnostics.active_contacts;
        csv_row.step_collision_impulses = regular.current_diagnostics.step_collision_impulses;
        csv_row.max_penetration = regular.current_diagnostics.max_penetration;
        regular.recorded_rows.push_back(csv_row);
        const fs::path regular_csv = output / "regular_run.csv";
        require(regular.export_recorded_csv(regular_csv.string()), "Regular-run CSV export failed");
        validate_numeric_csv(regular_csv, 1);

        std::ofstream summary(output / "summary.json");
        summary << std::setprecision(10)
                << "{\n  \"status\": \"pass\",\n"
                << "  \"selection_combinations\": 15,\n"
                << "  \"collision_cases\": 2,\n"
                << "  \"gravity_cases\": 2,\n"
                << "  \"convergence_levels\": 4,\n"
                << "  \"contact_configurations\": 4,\n"
                << "  \"scaling_workloads\": 4,\n"
                << "  \"cuda_available\": "
                << (scaling.rows.front().cuda_available ? "true" : "false") << "\n}\n";
        require(summary.good(), "Summary JSON write failed");

        std::cout << "Convergence/contact/scaling/CSV verification passed\n";
        std::cout << "Evidence directory: " << output << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Stage 4 verification FAILED: " << error.what() << '\n';
        return 1;
    }
}
