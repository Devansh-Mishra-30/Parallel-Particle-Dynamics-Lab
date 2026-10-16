# NVIDIA CUDA GPU Physics Engine: Verification & Numerical Correctness
## Parallel Dynamics Lab — Phase 3 Correctness & Safety Report

**Document Version:** 1.0.0
**Date:** 2026-10-16
**Module:** `sim_cuda_v2`
**Test Suite:** `tests/test_cuda_correctness.cpp` & `tests/test_cuda_spatial_grid.cpp`
**Target Architecture:** NVIDIA Ada Lovelace (RTX 4060 Laptop GPU, sm_89)

---

### 1. Verification Strategy & Acceptance Criteria

To confirm that GPU acceleration does not compromise scientific integrity, the `sim_cuda_v2` backend was evaluated against the rigorous acceptance criteria established during Phase 1C:
1. **Kinematic & Dynamic Invariance:** Rigid adherence to momentum conservation ($\|\Delta \mathbf{P}\| < 10^{-5}\text{ kg}\cdot\text{m/s}$) and symplectic energy conservation during ballistic and orbital motions.
2. **Analytical Collision Dynamics:** Exact match to theoretical pre- and post-impact velocities and kinetic energy dissipation for central, oblique, elastic, and inelastic 2-body impacts.
3. **Boundary Invariance:** Restitution and normal reflection against circular chamber walls.
4. **Spatial Partitioning Exactness:** Zero false negatives across all particle spatial distributions.
5. **Numerical Determinism:** Exact bitwise or machine-precision reproducibility across repeated simulation runs.

---

### 2. Stage 3: Physics Correctness Unit Verification (10/10 Passed)

The automated test suite `CudaCorrectnessVerification` executes 10 comprehensive physical scenarios, evaluating both analytical equations and numerical error bounds.

| Test ID | Test Scenario Description | Analytical Reference / Metric | Measured GPU Value | Tolerance | Status |
|:---|:---|:---|:---|:---|:---:|
| **CUDA-01** | Constant Velocity Inertial Flight | $\Delta \mathbf{v} = 0, \Delta E_k = 0, \Delta \mathbf{P} = 0$ | $\|\Delta \mathbf{v}\| = 0.00\text{ m/s}$ | $< 10^{-6}$ | **PASSED** |
| **CUDA-02** | Ballistic Flight under Uniform Gravity | Projectile trajectory $\mathbf{y}(t), \Delta E_{\text{tot}} \approx 0$ | $\Delta E_{\text{tot}} = 8.5\times 10^{-6}\text{ J}$ | $< 10^{-4}$ | **PASSED** |
| **CUDA-03** | Central Elastic Collision ($m_1 = m_2, e = 1.0$) | $v_1' = -0.80\text{ m/s}, v_2' = +0.80\text{ m/s}$ | $\mathbf{v}_1' = -0.8000, \mathbf{v}_2' = +0.8000$ | $< 10^{-5}$ | **PASSED** |
| **CUDA-04** | Unequal Mass Central Elastic ($m_2 = 2 m_1, e = 1.0$) | 1D momentum & energy exchange | $v_1' = -1.6667, v_2' = +0.3333$ | $< 10^{-4}$ | **PASSED** |
| **CUDA-05** | Inelastic Central Collision ($e = 0.528$) | Theoretical $\Delta E_k = -\frac{1}{2}\mu(1-e^2)v_n^2 = -8.1568\text{ J}$ | $\Delta E_k^{\text{gpu}} = -8.1568\text{ J}$ | $< 10^{-4}$ | **PASSED** |
| **CUDA-06** | Oblique Collision ($b = 0.04\text{ m}, e = 1.0$) | Normal velocity reversal, tangent preserved | Normal impulse matched | $< 10^{-4}$ | **PASSED** |
| **CUDA-07** | Circular Wall Reflection ($e_{\text{wall}} = 1.0$) | Reversal of normal velocity at boundary $R$ | $\|\mathbf{x}\| \le R - r$, $E_k$ preserved | $< 10^{-4}$ | **PASSED** |
| **CUDA-08** | Degenerate Zero-Distance Separation | Numerical singularity guard ($\|\Delta \mathbf{x}\| = 0$) | Fallback $\mathbf{n} = (1, 0)$, No NaN/Inf | Bounded | **PASSED** |
| **CUDA-09** | Dense Granular Heap Settling ($N = 48$) | Multi-contact equilibrium & penetration bounds | Max penetration $= 0.371\text{ mm}$ | $< 2.0\text{ mm}$ | **PASSED** |
| **CUDA-10** | Multi-Run Numerical Determinism | Repeatability across 3 independent executions | $\Delta \mathbf{P} = 0.000000, \Delta E_k = 0.000000$ | $0.00\times 10^0$ | **PASSED** |

**Summary Result:** `10/10 tests passed (100% success rate) in 0.49 seconds.`

---

### 3. Stage 5: GPU Spatial Grid Verification (Zero False Negatives)

Spatial broadphase algorithms in discrete element modeling must guarantee that no interacting particle pair is missed. An undetected contact leads to unphysical particle tunneling, massive energy explosion, and numerical instability.

The `CudaSpatialGridVerification` test suite compared candidate pairs found by the GPU uniform grid against a CPU brute-force all-pairs reference ($O(N^2)$ ground truth) across 6 distinct physical distributions:

| Scenario | Particle Distribution | $N$ | True Interacting Contacts | Grid Candidate Pairs | False Negatives | Culling Efficiency |
|:---|:---|:---:|:---:|:---:|:---:|:---:|
| **DIST-01** | Sparse Random System | 100 | 2 | 24 | **0 (None)** | 99.51% |
| **DIST-02** | Dense Granular Domain | 500 | 184 | 1,420 | **0 (None)** | 98.86% |
| **DIST-03** | Polydisperse (Unequal Radii) | 300 | 88 | 792 | **0 (None)** | 98.23% |
| **DIST-04** | Boundary Ring Clustered | 200 | 112 | 840 | **0 (None)** | 95.78% |
| **DIST-05** | Regular Lattice on Cell Borders | 121 | 220 | 512 | **0 (None)** | 92.95% |
| **DIST-06** | Dense Settled Heap (High Contact) | 1,000 | 2,103 | 11,840 | **0 (None)** | 97.63% |

**Result:** Across all 6 regimes, **zero false negatives** occurred. Every true interacting contact was captured in the candidate list.

---

### 4. Parallel Concurrency & Race-Freedom Analysis

#### 4.1 Hazard Elimination via Per-Particle Gathering
The primary race condition in parallel particle simulation occurs when two threads computing contact between pair $(i, j)$ and pair $(k, j)$ attempt to write impulse $\Delta \mathbf{v}_j$ into the same memory location simultaneously.

In `sim_cuda_v2`, this write-write race is structurally impossible:
1. Each GPU thread index $t$ is mapped exclusively to particle $i = t$.
2. Thread $i$ queries neighbors and calculates all inward impulses acting on particle $i$.
3. Thread $i$ writes strictly to `d_particles_next[i]`.
4. No thread ever writes to an index other than $i$.
5. Double buffering ensures that neighbor reads occur strictly from the immutable read-only buffer `d_particles`.

**Mathematical Invariant:**
$$\forall i \neq j, \quad \text{Addr}(\mathbf{x}_i^{\text{next}}) \cap \text{Addr}(\mathbf{x}_j^{\text{next}}) = \emptyset$$

#### 4.2 Dynamic Verification & Sanitizer Note
The CUDA implementation was verified with deterministic multi-run checks (`CUDA-10`).
It is important to document from an HPC verification standpoint:
- Dynamic sanitizers (such as `compute-sanitizer --tool racecheck` or ThreadSanitizer) detect race conditions occurring along specific execution traces and thread interleavings.
- They do not mathematically guarantee race freedom across all potential hardware schedules.
- True race freedom in `sim_cuda_v2` is guaranteed by the **architectural design** of Per-Particle Gathering and double-buffered memory ownership.
