# Engineering Coursework: Laboratory Module 4
## Contact Mechanics, Crowding, and Discrete Collision Tunneling

**Course:** Computational Dynamics & Mechanical Systems Simulation
**Platform:** Parallel Particle Dynamics Lab (`sim_lab`)
**Module Target:** Discrete Collision Detection (DCD), CFL Tunneling Limit, Baumgarte Stabilization, Dense Granular Piles

---

### 1. Educational Objectives

By completing this laboratory assignment, students will:
1. **Analyze the discrete collision detection (DCD) failure mode** known as *particle tunneling*, where high-speed particles pass through one another or container walls without collision detection triggering.
2. **Evaluate the Courant-Friedrichs-Lewy (CFL) displacement heuristic** ($\text{CFL} = \frac{v_{\max}\Delta t_{\text{sub}}}{r_{\min}}$) and understand how temporal substepping prevents tunneling.
3. **Investigate non-smooth contact stabilization** via mass-weighted Baumgarte positional projection ($\beta \in [0.2, 0.5]$).
4. **Compare iterative contact solvers** (Sequential Gauss-Seidel vs Parallel Jacobi contact relaxation) in dense granular column settling ($N = 48$ particles).

---

### 2. Theoretical Background & Governing Equations

#### 2.1 The High-Speed Tunneling Hazard in Discrete Time
In discrete collision detection (DCD), collisions are checked only at discrete sampling points $t_n, t_{n+1}$. Between steps, a particle moves along a discrete displacement vector $\Delta \mathbf{x} = \mathbf{v} \Delta t_{\text{sub}}$.

If the displacement magnitude exceeds the particle diameter ($2r$):
$$\|\Delta \mathbf{x}\| = \|\mathbf{v}\|\,\Delta t_{\text{sub}} > 2r$$
a particle can jump completely across an obstacle or another particle without ever reporting an overlap ($\delta \le 0$ at both $t_n$ and $t_{n+1}$).

To prevent tunneling, the discrete collision condition demands that particle displacement per substep is smaller than particle radius:
$$\text{CFL}_{\text{DCD}} = \frac{v_{\max}\,\Delta t_{\text{sub}}}{r_{\min}} < 1.0$$
- **$\text{CFL} < 0.5$ (Green):** Safe regime. The particle travels less than half its radius per substep; tunneling hazard is minimized.
- **$0.5 \le \text{CFL} < 1.0$ (Yellow):** Marginal regime. Grazing impacts or high accelerations may penetrate deeply.
- **$\text{CFL} \ge 1.0$ (Red):** Unstable regime. Severe hazard of particles tunneling through walls or one another.

> **Engineering Caution & Theoretical Limit:** The Courant-like displacement ratio $\text{CFL}_{\text{DCD}}$ is a discrete displacement-to-radius heuristic sentinel, **NOT** an absolute mathematical guarantee of zero tunneling. Because DCD samples particle positions only at discrete instants $t_n, t_{n+1}$, grazing oblique contacts, acute boundary corners, or large intra-step acceleration spikes can still induce tunneling even when $\text{CFL} < 0.5$. In safety-critical aerospace and robotics engines, Continuous Collision Detection (CCD) with swept volumes or conservative advancement is mandatory.

#### 2.2 Baumgarte Positional Penetration Stabilization
When two particles overlap by penetration depth $\delta = (r_i + r_j) - \|\mathbf{x}_j - \mathbf{x}_i\| > 0$, an instantaneous position correction is applied along contact normal $\mathbf{n}_{ij}$:
$$\Delta \mathbf{x}_i = -\beta \left(\frac{m_j}{m_i + m_j}\right) \delta\,\mathbf{n}_{ij}, \qquad \Delta \mathbf{x}_j = +\beta \left(\frac{m_i}{m_i + m_j}\right) \delta\,\mathbf{n}_{ij}$$
where $\beta \in [0.2, 0.5]$ is the Baumgarte stabilization factor.

**Crucial Computational Rule:** Positional projection $\Delta \mathbf{x}$ separates particles geometrically without altering velocity $\mathbf{v}$. Modifying velocities to fix geometric overlap would inject spurious kinetic energy into the simulation, causing granular piles to "boil" or explode.

#### 2.3 Gauss-Seidel vs Jacobi Iterative Relaxation
In dense granular systems, multiple particles touch simultaneously (forming contact networks):
1. **Gauss-Seidel (Sequential CPU):** Contacts are visited sequentially in pairs. When contact $(i, j)$ is resolved, updated positions $\mathbf{x}_i, \mathbf{x}_j$ are immediately visible to subsequent contacts $(j, k)$. Yields rapid convergence and high contact stiffness.
2. **Jacobi (Parallel OpenMP & CUDA GPU):** All contacts are evaluated in parallel using read-only states from the start of the iteration. Accumulators sum displacement corrections:
   $$\Delta \mathbf{x}_i^{\text{total}} = \sum_{j \in \mathcal{N}(i)} \Delta \mathbf{x}_{ij}$$
   Positions update simultaneously at iteration barriers. To prevent overshooting, multiple relaxation iterations ($K = 8$) are executed per substep.

---

### 3. Laboratory Procedure

#### Exercise 1: Inducing and Curing High-Speed Tunneling
1. Launch `sim_lab`:
   ```bash
   ./build/sim_lab
   ```
2. Select **Scenario: Two-Particle Impact (EXP_01)**.
3. In the Controls panel:
   - Set **Substeps** to $1$.
   - Increase particle velocities to $v_{1x} = +20.0\text{ m/s}$ and $v_{2x} = -20.0\text{ m/s}$.
4. Observe the **DCD Tunneling Sentinel (CFL)** readout in the Diagnostics panel:
   - Notice the CFL indicator turns **RED ($\text{CFL} > 1.0$)**.
5. Click **Play**:
   - Observe the particles: they tunnel straight through each other without bouncing!
6. Click **Pause**, then increase **Substeps** from $1$ to $8$ or $16$:
   - Observe the CFL indicator transition to **GREEN ($\text{CFL} < 0.5$)**.
7. Reset and replay:
   - Confirm that with sufficient substepping, the collision is properly detected and particles bounce correctly.

#### Exercise 2: Dense Granular Column Settling ($N = 48$)
1. Select **Scenario: Dense Granular Column (EXP_04)**.
2. Allow the particles to fall and settle under gravity into the bottom of the circular container ($R = 1.0\text{ m}$).
3. Observe the active diagnostics:
   - **Active Pairwise Contacts:** Note that after settling, the simulator reports between $50$ and $70$ simultaneous active contacts.
   - **Max Penetration Depth:** Observe that maximum penetration $\delta_{\max}$ stabilizes to $< 0.4\text{ mm}$ ($< 1\%$ of particle radius).
4. Switch solver backends between `[CPU Sequential]`, `[OpenMP Multicore]`, and `[CUDA GPU]`:
   - Verify that all three backends maintain a stable, non-exploding settled heap.
   - Inspect the **Floating-Point Health Sentinel**: confirm `100% FINITE (0 NaN, 0 Inf)`.

---

### 4. Analysis Questions

1. **Continuous Collision Detection (CCD):** How does Continuous Collision Detection (e.g. swept sphere volumes or time-of-impact root finding) prevent tunneling without requiring high substepping counts? Why is DCD with substepping often preferred in high-performance GPU engines?
2. **Contact-Count Normalization Hazard:** Some naive simulation codes divide the displacement correction by the number of contacts $K_i$: $\Delta \mathbf{x}_i = \frac{1}{K_i}\sum \delta_{ij}\mathbf{n}_{ij}$. Why does this heuristic cause bottom particles in tall granular columns to sink through the container floor?
3. **Jacobi Relaxation Over-Stiffness:** In the Jacobi solver, if particle $i$ is surrounded by 6 packed neighbors pushing outward, what happens if we apply full displacement corrections without dampening or sub-iterations?
