# Physics Design Decisions & Architecture Specification for Simulator V2 (Phase 1A Revised)

**Document:** Physics & Numerical Methods Architectural Decision Record
**Target System:** 2D Particle Simulation (Simulator V2)
**Workspace:** `/home/devansh/simulator-workspaces/cuda-particle-simulation-v2`
**Branch:** `feature/simulator-v2`
**Phase:** Phase 1A (Shared Physics Foundation & Sequential CPU Reference)
**Status:** Approved Reference Specification

---

## 1. Executive Design Summary

This document establishes the definitive physical equations, numerical integration method, contact mechanics formulation, coordinate conventions, conservation invariants, and verification standards for Simulator V2. It resolves all physical divergences and numerical defects identified during the audit and provides the foundation for the sequential CPU reference solver (`sim_core`).

---

## 2. Units, Dimensions, and Coordinate Conventions

To guarantee reproducibility and avoid arbitrary scaling factors, Simulator V2 operates strictly in standard **International System of Units (SI)** internally.

### 2.1 Explicit SI World Units

| Quantity | Symbol | SI Unit | Dimension | Notes / Standard Reference Values |
| :--- | :--- | :--- | :--- | :--- |
| **Time** | $t, \Delta t$ | seconds ($\text{s}$) | $[T]$ | Substep $\Delta t_{\text{sub}} \le 2.0833 \times 10^{-3}\,\text{s}$ ($S \ge 8$ at $60\,\text{Hz}$) |
| **Length / Position** | $\mathbf{x}, r$ | meters ($\text{m}$) | $[L]$ | Particle radius $r \in [0.01, 0.05]\,\text{m}$ ($1$ to $5\,\text{cm}$) |
| **Linear Velocity** | $\mathbf{v}$ | meters per second ($\text{m/s}$) | $[L T^{-1}]$ | Initial speeds $\sim 0.1 - 2.0\,\text{m/s}$ |
| **Linear Acceleration**| $\mathbf{a}, \mathbf{g}$ | meters per second squared ($\text{m/s}^2$) | $[L T^{-2}]$ | Standard gravity $g = 9.80665\,\text{m/s}^2$ |
| **Areal Mass Density** | $\rho$ | kilograms per square meter ($\text{kg/m}^2$)| $[M L^{-2}]$ | Standard density $\rho = 1000.0\,\text{kg/m}^2$ (or normalized $1.0\,\text{kg/m}^2$) |
| **Particle Mass** | $m$ | kilograms ($\text{kg}$) | $[M]$ | $m_i = \pi \rho r_i^2$ |
| **Linear Momentum** | $\mathbf{p}, \mathbf{P}$ | kilogram-meters per second ($\text{kg}\cdot\text{m/s}$) | $[M L T^{-1}]$ | $\mathbf{P} = \sum m_i \mathbf{v}_i$ |
| **Force** | $\mathbf{F}$ | Newtons ($\text{N} = \text{kg}\cdot\text{m/s}^2$) | $[M L T^{-2}]$ | Gravitational force $\mathbf{F}_g = m \mathbf{g}$ |
| **Impulse** | $\mathbf{J}$ | Newton-seconds ($\text{N}\cdot\text{s} = \text{kg}\cdot\text{m/s}$) | $[M L T^{-1}]$ | Instantaneous momentum transfer $\Delta \mathbf{p} = \mathbf{J}$ |
| **Kinetic Energy** | $E_k$ | Joules ($\text{J} = \text{kg}\cdot\text{m}^2/\text{s}^2$) | $[M L^2 T^{-2}]$ | $E_k = \frac{1}{2} \sum m_i \|\mathbf{v}_i\|^2$ |

### 2.2 Coordinate System Conventions
- **Physical Simulation Frame:** Standard right-handed Cartesian coordinate system:
  - Origin $(0.0, 0.0)\,\text{m}$ is centered at the geometry center (e.g. center of the circular container).
  - $+x$ axis points horizontally to the **right**.
  - $+y$ axis points vertically **upward**.
  - Gravitational acceleration acts strictly downward along the $-y$ axis:
    $$\mathbf{g} = (0.0, -9.80665)^T\,\text{m/s}^2$$
- **Decoupling Simulation Coordinates from Rendering:**
  The physical simulation engine knows **nothing** about pixel coordinates, window dimensions, or display inversion. Rendering subsystems map world meters to screen pixels via an explicit affine viewport transform:
  $$x_{\text{screen}} = c_x + s \cdot x_{\text{world}}, \quad y_{\text{screen}} = c_y - s \cdot y_{\text{world}}$$
  where $(c_x, c_y)$ is the screen center in pixels, and $s$ is the display scale factor (pixels per meter).

---

## 3. Recommended Numerical Integration: Substepped Semi-Implicit Euler

### 3.1 Integrator Selection Analysis in Non-Smooth Contact Dynamics

In celestial mechanics or molecular dynamics governed by smooth ($C^2$) potential fields, higher-order symplectic integrators (such as Velocity Verlet with $O(\Delta t^2)$ or Runge-Kutta 4 with $O(\Delta t^4)$) offer superior energy conservation.

However, in **rigid/hard particle contact dynamics**:
- Contact impacts produce instantaneous, discontinuous velocity jumps ($\Delta \mathbf{v}$) and non-differentiable force profiles.
- Across non-smooth contact events, formal Taylor series expansions break down, reducing the global convergence order of **any** numerical scheme across impacts to **$O(\Delta t)$**.
- Velocity Verlet requires two half-kicks per timestep ($v_{n+1/2}$ and $v_{n+1}$). When non-smooth collision impulses occur at position $x_{n+1}$, applying impulsive velocity updates to $v_{n+1/2}$ before the second kick or to $v_{n+1}$ after the second kick creates phase-coupling errors with the continuous acceleration field.

### 3.2 Chosen Formulation: Substepped Semi-Implicit Euler (Symplectic Euler)
For Phase 1A and the Simulator V2 architecture, **Substepped Semi-Implicit Euler** is selected as the canonical reference scheme.

For each substep $k \in \{1, \dots, S\}$ with timestep $\Delta t_{\text{sub}} = \Delta t_{\text{frame}} / S$:
1. **Velocity Prediction (External Accelerations):**
   $$\mathbf{v}_i^* = \mathbf{v}_i^n + \mathbf{g} \, \Delta t_{\text{sub}}$$
2. **Position Integration (Ballistic Drift):**
   $$\mathbf{x}_i^* = \mathbf{x}_i^n + \mathbf{v}_i^* \, \Delta t_{\text{sub}}$$
3. **Contact Detection & Pair Identification:**
   Identify overlapping pairs where $d_{ij} < r_i + r_j$.
4. **Frictionless Normal Collision Impulse Resolution:**
   Compute momentum-conserving normal impulses on velocity state $\mathbf{v}^*$, yielding updated physical velocities $\mathbf{v}^{n+1}$.
5. **Numerical Penetration Stabilization:**
   Apply position-level constraint projection directly to coordinates $\mathbf{x}^*$, yielding updated physical positions $\mathbf{x}^{n+1}$.
6. **Boundary Constraint Enforcement:**
   Apply boundary velocity reflection and position clamping.

---

## 4. Contact Mechanics Formulation

### 4.1 Strict Decoupling: Physical Impulses vs Positional Stabilization
Simulator V2 enforces an absolute architectural separation between:
1. **Physical Collision Response (Velocity Update):** Momentum-conserving normal impulses modeling restitution and kinetic energy transfer.
2. **Numerical Overlap Correction (Position Stabilization):** Geometric projection preventing interpenetration drift caused by finite timestepping.

```
+---------------------------------------------------------------------------------------------------+
| CONTACT MECHANICS DECOUPLING                                                                      |
|                                                                                                   |
|  [Contact Detection]  -->  [1. Velocity Impulse Response]  -->  [2. Positional Stabilization]     |
|  - Overlap: delta > 0      - Acts on relative velocity          - Acts on particle positions      |
|  - Normal: n               - Applies impulse J_n                - Resolves overlap delta          |
|  - Approaching: v_n < 0    - Conserves momentum                 - Does NOT inject velocity        |
+---------------------------------------------------------------------------------------------------+
```

### 4.2 Contact Kinematics & Singularity Regularization
For particles $i$ and $j$:
$$\mathbf{r}_{ij} = \mathbf{x}_i - \mathbf{x}_j, \quad d_{ij} = \|\mathbf{r}_{ij}\|$$
$$\text{Geometric Penetration Depth:} \quad \delta = (r_i + r_j) - d_{ij}$$

#### Singularity Regularization ($d_{ij} \to 0$):
If two particles are initialized or collide at identical coordinates ($d_{ij} \le \epsilon = 10^{-6}\,\text{m}$), standard unit normal evaluation divides by zero ($0.0 / 0.0 = \text{NaN}$). The contact normal is regularized deterministically:
$$\mathbf{n} = \begin{cases} \frac{\mathbf{r}_{ij}}{d_{ij}} & \text{if } d_{ij} > \epsilon \\ (1.0, 0.0)^T & \text{if } d_{ij} \le \epsilon \end{cases}$$
$\mathbf{n}$ points along the line of centers from particle $j$ toward particle $i$.

Relative contact velocity:
$$\mathbf{v}_{\text{rel}} = \mathbf{v}_i^* - \mathbf{v}_j^*$$
Normal component of relative velocity:
$$v_n = \mathbf{v}_{\text{rel}} \cdot \mathbf{n}$$

### 4.3 Frictionless Normal Collision Impulse
Per the Phase 1A design requirements, **tangential friction is omitted** until a physically consistent contact model incorporating rotational degrees of freedom is defined. Contacts are purely frictionless.

A collision impulse is applied **if and only if** particles are moving toward each other:
$$v_n < 0$$
(If $v_n \ge 0$, particles are separating; no physical impulse is applied).

#### Normal Impulse Magnitude ($J_n$):
For coefficient of restitution $e \in [0, 1]$:
$$m_{\text{eff}} = \frac{m_i m_j}{m_i + m_j}$$
$$J_n = -(1 + e) \, m_{\text{eff}} \, v_n$$
Since $v_n < 0$, $J_n > 0$.

#### Velocity Update:
$$\mathbf{v}_i \leftarrow \mathbf{v}_i + \frac{J_n}{m_i} \mathbf{n}$$
$$\mathbf{v}_j \leftarrow \mathbf{v}_j - \frac{J_n}{m_j} \mathbf{n}$$

### 4.4 Positional Penetration Stabilization
Due to discrete timestepping, particles overlap geometrically ($\delta > 0$). To prevent numerical penetrations from accumulating without adding artificial kinetic energy:
$$\Delta \mathbf{x}_i = +\alpha \, \delta \left(\frac{m_j}{m_i + m_j}\right) \mathbf{n}$$
$$\Delta \mathbf{x}_j = -\alpha \, \delta \left(\frac{m_i}{m_i + m_j}\right) \mathbf{n}$$
where $\alpha \in [0.2, 0.8]$ is the Baumgarte relaxation factor (default $\alpha = 0.5$).
$$\mathbf{x}_i \leftarrow \mathbf{x}_i + \Delta \mathbf{x}_i, \quad \mathbf{x}_j \leftarrow \mathbf{x}_j + \Delta \mathbf{x}_j$$

**CRITICAL INVARIANT:** $\Delta \mathbf{x}$ updates particle positions directly. It is **never** divided by $\Delta t$ to modify velocity.

### 4.5 Boundary Interactions (Circular Container)
Let the circular container have center $\mathbf{c} = (0, 0)\,\text{m}$ and radius $R$.
For particle $i$:
$$\mathbf{r}_b = \mathbf{x}_i - \mathbf{c}, \quad d_b = \|\mathbf{r}_b\|$$
Boundary collision occurs when $d_b > R - r_i$.
- **Inward Unit Normal:** $\mathbf{n}_b = -\frac{\mathbf{r}_b}{d_b}$ (points toward container center).
- **Normal Velocity:** $v_n = \mathbf{v}_i \cdot \mathbf{n}_b$.
- **Frictionless Velocity Reflection:** If moving outward ($v_n < 0$):
  $$\mathbf{v}_i \leftarrow \mathbf{v}_i - (1 + e_b) v_n \mathbf{n}_b$$
  where $e_b \in [0, 1]$ is boundary restitution.
- **Position Clamping:**
  $$\mathbf{x}_i \leftarrow \mathbf{c} + (R - r_i) \frac{\mathbf{r}_b}{d_b}$$

---

## 5. Simultaneous Contacts & The Risks of Contact-Count Normalization

In dense particle assemblies (e.g. resting beds under gravity), a single particle experiences simultaneous contacts with multiple neighbors ($K > 1$).

### 5.1 Analysis & Rejection of Contact-Count Normalization
In early prototype iterations, dividing accumulated displacements by contact count ($1/K$) was proposed to prevent displacement over-correction in parallel Jacobi solvers.

**Fatal Physical Flaws of Contact-Count Normalization:**
1. **Severe Loss of Contact Stiffness:** A particle at the bottom of a stack supporting a column of 5 particles has $K \approx 6$. Dividing normal impulses or displacements by 6 reduces constraint stiffness by an order of magnitude. The bottom particle sags and sinks unnaturally through the boundary or neighboring particles.
2. **Violation of Newton's Third Law (Action-Reaction Asymmetry):** If particle $i$ has $K_i = 1$ contact (with particle $j$) and particle $j$ has $K_j = 4$ contacts (with $i$ and three others), scaling particle $i$'s displacement by $1/1$ and particle $j$'s displacement by $1/4$ breaks bilateral symmetry:
   $$m_i \Delta \mathbf{x}_i + m_j \Delta \mathbf{x}_j \neq \mathbf{0}$$
   Action and reaction no longer balance, creating artificial external forces that move the center of mass!
3. **Arbitrary Geometric Artifacts:** Particles in loose clusters experience different contact stiffness than isolated particles, creating non-physical macroscopic compressibility.

### 5.2 Approved Approach for Sequential CPU Solver (Gauss-Seidel Pairwise Iteration)
In the sequential CPU reference solver, simultaneous contacts are resolved using **iterative pairwise Gauss-Seidel relaxation**:
- Contact pairs are identified and sorted deterministically.
- Over multiple solver iterations per substep ($N_{\text{iter}} \in [1, 4]$), pairwise impulses and displacements are applied in-place.
- Each pairwise interaction exactly preserves linear momentum and center of mass without any artificial $1/K$ dilution.

---

## 6. Conservation Properties of the Physics Engine

| Physical Property | Isolated Pair Collision | Dense Multi-Contact Pack | Conservative Regime ($e=1, e_b=1$) | Inelastic Regime ($e < 1$) |
| :--- | :--- | :--- | :--- | :--- |
| **Total Linear Momentum** ($\mathbf{P}$) | **Strictly Conserved** ($\Delta \mathbf{P} = \mathbf{0}$) | **Strictly Conserved** (closed domain) | **Strictly Conserved** (zero gravity) | **Strictly Conserved** (zero gravity) |
| **Center of Mass Position** ($\mathbf{X}_{\text{cm}}$)| **Strictly Invariant** under $\Delta \mathbf{x}$ | **Strictly Invariant** under $\Delta \mathbf{x}$ | **Strictly Invariant** (isolated) | **Strictly Invariant** (isolated) |
| **Kinetic Energy** ($E_k$) | **Conserved** for $e=1.0$ | Bounded Symplectic Oscillation | **Conserved** ($E_k + E_{\text{pot}} = \text{const}$) | **Strictly Dissipated** ($\Delta E_k < 0$) |

### Analytical Kinetic Energy Dissipation in Inelastic Collisions
For an isolated 2-particle collision with restitution $e$:
$$\Delta E_k = E_k^{\text{post}} - E_k^{\text{pre}} = -\frac{1}{2} \left(\frac{m_i m_j}{m_i + m_j}\right) (1 - e^2) v_n^2 \le 0$$
- If $e = 1.0 \implies \Delta E_k = 0$ (energy conserved).
- If $e = 0.0 \implies \Delta E_k = -\frac{1}{2} m_{\text{eff}} v_n^2$ (maximum kinetic energy dissipated into heat/deformation).
- **Rule:** Validation tests for inelastic collisions must verify this exact energy dissipation formula, rather than asserting energy conservation.

---

## 7. Limitations of Discrete Collision Detection & High-Speed Tunneling

### 7.1 The Tunneling Phenomenon
Simulator V2 employs **Discrete Collision Detection (DCD)**, evaluating overlaps at discrete time steps $t_n, t_{n+1}$.
If a particle moves with high velocity $v$ such that its displacement within a single substep exceeds the diameter of an obstacle:
$$\Delta x_{\text{step}} = v \, \Delta t_{\text{sub}} \ge 2 r_{\text{obstacle}}$$
the particle will be completely on one side of the obstacle at $t_n$ and completely on the other side at $t_{n+1}$. DCD will detect **zero overlap**, and the particle will pass cleanly through (tunneling).

### 7.2 Contact CFL Stability Condition
To prevent tunneling and guarantee that DCD detects all contacts:
$$\Delta t_{\text{sub}} \le \frac{C_{\text{CFL}} \, r_{\text{min}}}{v_{\text{max}}}$$
where $C_{\text{CFL}} \in [0.2, 0.5]$ is the safety factor.
For $r_{\text{min}} = 0.01\,\text{m}$ ($1\,\text{cm}$) and $v_{\text{max}} = 2.0\,\text{m/s}$:
$$\Delta t_{\text{sub}} \le \frac{0.25 \times 0.01}{2.0} = 1.25 \times 10^{-3}\,\text{s} \implies S \ge 14\,\text{substeps at } 60\,\text{Hz}$$

---

## 8. Justified Numerical Tolerances for Validation

All numerical tolerances are mathematically justified based on IEEE 754 single-precision float (`float32`) limits (machine epsilon $\epsilon_{\text{mach}} \approx 1.192 \times 10^{-7}$, mantissa precision 24 bits):

| Validation Case | Tested Metric | Justification | Tolerances |
| :--- | :--- | :--- | :--- |
| **TC01: Constant Velocity** | Position $\|\mathbf{x}(t) - (\mathbf{x}_0 + \mathbf{v}_0 t)\|$ | Exact integration; float32 roundoff accumulation | Absolute error $\le 10^{-5}\,\text{m}$ |
| **TC02: Constant Acceleration** | Position $y(t)$ & Velocity $v_y(t)$ | Symplectic Euler position drift error $\sim \frac{1}{2}g \Delta t$ | Position error $\le 10^{-3}\,\text{m}$; Velocity exact to $10^{-5}\,\text{m/s}$ |
| **TC03: 1D Equal-Mass Elastic** | Post-collision velocities & Energy | 1D kinematic swap; single contact impulse | Velocity error $\le 10^{-5}\,\text{m/s}$; Energy error $\le 10^{-5}\,\text{J}$ |
| **TC04: 1D Unequal-Mass Elastic**| Post-collision velocities & Energy | Mass ratio kinematic formulas | Velocity error $\le 10^{-5}\,\text{m/s}$; Energy error $\le 10^{-5}\,\text{J}$ |
| **TC05: Inelastic Collision** | Separation ratio $\|v_{\text{rel}}'/v_{\text{rel}}\|$ & Momentum | Restitution definition; momentum conservation | Ratio error $\le 10^{-4}$; Momentum $\Delta \mathbf{P} \le 10^{-6}\,\text{kg}\cdot\text{m/s}$ |
| **TC06: Specular Boundary** | Reflection angle $\theta_r$ & Speed $\|v'\|$ | Wall impulse reflection; frictionless | Angle error $\le 0.01^\circ$; Speed error $\le 10^{-5}\,\text{m/s}$ |
| **TC07: Coincident Particles** | Position & velocity validity | Zero-distance regularization test | Zero NaNs, zero Infs; Separation $> 0$ |
| **TC08: N-Body Momentum Drift** | Isolated total momentum $\mathbf{P}(t)$ | Multi-body pairwise impulse symmetry | Rel. drift $\frac{\|\mathbf{P}(t) - \mathbf{P}(0)\|}{\|\mathbf{P}(0)\|} \le 10^{-5}$ |
| **TC09: Dense Contact Stability**| Particle positions & finite states | Multi-contact Gauss-Seidel stacking under gravity | Zero NaNs, penetration $\delta \le 10^{-3}\,\text{m}$ |
