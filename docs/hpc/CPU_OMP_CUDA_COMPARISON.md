# Cross-Backend Comparative Study: CPU vs. OpenMP vs. NVIDIA CUDA
## Parallel Dynamics Lab — Phase 3 Cross-Architectural Verification & Analysis

**Document Version:** 1.0.0
**Date:** 2026-10-16
**Module:** `sim_core`, `sim_omp_v2`, `sim_cuda_v2`
**Test Suite:** `tests/test_cross_backend_verification.cpp`
**Hardware Platforms:** Intel Core i7-13620H (x86_64 CPU) & NVIDIA GeForce RTX 4060 Laptop GPU (Ada Lovelace)

---

### 1. Architectural Paradigms in Particle Dynamics

To prepare mechanical engineering students for modern computational engineering careers, the Parallel Particle Dynamics Lab implements three distinct computational paradigms under an identical mathematical physics specification:

| Architectural Property | Sequential CPU Reference (`sim_core`) | OpenMP Multicore CPU (`sim_omp_v2`) | NVIDIA CUDA GPU (`sim_cuda_v2`) |
|:---|:---|:---|:---|
| **Programming Paradigm** | Single-threaded von Neumann | Shared-Memory Multithreading (MIMD) | Single Instruction Multiple Threads (SIMT) |
| **Execution Units** | 1 CPU Core (ALU + Vector Units) | 8-16 Cores (Independent pipelines) | 24 Streaming Multiprocessors (3,072 Cores) |
| **Memory Architecture** | Private L1/L2, Shared L3 Cache | Shared L3 Cache, CC-NUMA Bus | High-Bandwidth Device GDDR6 VRAM |
| **Contact Solver Strategy** | Sequential Gauss-Seidel | Thread-Local Jacobi Accumulation | Per-Particle Jacobi Gather (Ping-Pong) |
| **Data Hazard Management** | Strictly Serialized (No hazards) | Thread-private delta buffers | Read-only current / Write-only next |
| **Optimal Workload Regime** | Small $N < 500$ (Low latency) | Medium $N \approx 500 - 2,000$ | Large $N \ge 5,000$ (Massive throughput) |

---

### 2. Numerical Formulation: Gauss-Seidel vs. Jacobi Contact Dynamics

A critical mechanical dynamics insight taught by this platform is the difference between **Gauss-Seidel** and **Jacobi** contact solvers.

#### 2.1 The Two-Body Isolated Collision Case
For any binary collision between particle 1 and particle 2:
- **Gauss-Seidel:**
  - Evaluates normal impulse: $J_n = -(1+e)\mu v_n$
  - Updates: $\mathbf{v}_1 \leftarrow \mathbf{v}_1 + \frac{J_n}{m_1}\mathbf{n}, \quad \mathbf{v}_2 \leftarrow \mathbf{v}_2 - \frac{J_n}{m_2}\mathbf{n}$
- **Jacobi (OpenMP / CUDA):**
  - Evaluates impulse from state at time $t$: $\Delta \mathbf{v}_1 = +\frac{J_n}{m_1}\mathbf{n}, \quad \Delta \mathbf{v}_2 = -\frac{J_n}{m_2}\mathbf{n}$
  - Applies accumulated deltas: $\mathbf{v}_1 \leftarrow \mathbf{v}_1 + \Delta \mathbf{v}_1, \quad \mathbf{v}_2 \leftarrow \mathbf{v}_2 + \Delta \mathbf{v}_2$

**Mathematical Theorem:** In isolated 2-body impacts, **Gauss-Seidel and Jacobi formulations are algebraically identical**.
This is why all three backends produce identical post-collision velocities down to machine precision.

#### 2.2 Dense Multi-Body Packing and Granular Columns
In dense particle packings where particle $i$ contacts neighbors $j_1, j_2, \dots, j_k$ simultaneously:
- **Gauss-Seidel (Sequential CPU):**
  - Processes pairs sequentially. As soon as pair $(i, j_1)$ resolves, particle $i$'s velocity changes. When pair $(i, j_2)$ is subsequently evaluated, it uses particle $i$'s *already modified* velocity.
  - **Consequence:** Introduces a slight directional bias depending on particle ordering in memory.
- **Jacobi (OpenMP & CUDA):**
  - Evaluates all candidate impulses simultaneously based on the unchanged state at the start of iteration $k$.
  - Takes the vector sum of all neighbor impulses:
    $$\Delta \mathbf{v}_i^{(k)} = \sum_{j \in \mathcal{N}(i)} \frac{J_n^{(k)}}{m_i} \mathbf{n}_{ij}$$
  - **Consequence:** Perfectly symmetric and isotropic; independent of memory ordering or thread schedule. Requires $K \ge 4$ relaxation iterations to propagate impulses through granular chains.

---

### 3. Floating-Point Non-Associativity in Heterogeneous Computing

A fundamental principle of numerical computation is that **floating-point addition is non-associative**:
$$(a + b) + c \neq a + (b + c)$$

In sequential CPU execution:
- Global momentum is accumulated sequentially:
  $$\mathbf{P}_{\text{seq}} = (((m_1\mathbf{v}_1 + m_2\mathbf{v}_2) + m_3\mathbf{v}_3) + \dots)$$
In OpenMP multicore execution:
- Each thread sums its partition into a thread-local accumulator; these partial sums are combined at the reduction barrier.
In CUDA GPU execution:
- Tree-reduction occurs within warp shuffle instructions and shared memory across thread blocks.

**Engineering Interpretation:**
Because summation orders differ across architectures, minuscule discrepancies ($\sim 10^{-7}\text{ kg}\cdot\text{m/s}$) naturally emerge in diagnostic variables. Students learn that these discrepancies are not bugs, but the inevitable consequence of IEEE 754 arithmetic in parallel architectures.

---

### 4. Stage 7 Cross-Backend Verification Results

The automated cross-backend verification test harness (`tests/test_cross_backend_verification.cpp`) runs identical scenarios across all three engines simultaneously, verifying cross-backend physical consistency.

| Test ID | Test Scenario | Verified Metric | CPU Reference | OpenMP (8T) | CUDA GPU (RTX 4060) | Agreement Status |
|:---|:---|:---|:---|:---|:---|:---:|
| **CROSS-01** | Ballistic Trajectory ($N=10$) | Final $y$-position error | $y = -0.4905\text{ m}$ | $y = -0.4905\text{ m}$ | $y = -0.4905\text{ m}$ | **Identical ($<10^{-6}$)** |
| **CROSS-02** | Binary Elastic Collision ($e=1.0$) | Final relative velocity $v_{\text{rel}}$ | $1.6000\text{ m/s}$ | $1.6000\text{ m/s}$ | $1.6000\text{ m/s}$ | **Identical ($<10^{-6}$)** |
| **CROSS-03** | Inelastic Impact ($e=0.528$) | Energy dissipated $\Delta E_k$ | $-8.1568\text{ J}$ | $-8.1568\text{ J}$ | $-8.1568\text{ J}$ | **Identical ($<10^{-6}$)** |
| **CROSS-04** | Granular Settling ($N=48$) | Equilibrium max penetration | $0.355\text{ mm}$ | $0.362\text{ mm}$ | $0.371\text{ mm}$ | **Physically Consistent** |

All 4 cross-backend verification tests pass with 100% compliance.

---

### 5. Theoretical Scaling: Amdahl's Law vs. Gustafson's Law

#### 5.1 Amdahl's Law (Strong Scaling)
Amdahl's law dictates that the speedup of a fixed-size workload is limited by its strictly serial fraction $s$:
$$S_{\text{latency}}(P) = \frac{1}{s + \frac{1-s}{P}}$$

In our particle lab, serial portions include host-device synchronization, frame orchestration, and memory allocation. At $N=100$, serial overhead dominates ($s \approx 0.95$), leading to no GPU speedup.

#### 5.2 Gustafson's Law (Weak Scaling)
When problem size scales with available computing capacity, Gustafson's law applies:
$$S_{\text{throughput}}(P) = s + P(1-s)$$

As mechanical engineering simulations scale to large degrees of freedom ($N \ge 10,000$), the parallel portion $(1-s)$ approaches $0.999$, allowing the NVIDIA RTX 4060 GPU to achieve an $18.16\times$ speedup over the optimized serial spatial grid and $>1,200\times$ over serial all-pairs.

---

### 6. Educational Guidance & Coursework Integration

Students are instructed to utilize the interactive GUI's **Scientific Comparison Header** to verify:
1. **Module 1 (Impulse Mechanics):** Verify that central impacts on CPU, OpenMP, and CUDA yield identical momentum conservation ($\mathbf{P}(t) = \text{const}$) and exact analytical $\Delta E_k$.
2. **Module 2 (Numerical Damping & Penetration):** Adjust Gauss-Seidel / Jacobi iteration count ($K = 2 \dots 16$) and observe how contact stiffness and settling stability improve on GPU.
3. **Module 3 (Parallel Architecture):** Switch between backends at $N=100$, $N=1000$, and $N=10,000$. Observe the shift from CPU latency dominance to GPU throughput dominance.
4. **Module 4 (Broadphase Complexity):** Toggle between All-Pairs $O(N^2)$ and Spatial Grid $O(N)$ on both CPU and GPU to witness the profound distinction between algorithmic and hardware acceleration.
