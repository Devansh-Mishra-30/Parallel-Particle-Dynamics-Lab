#include "sim_omp_v2/omp_solver.h"
#include "sim_omp_v2/spatial_grid.h"

#include <omp.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace sim_omp {

OmpSolverV2::OmpSolverV2(const sim_core::SimConfig& config, const sim_core::CircularBoundary& boundary)
    : config_(config), boundary_(boundary) {
    num_threads_ = std::max(1, omp_get_max_threads());
    metrics_.active_threads = num_threads_;
}

void OmpSolverV2::set_num_threads(int threads) {
    num_threads_ = (threads > 0) ? threads : std::max(1, omp_get_max_threads());
    metrics_.active_threads = num_threads_;
    resize_thread_buffers(particles_.size(), num_threads_);
}

void OmpSolverV2::set_config(const sim_core::SimConfig& config) {
    config_ = config;
}

void OmpSolverV2::add_particle(const sim_core::Particle& p) {
    particles_.push_back(p);
    resize_thread_buffers(particles_.size(), num_threads_);
}

const std::vector<sim_core::Particle>& OmpSolverV2::get_particles() const {
    return particles_;
}

std::vector<sim_core::Particle>& OmpSolverV2::get_particles() {
    return particles_;
}

void OmpSolverV2::set_particles(const std::vector<sim_core::Particle>& particles) {
    particles_ = particles;
    resize_thread_buffers(particles_.size(), num_threads_);
}

void OmpSolverV2::reset() {
    particles_.clear();
    resize_thread_buffers(0, num_threads_);
}

void OmpSolverV2::resize_thread_buffers(size_t n, int threads) {
    const int t_count = std::max(1, threads);
    thread_dv_.resize(t_count);
    thread_dx_.resize(t_count);
    thread_contact_counts_.resize(t_count);

    for (int t = 0; t < t_count; ++t) {
        thread_dv_[t].resize(n, sim_core::Vec2(0.0f, 0.0f));
        thread_dx_[t].resize(n, sim_core::Vec2(0.0f, 0.0f));
        thread_contact_counts_[t].resize(n, 0);
    }
}

void OmpSolverV2::integrate_particles_omp(float dt_sub) {
    const size_t n = particles_.size();
    if (n == 0) return;

    if (config_.integrator == sim_core::IntegratorType::ExplicitEuler) {
        #pragma omp parallel for num_threads(num_threads_) schedule(static)
        for (size_t i = 0; i < n; ++i) {
            sim_core::Integrator::step_forward_euler(particles_[i], config_.gravity, dt_sub);
        }
    } else {
        #pragma omp parallel for num_threads(num_threads_) schedule(static)
        for (size_t i = 0; i < n; ++i) {
            sim_core::Integrator::integrate_velocity(particles_[i], config_.gravity, dt_sub);
            sim_core::Integrator::integrate_position(particles_[i], dt_sub);
        }
    }
}

void OmpSolverV2::handle_boundaries_omp() {
    const size_t n = particles_.size();
    if (n == 0) return;

    #pragma omp parallel for num_threads(num_threads_) schedule(static)
    for (size_t i = 0; i < n; ++i) {
        sim_core::BoundaryHandler::apply_circular_boundary(particles_[i], boundary_, config_.boundary_restitution);
    }
}

void OmpSolverV2::resolve_contacts_all_pairs_gauss_seidel() {
    const size_t n = particles_.size();
    if (n < 2) return;

    const int iterations = (config_.solver_iterations > 0) ? config_.solver_iterations : 1;
    for (int iter = 0; iter < iterations; ++iter) {
        for (size_t i = 0; i < n; ++i) {
            for (size_t j = i + 1; j < n; ++j) {
                sim_core::ContactPair contact;
                if (sim_core::ContactSolver::detect_contact(particles_[i], particles_[j], contact)) {
                    sim_core::ContactSolver::resolve_collision_impulse(particles_[i], particles_[j], contact, config_.restitution);
                    sim_core::ContactSolver::resolve_penetration_stabilization(particles_[i], particles_[j], contact, config_.stabilization_factor);
                }
            }
        }
    }
}

void OmpSolverV2::resolve_contacts_all_pairs_jacobi() {
    const size_t n = particles_.size();
    if (n < 2) return;

    resize_thread_buffers(n, num_threads_);

    const int iterations = (config_.solver_iterations > 0) ? config_.solver_iterations : 1;
    const float restitution = config_.restitution;
    const float stabilization_factor = config_.stabilization_factor;

    #pragma omp parallel num_threads(num_threads_)
    {
        const int tid = omp_get_thread_num();
        auto& local_dv = thread_dv_[tid];
        auto& local_dx = thread_dx_[tid];
        auto& local_c = thread_contact_counts_[tid];

        for (int iter = 0; iter < iterations; ++iter) {
            // 1. Thread zeroes its private buffer
            std::fill(local_dv.begin(), local_dv.end(), sim_core::Vec2(0.0f, 0.0f));
            std::fill(local_dx.begin(), local_dx.end(), sim_core::Vec2(0.0f, 0.0f));
            std::fill(local_c.begin(), local_c.end(), 0);

            #pragma omp barrier

            // 2. Parallel Pairwise Contact Evaluation (Dynamic chunking for upper triangular matrix)
            #pragma omp for schedule(dynamic, 16)
            for (size_t i = 0; i < n; ++i) {
                for (size_t j = i + 1; j < n; ++j) {
                    sim_core::ContactPair contact;
                    if (sim_core::ContactSolver::detect_contact(particles_[i], particles_[j], contact)) {
                        // Collision normal impulse
                        const float v_n = sim_core::Vec2::dot(particles_[i].velocity - particles_[j].velocity, contact.normal);
                        if (v_n < 0.0f) {
                            const float inv_m_sum = particles_[i].inv_mass + particles_[j].inv_mass;
                            if (inv_m_sum > 0.0f) {
                                const float j_n = -(1.0f + restitution) * v_n / inv_m_sum;
                                const sim_core::Vec2 impulse_a = (j_n * particles_[i].inv_mass) * contact.normal;
                                const sim_core::Vec2 impulse_b = -(j_n * particles_[j].inv_mass) * contact.normal;
                                local_dv[i] += impulse_a;
                                local_dv[j] += impulse_b;
                            }
                        }

                        // Mass-weighted position stabilization
                        const sim_core::Vec2 r_ab = particles_[i].position - particles_[j].position;
                        const float min_dist = particles_[i].radius + particles_[j].radius;
                        const float dist = r_ab.length();
                        const float delta = min_dist - dist;
                        if (delta > 0.0f) {
                            constexpr float EPS = 1e-6f;
                            const sim_core::Vec2 n_geom = (dist > EPS) ? (r_ab / dist) : contact.normal;
                            const float inv_m_sum = particles_[i].inv_mass + particles_[j].inv_mass;
                            if (inv_m_sum > 0.0f) {
                                const float correction = stabilization_factor * delta / inv_m_sum;
                                const sim_core::Vec2 corr_a = (correction * particles_[i].inv_mass) * n_geom;
                                const sim_core::Vec2 corr_b = -(correction * particles_[j].inv_mass) * n_geom;
                                local_dx[i] += corr_a;
                                local_dx[j] += corr_b;
                            }
                        }

                        local_c[i]++;
                        local_c[j]++;
                    }
                }
            }

            // Barrier before reduction
            #pragma omp barrier

            // 3. Parallel Reduction Pass across threads for each particle
            #pragma omp for schedule(static)
            for (size_t i = 0; i < n; ++i) {
                sim_core::Vec2 sum_dv(0.0f, 0.0f);
                sim_core::Vec2 sum_dx(0.0f, 0.0f);
                int sum_c = 0;

                for (int t = 0; t < num_threads_; ++t) {
                    sum_dv += thread_dv_[t][i];
                    sum_dx += thread_dx_[t][i];
                    sum_c += thread_contact_counts_[t][i];
                }

                if (sum_c > 0) {
                    particles_[i].velocity += sum_dv;
                    particles_[i].position += sum_dx;
                }
            }

            // Barrier before next iteration
            #pragma omp barrier
        }
    }

    metrics_.collision_pairs_checked = (n * (n - 1)) / 2;
}

void OmpSolverV2::resolve_contacts_spatial_grid_jacobi() {
    const size_t n = particles_.size();
    if (n < 2) return;

    resize_thread_buffers(n, num_threads_);

    // 1. Determine cell size: 2 * max_radius
    float max_r = 0.01f;
    for (size_t i = 0; i < n; ++i) {
        if (particles_[i].radius > max_r) max_r = particles_[i].radius;
    }

    const auto t_bp_start = std::chrono::high_resolution_clock::now();
    UniformSpatialGrid grid(2.0f * max_r);
    grid.build(particles_);

    std::vector<PotentialPair> candidate_pairs;
    grid.find_potential_pairs_omp(particles_, candidate_pairs, num_threads_);
    const auto t_bp_end = std::chrono::high_resolution_clock::now();
    metrics_.broadphase_time_ms += std::chrono::duration<double, std::milli>(t_bp_end - t_bp_start).count();
    metrics_.collision_pairs_checked = candidate_pairs.size();

    const size_t num_pairs = candidate_pairs.size();
    if (num_pairs == 0) return;

    const int iterations = (config_.solver_iterations > 0) ? config_.solver_iterations : 1;
    const float restitution = config_.restitution;
    const float stabilization_factor = config_.stabilization_factor;

    #pragma omp parallel num_threads(num_threads_)
    {
        const int tid = omp_get_thread_num();
        auto& local_dv = thread_dv_[tid];
        auto& local_dx = thread_dx_[tid];
        auto& local_c = thread_contact_counts_[tid];

        for (int iter = 0; iter < iterations; ++iter) {
            std::fill(local_dv.begin(), local_dv.end(), sim_core::Vec2(0.0f, 0.0f));
            std::fill(local_dx.begin(), local_dx.end(), sim_core::Vec2(0.0f, 0.0f));
            std::fill(local_c.begin(), local_c.end(), 0);

            #pragma omp barrier

            #pragma omp for schedule(dynamic, 32)
            for (size_t k = 0; k < num_pairs; ++k) {
                const size_t i = candidate_pairs[k].i;
                const size_t j = candidate_pairs[k].j;

                sim_core::ContactPair contact;
                if (sim_core::ContactSolver::detect_contact(particles_[i], particles_[j], contact)) {
                    // Impulse
                    const float v_n = sim_core::Vec2::dot(particles_[i].velocity - particles_[j].velocity, contact.normal);
                    if (v_n < 0.0f) {
                        const float inv_m_sum = particles_[i].inv_mass + particles_[j].inv_mass;
                        if (inv_m_sum > 0.0f) {
                            const float j_n = -(1.0f + restitution) * v_n / inv_m_sum;
                            const sim_core::Vec2 impulse_a = (j_n * particles_[i].inv_mass) * contact.normal;
                            const sim_core::Vec2 impulse_b = -(j_n * particles_[j].inv_mass) * contact.normal;
                            local_dv[i] += impulse_a;
                            local_dv[j] += impulse_b;
                        }
                    }

                    // Position stabilization
                    const sim_core::Vec2 r_ab = particles_[i].position - particles_[j].position;
                    const float min_dist = particles_[i].radius + particles_[j].radius;
                    const float dist = r_ab.length();
                    const float delta = min_dist - dist;
                    if (delta > 0.0f) {
                        constexpr float EPS = 1e-6f;
                        const sim_core::Vec2 n_geom = (dist > EPS) ? (r_ab / dist) : contact.normal;
                        const float inv_m_sum = particles_[i].inv_mass + particles_[j].inv_mass;
                        if (inv_m_sum > 0.0f) {
                            const float correction = stabilization_factor * delta / inv_m_sum;
                            const sim_core::Vec2 corr_a = (correction * particles_[i].inv_mass) * n_geom;
                            const sim_core::Vec2 corr_b = -(correction * particles_[j].inv_mass) * n_geom;
                            local_dx[i] += corr_a;
                            local_dx[j] += corr_b;
                        }
                    }

                    local_c[i]++;
                    local_c[j]++;
                }
            }

            #pragma omp barrier

            #pragma omp for schedule(static)
            for (size_t i = 0; i < n; ++i) {
                sim_core::Vec2 sum_dv(0.0f, 0.0f);
                sim_core::Vec2 sum_dx(0.0f, 0.0f);
                int sum_c = 0;

                for (int t = 0; t < num_threads_; ++t) {
                    sum_dv += thread_dv_[t][i];
                    sum_dx += thread_dx_[t][i];
                    sum_c += thread_contact_counts_[t][i];
                }

                if (sum_c > 0) {
                    particles_[i].velocity += sum_dv;
                    particles_[i].position += sum_dx;
                }
            }

            #pragma omp barrier
        }
    }
}

void OmpSolverV2::substep(float dt_sub) {
    const size_t n = particles_.size();
    if (n == 0) return;

    // 1. Integration
    const auto t0 = std::chrono::high_resolution_clock::now();
    integrate_particles_omp(dt_sub);
    const auto t1 = std::chrono::high_resolution_clock::now();

    // 2. Contact Resolution
    if (contact_mode_ == ContactMode::Jacobi) {
        if (broadphase_mode_ == BroadphaseMode::SpatialGrid) {
            resolve_contacts_spatial_grid_jacobi();
        } else {
            resolve_contacts_all_pairs_jacobi();
        }
    } else {
        resolve_contacts_all_pairs_gauss_seidel();
    }
    const auto t2 = std::chrono::high_resolution_clock::now();

    // 3. Boundary Constraints
    handle_boundaries_omp();
    const auto t3 = std::chrono::high_resolution_clock::now();

    // Accumulate micro-timings
    metrics_.integrate_time_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
    metrics_.resolve_time_ms += std::chrono::duration<double, std::milli>(t2 - t1).count();
    metrics_.boundary_time_ms += std::chrono::duration<double, std::milli>(t3 - t2).count();
}

void OmpSolverV2::step() {
    const int s = (config_.substeps > 0) ? config_.substeps : 1;
    const float dt_sub = config_.dt_substep();

    metrics_.integrate_time_ms = 0.0;
    metrics_.broadphase_time_ms = 0.0;
    metrics_.narrowphase_time_ms = 0.0;
    metrics_.resolve_time_ms = 0.0;
    metrics_.boundary_time_ms = 0.0;

    const auto t_start = std::chrono::high_resolution_clock::now();
    for (int step_idx = 0; step_idx < s; ++step_idx) {
        substep(dt_sub);
    }
    const auto t_end = std::chrono::high_resolution_clock::now();

    metrics_.total_step_time_ms = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    metrics_.active_threads = num_threads_;
}

sim_core::SimulationDiagnostics OmpSolverV2::compute_diagnostics() const {
    sim_core::SimulationDiagnostics diag;
    const size_t n = particles_.size();
    if (n == 0) return diag;

    float total_mass = 0.0f;
    sim_core::Vec2 mass_weighted_pos(0.0f, 0.0f);
    sim_core::Vec2 total_p(0.0f, 0.0f);
    float total_ek = 0.0f;
    float total_ep = 0.0f;
    int nan_count = 0;

    // OpenMP parallel reduction for single-particle integrals
    #pragma omp parallel for num_threads(num_threads_) schedule(static) \
        reduction(+:total_mass, total_ek, total_ep, nan_count)
    for (size_t i = 0; i < n; ++i) {
        const auto& p = particles_[i];
        if (!p.position.is_finite() || !p.velocity.is_finite()) {
            nan_count++;
            continue;
        }

        total_mass += p.mass;
        total_ek += 0.5f * p.mass * p.velocity.length_squared();
        total_ep -= p.mass * sim_core::Vec2::dot(config_.gravity, p.position);
    }

    // Accumulate total momentum and mass-weighted center of mass
    for (size_t i = 0; i < n; ++i) {
        const auto& p = particles_[i];
        if (p.position.is_finite() && p.velocity.is_finite()) {
            total_p += p.mass * p.velocity;
            mass_weighted_pos += p.mass * p.position;
        }
    }

    diag.nan_count = nan_count;
    diag.is_finite = (nan_count == 0);
    diag.total_momentum = total_p;
    diag.total_kinetic_energy = total_ek;
    diag.total_potential_energy = total_ep;
    diag.total_mechanical_energy = total_ek + total_ep;
    if (total_mass > 0.0f) {
        diag.center_of_mass = mass_weighted_pos / total_mass;
    }

    // Pairwise contact counts and penetration
    int active_contacts = 0;
    float max_penetration = 0.0f;

    #pragma omp parallel for num_threads(num_threads_) schedule(dynamic, 16) \
        reduction(+:active_contacts) reduction(max:max_penetration)
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            const float dist = (particles_[i].position - particles_[j].position).length();
            const float r_sum = particles_[i].radius + particles_[j].radius;
            if (dist < r_sum) {
                active_contacts++;
                const float pen = r_sum - dist;
                if (pen > max_penetration) {
                    max_penetration = pen;
                }
            }
        }
    }

    diag.active_contacts = active_contacts;
    diag.max_penetration = max_penetration;
    return diag;
}

} // namespace sim_omp
