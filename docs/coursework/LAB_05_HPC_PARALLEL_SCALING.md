# Engineering Coursework: Laboratory Module 5
## High-Performance Computing Paradigms: Sequential vs. OpenMP vs. CUDA GPU

**Course:** Computational Dynamics & Mechanical Systems Simulation
**Platform:** Parallel Particle Dynamics Lab (`sim_lab`, `benchmark_omp`, `benchmark_cross_backend`)
**Module Target:** Shared-Memory MIMD, SIMT GPU, Amdahl's Law, Algorithmic vs Hardware Acceleration

---

### 1. Educational Objectives

By completing this laboratory assignment, students will:
1. **Compare three fundamental computing paradigms** in computational mechanics:
   - Sequential von Neumann CPU (`sim_core`)
   - Shared-memory MIMD multicore CPU with OpenMP (`sim_omp_v2`)
   - Massively parallel SIMT accelerator with NVIDIA CUDA (`sim_cuda_v2`)
2. **Disentangle algorithmic acceleration from hardware acceleration**, evaluating why spatial hashing ($O(N)$) achieves a greater speedup than throwing hardware cores at brute-force $O(N^2)$ loops.
3. **Analyze Amdahl's Law and strong scaling**, measuring parallel speedup $S(T)$ and parallel efficiency $E(T)$ across $1$ to $16$ threads.
4. **Identify the latency-to-throughput crossover regime** between CPU caches and GPU device memory across particle counts from $N=100$ to $N=10,000$.

---

### 2. Theoretical Background & Governing Equations

#### 2.1 Amdahl's Law (Strong Scaling)
Let $f \in [0, 1]$ represent the parallelizable fraction of the simulation workload, and $(1 - f)$ the strictly serial remainder (synchronization barriers, thread launch overheads, global diagnostic reductions).

The theoretical speedup $S(T)$ achievable on $T$ processing threads is:
$$S(T) = \frac{1}{(1 - f) + \frac{f}{T}}$$

As $T \to \infty$, the maximum possible speedup is bounded asymptotically by:
$$S_{\max} = \lim_{T \to \infty} S(T) = \frac{1}{1 - f}$$
For example, if $15\%$ of the code is serial ($f = 0.85$), the theoretical maximum speedup regardless of thread count is $\frac{1}{0.15} \approx 6.67\times$.

#### 2.2 Parallel Efficiency
Parallel efficiency measures how effectively additional compute hardware is utilized:
$$E(T) = \frac{S(T)}{T} \times 100\%$$
- Ideal linear scaling corresponds to $S(T) = T \implies E(T) = 100\%$.
- Real shared-memory multicore systems suffer efficiency dropoffs due to memory bus saturation, cache line bouncing, and barrier synchronizations.

#### 2.3 The Algorithmic vs. Hardware Acceleration Fallacy
A common error in computational science is running brute-force $O(N^2)$ all-pairs loops on a GPU or multicore CPU and reporting massive speedup over single-threaded brute force.

In reality, before parallelizing, an engineer must optimize the algorithm:
- **Brute Force All-Pairs:** Every particle compares against all $N-1$ particles $\implies \frac{N(N-1)}{2}$ distance checks ($O(N^2)$).
- **Uniform Spatial Grid:** Particles are binned into spatial cells of width $W \ge 2r_{\max}$. Each particle only checks neighbors in its own cell and 8 adjacent cells $\implies O(N)$ expected comparisons.

The total speedup of a parallel system over baseline is a **compound product**:
$$S_{\text{compound}} = S_{\text{algorithmic}} \times S_{\text{hardware}}$$

---

### 3. Laboratory Procedure

#### Exercise 1: Investigating Algorithmic vs. Hardware Speedup ($N = 1,024$)
Inspect the data generated in `output/benchmarks/broadphase_and_omp_breakdown.csv`:

| Mode | Algorithmic Strategy | Computing Hardware | Step Compute Time [ms] | Speedup vs Baseline | Acceleration Type |
|:---:|:---:|:---:|:---:|:---:|:---:|
| **Mode A** | All-Pairs $O(N^2)$ | 1 CPU Core | **24.99 ms** | $1.00\times$ | Reference Baseline |
| **Mode B** | Spatial Grid $O(N)$ | 1 CPU Core | **1.04 ms** | **$23.94\times$** | Pure Algorithmic |
| **Mode C** | All-Pairs $O(N^2)$ | 8 OpenMP Cores | **5.64 ms** | **$4.43\times$** | Pure Hardware Parallel |
| **Mode D** | Spatial Grid $O(N)$ | 8 OpenMP Cores | **0.73 ms** | **$34.27\times$** | Compound (Algo + HW) |

Open `output/plots/plot_broadphase_breakdown.png` and verify how Mode B achieves over $5\times$ greater acceleration than Mode C without requiring any multicore hardware!

#### Exercise 2: OpenMP Strong Scaling Sweep ($N = 512, 1024$)
Inspect `output/benchmarks/openmp_scaling_results.csv` and `output/plots/plot_openmp_scaling.png`:
1. Record measured speedup $S(T)$ for $N = 1,024$:
   - $T = 1$: $S(1) = 1.00\times$, $E(1) = 100\%$
   - $T = 2$: $S(2) = 1.75\times$, $E(2) = 87.5\%$
   - $T = 4$: $S(4) = 3.07\times$, $E(4) = 76.8\%$
   - $T = 8$: $S(8) = 3.79\times$, $E(8) = 47.4\%$
   - $T = 16$: $S(16) = 2.44\times$, $E(16) = 15.3\%$
2. Note what occurs between $T = 8$ and $T = 16$ on the Intel hybrid processor (6 Performance cores + 4 Efficiency cores):
   - Adding hyperthreads and scheduling across asymmetric E-cores causes thread contention, causing speedup to degrade from $3.79\times$ down to $2.44\times$.

#### Exercise 3: Cross-Backend Regime Crossover ($N = 100 \dots 10,000$)
Inspect `output/benchmarks/cross_backend_scaling_results.csv` and `output/plots/plot_cross_backend_scaling.png`:

| Particle Count $N$ | CPU Serial Grid [ms] | OpenMP 8T Grid [ms] | CUDA RTX 4060 [ms] | GPU Speedup vs CPU Grid | Dominant Limiting Factor |
|:---:|:---:|:---:|:---:|:---:|:---:|
| **100** | $0.090$ ms | $0.266$ ms | $1.553$ ms | $0.06\times$ (Slower) | GPU Kernel Launch Latency Floor |
| **500** | $0.616$ ms | $1.524$ ms | $1.568$ ms | $0.39\times$ (Slower) | GPU Overhead Dominance |
| **1,000** | $1.208$ ms | $0.720$ ms | $1.390$ ms | $0.87\times$ | **Crossover Regime** |
| **5,000** | $10.375$ ms | $4.660$ ms | $2.376$ ms | **$4.37\times$ Faster** | GPU SIMT Throughput Advantage |
| **10,000** | $37.207$ ms | $14.001$ ms | **$2.049$ ms** | **$18.16\times$ Faster** | Massive Massive Parallel Saturation |

---

### 4. Analysis Questions

1. **Why is CUDA slower than CPU for $N=100$?** Explain the concept of the *overhead floor* ($\approx 1.4\text{ ms}$). Include PCIe transfer latencies, CUDA driver kernel launch overhead, and thread warp occupancy in your answer.
2. **Cache Locality vs Memory Bandwidth:** At $N=100$, the entire particle dataset requires $100 \times 32\text{ bytes} = 3.2\text{ KB}$, fitting entirely inside the CPU's $48\text{ KB}$ L1 data cache. Why does the CPU perform so exceptionally well for small degrees of freedom?
3. **Scaling Superiority:** At $N=10,000$, the CPU requires $37.2\text{ ms}$ per step ($26.8\text{ FPS}$), breaking real-time interactive requirements ($>60\text{ FPS}$). What framerate does the RTX 4060 GPU achieve at $N=10,000$? What is its throughput in particles per second?
