# Prioritized Implementation Roadmap for Simulator V2 (Revised)

**Target System:** 2D Particle Simulation (Architecture & Modernization Roadmap)
**Workspace:** `/home/devansh/simulator-workspaces/cuda-particle-simulation-v2`
**Branch:** `feature/simulator-v2`
**Current Milestone:** Phase 0 Complete — STOP (Awaiting Implementation Approval)

---

## 1. Roadmap Principles & Architecture Vision

This roadmap outlines a phased, risk-managed path to transform the current prototype simulation into a scientifically validated, high-performance, and maintainable simulation platform.

### Core Architectural Axioms
1. **Single Source of Physical Truth:**
   The mathematical definition of states, integrators, forces, collision mechanics, and boundary geometries resides in a backend-agnostic core (`sim_core`).
2. **Strict Backend Equivalence:**
   Every backend (CPU Scalar, CPU OpenMP, and GPU CUDA) implements an identical public interface (`ISimulationBackend`) and executes mathematically equivalent numerical algorithms.
3. **Decoupled Engine Subsystems:**
   Broadphase spatial indexing, narrowphase collision response, numerical integration, and graphical rendering exist as decoupled, pluggable modules.
4. **Automated Scientific Regression Testing:**
   No commit will be considered viable unless it passes the automated analytical validation test suite.
5. **Separation of Measurement from Visualization:**
   Benchmarking runs strictly in isolated, headless compute harnesses with comprehensive statistical instrumentation.

---

## 2. Phased Implementation Sequence

```
+---------------------------------------------------------------------------------------------------------+
| SIMULATOR V2 IMPLEMENTATION SEQUENCE                                                                   |
|                                                                                                         |
|  [Phase 0: Design Freeze]        -->  [Phase 1: Physics Unification]  -->  [Phase 2: Correctness/Tests] |
|  - Physics decisions document         - Shared math/physics headers        - Race-free OpenMP           |
|  - Substepped Symplectic Euler        - Semi-implicit Euler integration    - Automated Catch2 test suite|
|  - Contact impulse & PBD split        - Mass-weighted momentum collisions  - Deterministic test fixtures|
|                                                                                                         |
|                                                  |                                                      |
|                                                  v                                                      |
|                                                                                                         |
|  [Phase 6: Re-benchmarking]    <--  [Phase 5: Rendering Decoupling]  <--  [Phase 3 & 4: HPC Broadphase] |
|  - Validated scaling sweeps           - Decoupled fixed-tick loop          - Unified CLI & benchmark    |
|  - Verified speedup matrix            - WSL2 OpenGL interop profiling      - Uniform spatial hash       |
|  - Final technical report             - Headless benchmarking harness      - Empirical AoS vs SoA study |
+---------------------------------------------------------------------------------------------------------+
```

---

### Phase 0: Mathematical Model, Physics Freeze & Design Decisions (STATUS: COMPLETE)
- Audit legacy prototype code.
- Finalize physics design decision record ([`PHYSICS_DESIGN_DECISIONS.md`](PHYSICS_DESIGN_DECISIONS.md)).
- Classify performance metrics into 4 tiers; eliminate misleading normalization claims.
- Identify the CUDA `std::time(nullptr)` initialization defect.
- Establish strict mathematical equations for contact impulses, positional stabilization, and boundary reflections.
- **Exit Status:** COMPLETE. Execution stopped awaiting user authorization.

---

### Phase 1: Shared Physics Core & Backend Unification
**Objective:** Replace disparate equations, integrators, and initial conditions with a unified physical specification.
**Priority:** Highest (Foundational)
**Estimated Complexity:** Medium

#### Deliverables:
1. **Module `sim_core` Creation:**
   - Define canonical `ParticleState` structure:
     `struct Particle { float px, py, vx, vy, ax, ay, radius, mass; uint32_t color; };`
   - Define global parameters: `gravity = (0, -980.0)`, `collision_restitution`, `wall_restitution`, `friction`.
2. **Substepped Semi-Implicit Euler Integrator:**
   - Implement velocity prediction and ballistic drift across CPU and CUDA:
     $$\mathbf{v}_i^* = \mathbf{v}_i^n + \mathbf{g}\Delta t_{\text{sub}}$$
     $$\mathbf{x}_i^* = \mathbf{x}_i^n + \mathbf{v}_i^*\Delta t_{\text{sub}}$$
3. **Momentum-Conserving Collision Response:**
   - Implement impulse-based collision response in CUDA and CPU using particle masses ($m = \pi \rho r^2$):
     $$J_n = -(1 + e) \left(\frac{m_i m_j}{m_i + m_j}\right) (\mathbf{v}_{\text{rel}} \cdot \mathbf{n})$$
   - Couple velocities to collisions on GPU (eliminating ghost velocities).
4. **Positional Penetration Stabilization:**
   - Separate positions along contact normals without altering velocities ($\mathbf{v}$ is untouched during position projection).
5. **Shared Deterministic PRNG Initialization:**
   - Implement unified PRNG (`std::mt19937_64` or PCG32) replacing `std::time(nullptr)` and libc `rand()` in CUDA.
   - Synchronize domain dimensions, radii distributions, and initial velocity distributions.

#### Exit Criteria:
- A single particle falling under gravity and bouncing against a circular wall produces matching trajectories across CPU and GPU backends to within $10^{-5}$ relative tolerance.

---

### Phase 2: Correctness, Thread Safety & Validation Suite
**Objective:** Eliminate OpenMP data races, remove zero-distance singularities, and build the automated test harness.
**Priority:** Highest (Correctness & Stability)
**Estimated Complexity:** High

#### Deliverables:
1. **Race-Free OpenMP Engine:**
   - Eliminate unsynchronized concurrent writes to `_particles[j]._cur_position`.
   - Implement Jacobi-style double-buffered displacement accumulation with atomic updates or thread-private reduction arrays.
   - Verify zero race conditions using ThreadSanitizer (`-fsanitize=thread`).
2. **Numerical Singularity Regularization:**
   - Guard all vector normalization operations ($d_{ij} \le \epsilon \implies \mathbf{n} = (1, 0)^T$).
   - Completely eliminate `NaN` generation under coincidental particle overlap.
3. **Automated Validation Test Suite (`test_physics`):**
   - Implement test cases TC01 through TC13 from `docs/audit/VALIDATION_PLAN.md`:
     - Free motion & ballistic gravity
     - 1D equal and unequal mass collisions
     - Inelastic restitution sweeps (with momentum conservation; no artificial energy conservation demanded)
     - N-body momentum conservation ($d\mathbf{P}/dt = 0$)
     - Symplectic energy conservation in closed conservative regimes
     - Coincident particle overlap recovery
   - Integrate deterministic test fixtures (`fixture_2body_elastic.bin`, `fixture_gas_100.bin`, `fixture_settling_5000.bin`).

#### Exit Criteria:
- `test_physics` passes 100% of analytical and conservation tests across CPU Scalar, OpenMP (1, 2, 4, 8 threads), and CUDA.

---

### Phase 3: Rigorous Benchmarking Infrastructure
**Objective:** Build a trustworthy, standardized benchmarking harness with complete workload parity.
**Priority:** High (Measurement Integrity)
**Estimated Complexity:** Medium

#### Deliverables:
1. **Unified CLI Parser:**
   - Implement shared CLI argument parser across all backend executables:
     `--n`, `--substeps`, `--dt`, `--seconds`, `--steps`, `--warmup`, `--seed`, `--domain`, `--csv`, `--headless`.
2. **Strict Workload Parity Enforcement:**
   - Implement explicit substep loop in CUDA benchmark (`runSubsteps(S, dt_sub)`).
   - Ensure CPU and GPU execute identical numbers of physical substeps per recorded frame.
   - Enforce mandatory 200-frame warmup before timing.
3. **Four-Layer Timing Instrumentation:**
   - Record and output separate timings for:
     - $T_{\text{substep}}$ (Pure physics step)
     - $T_{\text{frame\_sim}}$ ($S \times T_{\text{substep}}$)
     - $T_{\text{d2h}}$ (GPU state copy to host)
     - $T_{\text{render}}$ (VBO upload and draw)
     - $T_{\text{frame\_total}}$ (Wall clock end-to-end)
4. **Canonical CSV Output & Analysis Pipeline:**
   - Standardize CSV output schema per `docs/audit/BENCHMARK_SPEC.md`.

#### Exit Criteria:
- Benchmark scripts run reproducibly from clean slate; timing metrics verify identical substep and physical work across all backends.

---

### Phase 4: HPC Broadphase Modernization & Algorithmic Scaling
**Objective:** Replace $O(N^2)$ brute-force and degraded SAP algorithms with $O(N)$ spatial hashing.
**Priority:** High (Performance & Scalability)
**Estimated Complexity:** High

#### Deliverables:
1. **CPU Uniform Spatial Grid:**
   - Implement spatial hash grid with cell size $h = 2 r_{\text{max}}$.
   - Linear-time particle-to-cell binning and 9-neighborhood candidate queries.
   - Profile performance under uniform gas dispersion vs gravitational slump.
2. **CUDA GPU Spatial Grid / Morton-Coded Spatial Hash:**
   - Replace $O(N^2)$ `collideKernel` with GPU spatial partitioning:
     - Compute Morton code (Z-order curve) or 2D cell hash for each particle.
     - Sort particle indices by cell hash using `cub::DeviceRadixSort`.
     - Find cell start/end offsets via binary search / `cub::DeviceRunLengthEncode`.
     - Kernel processes neighboring 9 cells per particle.
3. **Empirical Memory Layout Profiling (AoS vs SoA):**
   - Profile AoS vs SoA layouts using NVIDIA Nsight Compute (`ncu`) and Linux `perf`.
   - Adopt memory layout based strictly on measured memory bandwidth and coalescing metrics.

#### Exit Criteria:
- Measured execution times demonstrate linear scaling ($O(N)$) under uniform spatial distributions on CPU and GPU.

---

### Phase 5: Rendering Decoupling & High-Scale Visualization
**Objective:** Decouple physical updates from SFML rendering and evaluate GPU visualization.
**Priority:** Medium (Usability & Large-Scale Demos)
**Estimated Complexity:** Medium

#### Deliverables:
1. **Fixed-Tick / Variable-Frame Decoupled Loop:**
   - Implement semi-fixed timestep accumulator (Glenn Fiedler's "Fix Your Timestep"):
     Physics advances at fixed $\Delta t_{\text{sub}}$, while renderer displays interpolated states at display refresh rate.
2. **Headless Execution Isolation:**
   - Headless mode completely removes SFML window creation, OpenGL context initialization, and display linking.
3. **WSL2 CUDA-OpenGL Interop Profiling:**
   - Empirically profile `cudaGraphicsGLRegisterBuffer` versus asynchronous host memory copy inside the WSL2 environment.
   - Determine whether interop provides measurable speedup given WSL2 driver staging behavior.

#### Exit Criteria:
- Interactive demonstration runs smoothly decoupled from render framerate.

---

### Phase 6: Comprehensive Re-benchmarking & Scientific Report
**Objective:** Produce publication-grade, fully reproducible benchmarks and documentation.
**Priority:** High (Final Verification)
**Estimated Complexity:** Low

#### Deliverables:
1. **Full Scaling Sweep Execution:**
   - Run complete parameter sweeps across CPU 1T, OpenMP (2T–16T), and CUDA for $N \in [10^3, 2 \times 10^5]$ at verified equivalent workloads.
2. **Verified Speedup Matrix:**
   - Replace historical speedup tables with measured, verified speedup metrics.
3. **Comprehensive Technical Report:**
   - Publish complete architectural documentation, profiling analysis, and validation results.

---

## 3. Work Package Breakdown & Dependencies

```
+-------------------------------------------------------------------------------------------------------+
| WORK PACKAGE DEPENDENCY MATRIX                                                                        |
+----+--------------------------------+----------------------------+-----------------+------------------+
| WP | Work Package Name              | Core Deliverable           | Dependencies    | Risk Level       |
+----+--------------------------------+----------------------------+-----------------+------------------+
| 0  | Physics Architecture Freeze    | Decision record complete   | None            | Done             |
| 1  | Shared Physics Core            | Symplectic Euler & Impulses| WP 0            | Low              |
| 2  | OpenMP Race Elimination        | Race-free multi-threading  | WP 1            | Medium           |
| 3  | Validation Test Harness        | Headless test suite (TC01-13)| WP 1, WP 2    | Low              |
| 4  | Benchmark Harness Parity       | Substep parity & CLI parser| WP 1, WP 2      | Low              |
| 5  | Spatial Hash Broadphase (CPU)  | O(N) CPU collision grid    | WP 1, WP 3      | Medium           |
| 6  | GPU Spatial Hash (CUDA)        | CUB radix sort grid        | WP 1, WP 4      | High (GPU Kernel)|
| 7  | WSL2 Interop & Render Decouple | Headless/Render decoupling | WP 6            | Medium           |
| 8  | Final Re-benchmark & Reporting | Validated Speedup Matrix   | WP 4, WP 5, WP 6| Low              |
+----+--------------------------------+----------------------------+-----------------+------------------+
```

---

## 4. Definition of Done & Stop Condition

### Definition of Done for Simulator V2:
1. All backends solve identical physical equations with identical parameters and conservation properties.
2. OpenMP backend executes 100% race-free under ThreadSanitizer.
3. Automated test suite (`test_physics`) passes 100% of analytical and conservation tests.
4. Benchmarks execute with guaranteed workload and substep equivalence.
5. All legacy $O(N^2)$ all-pairs bottlenecks are replaced with verified spatial hashing.
6. Documentation, speedup tables, and scaling plots reflect scientifically verified data.

---

### STOP CONDITION
Per user instructions:
- Phase 0 (Mathematical Model, Physics Freeze, and Design Decisions) is complete.
- Documentation in `docs/audit/` has been fully revised and finalized.
- No simulation implementation code has been modified.
- No existing benchmark data has been altered.
- **Execution stops now. Awaiting user review and authorization to proceed with Phase 1 implementation.**
