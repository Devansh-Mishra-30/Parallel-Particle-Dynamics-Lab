# Parallel Particle Dynamics Lab: Release Notes v2.0-rc1
## High-Performance Multi-Backend Scientific Simulation Workbench

**Release Version:** `v2.0-rc1`
**Workspace:** `/home/devansh/simulator-workspaces/cuda-particle-simulation-v2`
**Git Branch:** `feature/simulator-v2`
**Target Architecture:** x86_64 Linux (WSL2 / Ubuntu 22.04 LTS), C++17, OpenMP 4.5+, CUDA 12.0+ (sm_89)
**Reference Hardware:** Intel Core i7-13620H (10 Cores / 16 Threads) + NVIDIA GeForce RTX 4060 Laptop GPU (8 GB GDDR6)

---

### Executive Overview

Release `v2.0-rc1` delivers the completed **Parallel Particle Dynamics Lab**, a multi-backend simulation workbench designed for mechanical engineering coursework, computational mechanics research, and high-performance computing education.

Students and researchers can seamlessly simulate, interact with, and quantitatively analyze identical physical dynamics across three major computing paradigms:
1. **Sequential CPU Reference (`sim_core`):** Single-threaded deterministic baseline utilizing sequential Gauss-Seidel contact relaxation.
2. **OpenMP Multicore CPU (`sim_omp_v2`):** Shared-memory MIMD parallelism with thread-local Jacobi contact accumulation and parallel spatial hashing.
3. **NVIDIA CUDA GPU (`sim_cuda_v2`):** Massive SIMT parallelism with per-particle Jacobi gathering, Thrust radix-sorted uniform spatial hashing, and multi-block shared memory diagnostic reduction.

Every backend operates under the exact same physical model: Symplectic Euler-Cromer integration, mass-density relations ($m = \rho \pi r^2$), momentum-conserving frictionless collision impulses ($J_n = -(1+e)\mu v_n$), mass-weighted Baumgarte penetration stabilization ($\beta = 0.5$), and circular boundary restitution ($e_{\text{wall}}$).

---

### Key Architectural Deliverables

#### 1. Core Physics Engine (`sim_core/`)
- Pure C++17 backend-agnostic formulation of states, configurations, diagnostics, and boundary projections.
- Fully analytical 2-body collision verification, ballistic projectile kinematics, and symplectic energy tracking.
- Zero-distance singularity guards ($d \le \epsilon \implies \mathbf{n} = (1, 0)^T$) preventing `NaN`/`Inf` numerical explosions.

#### 2. Multicore OpenMP Engine (`sim_omp_v2/`)
- Thread-safe Jacobi contact formulation with thread-private displacement accumulation buffers.
- 2D `UniformSpatialGrid` with cell-linked lists ($W \ge 2r_{\max}$) and parallel pair discovery.
- ThreadSanitizer-audited execution guaranteeing race-free parallel physics updates across $1$ to $16$ threads.

#### 3. NVIDIA CUDA GPU Subsystem (`sim_cuda_v2/`)
- Optimized for NVIDIA Ada Lovelace architectures (RTX 4060 Laptop GPU, sm_89, 24 SMs, 3,072 CUDA cores).
- 16-byte aligned device particle layout (`float4` / 32-byte chunks) delivering $>92\%$ memory coalescing efficiency.
- Race-free per-particle Jacobi gathering with double-buffered ping-pong particle memory arrays (zero atomic operations required).
- GPU uniform spatial hash grid via `thrust::sort_by_key` with zero false negatives across sparse, dense, boundary, and polydisperse distributions.
- Multi-block shared-memory parallel reduction for real-time diagnostics (momentum, kinetic energy, potential energy, active contacts, penetration depth).

#### 4. Interactive Desktop Laboratory (`lab_gui/` & `sim_lab`)
- 3-Panel scientific layout built on GLFW, Dear ImGui v1.90.4-docking, and ImPlot v0.16.
- 3-Way live backend switcher (`[CPU Sequential]`, `[OpenMP Multicore]`, `[CUDA GPU (RTX 4060)]`).
- Real-time device telemetry, active contact monitors, max penetration depth gauges, and DCD CFL tunneling sentinels.
- Integrated docking support (`ImGuiConfigFlags_DockingEnable`) and persistent layout saving (`imgui.ini`).
- OpenGL framebuffer screenshot capture engine (`stb_image_write`) supporting `--screenshot <path>` CLI and HUD controls.

#### 5. Educational Coursework Suite (`docs/coursework/`)
Five self-contained, publication-grade student lab worksheets accompanied by a complete instructor solutions manual:
- **Lab 1 (`LAB_01_IMPULSE_COLLISION.md`):** Collision dynamics, restitution sweep ($e \in [0, 1]$), kinetic energy partition $\Delta E_k = -\frac{1}{2}\mu(1-e^2)v_{\text{rel}}^2$, and momentum conservation.
- **Lab 2 (`LAB_02_INTEGRATOR_STABILITY.md`):** Symplectic Euler vs Explicit Forward Euler stability, discrete truncation error $\frac{1}{2}gt\Delta t$, and phase-space volume preservation.
- **Lab 3 (`LAB_03_CONVERGENCE_STUDY.md`):** Timestep refinement study ($\Delta t = 0.04\text{ s} \to 0.0025\text{ s}$), $L_2 / L_\infty$ error norms, and empirical order verification ($p \approx 1.000$).
- **Lab 4 (`LAB_04_CONTACT_MECHANICS_TUNNELING.md`):** Discrete collision detection tunneling, CFL heuristic ($v\Delta t/r$), Baumgarte stabilization, and dense granular column settling.
- **Lab 5 (`LAB_05_HPC_PARALLEL_SCALING.md`):** Parallel computing paradigms, Amdahl's Law, thread scaling ($1\dots16$), CPU latency vs GPU throughput crossover, and algorithmic vs hardware acceleration.
- **Instructor Manual (`INSTRUCTOR_MANUAL.md`):** Complete analytical solutions, reference data tables, discussion questions, and 100-point grading rubrics.

---

### Verification and Benchmark Summary

#### CTest Automated Test Suite (100% Pass Rate):
```
    Start 1: PhysicsVerification
1/9 Test #1: PhysicsVerification ...............   Passed    0.01 sec
    Start 2: ExperimentFrameworkVerification
2/9 Test #2: ExperimentFrameworkVerification ...   Passed    0.00 sec
    Start 3: CollisionLaboratoryVerification
3/9 Test #3: CollisionLaboratoryVerification ...   Passed    0.00 sec
    Start 4: OpenMpCorrectnessVerification
4/9 Test #4: OpenMpCorrectnessVerification .....   Passed    0.62 sec
    Start 5: SpatialGridVerification
5/9 Test #5: SpatialGridVerification ...........   Passed    0.01 sec
    Start 6: CudaCorrectnessVerification
6/9 Test #6: CudaCorrectnessVerification .......   Passed    0.85 sec
    Start 7: CudaSpatialGridVerification
7/9 Test #7: CudaSpatialGridVerification .......   Passed    0.28 sec
    Start 8: CrossBackendVerification
8/9 Test #8: CrossBackendVerification ..........   Passed    0.66 sec
    Start 9: LabGuiSmokeTest
9/9 Test #9: LabGuiSmokeTest ...................   Passed    0.24 sec

100% tests passed, 0 tests failed out of 9
```

#### Performance Summary Matrix ($N = 10,000$ particles, Spatial Grid):
| Backend | Hardware Configuration | Step Time | Speedup vs CPU Grid | Throughput |
|:---|:---|:---:|:---:|:---:|
| **Sequential CPU** | Intel Core i7-13620H (1 Thread) | $37.21\text{ ms}$ | $1.00\times$ (Ref) | $0.27\text{M p/s}$ |
| **OpenMP Multicore** | Intel Core i7-13620H (8 Threads) | $14.00\text{ ms}$ | $2.66\times$ | $0.71\text{M p/s}$ |
| **NVIDIA CUDA GPU** | RTX 4060 Laptop (3072 Cores) | **$2.05\text{ ms}$** | **$18.16\times$** | **$4.88\text{M p/s}$** |

---

### Quick Start Guide

#### 1. Build the Entire System
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

#### 2. Run Verification Suite
```bash
ctest --test-dir build --output-on-failure
```

#### 3. Launch Interactive Workbench
```bash
./build/sim_lab
```

#### 4. Run Headless Benchmarks & Plot Generation
```bash
./build/benchmark_omp
./build/benchmark_broadphase_breakdown
./build/benchmark_cross_backend
python3 scripts/plot_hpc_benchmarks.py
```
