# High-Performance Computing Benchmark Specification & Methodology (Revised)

**Target System:** 2D Particle Simulation (HPC Benchmarking Suite)
**Workspace:** `/home/devansh/simulator-workspaces/cuda-particle-simulation-v2`
**Branch:** `feature/simulator-v2`
**Document Status:** Complete HPC Benchmarking Specification (Phase 0 Revised)

---

## 1. Benchmark Principles & Fairness Criteria

The core purpose of this specification is to eliminate methodology artifacts, ensure strictly equitable comparisons between CPU and GPU architectures, and establish reproducible performance measurements.

### 1.1 Strict Workload Equivalence Axiom
A benchmark comparison between CPU, OpenMP, and CUDA is valid **if and only if** all backends execute:
1. **Identical Mathematical Work:** The same numerical integrator (Substepped Semi-Implicit Euler), identical substepping count ($S$), and identical timestep per substep ($\Delta t_{\text{sub}}$).
2. **Identical Physical Problem:** Matching particle counts ($N$), identical initial spatial distributions and velocities (via deterministic PRNG seeds), identical mass and collision restitution ($e$), matching boundary geometry, and identical gravitational acceleration ($\mathbf{g}$).
3. **Identical Precision:** IEEE 754 float32 precision across all arithmetic operations.

### 1.2 Four-Tier Classification of Performance Metrics
To prevent misleading speedup figures from being broadcast as empirical facts, all performance reporting must strictly categorize numbers into one of four tiers:
1. **Historical Reported Performance:** The raw historical values recorded in legacy project artifacts (e.g., $65.97\,\text{ms}$ on CPU vs $2.565\,\text{ms}$ on CUDA at $N=20,000$, resulting in an unadjusted ratio of $25.72\times$).
2. **Arithmetic Normalization Estimates:** Back-of-the-envelope calculations adjusting for known parameter mismatches (e.g., dividing CPU evolve time by 8 substeps: $65.97 / 8 = 8.246\,\text{ms} \implies 8.246 / 2.565 \approx 3.21\times$).
   *MANDATORY CAVEAT:* Arithmetic normalization estimates are **strictly unverified mathematical heuristics, NOT measured results**. Running 8 substeps on CUDA or 1 substep on CPU alters memory access patterns, cache hit rates, kernel launch latencies, atomic collision contention, and physical particle dispersion.
3. **Actually Measured Results:** Timing data directly captured by instrumented, synchronized benchmarking harnesses under documented system parameters.
4. **Validated Equivalent-Work Speedups:** The definitive speedup ratio obtained when both CPU and GPU backends execute identical equations of motion, identical substeps, identical boundary physics, and identical initial particle states.

---

## 2. Timing Boundaries & Instrumentation

Performance timings must never blend simulation compute with memory transfers or graphical presentation. All benchmarks must measure and report the following four distinct timing layers independently:

```
+---------------------------------------------------------------------------------------------------+
| SIMULATION & RENDERING PIPELINE BREAKDOWN                                                         |
|                                                                                                   |
|  [Layer 1: Compute Substep]  -->  [Layer 2: Sim Frame]  -->  [Layer 3: Data Sync] --> [Layer 4: Render]
|  - Broadphase search              - S x Substeps              - D2H / H2D transfer    - VBO upload
|  - Narrowphase contact            - State update              - Zero-copy buffer map  - Draw calls
|  - Position/Velocity update                                                           - Buffer swap
+---------------------------------------------------------------------------------------------------+
```

### 2.1 Metric Definitions

| Metric | Symbol | Definition | Timing Instrument |
| :--- | :--- | :--- | :--- |
| **Substep Compute Time** | $T_{\text{substep}}$ | Time to advance physical state by $\Delta t_{\text{sub}}$ | CPU: `steady_clock`<br>GPU: `cudaEvent` |
| **Frame Simulation Time**| $T_{\text{frame\_sim}}$ | Pure compute time to advance $S$ substeps ($\Delta t_{\text{frame}} = 1/60\,\text{s}$) | $\sum_{s=1}^S T_{\text{substep}, s}$ |
| **Device-Host Transfer**  | $T_{\text{d2h}}$ | Time to copy device particle state to host RAM (GPU only) | `cudaEvent` around `cudaMemcpyAsync` |
| **Rendering Time**        | $T_{\text{render}}$ | SFML vertex buffer update, draw call, and window display | `steady_clock` around render call |
| **Total Frame Time**      | $T_{\text{frame\_total}}$| End-to-end wall clock frame time from start of sim to display swap | Monotonic wall clock |

### 2.2 Instrumentation Rules
1. **Host-Side Instrumentation:** Use `std::chrono::steady_clock` exclusively. Avoid `std::chrono::high_resolution_clock` (which may alias to system time) or `clock()` (which sums CPU core time across threads in OpenMP).
2. **GPU Instrumentation:** GPU kernel execution must be measured using `cudaEventRecord` and `cudaEventElapsedTime` on the dedicated compute stream.
3. **Synchronization Boundaries:** Explicit `cudaStreamSynchronize` must only be called at the boundary between timing phases to prevent driver queue bubbles.

---

## 3. Warmup, Steady-State, and Statistical Metrics

### 3.1 Warmup Protocol
Before recording any timing metrics:
- Every benchmark run must execute a minimum of **200 warmup frames** (or 1,600 substeps at $S=8$).
- Warmup eliminates initial memory allocation page faults, JIT driver kernel compilation, thread pool spin-up, and CPU/GPU thermal frequency scaling transients.

### 3.2 Steady-State Measurement Window
- Benchmark runs must execute for a fixed duration of **10.0 wall-clock seconds** OR a minimum of **1,000 recorded frames**, whichever is longer.
- Every individual frame/substep duration must be recorded in an in-memory sample vector for full distributional analysis.

### 3.3 Statistical Metric Reporting
The benchmark output must report:
1. **Mean ($\mu$):** $\frac{1}{M}\sum T_k$
2. **Standard Deviation ($\sigma$):** $\sqrt{\frac{1}{M-1}\sum (T_k - \mu)^2}$
3. **Median (p50):** 50th percentile of sorted durations
4. **95th Percentile (p95):** Tail latency indicator (critical for frame stability)
5. **99th Percentile (p99):** Worst-case frame drop indicator
6. **Minimum / Maximum:** Global extrema
7. **Simulation FPS:** $1000.0 / T_{\text{frame\_sim, avg}}$
8. **Validated Speedup:**
   $$\text{Speedup vs CPU Baseline} = \frac{T_{\text{substep}}^{\text{CPU\_Scalar}}}{T_{\text{substep}}^{\text{Target}}}$$

---

## 4. Algorithmic Profiling & HPC Architecture Hypotheses

All performance claims must be rigorously grounded. Theoretical complexity, engineering hypotheses, profiling measurements, and validated results must be strictly distinguished.

### 4.1 Grounded Broadphase Complexity Analysis
- **Sweep-and-Prune (SAP):**
  - *Theoretical Ideal:* $O(N \log N)$ when sorting dominates and overlaps along the axis are sparse.
  - *Degenerate Complexity:* $O(N^2)$ when particles settle vertically under gravity, causing extensive interval overlap along the projection axis.
- **Uniform Spatial Grid / Hash:**
  - *Theoretical Ideal:* $O(N)$ expected complexity under quasi-uniform particle distributions.
  - *Degenerate Complexity:* If particles collapse into a localized cluster (e.g. gravitational pooling in a corner), cell occupancy $N_c$ surges, and pairwise checks within crowded cells scale as $O(N_c^2)$. A uniform grid does **not** guarantee $O(N)$ under arbitrary density distributions.
  - *Profiling Protocol:* Evaluate cell occupancy variance and execution times across:
    - Case A: Uniform gas dispersion across domain.
    - Case B: Dense gravitational bed at container bottom.

### 4.2 Memory Layout Profiling Protocol: AoS vs SoA
- **Array-of-Structures (AoS):** Current 24-byte `ParticleData` or 32-byte `Particle`.
- **Structure-of-Arrays (SoA):** Separate arrays for `px`, `py`, `vx`, `vy`, `radius`.
- **Engineering Principle:** SoA is **not** assumed to be faster than AoS without hardware profiling.
  - On CPU, AoS can provide spatial locality when multiple fields of particle $i$ are read together.
  - On GPU, SoA enables coalesced 32-byte memory transactions, but register pressure and cache reuse across warp broadcasts must be profiled using NVIDIA Nsight Compute (`ncu` metrics `sm__sass_average_data_bytes_per_sector_mem_global_op_ld.pct` and `dram__bytes_read.sum`).

### 4.3 GPU Execution & Interop Hypotheses
- **Warp Divergence in All-Pairs:** Outer-loop bounds $j \in [i+1, N-1]$ produce variable warp trip counts. The exact warp divergence percentage is input-dependent and must be measured using `ncu` metric `smsp__warp_issue_stalled_divergent_per_warp_active.pct`, rather than assumed fixed at 50%.
- **CUDA-OpenGL Interop Zero-Copy:** In native Linux environments, `cudaGraphicsGLRegisterBuffer` provides zero-copy rendering. However, inside WSL2 virtualization (using DXGK / Wayland / X11 bridge), interop may introduce hidden staging copies in the driver. Throughput must be profiled directly against host memory copies before asserting zero-copy performance.
- **High-Scale Rendering Target:** Achieving 60 FPS at $N = 100,000$ particles is an architectural performance goal, not a guaranteed result.

---

## 5. OpenMP Scalability & Threading Harness

### 5.1 Multi-Threaded Scaling Protocol
Evaluate OpenMP scaling across thread counts $T \in \{1, 2, 4, 8, 16\}$:
```bash
export OMP_NUM_THREADS=$T
export OMP_PROC_BIND=close
export OMP_PLACES=cores
export OMP_SCHEDULE=static
```

### 5.2 Scaling Metrics
1. **Speedup:** $S(T) = \frac{T_{\text{substep}}(1)}{T_{\text{substep}}(T)}$
2. **Parallel Efficiency:** $E(T) = \frac{S(T)}{T} \times 100\%$
3. **Amdahl's Law Serial Fraction Estimate:**
   $$f_{\text{serial}} = \frac{\frac{1}{S(T)} - \frac{1}{T}}{1 - \frac{1}{T}}$$

---

## 6. Standard Output Schemas

All backends must output results adhering to this standardized CSV schema:

### Standard CSV Schema (`results_canonical.csv`)
```csv
backend,run_id,mode,n_particles,substeps,dt_substep,dt_frame,warmup_steps,measured_steps,wall_seconds,ms_substep_avg,ms_substep_p50,ms_substep_p95,ms_substep_p99,ms_sim_frame_avg,sim_fps,ms_d2h_avg,ms_render_avg,ms_frame_total_avg,nan_count
```
