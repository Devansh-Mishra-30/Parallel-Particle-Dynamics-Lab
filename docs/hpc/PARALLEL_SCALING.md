# Parallel Particle Dynamics Lab: Theoretical Parallel Scaling & Roofline Analysis
## Parallel Dynamics Lab — Phase 2 HPC Theoretical Analysis

**Document Version:** 1.0.0
**Date:** 2026-10-16
**Module:** `sim_omp_v2`
**Theoretical Focus:** Amdahl's Law, Strong vs. Weak Scaling, Memory Hierarchy & Asymptotic Complexity

---

### 1. Amdahl's Law & Strong Scaling Derivation

#### 1.1 Mathematical Formulation of Strong Scaling
In strong scaling, problem size $N$ remains fixed while thread count $T$ increases from $1$ to $16$. The theoretical maximum speedup $S(T)$ is bounded by Amdahl's Law:

$$S(T) = \frac{1}{(1 - f) + \frac{f}{T}}$$

where $f \in [0, 1]$ represents the parallelizable fraction of execution time, and $(1 - f)$ represents the strictly serial portion (synchronization, memory fences, diagnostic reductions, and system scheduling).

#### 1.2 Empirical Parallel Fraction Fit ($N = 512$ and $N = 1024$)
Using measured median execution times from `output/benchmarks/openmp_scaling_results.csv`:
- At $N = 512$, $S(8) = 3.99\times$:
  $$\frac{1}{3.99} \approx 0.2506 = (1 - f) + \frac{f}{8} \implies 0.2506 = 1 - \frac{7}{8}f \implies \frac{7}{8}f = 0.7494 \implies f \approx 0.856\ (85.6\%)$$
- At $N = 1024$, $S(8) = 3.79\times$:
  $$\frac{1}{3.79} \approx 0.2639 = (1 - f) + \frac{f}{8} \implies \frac{7}{8}f = 0.7361 \implies f \approx 0.841\ (84.1\%)$$

The engine achieves an effective parallel fraction of $f \approx 84\% - 86\%$.

```
Strong Scaling Curve (N = 1024): Ideal vs. Amdahl (f=0.85) vs. Measured

Speedup (x)
 8.0 |                                              / (Ideal Linear T)
     |                                        /----* (Amdahl f=0.85: 4.32x)
 4.0 |                            /----------* 3.79x (Measured T=8)
     |                /----------* 3.07x (Measured T=4)
 2.0 |    /----------* 1.75x (Measured T=2)
     |   /
 1.0 |--* 1.0x (T=1)
     |
 0.0 +---+-----------+-----------+-----------+
        T=1         T=2         T=4         T=8
```

#### 1.3 Sources of the Serial Remainder ($1 - f \approx 15\%$)
1. **Inner-Loop OpenMP Synchronization:**
   Each substep conducts $K = 8$ Jacobi contact relaxation iterations. Between pairwise impulse accumulation and reduction, OpenMP barrier synchronizations (`#pragma omp barrier`) enforce data visibility across threads.
2. **Global Diagnostic Reductions:**
   Computing kinetic energy, gravitational potential energy, and center-of-mass momentum requires an OpenMP parallel reduction step, which incurs logarithmic reduction tree synchronization latency.
3. **Cache Line Coherency Protocols:**
   Reducing thread-local delta buffers $\sum_{t=0}^{T-1} \Delta \mathbf{v}_i^{(t)}$ induces L2/L3 cache cross-talk across core complexes.

---

### 2. Architecture & Core Heterogeneity (Intel Hybrid Topology)

The test processor (Intel Core i7-13620H) features an asymmetric hybrid architecture:
- **6 Performance Cores (Golden Cove):** High IPC, 2 hardware threads per core (Hyper-Threading), clock speeds up to 4.90 GHz.
- **4 Efficient Cores (Gracemont):** Lower IPC, single-threaded, shared L2 cache per 4-core cluster, clock speeds up to 3.60 GHz.

#### 2.1 The 16-Thread Hyperthreading / Heterogeneity Plateau
As demonstrated in empirical benchmarks:
- For $T \le 6$ (allocating strictly on Performance Cores), speedup scales near-linearly.
- At $T = 8$ (utilizing 6 P-cores + 2 E-cores), scaling peaks at $3.99\times$ ($N=512$).
- At $T = 16$ (utilizing all 12 hyperthreads across P-cores plus 4 E-cores), performance drops from $3.79\times$ to $2.49\times$ ($N=1024$).

**Root Causes:**
1. **Barrier Straggler Effect:** In OpenMP static scheduling, each thread is assigned $\lceil N / T \rceil$ particles. An E-core running at 3.6 GHz with lower IPC executes its assigned workload slower than a P-core running at 4.9 GHz. Because of `#pragma omp barrier`, fast P-core threads idle waiting for the slowest E-core thread.
2. **Resource Contention on Hyperthreads:** When two OpenMP threads run on the same physical P-core, they contend for vector execution units (FMA units) and L1/L2 data cache ports, reducing per-thread throughput.

---

### 3. Memory Hierarchy & Roofline Analysis

#### 3.1 Working Set Footprint
Each `Particle` occupies:
- Position: `Vec2` ($8\text{ bytes}$)
- Velocity: `Vec2` ($8\text{ bytes}$)
- Force: `Vec2` ($8\text{ bytes}$)
- Mass, inv_mass, radius, density, material_id: ($20\text{ bytes}$)
- Alignment padding: Total size = $56\text{ bytes}$.

For $N = 1024$ particles:
$$\text{Memory Footprint} = 1024 \times 56\text{ bytes} \approx 57.3\text{ KB}$$

- **L1 Data Cache:** $48\text{ KB}$ per P-core.
- **L2 Cache:** $1.25\text{ MB}$ per P-core.
- **L3 Shared Cache:** $24\text{ MB}$.

**Critical Finding:** The entire particle simulation dataset fits entirely within CPU on-chip caches (L1/L2/L3). The simulation is **compute-bound, not DRAM-bandwidth-bound**. The primary bottlenecks are instruction pipeline latency, branch misprediction during contact pruning, and synchronization barriers, rather than main memory bus saturation.

---

### 4. Algorithmic Asymptotics & The Roadmap to CUDA (Phase 3)

| Backend / Strategy | Broadphase Complexity | Narrowphase Complexity | Contact Resolution | Scaling Limit ($N$ @ 60 FPS) |
|:---|:---:|:---:|:---:|:---:|
| **CPU Sequential Reference** | $O(N^2)$ All-Pairs | $O(N^2)$ Pairwise | Gauss-Seidel (In-Place) | $N \approx 500$ |
| **OpenMP V2 (All-Pairs)** | $O(N^2 / T)$ All-Pairs | $O(N^2 / T)$ Pairwise | Jacobi (Thread-Local) | $N \approx 1,500$ |
| **OpenMP V2 (Spatial Grid)**| $O(N / T)$ Uniform Grid | $O(C / T)$ ($C \ll N^2$) | Jacobi (Thread-Local) | $N \approx 5,000$ |
| **CUDA GPU (Phase 3 Preview)** | $O(N \log N)$ Radix Grid | $O(C / \text{SM})$ Parallel Tiles | Massively Parallel Jacobi | $N \ge 50,000$ |

#### Transition to Phase 3:
While OpenMP multicore provides a $4\times$ speedup on 8 CPU threads, scaling beyond $N > 5,000$ particles requires tens of thousands of concurrent SIMD threads. NVIDIA CUDA GPUs offer over 3,000 CUDA cores and $160+\text{ GB/s}$ of GDDR6 memory bandwidth, which will form the computational engine for Phase 3.
