#pragma once
// Stage 4: provenance-backed measurements. No ImGui dependency.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <vector>

namespace lab_gui {

enum class EvidenceStudy { None, ContactSensitivity, BackendScaling };
struct EvidenceRow {
    int particles{0};
    int requested_particles{0};
    int generated_particles{0};
    int physics_steps{0};
    int benchmark_samples{0};
    int substeps{0};
    int contact_iterations{0};
    double simulated_seconds{0};
    double cpu_ms_per_step{0};
    double omp_ms_per_step{0};
    double cuda_ms_per_step{0}; // NaN means not available
    double cpu_total_ms{0};
    double omp_speedup{0};
    double cuda_speedup{0};
    float maximum_penetration_m{0}; // at common final physical time
    float peak_penetration_m{0};    // maximum sampled during the run
    float kinetic_energy_j{0};
    int active_contacts{0};
    float omp_max_position_difference_m{0};
    float cuda_max_position_difference_m{0};
    float omp_max_velocity_difference_m_s{0};
    float cuda_max_velocity_difference_m_s{0};
    bool cuda_available{false};
    bool finite{true};
};

struct WorkbenchEvidence {
    EvidenceStudy kind{EvidenceStudy::None};
    bool valid{false};
    std::string scenario;
    std::string protocol;
    std::string interpretation;
    std::vector<EvidenceRow> rows;
    // Full configuration provenance. These document the input snapshot,
    // not necessarily every parameter that the controlled sweep overrides.
    float dt_s{0};
    int initial_substeps{0};
    int initial_iterations{0};
    float restitution{0};
    float boundary_restitution{0};
    float gravity_x{0};
    float gravity_y{0};
    float boundary_radius_m{0};
    int omp_threads{0};
    int repeats{0};
};

inline double safe_speedup(double baseline, double candidate) {
    return baseline > 0.0 && candidate > 0.0 ? baseline / candidate : 0.0;
}

inline double median_measurement(std::vector<double> samples) {
    if (samples.empty()) return std::numeric_limits<double>::quiet_NaN();
    std::sort(samples.begin(), samples.end());
    const size_t n = samples.size();
    return n % 2 != 0 ? samples[n / 2] : 0.5 * (samples[n / 2 - 1] + samples[n / 2]);
}

inline bool export_evidence_csv(const WorkbenchEvidence& evidence,
                                const std::string& path) {
    if (!evidence.valid || evidence.rows.empty()) return false;
    std::ofstream out(path);
    if (!out) return false;
    out << std::setprecision(10);
    out << "# kind," << (evidence.kind == EvidenceStudy::BackendScaling ? "backend_scaling" : "contact_sensitivity") << '\n';
    out << "# scenario," << evidence.scenario << '\n';
    out << "# protocol," << evidence.protocol << '\n';
    out << "# config_dt_s," << evidence.dt_s << '\n';
    out << "# config_substeps," << evidence.initial_substeps << '\n';
    out << "# config_contact_iterations," << evidence.initial_iterations << '\n';
    out << "# config_restitution," << evidence.restitution << '\n';
    out << "# config_boundary_restitution," << evidence.boundary_restitution << '\n';
    out << "# config_gravity_x," << evidence.gravity_x << '\n';
    out << "# config_gravity_y," << evidence.gravity_y << '\n';
    out << "# config_boundary_radius_m," << evidence.boundary_radius_m << '\n';
    out << "# omp_threads," << evidence.omp_threads << '\n';
    out << "# measured_repeats," << evidence.repeats << '\n';
    out << "particles,requested_particles,generated_particles,physics_steps,benchmark_samples,"
           "substeps,contact_iterations,simulated_seconds,"
           "cpu_total_ms,cpu_ms_per_step,omp_ms_per_step,cuda_ms_per_step,"
           "omp_speedup,cuda_speedup,cuda_available,"
           "max_penetration_m,peak_penetration_m,kinetic_energy_j,active_contacts,"
           "omp_max_position_difference_m,cuda_max_position_difference_m,"
           "omp_max_velocity_difference_m_s,cuda_max_velocity_difference_m_s,finite\n";
    for (const auto& x : evidence.rows) {
        out << x.particles << ',' << x.requested_particles << ',' << x.generated_particles << ','
            << x.physics_steps << ',' << x.benchmark_samples << ',' << x.substeps << ','
            << x.contact_iterations << ',' << x.simulated_seconds << ','
            << x.cpu_total_ms << ',' << x.cpu_ms_per_step << ',' << x.omp_ms_per_step << ','
            << x.cuda_ms_per_step << ',' << x.omp_speedup << ',' << x.cuda_speedup << ','
            << (x.cuda_available ? 1 : 0) << ',' << x.maximum_penetration_m << ','
            << x.peak_penetration_m << ',' << x.kinetic_energy_j << ',' << x.active_contacts << ','
            << x.omp_max_position_difference_m << ','
            << x.cuda_max_position_difference_m << ','
            << x.omp_max_velocity_difference_m_s << ','
            << x.cuda_max_velocity_difference_m_s << ',' << (x.finite ? 1 : 0) << '\n';
    }
    return out.good();
}

} // namespace lab_gui
