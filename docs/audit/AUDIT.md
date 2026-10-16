# Comprehensive Physics, Numerical Methods, Software Architecture, and HPC Performance Audit (Revised)

**Target System:** 2D Particle Simulation (CPU / OpenMP / CUDA)
**Workspace:** `/home/devansh/simulator-workspaces/cuda-particle-simulation-v2`
**Branch:** `feature/simulator-v2`
**Date of Audit:** October 2026 (Revised Phase 0)
**Audit Status:** Phase 0 Complete — STOP (Awaiting Implementation Approval)

---

## 1. Executive Summary

This audit constitutes a forensic examination of the physical validity, numerical consistency, software architecture, and high-performance computing (HPC) claims of the particle simulation codebase across its three execution backends: Single-Threaded CPU (`sim_cpu`), Multithreaded OpenMP (`sim_omp`), and GPU CUDA (`particle_sim`).

### Core Audit Discoveries:

1. **Benchmark Workload Inequity & Performance Metrics Classification:**
   The historical headline result claiming a **25.7x CUDA speedup** over CPU at $N=20,000$ particles compares fundamentally disparate workloads:
   - The CPU benchmark loop timed `Simulator::evolve()`, which executes **8 sub-iterations (substeps)** per recorded frame.
   - The CUDA benchmark harness timed a single GPU kernel invocation executing **1 substep**.

   To maintain scientific integrity, this audit establishes a strict 4-tier performance classification:
   - **Historical Reported Performance:** The raw historical figures recorded in existing artifacts ($65.97\,\text{ms}$ on CPU vs $2.565\,\text{ms}$ on CUDA at $N=20,000$, yielding an unadjusted ratio of $25.72\times$).
   - **Arithmetic Normalization Estimates:** A naive back-of-the-envelope calculation dividing CPU time by 8 ($65.97 / 8 \approx 8.25\,\text{ms}$), suggesting an estimated ratio of $\approx 3.2\times$. **This is strictly a mathematical normalization estimate, NOT an empirically measured or validated result.** Running 8 substeps on GPU or 1 substep on CPU alters kernel launch overhead, cache hit rates, particle collision frequencies, and convergence behavior.
   - **Actually Measured Results:** Measurements gathered under strictly recorded, instrumented execution conditions.
   - **Validated Equivalent-Work Speedups:** The true physical speedup, which can only be determined once both backends solve identical equations of motion with identical substep counts, identical initial states, and verified identical numerical workloads.

2. **Random Initialization Divergence & Timestamp Seeding in CUDA:**
   A critical defect was identified in the CUDA particle initialization routine (`gpuSim/main.cpp`, line 203):
   ```cpp
   const unsigned seed = static_cast<unsigned>(std::time(nullptr));
   initParticles(particles, args.boundaryRadius, seed);
   ```
   While `struct Args` contains a `seed = 1` field, the benchmark binary ignored it and seeded the C-runtime `std::rand()` with the wall-clock timestamp `std::time(nullptr)`. Furthermore:
   - **PRNG Implementations Differ:** CPU uses `std::mt19937` (Mersenne Twister); CUDA uses libc `rand()` (linear congruential generator).
   - **Initial Velocity Distributions Differ:** CPU initializes velocities in 2D: $v_x, v_y \in [-500, +500]\,\text{px/s}$; CUDA initializes horizontal velocity only: $v_x \in [-50, +50]\,\text{px/s}, v_y = 0.0\,\text{px/s}$ ($10\times$ lower magnitude, zero vertical velocity).
   - **Radii Distributions Differ:** CPU samples continuous floats $r \in [2.0, 10.0]$; CUDA samples discrete integers $r \in \{2, 3, \dots, 12\}$.
   - **Domain Dimensions Differ:** CPU boundary radius is $400\,\text{px}$ in a $950 \times 950$ window; CUDA boundary radius is $350\,\text{px}$ in an $800 \times 800$ window.
   Every CUDA benchmark run was executed from a non-deterministic, time-dependent initial state with completely different particle kinematics than the CPU baseline.

3. **Physical Backend Divergence:**
   The CPU and CUDA backends solve fundamentally different physical models:
   - **Integrators:** CPU uses 2nd-order Position Verlet without explicit velocity storage; CUDA uses 1st-order Semi-implicit Euler with explicit velocity storage.
   - **Collisions:** CPU performs mass-weighted Position-Based Dynamics (PBD) displacement ($m \propto r^2$); CUDA applies equal 50% displacement regardless of particle mass/radius and **never alters particle velocities during collisions**. Particle momentum is completely uncoupled from velocity in CUDA.
   - **Boundaries:** CPU Circle boundary clamps positions without velocity reflection (inelastic slide/stick), CPU Rectangle boundary specularly reflects the implicit Verlet velocity ($2x_{\text{cur}} - x_{\text{last}}$), and CUDA specularly reflects physical velocity vectors ($v' = v - 2(v \cdot n)n$).
   - **Gravity & Coordinate Systems:** CPU uses an inverted viewport ($y$-up) with gravity $g = -980\,\text{px/s}^2$; CUDA uses a non-inverted viewport ($y$-down) with gravity $g = +100\,\text{px/s}^2$ ($9.8\times$ difference in acceleration magnitude).

4. **OpenMP Data Race Conditions:**
   [`Simulator::resolveCollisionsSweepPrune()`](../../2D-particle-sim/cpuSim/simulator.cpp) parallelizes the outer loop with `#pragma omp parallel for` while concurrently writing to inner particle positions (`_particles[j]._cur_position`) across threads without atomics, mutexes, spatial partitioning, or reductions. This causes memory race conditions, non-deterministic trajectories, and momentum corruption.

5. **Numerical Stability Flaws:**
   Division-by-zero vulnerabilities exist in [`Simulator::resolveCollisions`](../../2D-particle-sim/cpuSim/simulator.cpp) when particles coincide ($d_{ij} = 0$), causing `NaN` positions that contaminate the simulation.

---

## 2. Critical Problem Inventory

```
+--------------------------------------------------------------------------------------------------------------------+
| CRITICAL PROBLEM INDEX                                                                                             |
+----+---------------------------------------------------------------+------------------------+----------------------+
| ID | Title                                                         | Primary Location       | Severity             |
+----+---------------------------------------------------------------+------------------------+----------------------+
| P1 | Workload Asymmetry in Benchmark Harness (8 CPU vs 1 GPU Step) | main.cpp / harness     | FATAL (Invalidates Data)
| P2 | OpenMP Collision Data Race and State Corruption               | cpuSim/simulator.cpp   | CRITICAL (Incorrect Physics)
| P3 | Integrator Mismatch & Ghost Velocities in CUDA Collision      | simulator_cuda.cu      | CRITICAL (Broken Physics)
| P4 | CUDA Ignores Particle Mass & Restitution in Collisions        | simulator_cuda.cu      | CRITICAL (Broken Physics)
| P5 | Incompatible Boundary Conditions Across Geometries & Backends | simulator.cpp / .cu    | HIGH (Inconsistent Physics)
| P6 | Zero-Distance Singularity & Unchecked NaN Propagation         | cpuSim/simulator.cpp   | HIGH (Numerical Instability)
| P7 | CLI Argument Ignorance & Harness Warmup Divergence            | main.cpp (CPU & GPU)   | MEDIUM (Benchmark Flaw)
| P8 | Algorithmic Scalability Collapse: SAP & CUDA All-Pairs        | simulator.cpp / .cu    | HIGH (Performance Barrier)
| P9 | Initial Verlet Position Step Discontinuity (8x Velocity Jump) | cpuSim/main.cpp        | HIGH (Initial Value Error)
| P10| Coordinate System & Gravity Magnitude Divergence              | main.cpp (CPU & GPU)   | HIGH (Physical Discrepancy)
| P11| Random Initialization Divergence & Timestamp Seeding in CUDA | gpuSim/main.cpp        | HIGH (Non-reproducible State)
+----+---------------------------------------------------------------+------------------------+----------------------+
```

---

### Critical Problem P1: Workload Asymmetry in Benchmark Harness (8 CPU Substeps vs 1 GPU Step)

- **Affected Files & Functions:**
  - `2D-particle-sim/cpuSim/main.cpp` (Lines 312–318, 349–356)
  - `2D-particle-sim/cpuSim/simulator.h` (Lines 229–232)
  - `2D-particle-sim/cpuSim/simulator.cpp` (Lines 148–154)
  - `2D-particle-sim/gpuSim/main.cpp` (Lines 208–213)
  - `2D-particle-sim/gpuSim/simulator_cuda.cu` (Lines 218–228)
  - `2D-particle-sim/plot_cpu_omp_cuda.py` (Lines 12–26, 65–76)
  - `2D-particle-sim/make_portfolio_table.py` (Lines 11–43, 56–63)
  - `2D-particle-sim/portfolio_speedup_table.md` (Lines 1–12)
  - `README.md` (Lines 41–50)

- **Technical Explanation:**
  In `cpuSim/simulator.h`:
  ```cpp
  void setSubsteps(const int& substeps) {
      _substeps = substeps;
      _dt = _dt / substeps;
  }
  ```
  In `cpuSim/simulator.cpp`:
  ```cpp
  void Simulator::evolve() {
      for (int i = 0; i < _substeps; ++i) {
          resolveCollisionsSweepPrune();
          applyBoundaryConstraints();
          updatePositions();
      }
  }
  ```
  In `cpuSim/main.cpp`:
  ```cpp
  simulator.setSubsteps(args.substeps); // default substeps = 8
  ...
  const auto s0 = clock::now();
  simulator.evolve();
  const auto s1 = clock::now();
  evolve_ms.push_back(step.count());
  ```
  Every timed iteration on the CPU executes **8 full collision detection, boundary resolution, and position update cycles**.

  Conversely, in `gpuSim/simulator_cuda.cu`:
  ```cpp
  float Simulator::benchmarkSteps(float dt, int warmupSteps, int measuredSteps) {
      ...
      cudaCheck(cudaEventRecord(evStart, stream), "event record start");
      for (int i = 0; i < measuredSteps; ++i) {
          runKernels(dt);
      }
      cudaCheck(cudaEventRecord(evStop, stream), "event record stop");
      ...
      return ms / static_cast<float>(measuredSteps);
  }
  ```
  And `runKernels(dt)` launches exactly 1 call to `integrateKernel`, 1 call to `collideKernel`, and 1 call to `applyOffsetsKernel`.
  The GPU benchmark timer records the elapsed time for **exactly 1 substep**.
  The comparison scripts directly divide CPU `avg_ms_evolve` (8 substeps) by CUDA `ms_per_step` (1 substep):
  $$\text{Historical Reported Ratio} = \frac{T_{\text{CPU}}(8\,\text{substeps})}{T_{\text{CUDA}}(1\,\text{substep})} = \frac{65.97\,\text{ms}}{2.565\,\text{ms}} = 25.72\times$$

- **Scientific Classification of Performance Metrics:**
  - **Historical Reported Ratio:** $25.72\times$ at $N=20,000$ (comparing 8 CPU substeps to 1 GPU substep).
  - **Arithmetic Normalization Estimate:** $65.97 / 8 = 8.246\,\text{ms} \implies 8.246 / 2.565 \approx 3.21\times$.
    *Critical Caveat:* This value is strictly a rough arithmetic normalization estimate. It does **not** represent a measured result. Executing 8 substeps on CUDA involves 40 kernel launches per frame, varying atomic contention, and temporal cache locality; executing 1 substep on CPU alters sorting efficiency and particle displacement scale.
  - **Measured Results:** Only the raw step times under documented conditions ($T_{\text{CPU, 8-substeps}} = 65.97\,\text{ms}$, $T_{\text{CUDA, 1-substep}} = 2.565\,\text{ms}$).
  - **Validated Equivalent-Work Speedup:** Must be measured experimentally in Phase 3 after implementing identical substepping and physical models across both backends.

---

### Critical Problem P2: OpenMP Collision Data Race and State Corruption

- **Affected Files & Functions:**
  - `2D-particle-sim/cpuSim/simulator.cpp` (`resolveCollisions`, Lines 30–50; `resolveCollisionsSweepPrune`, Lines 60–85)

- **Technical Explanation:**
  The outer loop over index $i$ is distributed across OpenMP threads. When Thread $A$ ($i=2$) detects a collision with particle $j=10$, it writes to `_particles[10]._cur_position`. Simultaneously, Thread $B$ ($i=5$) detects a collision with particle $j=10$ and writes to `_particles[10]._cur_position`. Neither atomics nor mutexes protect `sf::Vector2f`.
  Torn writes occur, simulation results become non-deterministic across thread counts, and linear momentum conservation is violated.

- **Recommended Solution:**
  Use Jacobi-style double-buffered displacement accumulation with OpenMP atomics or thread-private reduction arrays, or spatial domain decomposition.

---

### Critical Problem P3: Integrator Mismatch & Ghost Velocities in CUDA Collision

- **Affected Files & Functions:**
  - `2D-particle-sim/cpuSim/simulator.h` (Lines 67–71)
  - `2D-particle-sim/gpuSim/particle_data.h` (Lines 4–9)
  - `2D-particle-sim/gpuSim/simulator_cuda.cu` (Lines 21–28, 51–88, 90–102)

- **Technical Explanation:**
  CPU uses 2nd-order Position Verlet ($x_{n+1} = 2x_n - x_{n-1} + a\Delta t^2$) with implicit velocity. CUDA stores explicit velocities $(v_x, v_y)$ and updates them under gravity in `integrateKernel`. In `collideKernel` and `applyOffsetsKernel`, CUDA modifies positions $(p_x, p_y)$ via `d_dx` and `d_dy`, but **never modifies velocities `p.vx` and `p.vy`**.
  Colliding particles retain their pre-collision velocities, slamming back together in the subsequent integration step.

- **Recommended Solution:**
  Adopt a unified symplectic integration scheme (Velocity Verlet or Semi-implicit Euler) with explicit impulse-based velocity updates coupled to particle masses.

---

### Critical Problem P4: CUDA Ignores Particle Mass & Restitution in Collisions

- **Affected Files & Functions:**
  - `2D-particle-sim/gpuSim/simulator_cuda.cu` (Lines 60–86)
  - `2D-particle-sim/cpuSim/simulator.cpp` (Lines 42–47, 77–82)

- **Technical Explanation:**
  CPU weights collision displacements by mass ($m \propto r^2$). CUDA applies an unweighted 50/50 split ($\pm 0.5 \times \text{overlap}$) regardless of particle radius. A particle with $r=2$ ($m=4$) pushes a particle with $r=12$ ($m=144$, $36\times$ heavier) by the exact same distance. CUDA also lacks a coefficient of restitution parameter.

---

### Critical Problem P5: Incompatible Boundary Conditions Across Geometries & Backends

- **Affected Files & Functions:**
  - `2D-particle-sim/cpuSim/simulator.cpp` (Lines 91–102 vs 105–138)
  - `2D-particle-sim/gpuSim/simulator_cuda.cu` (Lines 29–48, 107–122)

- **Technical Explanation:**
  CPU Circle boundary only clamps position (causing inelastic sticking/sliding). CPU Rectangle boundary clamps position AND reflects $x_{\text{last}}$, inverting implicit velocity (elastic specular reflection). CUDA boundary specularly reflects the explicit velocity vector in both `integrateKernel` and `applyOffsetsKernel`.

---

### Critical Problem P6: Zero-Distance Singularity & Unchecked NaN Propagation

- **Affected Files & Functions:**
  - `2D-particle-sim/cpuSim/simulator.cpp` (Lines 40–42, 75–77)

- **Technical Explanation:**
  When two particles share identical positions ($d_{ij} = 0$), computing `n = dist / dist.length()` divides by zero: `0.0 / 0.0 = NaN`. The `NaN` propagates through position vectors and corrupts the particle buffer.

---

### Critical Problem P7: CLI Argument Ignorance & Harness Warmup Divergence

- **Affected Files & Functions:**
  - `2D-particle-sim/cpuSim/main.cpp` (Lines 103–144)
  - `2D-particle-sim/gpuSim/main.cpp` (Lines 64–145)

- **Technical Explanation:**
  `cpuSim/main.cpp` does not parse `--warmup` or `--dt`, resulting in 0 warmup steps. `gpuSim/main.cpp` does not parse `--substeps` or `--seconds`, but executes 200 warmup steps.

---

### Critical Problem P8: Algorithmic Scalability Collapse: SAP & CUDA All-Pairs

- **Affected Files & Functions:**
  - `2D-particle-sim/cpuSim/simulator.cpp` (Lines 52–85)
  - `2D-particle-sim/gpuSim/simulator_cuda.cu` (Lines 51–88)

- **Technical Explanation:**
  - Sweep-and-Prune (SAP) degrades from $O(N \log N)$ to $O(N^2)$ in dense vertical settling regimes because overlapping $x$-intervals prevent early loop termination.
  - CUDA `collideKernel` performs an un-tiled, brute-force all-pairs check ($\frac{N(N-1)}{2}$ pairs). At $N=200,000$, this requires $1.99 \times 10^{10}$ pair checks per step with high atomic serialization on `d_dx`/`d_dy`.

---

### Critical Problem P9: Initial Verlet Position Step Discontinuity (8x Velocity Jump)

- **Affected Files & Functions:**
  - `2D-particle-sim/cpuSim/main.cpp` (Lines 280–288, 293–295)

- **Technical Explanation:**
  `prev_pos` is initialized assuming macro timestep $\Delta t = \text{DT} = 1/60\,\text{s}$. Then `setSubsteps(8)` divides simulator timestep by 8 ($\Delta t = 1/480\,\text{s}$). In the first step of Verlet, displacement is $v \cdot \text{DT} = 8v \cdot \Delta t$, causing an instantaneous $8\times$ velocity shock.

---

### Critical Problem P10: Coordinate System & Gravity Magnitude Divergence

- **Affected Files & Functions:**
  - `2D-particle-sim/cpuSim/main.cpp` (Lines 245–260, 400–403)
  - `2D-particle-sim/gpuSim/main.cpp` (Lines 42–45, 224–228)

- **Technical Explanation:**
  CPU uses inverted SFML view coordinates ($y$-up) with gravity $(0, -980.0)\,\text{px/s}^2$. CUDA uses standard window coordinates ($y$-down) with gravity $(0, +100.0)\,\text{px/s}^2$. The backends operate under completely different acceleration scales ($9.8\times$ discrepancy).

---

### Critical Problem P11: Random Initialization Divergence & Timestamp Seeding in CUDA

- **Affected Files & Functions:**
  - `2D-particle-sim/gpuSim/main.cpp` (Lines 148–169, 202–204)
  - `2D-particle-sim/cpuSim/main.cpp` (Lines 262–288)

- **Technical Explanation:**
  In `gpuSim/main.cpp`:
  ```cpp
  const unsigned seed = static_cast<unsigned>(std::time(nullptr));
  initParticles(particles, args.boundaryRadius, seed);
  ```
  The CUDA benchmark ignores `args.seed` and seeds `std::rand()` with the current epoch timestamp, causing every run to begin from a non-deterministic initial state.
  Furthermore, CPU initializes velocities in $[-500, +500]\,\text{px/s}$ in both $x$ and $y$, while CUDA initializes $v_x \in [-50, +50]\,\text{px/s}$ and sets $v_y = 0.0$. Radii distributions and container radii also differ.

---

## 3. Separation of Confirmed Bugs vs Testable Hypotheses

### A. Confirmed Bugs (Definitive Code Invalidation)
1. **P1 (Benchmark Asymmetry):** Confirmed in source code. CPU measures 8 substeps; CUDA measures 1 substep.
2. **P2 (OpenMP Race Conditions):** Confirmed in source code. Unsynchronized parallel writes to `_particles[j]._cur_position`.
3. **P3 (CUDA Velocity Decoupling):** Confirmed in source code. GPU collision kernel leaves `p.vx, p.vy` untouched.
4. **P4 (CUDA Mass Neglect):** Confirmed in source code. Particle radius is not used to compute mass or weight collision displacements.
5. **P5 (Boundary Discrepancies):** Confirmed in source code. Incompatible boundary conditions across geometries and backends.
6. **P6 (Zero-Distance NaN):** Confirmed in source code. Missing regularizer on vector normalization.
7. **P7 (Ignored CLI Arguments):** Confirmed in source code. CLI parsing omissions on CPU and GPU.
8. **P9 (Verlet Velocity Discontinuity):** Confirmed in source code. Macroscopic DT used to initialize Verlet previous position.
9. **P10 (Gravity/Coordinate Discrepancy):** Confirmed in source code. Viewport inversion and $9.8\times$ gravity magnitude difference.
10. **P11 (Non-deterministic CUDA Seeding):** Confirmed in source code. `std::time(nullptr)` used instead of fixed seed.

### B. Engineering Hypotheses Requiring Hardware Profiling
1. **Hypothesis H1 (Sweep-and-Prune Settling Collapse):**
   *Claim:* In a settled state under gravity, SAP broadphase degrades towards $O(N^2)$ as $x$-intervals overlap.
   *Classification:* Engineering Hypothesis. Requires profiling pair comparison counts during transition from gas dispersion to settled bed.
2. **Hypothesis H2 (Structure-of-Arrays vs Array-of-Structures on GPU):**
   *Claim:* Converting `ParticleData` from 24-byte AoS to SoA will improve memory coalescing and throughput.
   *Classification:* Engineering Hypothesis. Requires empirical measurement with NVIDIA Nsight Compute (`ncu`). SoA is not guaranteed to outperform AoS without profiling.
3. **Hypothesis H3 (Uniform Spatial Grid vs LBVH under Non-Uniform Clustering):**
   *Claim:* A uniform grid provides $O(N)$ under uniform spatial density, but degrades towards localized $O(N_c^2)$ when particles cluster heavily into a few cells.
   *Classification:* Theoretical Property & Engineering Hypothesis. Requires measuring cell occupancy variance under gravitational slump.
4. **Hypothesis H4 (CUDA Warp Divergence in All-Pairs):**
   *Claim:* Loop bounds $j \in [i+1, N-1]$ create variable warp activity.
   *Classification:* Engineering Hypothesis. The exact percentage of warp stall and divergence must be profiled using `ncu` metrics, rather than assumed fixed at 50%.
5. **Hypothesis H5 (CUDA-OpenGL Interop Zero-Copy Performance):**
   *Claim:* CUDA-OpenGL interop eliminates host memory copy bottlenecks.
   *Classification:* Engineering Hypothesis. In WSL2 virtualization environments (DXGK / Wayland / X11 bridge), interop may introduce hidden driver copies. Must be verified empirically.

---

## 4. Rigorous HPC Architecture Assessment

### Algorithmic Complexity Grounding

| Algorithm | Stated / Ideal Complexity | Worst-Case / Degenerate Complexity | Practical Limit ($N$) | Current Codebase Status |
| :--- | :--- | :--- | :--- | :--- |
| **Sweep-and-Prune (CPU)** | $O(N \log N)$ | $O(N^2)$ (Settled / $x$-aligned) | $\sim 20,000$ | Implemented in `cpuSim` |
| **All-to-All Pairwise (CUDA)** | $O(N^2)$ | $O(N^2)$ (Always) | $\sim 20,000$ | Implemented in `gpuSim` |
| **Uniform Spatial Grid / Hash** | $O(N)$ (Quasi-uniform density) | $O(N^2)$ (All particles in 1 cell) | $> 500,000$ | Planned for V2 |
| **Linear BVH (LBVH on GPU)** | $O(N \log N)$ | $O(N \log N)$ (Hierarchical) | $> 1,000,000$ | Architectural Candidate |

*Important Clarification:* A uniform spatial grid does **not** guarantee $O(N)$ in all scenarios. If particles collapse into a localized region under strong gravitational settling, cell occupancy $N_c$ increases, and collision checks within crowded cells scale as $O(N_c^2)$.

---

## 5. Audit Conclusions

The prototype codebase contains significant physical incompatibilities, a race-corrupted OpenMP implementation, non-deterministic CUDA initial conditions, and an invalid benchmark harness comparing 8 substeps against 1 substep.

Implementation has been stopped in Phase 0. Architectural design decisions have been finalized in [`PHYSICS_DESIGN_DECISIONS.md`](PHYSICS_DESIGN_DECISIONS.md).
