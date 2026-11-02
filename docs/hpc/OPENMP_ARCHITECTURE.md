# OpenMP Multicore Physics Engine: Architecture & Parallel Design
## Parallel Dynamics Lab — Phase 2 Architecture Specification

**Document Version:** 1.0.0
**Date:** 2026-10-16
**Module:** `sim_omp_v2`
**Target Hardware:** Multi-core x86_64 CPUs (e.g. Intel Core i7-13620H, 16 hardware threads)
**Compiler Support:** OpenMP 4.5+ / C++17 (`-fopenmp`)

---

### 1. Executive Summary & Design Mission

The objective of `sim_omp_v2` is to deliver a thread-safe, scalable, high-performance multicore physics engine that strictly preserves the physical governing equations and analytical invariants verified in Phase 1C:
1. **Newtonian Mechanics & Invariants:** Strict linear momentum conservation in isolated systems, theoretical kinetic energy dissipation $\Delta E_k = -\frac{1}{2}\mu(1-e^2)v_n^2$, and symplectic energy stability.
2. **Deterministic & Race-Free Execution:** Elimination of unsynchronized concurrent writes or race conditions across $1, 2, 4, 8, 16$ threads.
3. **Transparent Sequential Equivalence:** Identical physical outcomes to the sequential CPU reference for all isolated and binary collision dynamics, with well-documented, mathematically justified differences during dense multi-contact settling.

---

### 2. Comparative Evaluation of Parallelization Strategies

In particle dynamics simulations, four primary execution phases occur every substep:
1. **Particle Integration:** $\mathbf{v}_i \leftarrow \mathbf{v}_i + \frac{\Delta t}{m_i} \mathbf{F}_i^{\text{ext}}$, $\mathbf{x}_i \leftarrow \mathbf{x}_i + \Delta t \mathbf{v}_i$.
2. **Boundary Collision:** Inward projection and velocity reversal against the container perimeter.
3. **Collision Detection (Narrowphase / Broadphase):** Identifying overlapping pairs $(i, j)$ where $\|\mathbf{x}_i - \mathbf{x}_j\| < r_i + r_j$.
4. **Collision Response (Contact Resolution):** Computing normal impulses $J_n$ and Baumgarte position corrections.

While phases 1 and 2 are embarrassingly parallel ($O(1)$ independent per particle), collision response introduces data hazards whenever multiple contacts share particles (e.g. particle $j$ touching both $i$ and $k$).

| Strategy | Description | Race Freedom | Memory Overhead | Sync Cost | Scalability | Complexity |
|:---|:---|:---:|:---:|:---:|:---:|:---:|
| **A. Naive OpenMP Loop** | `#pragma omp for` over pairs with in-place updates | **FAIL (Races)** | None | Low | Unstable | Trivial |
| **B. Critical / Mutex Locking** | Locks per particle or global critical section | Pass | Lock array ($O(N)$) | **Extremely High** | Degrades ($<1\times$) | Low |
| **C. Contact Graph Coloring** | Partition pairs into independent sets (colors) | Pass | Graph buffers ($O(M)$) | High (Barriers) | Moderate | Very High |
| **D. Thread-Local Accumulation (Jacobi)** | Thread-local impulse/delta buffers reduced per particle | **Pass (100% Safe)** | $O(T \cdot N)$ | Low (Single barrier) | **High ($8\times$+)** | Moderate |
| **E. Spatial Domain Decomposition** | 2D uniform grid with checkerboard cell coloring | Pass | Grid headers | Moderate | High (large $N$) | High |

#### Selected Architecture: Hybrid Strategy (Thread-Local Jacobi + Spatial Broadphase)
Based on comparative analysis:
1. **Phase 1 (Integration & Boundaries):** Parallel for-loops across particles.
2. **Phase 2 (Contact Resolution):** **Thread-Local Jacobi Accumulation**. Each thread accumulates velocity and position corrections into thread-local buffers (or contiguous particle delta arrays). A parallel reduction step applies the accumulated deltas to particles.
   - **Why Jacobi over Gauss-Seidel for Parallelism?**
     In sequential Gauss-Seidel, updates to particle $j$ are immediately visible to subsequent pairs involving $j$. In parallel execution, enforcing sequential order requires serialization. Jacobi evaluates all contact impulses simultaneously based on the beginning-of-substep state, ensuring **100% determinism regardless of thread scheduling or thread count**.
   - **Equivalence in Binary Collisions:** For 2-body impacts (isolated collisions), Jacobi and Gauss-Seidel are **mathematically identical**, because each particle has at most one contact pair.
   - **Behavior in Dense Packings:** In dense multi-body packings, Jacobi applies the vector sum of all neighbor impulses. With $K$ relaxation iterations (default $K = 4$ to $8$), it converges smoothly to non-penetrating equilibrium without directional bias.

---

### 3. Mathematical Formulation of the Parallel Engine

#### 3.1 Parallel Symplectic Euler Time-Stepping
For particle $i \in [0, N-1]$ executed in parallel:

$$\mathbf{v}_i^{n+1} = \mathbf{v}_i^n + \Delta t_{\text{sub}} \mathbf{g}$$

$$\mathbf{x}_i^{n+1} = \mathbf{x}_i^n + \Delta t_{\text{sub}} \mathbf{v}_i^{n+1}$$

Independent across threads, vectorizable with SIMD.

#### 3.2 Thread-Safe Jacobi Contact Resolution
For a set of active contact pairs $\mathcal{C} = \{(i, j) \mid \|\mathbf{x}_i - \mathbf{x}_j\| < r_i + r_j\}$:

1. **Delta Initialization:** Each thread or particle maintains:
   $$\Delta \mathbf{v}_i = \mathbf{0}, \quad \Delta \mathbf{x}_i = \mathbf{0}, \quad c_i = 0$$
2. **Pairwise Impulse Evaluation (Parallel across pairs):**
   For pair $(i, j)$ with normal $\mathbf{n} = \frac{\mathbf{x}_i - \mathbf{x}_j}{\|\mathbf{x}_i - \mathbf{x}_j\|}$ and penetration $\delta = r_i + r_j - \|\mathbf{x}_i - \mathbf{x}_j\| > 0$:
   $$v_n = (\mathbf{v}_i - \mathbf{v}_j) \cdot \mathbf{n}$$
   If $v_n < 0$:
   $$J_n = -(1 + e) \mu v_n, \quad \mu = \frac{m_i m_j}{m_i + m_j}$$
   $$\delta \mathbf{v}_i = +\frac{J_n}{m_i} \mathbf{n}, \quad \delta \mathbf{v}_j = -\frac{J_n}{m_j} \mathbf{n}$$
   Position correction (mass-weighted Baumgarte):
   $$\delta \mathbf{x}_i = +\beta \frac{m_j}{m_i + m_j} \delta \mathbf{n}, \quad \delta \mathbf{x}_j = -\beta \frac{m_i}{m_i + m_j} \delta \mathbf{n}$$
3. **Atomic / Thread-Local Accumulation:**
   To guarantee race freedom without thread stalls:
   - For thread-local buffers $\mathbf{A}^{(t)}_i$: thread $t$ writes only to its own buffer $\mathbf{A}^{(t)}_i$.
   - Reduction phase: `#pragma omp for` across $i \in [0, N-1]$, sum $\Delta \mathbf{v}_i = \sum_t \delta \mathbf{v}_i^{(t)}$ and $\Delta \mathbf{x}_i = \sum_t \delta \mathbf{x}_i^{(t)}$.
4. **Application:**
   $$\mathbf{v}_i \leftarrow \mathbf{v}_i + \Delta \mathbf{v}_i$$
   $$\mathbf{x}_i \leftarrow \mathbf{x}_i + \Delta \mathbf{x}_i$$

---

### 4. Memory Layout & Cache Optimization

1. **False Sharing Mitigation:**
   Accumulator arrays are padded or structured to prevent multiple threads from writing to the same 64-byte cache line. Thread-private accumulators are sized as `std::vector<Vec2> local_dv[MAX_THREADS]`.
2. **Contiguous Memory:**
   Particles are stored in contiguous `std::vector<Particle>` memory, maximizing hardware prefetcher efficiency during parallel iterations.

---

### 5. Modular Class Hierarchy (`sim_omp_v2`)

```cpp
namespace sim_omp {

class OmpSolverV2 : public sim_core::ISolver {
public:
    explicit OmpSolverV2(const sim_core::SimConfig& config,
                         const sim_core::CircularBoundary& boundary);

    // ISolver Interface
    void step() override;
    void reset() override;
    void add_particle(const sim_core::Particle& p) override;
    const std::vector<sim_core::Particle>& get_particles() const override;
    void set_particles(const std::vector<sim_core::Particle>& particles) override;
    void set_config(const sim_core::SimConfig& config) override;
    sim_core::SimulationDiagnostics compute_diagnostics() const override;

    // Multicore Controls
    void set_num_threads(int threads);
    int get_num_threads() const;

    // HPC Profiling Telemetry
    struct StepTimings {
        double integrate_us{0.0};
        double broadphase_us{0.0};
        double narrowphase_us{0.0};
        double resolve_us{0.0};
        double boundary_us{0.0};
        double total_step_us{0.0};
    };
    const StepTimings& get_last_timings() const;

private:
    void integrate_particles_omp(float dt_sub);
    void handle_boundaries_omp();
    void resolve_contacts_all_pairs_omp();
    void resolve_contacts_grid_omp();
};

} // namespace sim_omp
```
