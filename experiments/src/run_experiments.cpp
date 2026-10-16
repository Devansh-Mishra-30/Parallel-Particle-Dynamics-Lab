#include "experiments/collision_experiment.h"
#include "experiments/convergence_experiment.h"
#include "experiments/gravity_experiment.h"
#include <filesystem>
#include <iostream>
#include <string>

using namespace lab_experiments;

int main(int argc, char** argv) {
    std::string output_dir = "output/csv";
    std::string exp_target = "all";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--out" && i + 1 < argc) output_dir = argv[++i];
        else if (arg == "--exp" && i + 1 < argc) exp_target = argv[++i];
    }

    std::filesystem::create_directories(output_dir);

    std::cout << "======================================================================\n";
    std::cout << "  PARALLEL PARTICLE DYNAMICS LAB: EXPERIMENT EXECUTION HARNESS\n";
    std::cout << "======================================================================\n";
    std::cout << "Target: " << exp_target << " | Output Directory: " << output_dir << "\n\n";

    if (exp_target == "all" || exp_target == "collision") {
        std::cout << "[1/4] Running Two-Particle Elastic Collision...\n";
        CollisionParams p_el;
        p_el.restitution = 1.0f;
        TwoParticleCollisionExperiment exp_el(p_el);
        exp_el.run();
        std::string csv_el = output_dir + "/collision_elastic.csv";
        exp_el.export_csv(csv_el);
        std::cout << "      Wrote " << csv_el << " (Max momentum error: "
                  << exp_el.get_summary().max_momentum_error << " kg*m/s)\n";

        std::cout << "[2/4] Running Two-Particle Inelastic Collision (e = 0.5)...\n";
        CollisionParams p_inel;
        p_inel.restitution = 0.5f;
        TwoParticleCollisionExperiment exp_inel(p_inel);
        exp_inel.run();
        std::string csv_inel = output_dir + "/collision_inelastic.csv";
        exp_inel.export_csv(csv_inel);
        std::cout << "      Wrote " << csv_inel << " (Actual dEk: "
                  << exp_inel.get_summary().actual_dEk << " J, Theory: "
                  << exp_inel.get_summary().theoretical_dEk << " J)\n";

        std::cout << "[3/4] Running Unequal-Mass Elastic Collision (m2 = 4*m1)...\n";
        CollisionParams p_uneq;
        p_uneq.r1 = 0.02f;
        p_uneq.r2 = 0.04f; // m2 = 4 * m1
        p_uneq.v1 = sim_core::Vec2(2.0f, 0.0f);
        p_uneq.v2 = sim_core::Vec2(0.0f, 0.0f);
        p_uneq.restitution = 1.0f;
        TwoParticleCollisionExperiment exp_uneq(p_uneq);
        exp_uneq.run();
        std::string csv_uneq = output_dir + "/collision_unequal.csv";
        exp_uneq.export_csv(csv_uneq);
        std::cout << "      Wrote " << csv_uneq << " (Final v1: "
                  << exp_uneq.get_data().back().vx1 << " m/s, v2: "
                  << exp_uneq.get_data().back().vx2 << " m/s)\n";
    }

    if (exp_target == "all" || exp_target == "gravity") {
        std::cout << "[4/4] Running Gravity Free Fall & Trajectory Study...\n";
        GravityParams p_grav;
        p_grav.duration = 1.0f;
        GravityFreeMotionExperiment exp_grav(p_grav);
        exp_grav.run();
        std::string csv_grav = output_dir + "/gravity_trajectory.csv";
        exp_grav.export_csv(csv_grav);
        std::cout << "      Wrote " << csv_grav << " (Max position error: "
                  << exp_grav.get_summary().max_pos_error << " m)\n";
    }

    if (exp_target == "all" || exp_target == "convergence") {
        std::cout << "[5/5] Running Timestep Convergence Study...\n";
        ConvergenceParams p_conv;
        p_conv.timesteps = {0.04f, 0.02f, 0.01f, 0.005f, 0.0025f};
        p_conv.duration = 0.2f;
        TimestepConvergenceExperiment exp_conv(p_conv);
        exp_conv.run();
        std::string csv_conv = output_dir + "/convergence_study.csv";
        exp_conv.export_csv(csv_conv);
        std::cout << "      Wrote " << csv_conv << "\n";
        for (const auto& r : exp_conv.get_results()) {
            std::cout << "      dt=" << r.dt << " s | Linf error=" << r.linf_error
                      << " m | Order=" << r.empirical_order << " | Time=" << r.compute_time_ms << " ms\n";
        }
    }

    std::cout << "\nAll requested experiments completed successfully.\n";
    return 0;
}
