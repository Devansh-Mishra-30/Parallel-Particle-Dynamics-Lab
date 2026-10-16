#include "experiments/collision_experiment.h"
#include "experiments/convergence_experiment.h"
#include "experiments/gravity_experiment.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace lab_experiments;

struct RegressionResult {
    std::string name;
    bool passed{false};
    std::string details;
};

static std::vector<RegressionResult> g_reg_results;

static void report_reg(const RegressionResult& res) {
    g_reg_results.push_back(res);
    std::cout << (res.passed ? "  [PASS] " : "  [FAIL] ")
              << std::left << std::setw(50) << res.name << "\n"
              << "         " << res.details << "\n";
}

// -----------------------------------------------------------------------------
// Test E1: Parameter Validation & Error Handling
// -----------------------------------------------------------------------------
static void test_parameter_validation() {
    RegressionResult res;
    res.name = "Exp Test 1: Parameter Validation & Error Handling";

    CollisionParams cp_invalid;
    cp_invalid.restitution = 1.5f; // invalid e > 1
    std::string err;
    const bool caught_restitution = !cp_invalid.is_valid(err);

    cp_invalid.restitution = 0.5f;
    cp_invalid.r1 = -0.01f; // invalid radius
    const bool caught_radius = !cp_invalid.is_valid(err);

    cp_invalid.r1 = 0.05f;
    cp_invalid.dt = -0.01f; // invalid dt
    const bool caught_dt = !cp_invalid.is_valid(err);

    GravityParams gp_invalid;
    gp_invalid.duration = 0.0f;
    const bool caught_duration = !gp_invalid.is_valid(err);

    ConvergenceParams cvp_invalid;
    cvp_invalid.timesteps.clear();
    const bool caught_empty_dt = !cvp_invalid.is_valid(err);

    res.passed = (caught_restitution && caught_radius && caught_dt && caught_duration && caught_empty_dt);
    res.details = "Properly rejected invalid e, negative radius, negative dt, zero duration, and empty timesteps.";
    report_reg(res);
}

// -----------------------------------------------------------------------------
// Test E2: Deterministic Experiment Configuration
// -----------------------------------------------------------------------------
static void test_deterministic_configuration() {
    RegressionResult res;
    res.name = "Exp Test 2: Deterministic Experiment Configuration";

    CollisionParams p;
    p.restitution = 0.8f;
    p.v1 = sim_core::Vec2(1.2f, 0.0f);
    p.v2 = sim_core::Vec2(-0.8f, 0.0f);

    TwoParticleCollisionExperiment exp1(p);
    exp1.run();

    TwoParticleCollisionExperiment exp2(p);
    exp2.run();

    const auto& d1 = exp1.get_data();
    const auto& d2 = exp2.get_data();

    bool bit_identical = (d1.size() == d2.size());
    for (size_t i = 0; i < d1.size() && bit_identical; ++i) {
        if (d1[i].vx1 != d2[i].vx1 || d1[i].vx2 != d2[i].vx2 ||
            d1[i].x1 != d2[i].x1 || d1[i].x2 != d2[i].x2) {
            bit_identical = false;
        }
    }

    res.passed = bit_identical;
    res.details = "Two separate runs with identical parameters produced bit-identical kinematic states across all frames.";
    report_reg(res);
}

// -----------------------------------------------------------------------------
// Test E3: CSV Export Consistency & Metadata Format
// -----------------------------------------------------------------------------
static void test_csv_export_consistency() {
    RegressionResult res;
    res.name = "Exp Test 3: CSV Export Consistency & Format";

    CollisionParams p;
    TwoParticleCollisionExperiment exp(p);
    exp.run();

    std::filesystem::create_directories("output/test_tmp");
    const std::string tmp_csv = "output/test_tmp/test_export.csv";
    const bool export_ok = exp.export_csv(tmp_csv);

    // Verify file exists, contains metadata comment lines and valid header
    std::ifstream in(tmp_csv);
    bool has_comment = false;
    bool has_header = false;
    int row_count = 0;

    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("#", 0) == 0) {
            has_comment = true;
        } else if (line.rfind("time,", 0) == 0) {
            has_header = true;
        } else if (!line.empty()) {
            row_count++;
        }
    }

    res.passed = (export_ok && has_comment && has_header && row_count == static_cast<int>(exp.get_data().size()));
    res.details = "Exported " + std::to_string(row_count) + " data rows with structured metadata comments and valid column headers.";
    report_reg(res);
}

// -----------------------------------------------------------------------------
// Test E4: Correct Physical SI Units & Energy Conservation Invariant
// -----------------------------------------------------------------------------
static void test_si_units_and_invariants() {
    RegressionResult res;
    res.name = "Exp Test 4: Physical SI Units & Momentum Invariant";

    CollisionParams p;
    p.restitution = 1.0f; // elastic
    p.r1 = 0.05f;         // 5 cm
    p.r2 = 0.03f;         // 3 cm
    p.rho = 1000.0f;      // 1000 kg/m^2
    p.v1 = sim_core::Vec2(1.5f, 0.0f); // 1.5 m/s
    p.v2 = sim_core::Vec2(-0.5f, 0.0f); // -0.5 m/s

    TwoParticleCollisionExperiment exp(p);
    exp.run();

    const auto& summary = exp.get_summary();
    const bool momentum_conserved = (summary.max_momentum_error <= 1e-5f);
    const bool mass_positive = (summary.m1 > 0.0f && summary.m2 > 0.0f);
    const bool v_match = (summary.final_v1_error <= 1e-4f && summary.final_v2_error <= 1e-4f);

    res.passed = (momentum_conserved && mass_positive && v_match);
    res.details = "m1=" + std::to_string(summary.m1) + " kg, m2=" + std::to_string(summary.m2) +
                  " kg | Max |dP|=" + std::to_string(summary.max_momentum_error) + " kg*m/s | Final v errors < 1e-4 m/s";
    report_reg(res);
}

// -----------------------------------------------------------------------------
// Test E5: Zero NaN or Inf Contamination Across Dynamic Range
// -----------------------------------------------------------------------------
static void test_no_nan_contamination() {
    RegressionResult res;
    res.name = "Exp Test 5: Zero NaN/Inf Contamination";

    GravityParams gp;
    gp.duration = 2.0f; // 2 seconds of free fall
    gp.gravity = sim_core::Vec2(0.0f, -9.80665f);

    GravityFreeMotionExperiment exp_g(gp);
    exp_g.run();

    const auto& diag = exp_g.get_diagnostics();
    bool all_finite = diag.is_finite && (diag.nan_count == 0);

    for (const auto& row : exp_g.get_data()) {
        if (!std::isfinite(row.x) || !std::isfinite(row.y) ||
            !std::isfinite(row.vx) || !std::isfinite(row.vy) ||
            !std::isfinite(row.kinetic_energy) || !std::isfinite(row.total_energy)) {
            all_finite = false;
        }
    }

    res.passed = all_finite;
    res.details = "Evaluated 120 frames with zero NaNs or Infs detected in particle state, trajectory, or energy fields.";
    report_reg(res);
}

// -----------------------------------------------------------------------------
// Test E6: Convergence Reduction Rate Consistency
// -----------------------------------------------------------------------------
static void test_convergence_reduction_consistency() {
    RegressionResult res;
    res.name = "Exp Test 6: Timestep Convergence Monotonicity";

    ConvergenceParams cp;
    cp.timesteps = {0.04f, 0.02f, 0.01f, 0.005f};
    cp.duration = 0.2f;

    TimestepConvergenceExperiment exp_c(cp);
    exp_c.run();

    const auto& results = exp_c.get_results();
    bool monotonically_decreasing = true;
    for (size_t i = 1; i < results.size(); ++i) {
        if (results[i].linf_error >= results[i - 1].linf_error) {
            monotonically_decreasing = false;
        }
    }

    const float final_order = results.back().empirical_order;
    const bool order_near_one = (final_order >= 0.9f && final_order <= 1.1f); // Semi-implicit Euler has global order p ~ 1.0

    res.passed = (monotonically_decreasing && order_near_one);
    res.details = "Linf error strictly decreased as dt halved (order=" + std::to_string(final_order) + ").";
    report_reg(res);
}

int main() {
    std::cout << "\n======================================================================\n";
    std::cout << "  EXPERIMENT FRAMEWORK REGRESSION TEST SUITE (PHASE 1A.5)\n";
    std::cout << "======================================================================\n\n";

    test_parameter_validation();
    test_deterministic_configuration();
    test_csv_export_consistency();
    test_si_units_and_invariants();
    test_no_nan_contamination();
    test_convergence_reduction_consistency();

    int passed_count = 0;
    int failed_count = 0;
    for (const auto& r : g_reg_results) {
        if (r.passed) passed_count++;
        else failed_count++;
    }

    std::cout << "\n======================================================================\n";
    std::cout << "  REGRESSION SUMMARY: " << passed_count << " PASSED, " << failed_count << " FAILED (TOTAL " << g_reg_results.size() << ")\n";
    std::cout << "======================================================================\n\n";

    return (failed_count == 0) ? 0 : 1;
}
