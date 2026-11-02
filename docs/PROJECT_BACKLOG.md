# Parallel Particle Dynamics Lab: Project Backlog
## Issue Tracking, Scientific Validation Gaps, and Feature Roadmap

**Status Date:** 2026-10-16
**Target Branch:** `feature/simulator-v2`
**Classification:** CRITICAL, HIGH, MEDIUM, LOW

---

## 1. Critical Priority (Scientific Correctness & Core Engine)

- [x] **[BUG-01] Collision Analytical $\Delta E_k$ Discrepancy & State Desynchronization**
  - **Component:** `lab_gui/experiment_presets.h`, `lab_gui/controls_panel.cpp`, `lab_gui/diagnostics_panel.cpp`, `tests/test_collision_acceptance.cpp`.
  - **Symptom:** In inelastic collisions ($e = 0.528$), kinetic energy dropped from $6.6\,\text{J}$ to $1.82\,\text{J}$, but the analytical panel reported `Theoretical ΔEk: 0.0000 J`.
  - **Root Cause:** Theoretical $\Delta E_k$ was evaluated only once at startup ($e=1.0 \implies \Delta E_k = 0$) and never re-evaluated when $e$ was adjusted. When velocities were updated, it sampled current post-collision velocities rather than immutable pre-impact states. In oblique collisions, contact normal was evaluated at $t=0$ positions rather than contact distance $r_1+r_2$.
  - **Resolution:** Implemented `InitialCollisionSetup` storing immutable pre-impact states ($\mathbf{x}_1^0, \mathbf{v}_1^0, m_1, \mathbf{x}_2^0, \mathbf{v}_2^0, m_2, b$). Recompute theoretical post-impact velocities and energy dissipation $\Delta E_k = -0.5 \mu (1 - e^2) v_n^2$ dynamically whenever $e$, radii, or velocities change. Verified across 7 test cases with $<10^{-6}\,\text{J}$ error.

- [x] **[BUG-02] Omission of Pairwise Contact Detection in Diagnostics Calculator**
  - **Component:** `sim_core/src/diagnostics.cpp` (`DiagnosticsCalculator::compute`).
  - **Symptom:** `SimulationDiagnostics::active_contacts` was always 0 and `max_penetration` was always 0.000 mm regardless of particle crowding.
  - **Root Cause:** `compute()` only looped over individual particles for mass, momentum, and kinetic energy, completely omitting the $O(N^2)$ pairwise overlap detection loop.
  - **Resolution:** Added pairwise distance check $\forall i < j$: detects distance $< r_i + r_j$, increments `active_contacts`, and computes $\max(r_i + r_j - d)$. Verified in EXP_04: correctly reports 68 active pairwise contacts and $0.255\,\text{mm}$ penetration.

- [x] **[BUG-03] Potential Energy Datum & Y-Axis Plot Clipping**
  - **Component:** `sim_core/src/diagnostics.cpp`, `lab_gui/src/diagnostics_panel.cpp`.
  - **Symptom:** Gravitational potential energy $E_p$ referenced to $y=0$ becomes negative when particles fall below the container center, and was clipped by the energy graph.
  - **Root Cause:** Graph lacked Y-axis auto-fitting or clear datum documentation.
  - **Resolution:** Added clear UI datum label: `Potential energy datum: Ep = -m*(g.x), referenced to origin y = 0 m`. Enabled `ImPlotAxisFlags_AutoFit` on the ImPlot Y-axis, rendering negative potential energy regimes seamlessly.

---

## 2. High Priority (Numerical Verification & Interface Integrity)

- [x] **[BUG-04] CFL Indicator Misleading Terminology**
  - **Component:** `lab_gui/src/diagnostics_panel.cpp`, `lab_gui/src/viewport.cpp`.
  - **Resolution:** Clarified that $v_{\max}\Delta t_{\text{sub}} / r_{\min}$ is a displacement-to-radius tunneling heuristic for discrete collision detection (DCD), not a universal mathematical CFL condition. Added explicit UI disclaimer and color-coded stability thresholds.

- [x] **[FEAT-01] Multi-Timestep Convergence Study (EXP_03)**
  - **Component:** `lab_gui/src/controls_panel.cpp`, `lab_gui/src/diagnostics_panel.cpp`, `experiments/src/convergence_experiment.cpp`.
  - **Resolution:** Implemented automated multi-timestep sweep ($\Delta t = 0.04\,\text{s}, 0.02\,\text{s}, 0.01\,\text{s}, 0.005\,\text{s}$) comparing against exact analytical projectile reference. Tabulates $L_2$ errors, reduction ratios ($2.00$), and confirms empirical order $p \approx 1.00$.

- [x] **[BUG-05] Parameter Mutation While Simulating**
  - **Component:** `lab_gui/src/controls_panel.cpp`.
  - **Resolution:** Connected collision setup sliders (radii, masses, velocities, impact parameter $b$) to automatically reinitialize particle positions and update analytical baselines cleanly without physics engine corruption.

---

## 3. Medium Priority (Data Provenance & Usability)

- [x] **[ENH-01] Scientific CSV Schema Standardization**
  - **Component:** `lab_gui/include/lab_gui/app_state.h`, `experiments/include/experiments/experiment_base.h`.
  - **Resolution:** Exported CSV files now include comprehensive commented metadata headers: experiment ID, integrator name, physical duration, frame dt, substeps, effective dt_sub, restitution, wall restitution, gravity vector, particle count, and SI column definitions.

- [x] **[ENH-02] OpenGL Framebuffer Screenshot Capture Engine**
  - **Component:** `lab_gui/src/app.cpp`, `lab_gui/src/main.cpp`, `lab_gui/src/screenshot_util.cpp`.
  - **Resolution:** Integrated `stb_image_write` and `glReadPixels` supporting CLI `--screenshot <path>` flag and HUD button. Successfully captured 4 genuine application screenshots in WSLg at 1600x920.

---

## 4. Phase 2 & Phase 3 Roadmap (Post-Acceptance)

- [x] **[PAR-01] OpenMP V2 Multicore Solver (`sim_omp_v2`)**
  - **Component:** `sim_omp_v2/`, `lab_gui/`, `tests/test_omp_correctness.cpp`, `tests/test_spatial_grid.cpp`.
  - **Resolution:** Delivered thread-safe Jacobi contact resolution with thread-private accumulator buffers, parallel Symplectic Euler integration, parallel boundary projection, and diagnostic reduction. Implemented 2D `UniformSpatialGrid` with zero false negatives (culling up to 99.8% of non-colliding pairs). Integrated live solver backend switcher into desktop GUI. Verified 46/46 correctness tests across 1 to 16 threads. Achieved up to 3.99x speedup on 8 threads.
- [x] **[PAR-02] CUDA V2 GPU Solver (`sim_cuda_v2`)**
  - **Component:** `sim_cuda_v2/`, `lab_gui/`, `tests/test_cuda_correctness.cpp`, `tests/test_cuda_spatial_grid.cpp`, `tests/test_cross_backend_verification.cpp`.
  - **Resolution:** Delivered high-throughput CUDA backend targeting NVIDIA RTX 4060 Laptop GPU (Ada Lovelace, sm_89). Implemented race-free Per-Particle Jacobi gathering with double-buffered particle arrays, GPU uniform spatial hash grid via `thrust::sort_by_key` with zero false negatives across 6 physical regimes, and multi-block shared-memory parallel reduction for real-time diagnostics. Scaled to $4.88\times 10^6$ particles/sec at $N=10,000$ ($18.16\times$ speedup over serial spatial grid). Integrated 3-way live backend switcher into `sim_lab` GUI. Verified 10/10 correctness tests and 4/4 cross-backend consistency tests.
- [x] **[OPT-01] Docking Layout Persistence & Customization**
  - **Component:** `lab_gui/src/main.cpp`, `lab_gui/src/app.cpp`.
  - **Resolution:** Enabled `ImGuiConfigFlags_DockingEnable` in Dear ImGui context and transitioned panel positioning from `ImGuiCond_Always` to `ImGuiCond_FirstUseEver`. The desktop workbench now allows full docking, window detachment, and user resizing while persisting custom panel configurations to `imgui.ini` across sessions.
