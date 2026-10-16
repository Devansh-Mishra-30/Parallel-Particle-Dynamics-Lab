# OpenMP Multicore Physics Engine: Empirical Benchmarks & Performance Report
## Parallel Dynamics Lab — Phase 2 HPC Performance Specification

**Document Version:** 1.0.0
**Date:** 2026-10-16
**Module:** `sim_omp_v2`
**Benchmark Executable:** `build/benchmark_omp`
**Raw Benchmark Data:** `output/benchmarks/openmp_scaling_results.csv`

---

### 1. Benchmark Environment & Test Rig

All benchmarks were conducted natively in WSL2 Ubuntu 24.04 LTS under non-thermal-throttling conditions with CPU governor set to performance.

- **Host Processor:** Intel Core i7-13620H
  - Physical Cores: 10 (6 Performance Cores + 4 Efficient Cores)
  - Logical Threads: 16
  - Performance Core Max Frequency: 4.90 GHz
  - Efficient Core Max Frequency: 3.60 GHz
  - L3 Cache: 24 MB Intel Smart Cache
- **System Memory:** 32 GB High-Speed Dual-Channel LPDDR5
- **Operating System:** Ubuntu 24.04 LTS on Windows Subsystem for Linux 2 (WSL2)
  - Linux Kernel: `5.15.167.4-microsoft-standard-WSL2`
- **Compiler:** GCC 13.2.0 (`x86_64-linux-gnu`)
- **Compilation Flags:** `-O3 -march=native -fopenmp -std=c++17 -Wall -Wextra`
- **Benchmark Protocol:**
  - 3 repetitions per configuration after warm-up iteration.
  - Metrics collected: Median step time (ms), Min step time (ms), Max step time (ms), Sample standard deviation (ms), Speedup relative to CPU reference, and Parallel efficiency.
  - Tested Problem Sizes ($N$): 32, 64, 128, 256, 512, 1024 particles.
  - Tested Thread Counts ($T$): CPU Reference (sequential baseline), 1, 2, 4, 8, 16 threads.

---

### 2. Empirical Scaling Results Table

The following empirical results were recorded directly from execution of `build/benchmark_omp`:

| Particles ($N$) | Backend / Threads | Median Time (ms) | Min Time (ms) | Max Time (ms) | StdDev (ms) | Speedup vs Ref | Parallel Efficiency |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **32** | CPU Ref (0) | 0.0264 | 0.0245 | 0.0279 | 0.0014 | $1.00\times$ | $100.0\%$ |
| 32 | OMP (1) | 0.0503 | 0.0465 | 0.0571 | 0.0044 | $0.53\times$ | $52.6\%$ |
| 32 | OMP (2) | 0.0703 | 0.0700 | 0.0939 | 0.0112 | $0.38\times$ | $18.8\%$ |
| 32 | OMP (4) | 0.1264 | 0.1008 | 0.1468 | 0.0188 | $0.21\times$ | $5.2\%$ |
| 32 | OMP (8) | 0.2591 | 0.2325 | 0.2948 | 0.0255 | $0.10\times$ | $1.3\%$ |
| 32 | OMP (16) | 0.5133 | 0.3721 | 1.3240 | 0.4194 | $0.05\times$ | $0.3\%$ |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **64** | CPU Ref (0) | 0.1391 | 0.1298 | 0.1429 | 0.0055 | $1.00\times$ | $100.0\%$ |
| 64 | OMP (1) | 0.1578 | 0.1384 | 0.1641 | 0.0109 | $0.88\times$ | $88.1\%$ |
| 64 | OMP (2) | 0.1727 | 0.1712 | 0.1806 | 0.0041 | $0.81\times$ | $40.3\%$ |
| 64 | OMP (4) | 0.1290 | 0.1258 | 0.1471 | 0.0094 | **$1.08\times$** | $27.0\%$ |
| 64 | OMP (8) | 0.1859 | 0.1847 | 0.2016 | 0.0077 | $0.75\times$ | $9.4\%$ |
| 64 | OMP (16) | 0.6137 | 0.5885 | 0.6432 | 0.0224 | $0.23\times$ | $1.4\%$ |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **128** | CPU Ref (0) | 0.5330 | 0.4557 | 0.5388 | 0.0379 | $1.00\times$ | $100.0\%$ |
| 128 | OMP (1) | 0.5178 | 0.4724 | 0.5224 | 0.0226 | $1.03\times$ | $102.9\%$ |
| 128 | OMP (2) | 0.3023 | 0.2681 | 0.3282 | 0.0246 | **$1.76\times$** | $88.2\%$ |
| 128 | OMP (4) | 0.2089 | 0.2016 | 0.2110 | 0.0040 | **$2.55\times$** | $63.8\%$ |
| 128 | OMP (8) | 0.3377 | 0.3248 | 0.3716 | 0.0197 | $1.58\times$ | $19.7\%$ |
| 128 | OMP (16) | 0.8795 | 0.7255 | 0.9557 | 0.0957 | $0.61\times$ | $3.8\%$ |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **256** | CPU Ref (0) | 1.6397 | 1.5895 | 1.8423 | 0.1093 | $1.00\times$ | $100.0\%$ |
| 256 | OMP (1) | 1.7321 | 1.7093 | 1.7437 | 0.0143 | $0.95\times$ | $94.7\%$ |
| 256 | OMP (2) | 0.9831 | 0.9220 | 1.0139 | 0.0382 | **$1.67\times$** | $83.4\%$ |
| 256 | OMP (4) | 0.5856 | 0.5702 | 0.5860 | 0.0073 | **$2.80\times$** | $70.0\%$ |
| 256 | OMP (8) | 0.6087 | 0.5738 | 0.6206 | 0.0198 | **$2.69\times$** | $33.7\%$ |
| 256 | OMP (16) | 1.0493 | 1.0266 | 1.3932 | 0.1677 | $1.56\times$ | $9.8\%$ |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **512** | CPU Ref (0) | 6.1916 | 6.1774 | 6.2383 | 0.0260 | $1.00\times$ | $100.0\%$ |
| 512 | OMP (1) | 6.5215 | 6.3392 | 6.6322 | 0.1208 | $0.95\times$ | $94.9\%$ |
| 512 | OMP (2) | 3.9082 | 3.6150 | 3.9114 | 0.1390 | **$1.58\times$** | $79.2\%$ |
| 512 | OMP (4) | 2.0844 | 2.0362 | 2.1042 | 0.0285 | **$2.97\times$** | $74.3\%$ |
| 512 | OMP (8) | 1.5518 | 1.5321 | 1.6022 | 0.0295 | **$3.99\times$** | $49.9\%$ |
| 512 | OMP (16) | 1.8203 | 1.8114 | 2.0423 | 0.1068 | **$3.40\times$** | $21.3\%$ |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **1024** | CPU Ref (0) | 24.4042 | 24.0548 | 24.4374 | 0.1731 | $1.00\times$ | $100.0\%$ |
| 1024 | OMP (1) | 26.5204 | 25.7278 | 27.2770 | 0.6325 | $0.92\times$ | $92.0\%$ |
| 1024 | OMP (2) | 13.9551 | 13.6087 | 14.1370 | 0.2191 | **$1.75\times$** | $87.4\%$ |
| 1024 | OMP (4) | 7.9401 | 7.7244 | 8.0364 | 0.1305 | **$3.07\times$** | $76.8\%$ |
| 1024 | OMP (8) | 6.4382 | 5.5923 | 6.6397 | 0.4538 | **$3.79\times$** | $47.4\%$ |
| 1024 | OMP (16) | 9.7847 | 8.7558 | 10.4563 | 0.6993 | **$2.49\times$** | $15.6\%$ |

---

### 3. Key Performance Analysis & Observations

```
Speedup vs CPU Reference Baseline Across Problem Sizes (N):

Speedup (x)
 4.0 |                                      * 3.99x (N=512, 8T)
     |                                      |      * 3.79x (N=1024, 8T)
 3.0 |                           * 2.80x    |      |
     |              * 2.55x      |          * 2.97x* 3.07x
 2.0 |              |            * 1.67x    * 1.58x* 1.75x
     |              * 1.76x      |          |      |
 1.0 |-------* 1.08x|------------|----------|------|-------- Baseline (1.0x)
     |       |      |            |          |      |
 0.0 +-------+------+------------+----------+------+--------
           N=64   N=128        N=256      N=512  N=1024
```

1. **Amdahl's Law in the Low-$N$ Regime ($N \le 64$):**
   For small workloads ($N = 32$), the total step computation takes merely $26\ \mu\text{s}$ sequentially. The overhead of OpenMP thread management, cache synchronization, and barrier reductions ($10 - 50\ \mu\text{s}$) overwhelms the parallel computation, yielding an apparent slowdown ($0.05\times - 0.53\times$). This provides students an authentic case study in parallelization overhead and parallel efficiency thresholds.
2. **Optimal Scaling Plateau ($N \ge 512$, $T = 8$):**
   As $N$ expands to $512$ and $1024$, parallel work heavily dominates synchronization costs:
   - At $N = 512$, 8 threads reduces step execution from $6.19\text{ ms}$ to $1.55\text{ ms}$ (**$3.99\times$ speedup**).
   - At $N = 1024$, 8 threads reduces step execution from $24.40\text{ ms}$ to $6.44\text{ ms}$ (**$3.79\times$ speedup**).
3. **Core Heterogeneity & Hyperthreading Behavior ($T = 16$):**
   The host machine possesses 6 Performance Cores and 4 Efficient Cores. When allocating 16 OpenMP threads:
   - Execution spans across hyperthreads and power-constrained E-cores.
   - Cache contention on the shared 24 MB L3 cache increases.
   - Thread barrier synchronization must wait for the slowest E-core thread to finish its chunk.
   - As a consequence, performance regresses at 16 threads relative to 8 threads, matching theoretical predictions of heterogeneous multicore scheduling.

---

### 4. Task 7 Algorithmic Broadphase Benchmark (`UniformSpatialGrid`)

To validate algorithmic scalability beyond brute-force $O(N^2)$ all-pairs testing, the 2D `UniformSpatialGrid` was evaluated across multiple particle densities:

| Particle Count ($N$) | True Contact Pairs | Spatial Grid Candidates | Brute-Force Pairs $\frac{N(N-1)}{2}$ | Culling Efficiency | Verification Status |
|:---:|:---:|:---:|:---:|:---:|:---:|
| **50** | 15 | 66 | 1,225 | **$94.6\%$** | Zero False Negatives (PASS) |
| **200** | 56 | 275 | 19,900 | **$98.6\%$** | Zero False Negatives (PASS) |
| **500** | 146 | 698 | 124,750 | **$99.4\%$** | Zero False Negatives (PASS) |
| **1000** | 205 | 1,154 | 499,500 | **$99.8\%$** | Zero False Negatives (PASS) |

- **Zero False-Negative Guarantee:** The cell dimension $W_{\text{cell}} \ge 2 r_{\max}$ guarantees that every pair of contacting particles resides either within the same grid cell or in immediately adjacent cells.
- **Asymptotic Complexity:** Narrowphase collision evaluations are reduced from $O(N^2)$ to $O(N)$, providing over **$400\times$ candidate culling** at $N=1000$.
- **Distribution Sensitivity Caveat:** The $O(N)$ complexity bound holds strictly for quasi-uniform spatial densities where particles per cell $k = O(1)$. Under pathological distributions (e.g., severe localized clustering where all $N$ particles collapse into a single cell), spatial grid broadphase degrades to $O(N^2)$ narrowphase checks plus grid overhead.

---

### 5. Disentangling Algorithmic Speedup vs Parallel Speedup: 4-Way Comparison

To prevent attributing algorithmic complexity gains to multicore parallelism, the application explicitly benchmarks four distinct execution pathways on the identical physical system:

1. **Mode A:** Sequential CPU All-Pairs (`CpuReferenceSolver`, Gauss-Seidel baseline)
2. **Mode B:** Sequential CPU Spatial Grid (1 thread, `UniformSpatialGrid` + single-thread Jacobi)
3. **Mode C:** OpenMP All-Pairs (8 threads, All-Pairs Jacobi)
4. **Mode D:** OpenMP Spatial Grid (8 threads, `UniformSpatialGrid` + 8-thread Jacobi)

Data recorded from `output/benchmarks/broadphase_and_omp_breakdown.csv`:

| Particle Count ($N$) | Mode A: Seq All-Pairs | Mode B: Seq Spatial Grid | Mode C: OMP All-Pairs (8T) | Mode D: OMP Spatial Grid (8T) | Algorithmic Speedup (B / A) | Parallel Speedup (C / A) | Combined Speedup (D / A) |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **64** | $0.128\text{ ms}$ | $0.060\text{ ms}$ | $0.177\text{ ms}$ | $0.162\text{ ms}$ | **$2.12\times$** | $0.72\times$ | $0.79\times$ |
| **128** | $0.436\text{ ms}$ | $0.120\text{ ms}$ | $0.335\text{ ms}$ | $0.207\text{ ms}$ | **$3.64\times$** | $1.30\times$ | $2.11\times$ |
| **256** | $1.678\text{ ms}$ | $0.225\text{ ms}$ | $0.620\text{ ms}$ | $0.300\text{ ms}$ | **$7.45\times$** | $2.71\times$ | $5.59\times$ |
| **512** | $6.279\text{ ms}$ | $0.532\text{ ms}$ | $1.814\text{ ms}$ | $0.437\text{ ms}$ | **$11.80\times$** | $3.46\times$ | **$14.37\times$** |
| **1024** | $24.985\text{ ms}$ | $1.044\text{ ms}$ | $5.637\text{ ms}$ | $0.729\text{ ms}$ | **$23.93\times$** | $4.43\times$ | **$34.26\times$** |

#### Pedagogical Insights for Engineering Students:
1. **Algorithmic Dominance at High $N$:** At $N=1024$, culling non-colliding pairs with the spatial grid yields a **$23.93\times$ speedup purely sequentially** (Mode B vs Mode A), outpacing 8-thread OpenMP parallel brute force (**$4.43\times$**, Mode C vs Mode A).
2. **Multiplication of Benefits:** Combining algorithmic culling with OpenMP multicore parallelism yields a compound **$34.26\times$ speedup** (Mode D vs Mode A), reducing full-step compute from $24.99\text{ ms}$ ($40\text{ FPS}$) down to $0.73\text{ ms}$ ($1370\text{ FPS}$).
