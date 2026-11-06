> **Historical reference (2026-10/early 2026-11).** This document records an earlier engineering checkpoint and may contain workload-specific or superseded metrics. For the verified November 6 workbench and current release position, use the [Stage 4 verification report](audit/STAGE_4_VERIFICATION_REPORT.md) and the [main README](../README.md). Do not compare historical speedups directly with the Stage 4 scaling study.

# Parallel Particle Dynamics Lab: Project Progress Log
## Evidence-Based Verification, Bug Fixes, and Development Milestone Ledger

**Workspace:** `/home/devansh/simulator-workspaces/cuda-particle-simulation-v2`
**Branch:** `feature/simulator-v2`
**Current Milestone:** Phase 4 Complete — Educational Release v2.0 Delivered

---

## Milestone Ledger

| Milestone | Phase | Scope | Status | Acceptance Pass Rate |
|:---|:---:|:---|:---:|:---:|
| **M0** | 0 | Codebase Audit & Physics Spec Freeze | COMPLETE | 100% |
| **M1** | 1A | Shared C++17 `sim_core` Physics Engine | COMPLETE | 13/13 (100%) |
| **M2** | 1A.5 | Scientific Experiment Framework & CSV | COMPLETE | 6/6 (100%) |
| **M3** | 1B | Interactive ImGui/ImPlot Desktop GUI | COMPLETE | Delivered & Tested |
| **M4** | 1C | GUI Acceptance & Scientific Verification | COMPLETE | 30/30 (100% CTest + Visual) |
| **M5** | 2 | OpenMP V2 Multicore Solver (`sim_omp_v2`) | COMPLETE | 46/46 (100% Verified) |
| **M6** | 3 | CUDA V2 GPU Solver (RTX 4060) | COMPLETE | 20/20 (100% Verified) |
| **M7** | 4 | Educational Release & Coursework | COMPLETE | 100% (5 Labs + Manual + Plots) |

---

## Detailed Milestone Log

### Milestone M4: Phase 1C — GUI Acceptance & Scientific Diagnostics Verification
- **Status:** COMPLETE (100% Verified)
- **Defects Resolved:**
  1. `BUG-01`: Collision analytical theoretical $\Delta E_k$ desynchronization resolved via immutable pre-impact state tracking and dynamic re-evaluation for arbitrary $e$.
  2. `BUG-02`: Pairwise contact detection implemented in `DiagnosticsCalculator::compute()`, correctly reporting 68 active contacts in dense granular column settling.
  3. `BUG-03`: Potential energy datum explicitly documented and ImPlot Y-axis auto-fitting enabled to prevent negative potential energy clipping.
  4. `BUG-04`: CFL indicator refined into DCD Tunneling Sentinel with clear displacement-to-radius heuristic explanation.
  5. `BUG-05`: Collision initial parameter sliders cleanly reset initial conditions and update analytical targets.
- **Verification Evidence:**
  - Automated Physics Tests: 13/13 passed.
  - Automated Experiment Tests: 6/6 passed.
  - Automated Collision Acceptance Cases: 7/7 passed.
  - Headless GUI Smoke Test: 100 frames passed across 4 presets.
  - Overall CTest Suite: 4/4 suites passed (100%).
  - WSLg Real OpenGL Screenshots: 4 framebuffers captured at 1600x920.
  - Documentation Suite: Full audit, physics verification, GUI acceptance, numerical experiment results, and known limitations generated in `docs/verification/`.

### Milestone M5: Phase 2 — OpenMP Multicore Physics Engine (`sim_omp_v2`)
- **Status:** COMPLETE (100% Verified)
- **Engine Features Implemented:**
  1. Thread-local Jacobi impulse and Baumgarte penetration resolution eliminating data hazards.
  2. OpenMP parallel Symplectic Euler integration and circular boundary enforcement.
  3. Parallel diagnostic reduction for kinetic energy, potential energy, and total linear momentum.
  4. 2D `UniformSpatialGrid` with cell-linked lists ($W \ge 2r_{\max}$) and parallel pair discovery.
  5. Interactive GUI integration (`sim_lab`): live backend switcher (CPU vs OpenMP), dynamic thread slider (1 to 16), broadphase algorithm selector, and real-time compute time display.
- **Verification Evidence:**
  - Automated OpenMP Correctness Suite: 46/46 tests passed (100%) across 1, 2, 4, 8, 16 threads.
  - Broadphase Spatial Grid Suite: Zero false negatives verified across $N=50, 200, 500, 1000$ particles; up to 99.8% candidate culling.
  - ThreadSanitizer Audit: Clean run certifying zero particle state data races.
  - Benchmark Suite: Empirical scaling measured across $N=32..1024$ and $T=1..16$. Peak speedup of $3.99\times$ at $N=512$ (8 threads) and $3.79\times$ at $N=1024$ (8 threads).
  - Overall CTest Suite: 6/6 test suites passed (100%).
  - WSLg Real OpenGL Screenshots: Captured OpenMP Multicore backend active in EXP_01 and EXP_04 at 1600x920.
  - Documentation Suite: Complete HPC documentation delivered in `docs/hpc/` (`OPENMP_ARCHITECTURE.md`, `OPENMP_CORRECTNESS.md`, `OPENMP_BENCHMARKS.md`, `PARALLEL_SCALING.md`).

### Milestone M6: Phase 3 — NVIDIA CUDA GPU Physics Engine (`sim_cuda_v2`)
- **Status:** COMPLETE (100% Verified)
- **Engine Features Implemented:**
  1. High-throughput CUDA V2 backend (`sim_cuda_v2`) targeting NVIDIA Ada Lovelace RTX 4060 Laptop GPU (sm_89, 24 SMs, 3072 CUDA cores, 8188 MB VRAM).
  2. Per-Particle Jacobi gathering eliminating write-write data hazards and atomics; double-buffered ping-pong particle memory.
  3. Parallel Symplectic Euler time integration and circular boundary collision kernels.
  4. GPU-accelerated 2D uniform spatial grid leveraging Thrust radix sorting (`thrust::sort_by_key`) with zero false negatives across sparse, dense, boundary, and polydisperse distributions.
  5. Multi-block shared-memory parallel reduction for real-time diagnostics (momentum, kinetic energy, potential energy, active contacts, penetration depth).
  6. Desktop GUI integration (`sim_lab`): 3-way live backend switcher (`[CPU Sequential]`, `[OpenMP Multicore]`, `[CUDA GPU (RTX 4060)]`), broadphase toggle, real-time GPU telemetry, and educational comparison pane.
  7. Cross-backend verification test harness proving mathematical equivalence across CPU, OpenMP (8T), and CUDA GPU backends.
- **Verification Evidence:**
  - Automated CUDA Correctness Suite (`test_cuda_correctness`): 10/10 tests passed (100%) (constant velocity, ballistic flight, equal/unequal elastic, inelastic $e=0.528$ $\Delta E_k$, oblique, boundary, zero-distance guard, dense heap, 3-run determinism).
  - Automated GPU Spatial Grid Suite (`test_cuda_spatial_grid`): 6/6 distributions passed with 0 false negatives.
  - Automated Cross-Backend Suite (`test_cross_backend_verification`): 4/4 scenarios verified matching across CPU, OpenMP, and CUDA GPU.
  - Nsight Systems Profile: Trace recorded in `output/benchmarks/cuda_profile_report.qdstrm`.
  - Scaling Benchmark (`output/benchmarks/cross_backend_scaling_results.csv`): Peak throughput of 4.88 Million particles/sec at $N=10,000$ ($18.16\times$ speedup over serial spatial grid, $>1,200\times$ over serial all-pairs).
  - Headless GUI Smoke Test: Passed 60 frames under CUDA backend.
  - Overall CTest Suite: 9/9 test suites passed (100%).
  - WSLg Real OpenGL Screenshots: Captured CUDA GPU backend active in EXP_01 and EXP_04 at 1600x920.
  - Documentation Suite: Complete Phase 3 HPC documentation delivered in `docs/hpc/` (`CUDA_ARCHITECTURE.md`, `CUDA_CORRECTNESS.md`, `CUDA_PERFORMANCE.md`, `CPU_OMP_CUDA_COMPARISON.md`, `GPU_PROFILING_REPORT.md`).

### Milestone M7: Phase 4 — Educational Release, Coursework & Workbench Polish
- **Status:** COMPLETE (100% Delivered)
- **Coursework Modules Created (`docs/coursework/`):**
  1. `LAB_01_IMPULSE_COLLISION.md`: Normal impact mechanics, restitution sweep ($e=1.0\dots0.0$), kinetic energy partition $\Delta E_k = -\frac{1}{2}\mu(1-e^2)v_{\text{rel}}^2$, and momentum conservation.
  2. `LAB_02_INTEGRATOR_STABILITY.md`: Discrete truncation error $\Delta y = \frac{1}{2}gt\Delta t$, symplectic phase-space preservation ($\det(J)=1$), and Explicit Forward Euler blowup analysis.
  3. `LAB_03_CONVERGENCE_STUDY.md`: Timestep refinement study ($\Delta t \in \{0.04, 0.02, 0.01, 0.005, 0.0025\}$), $L_2 / L_\infty$ error norms, and empirical order verification ($p \approx 1.000$).
  4. `LAB_04_CONTACT_MECHANICS_TUNNELING.md`: DCD tunneling hazard, Courant displacement sentinel ($v\Delta t/r$), Baumgarte stabilization factor $\beta$, and Gauss-Seidel vs Jacobi contact relaxation.
  5. `LAB_05_HPC_PARALLEL_SCALING.md`: Computing paradigms (CPU vs OpenMP vs CUDA), Amdahl's Law, thread scaling ($T=1\dots16$), CPU latency vs GPU throughput crossover, and algorithmic ($O(N)$) vs hardware acceleration.
  6. `INSTRUCTOR_MANUAL.md`: Full analytical solutions, reference data tables, discussion questions, and 100-point coursework grading rubric.
- **Workbench Improvements & Bug Fixes:**
  - `OPT-01`: Enabled Dear ImGui docking flags (`ImGuiConfigFlags_DockingEnable`) and switched window placement to `ImGuiCond_FirstUseEver` for layout customization and persistence across sessions via `imgui.ini`.
- **HPC Publication Benchmark Plots (`scripts/plot_hpc_benchmarks.py`):**
  - `output/plots/plot_openmp_scaling.png`: OpenMP speedup and parallel efficiency with Amdahl fit.
  - `output/plots/plot_broadphase_breakdown.png`: Algorithmic spatial grid ($23.9\times$) vs multicore OpenMP ($4.4\times$).
  - `output/plots/plot_cross_backend_scaling.png`: Execution time and throughput scaling across CPU, OpenMP 8T, and NVIDIA RTX 4060 GPU.
- **Verification:** All 9/9 CTest test suites pass 100%. Headless smoke tests pass across all scenarios.
