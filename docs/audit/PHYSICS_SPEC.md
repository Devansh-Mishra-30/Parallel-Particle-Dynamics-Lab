# Physics Specification & Mathematical Foundations for Simulator V2 (Revised)

**Target System:** 2D Particle Simulation (Unified Architecture)
**Workspace:** `/home/devansh/simulator-workspaces/cuda-particle-simulation-v2`
**Branch:** `feature/simulator-v2`
**Document Status:** Complete Physics Reference Specification (Phase 0 Revised)

---

## 1. Overview & Mathematical Model

This specification establishes a mathematically unified physical foundation for all execution backends (CPU Scalar, CPU OpenMP, and GPU CUDA). By adopting a single mathematical model, all backends solve identical equations of motion, exhibit identical conservation laws, and produce verifiable, reproducible physical results.

### Particle State Representation
Each particle $i \in \{1, \dots, N\}$ is modeled as a 2D rigid circular disk with:
- Position: $\mathbf{x}_i = (x_i, y_i)^T \in \mathbb{R}^2$
- Velocity: $\mathbf{v}_i = (u_i, v_i)^T \in \mathbb{R}^2$
- Acceleration: $\mathbf{a}_i \in \mathbb{R}^2$
- Radius: $r_i \in \mathbb{R}^+$
- Material Mass: $m_i = \pi \rho r_i^2$ (areal density $\rho = 1.0\,\text{kg/m}^2$, standardized across all backends)
- Coefficient of Restitution: $e \in [0, 1]$
- Coefficient of Friction: $\mu \ge 0$

---

## 2. Governing Equations of Motion

Particle trajectories are governed by Newton's Second Law for translational motion:
$$m_i \frac{d^2 \mathbf{x}_i}{dt^2} = m_i \frac{d\mathbf{v}_i}{dt} = \mathbf{F}_i(t)$$

where the total force $\mathbf{F}_i(t)$ acting on particle $i$ decomposes into:
$$\mathbf{F}_i(t) = \mathbf{F}_{\text{ext}, i} + \mathbf{F}_{\text{boundary}, i} + \sum_{j \neq i} \mathbf{F}_{\text{contact}, ij}$$

### External Forces (Gravity)
In a uniform gravitational field $\mathbf{g} = (0, -g_{\text{world}})^T$:
$$\mathbf{F}_{\text{ext}, i} = m_i \mathbf{g}$$
Standard convention for Simulator V2:
- Coordinate frame: Standard Cartesian (identical across all backends).
- Gravity magnitude: $g_{\text{world}} = 980.0\,\text{units/s}^2$ globally standardized across CPU, OpenMP, and CUDA.

---

## 3. Numerical Integration Formulations & Selection

In collision-dominated particulate flows, contact impacts produce discontinuous ($C^0$) or impulsive velocity jumps. Formal high-order smoothness assumptions (such as $O(\Delta t^2)$ for Verlet or $O(\Delta t^4)$ for RK4) drop to $O(\Delta t)$ across contact events. Integrators must therefore be evaluated based on contact-discontinuity stability, symplectic properties, and substepping efficiency.

### 3.1 Integrator Comparison
1. **Position Verlet:** Symplectic and compact, but velocity is implicit ($v = (x_{n+1} - x_{n-1})/2\Delta t$), complicating impulse application and dynamic substepping.
2. **Velocity Verlet:** 2nd-order for smooth trajectories, but requires splitting into two half-kicks ($v_{n+1/2}$ and $v_{n+1}$), which introduces subtle phase-coupling errors when non-smooth contact impulses occur at position $x_{n+1}$.
3. **Substepped Semi-Implicit Euler (Symplectic Euler) — SELECTED FOR V2:**
   - Velocity Prediction:
     $$\mathbf{v}_{n+1}^* = \mathbf{v}_n + \mathbf{g} \Delta t_{\text{sub}}$$
   - Position Integration:
     $$\mathbf{x}_{n+1}^* = \mathbf{x}_n + \mathbf{v}_{n+1}^* \Delta t_{\text{sub}}$$
   - Advantages: Exactly 1 force evaluation per substep, symplectic phase-space preservation during free motion, explicit velocity state, and natural sequencing with contact impulse solvers.

---

## 4. Collision Mechanics: Impulses vs Positional Stabilization

Simulator V2 strictly decouples physical collision response from numerical overlap correction:
- **Collision Impulses:** Modify **velocities** to satisfy physical momentum exchange and restitution.
- **Positional Stabilization:** Modifies **positions** to resolve geometric overlap without injecting phantom kinetic energy.

### 4.1 Contact Kinematics & Regularization
For particles $i$ and $j$:
$$\mathbf{r}_{ij} = \mathbf{x}_i - \mathbf{x}_j, \quad d_{ij} = \|\mathbf{r}_{ij}\|$$
$$\text{Unit Contact Normal:} \quad \mathbf{n} = \begin{cases} \frac{\mathbf{r}_{ij}}{d_{ij}} & \text{if } d_{ij} > \epsilon \\ (1.0, 0.0)^T & \text{if } d_{ij} \le \epsilon \end{cases}$$
where $\epsilon = 10^{-6}\,\text{units}$. This regularization prevents division-by-zero singularities and `NaN` propagation.

Overlap depth:
$$\delta = (r_i + r_j) - d_{ij} > 0$$
Relative contact velocity:
$$\mathbf{v}_{\text{rel}} = \mathbf{v}_i - \mathbf{v}_j, \quad v_n = \mathbf{v}_{\text{rel}} \cdot \mathbf{n}$$

### 4.2 Physical Collision Impulse Response
When particles approach ($v_n < 0$), an instantaneous contact impulse acts along the normal:
$$J_n = -(1 + e) \left(\frac{m_i m_j}{m_i + m_j}\right) v_n$$

Under regularized Coulomb friction with coefficient $\mu \ge 0$:
$$\mathbf{v}_t = \mathbf{v}_{\text{rel}} - v_n \mathbf{n}, \quad \mathbf{t} = \begin{cases} \frac{\mathbf{v}_t}{\|\mathbf{v}_t\|} & \text{if } \|\mathbf{v}_t\| > \epsilon \\ (0, 0)^T & \text{if } \|\mathbf{v}_t\| \le \epsilon \end{cases}$$
$$J_t = -\min\left(\mu |J_n|, \left(\frac{m_i m_j}{m_i + m_j}\right) \|\mathbf{v}_t\|\right)$$

Velocity updates:
$$\mathbf{v}_i \leftarrow \mathbf{v}_i + \frac{J_n}{m_i}\mathbf{n} + \frac{J_t}{m_i}\mathbf{t}$$
$$\mathbf{v}_j \leftarrow \mathbf{v}_j - \frac{J_n}{m_j}\mathbf{n} - \frac{J_t}{m_j}\mathbf{t}$$

Total linear momentum is analytically conserved:
$$\Delta (m_i \mathbf{v}_i + m_j \mathbf{v}_j) = \mathbf{0}$$

### 4.3 Positional Overlap Correction (Stabilization)
To eliminate numerical interpenetration from discrete timesteps, positions are projected apart:
$$\Delta \mathbf{x}_i = +\alpha \, \delta \left(\frac{m_j}{m_i + m_j}\right) \mathbf{n}$$
$$\Delta \mathbf{x}_j = -\alpha \, \delta \left(\frac{m_i}{m_i + m_j}\right) \mathbf{n}$$
where $\alpha \in [0.2, 0.8]$ is the stabilization relaxation factor.
**Rule:** Positional displacements are applied to positions ONLY. They are **never** divided by $\Delta t$ to modify velocity.

---

## 5. Boundary Interactions

Container walls (circular or rectangular) are treated as infinite-mass obstacles ($m_{\text{wall}} \to \infty$):
1. **Normal Velocity Reflection:**
   For inward normal $\mathbf{n}_b$, if $v_n = \mathbf{v}_i \cdot \mathbf{n}_b < 0$:
   $$\mathbf{v}_i \leftarrow \mu_b \mathbf{v}_t - e_b v_n \mathbf{n}_b$$
   where $e_b \in [0, 1]$ is boundary restitution and $\mu_b \in [0, 1]$ is boundary friction.
2. **Position Clamping:**
   Particle coordinates are clamped to the valid interior domain to prevent wall tunneling.

---

## 6. Conservation Laws & Numerical Invariants

A physically valid simulation must satisfy the following invariants under appropriate conditions:

### 1. Conservation of Linear Momentum
In the absence of external forces and boundary collisions:
$$\mathbf{P}(t) = \sum_{i=1}^N m_i \mathbf{v}_i(t) = \mathbf{P}(0) \quad \forall t$$

### 2. Kinetic Energy Invariance in Conservative Regimes
For perfectly elastic collisions ($e = 1.0, e_b = 1.0, \mu = 0$) in a conservative field:
$$E_{\text{total}} = E_{\text{kin}} + E_{\text{pot}} = \text{const}$$
*Note on Inelastic Regimes:* In dissipative collisions ($e < 1.0$), kinetic energy is **not** conserved; it must dissipate strictly according to the coefficient of restitution $e$.

---

## 7. Substepping & Stability Criteria

To satisfy the Contact Courant-Friedrichs-Lewy (CFL) condition:
$$\Delta t_{\text{sub}} \le \frac{C_{\text{CFL}} \, r_{\text{min}}}{v_{\text{max}}}$$
where $C_{\text{CFL}} \in [0.2, 0.5]$ is the safety factor.
For target frame time $\Delta t_{\text{frame}} = 1/60\,\text{s}$, a fixed substep count $S \in [8, 16]$ is executed per visual frame across all backends.
