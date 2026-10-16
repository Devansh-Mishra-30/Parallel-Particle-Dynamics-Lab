# Parallel Particle Dynamics Lab: Known Limitations & Numerical Boundary Envelope
## Phase 1C Scope Boundaries, Physical Assumptions, and Engineering Guidance

**Document Version:** 1.0.0
**Date:** 2026-10-16
**Classification:** Engineering Design & Numerical Limitations

---

### 1. Scope Boundary Notice

The primary objective of Phase 1C is establishing an educational verification workbench connecting analytical theory, discrete numerical methods, interactive simulation, and quantitative diagnostics.

Certain physical idealizations and algorithmic boundaries are intentional pedagogical choices or phase-specific scopes. This document defines these boundaries explicitly so that students and researchers do not misinterpret model approximations as software bugs.

---

### 2. Physical Modeling Assumptions & Limitations

#### 2.1 Frictionless Contacts
- **Current Model:** Particle-particle and particle-boundary collision responses are strictly normal Poisson/Newton impulses ($J_n = -(1+e)\mu v_n$). Tangential forces are assumed to be zero ($\mathbf{F}_{\text{tangential}} = \mathbf{0}$).
- **Physical Consequence:** Particles do not experience Coulomb dry friction ($\mu_s, \mu_k$), slip resistance, or rolling resistance. In granular heaps (EXP_04), particles settle at lower angles of repose than frictional sand or gravel.
- **Future Roadmap:** Coulomb friction cone projection can be introduced in educational extensions.

#### 2.2 Absence of Rotational Degrees of Freedom
- **Current Model:** Particles are idealized as 2D smooth cylinders/discs with 2 translational degrees of freedom $(x, y, v_x, v_y)$. Angular displacement $\theta$, angular velocity $\omega$, and rotational moment of inertia $I = \frac{1}{2}m r^2$ are not modeled.
- **Physical Consequence:** Oblique impacts do not induce spin or transfer translational kinetic energy into rotational kinetic energy.

#### 2.3 Hard Circular Boundary Projection
- **Current Model:** The circular boundary enforces containment via kinematic normal velocity reversal and position clamping: $\mathbf{x} \leftarrow \mathbf{c} + (R - r)(-\mathbf{n}_{\text{wall}})$.
- **Physical Consequence:** If an external force accelerates a particle such that $\Delta x_{\text{sub}} > 2(R - r)$, tunneling outside the circle may occur unless substeps are sufficiently dense.

---

### 3. Numerical & Algorithmic Limitations

#### 3.1 First-Order Symplectic Integration ($O(\Delta t^1)$)
- **Current Scheme:** Semi-Implicit Symplectic Euler.
- **Accuracy Order:** Global position error is first-order $O(\Delta t^1)$, verified in EXP_03.
- **Pedagogical Value:** Demonstrates phase-space volume preservation (symplecticity) and long-term energy stability without secular drift, while clearly exhibiting $O(\Delta t)$ convergence under refinement.
- **Limitation:** For high-precision ballistic trajectories requiring $10^{-6}\,\text{m}$ accuracy over long durations, higher-order schemes (Velocity Verlet $O(\Delta t^2)$ or RK4 $O(\Delta t^4)$) would be required, though classical non-symplectic RK4 suffers from long-term artificial dissipation in orbital problems.

#### 3.2 Discrete Collision Detection (DCD) vs Continuous Collision Detection (CCD)
- **Current Mechanism:** Collisions are detected only at discrete substep sample times $t_n = n \Delta t_{\text{sub}}$.
- **Tunneling Boundary (Sentinel Ratio):**
  $$\text{Ratio} = \frac{v_{\max} \Delta t_{\text{sub}}}{r_{\min}}$$
  If $\text{Ratio} > 1.0$, a fast-moving particle can pass entirely through another particle or boundary within a single substep.
- **Mitigation:** The application features a dynamic DCD Tunneling Sentinel in the HUD and right diagnostics panel (green $<0.5$, yellow $0.5 - 1.0$, red $>1.0$), educating students to increase substeps $N_{\text{sub}}$ when studying high-speed dynamics.

#### 3.3 Gauss-Seidel Contact Order Dependency
- **Current Solver:** Pairwise contacts and penetration corrections are relaxed sequentially using a Gauss-Seidel loop.
- **Consequence:** In dense columns, the ordering of particle indexing introduces a minor directional bias in the propagation of contact shock waves. Increasing Gauss-Seidel iterations from $2$ to $8$ suppresses this effect effectively.

---

### 4. Computational Performance Boundaries

#### 4.1 All-Pairs $O(N^2)$ Complexity in Reference Solver
- **Current Implementation:** `sim_core` implements an all-pairs $O(N^2)$ collision sweep.
- **Operating Envelope:**
  - $N \le 100$: Full 60 FPS interactive rate ($< 1\,\text{ms}$ step time).
  - $N = 200$: Interactive rate maintained ($2 - 4\,\text{ms}$ step time).
  - $N > 500$: Noticeable frame drops on sequential CPU.
- **Phase 2 & 3 Evolution:** Spatial hash grid partitioning and uniform grid binning will be introduced in Phase 2 (OpenMP) and Phase 3 (CUDA), scaling performance to $N \ge 100,000$ particles.

---

### 5. Architectural Readiness for Parallel Acceleration

The code architecture in `sim_core` was intentionally constructed with parallelization in mind:
1. **Separation of Concerns:** `ISolver` interface cleanly isolates integration and collision resolution.
2. **Particle Structure of Arrays (SoA) Readiness:** `Particle` struct uses standard float vectors suitable for vectorized SIMD loading.
3. **Reproducibility Guarantee:** Standalone experiment harnesses (`sim_experiments`) provide exact golden baselines against which OpenMP multicore and CUDA GPU solvers will be quantitatively validated.
