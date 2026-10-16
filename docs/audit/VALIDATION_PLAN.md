# Physics & Numerical Methods Validation Plan (Revised)

**Target System:** 2D Particle Simulation (Validation Test Suite)
**Workspace:** `/home/devansh/simulator-workspaces/cuda-particle-simulation-v2`
**Branch:** `feature/simulator-v2`
**Document Status:** Complete Test & Verification Specification (Phase 0 Revised)

---

## 1. Verification Framework Architecture

To ensure scientific validity and eliminate backend divergence, Simulator V2 incorporates an automated, headless physics validation suite. Every backend (CPU Scalar, CPU OpenMP, and GPU CUDA) executes identical test configurations and is evaluated against closed-form analytical solutions and conservation laws.

### Test Architecture
- **Harness:** Standalone C++ test binary (`test_physics`) using Catch2.
- **Execution Mode:** 100% headless (zero SFML window/renderer dependencies).
- **Execution Matrix:** Each test case executes against all three backends:
  $$\text{Target} \in \{\text{CPU\_Scalar}, \text{CPU\_OpenMP}(1, 2, 4, 8\,\text{threads}), \text{CUDA}\}$$
- **Pass/Fail Criteria:**
  - Absolute Position Tolerance: $\|\mathbf{x}_{\text{sim}} - \mathbf{x}_{\text{exact}}\| \le \epsilon_{\text{pos}}$
  - Velocity Tolerance: $\|\mathbf{v}_{\text{sim}} - \mathbf{v}_{\text{exact}}\| \le \epsilon_{\text{vel}}$
  - Invariant Conservation: $\frac{|I(t) - I(0)|}{|I(0)| + 10^{-6}} \le \tau_{\text{drift}}$

---

## 2. Analytical Benchmark Test Cases

```
+---------------------------------------------------------------------------------------------------------+
| VALIDATION TEST SUITE MATRIX                                                                            |
+-----+----------------------------------+-----------------------+----------------------------------------+
| Case| Test Description                 | Target Metric         | Analytical Truth                       |
+-----+----------------------------------+-----------------------+----------------------------------------+
| TC01| Free Ballistic Motion            | $\mathbf{x}(t)$       | $\mathbf{x}_0 + \mathbf{v}_0 t$        |
| TC02| Constant Gravity Free Fall       | Trajectory & Velocity | $y_0 + v_0 t + \frac{1}{2}gt^2$        |
| TC03| 1D Elastic Collision (Equal Mass)| $v_1', v_2'$ Swap     | $v_1' = v_2, v_2' = v_1$               |
| TC04| 1D Unequal Mass Elastic Collision| Post-Collision Veloc. | 1D Kinematic Equations                 |
| TC05| Inelastic Collision Restitution  | Restitution Ratio     | $\|v_{\text{rel}}'/v_{\text{rel}}\| = e$|
| TC06| Specular Boundary Reflection     | Reflection Angles     | $\theta_r = \theta_i, v_n' = -e_b v_n$ |
| TC07| Planar Wall Bounce               | Reversal & Energy     | Inverted $v_\perp$                     |
| TC08| Isolated N-Body Momentum Drift   | $\mathbf{P}(t)$       | $d\mathbf{P}/dt = 0$                   |
| TC09| Hamiltonian Energy Conservation  | $\Delta E(t) / E_0$   | Bounded Symplectic Oscillation ($e=1$) |
| TC10| Coincident Particle Singularity  | NaN Detection         | Zero NaNs, clean separation            |
| TC11| High-Velocity Tunneling Limit    | Penetration Depth     | Zero tunneling beyond boundary         |
| TC12| Timestep Convergence Rate        | Empirical Order (EOC) | $O(\Delta t)$ across collisions        |
| TC13| Cross-Backend State Parity       | State Vector Diff     | $\|\mathbf{S}_{\text{CPU}} - \mathbf{S}_{\text{GPU}}\| < 10^{-4}$|
+-----+----------------------------------+-----------------------+----------------------------------------+
```

---

### Detailed Test Specifications

#### Test Case TC01: Free Ballistic Motion (Zero Gravity, Zero Collision)
- **Configuration:** $N = 1$, $\mathbf{g} = (0, 0)$, domain size $1000 \times 1000$.
- **Initial State:** $\mathbf{x}_0 = (100.0, 100.0)$, $\mathbf{v}_0 = (250.0, -150.0)$, $t \in [0, 2.0]\,\text{s}$.
- **Analytical Solution:**
  $$\mathbf{x}_{\text{exact}}(t) = \mathbf{x}_0 + \mathbf{v}_0 t, \quad \mathbf{v}_{\text{exact}}(t) = \mathbf{v}_0$$
- **Verification Criterion:**
  $$\max_{t} \|\mathbf{x}_{\text{sim}}(t) - \mathbf{x}_{\text{exact}}(t)\| < 10^{-5}\,\text{units}, \quad \max_{t} \|\mathbf{v}_{\text{sim}}(t) - \mathbf{v}_{\text{exact}}(t)\| < 10^{-6}$$

#### Test Case TC02: Constant Acceleration Gravity Free Fall
- **Configuration:** $N = 1$, $\mathbf{g} = (0, -980.0)$, initial $\mathbf{x}_0 = (0, 500)$, $\mathbf{v}_0 = (50, 0)$.
- **Analytical Solution:**
  $$x(t) = x_0 + u_0 t, \quad y(t) = y_0 + v_0 t + \frac{1}{2} g t^2$$
  $$u(t) = u_0, \quad v(t) = v_0 + g t$$
- **Verification Criterion:**
  Trajectory must match exact analytical position within $10^{-4}\,\text{units}$.

#### Test Case TC03: 1D Equal-Mass Central Elastic Collision
- **Configuration:** $N = 2$, $r_1 = r_2 = 5.0$, $m_1 = m_2 = \pi \rho r^2$, $e = 1.0$, $\mathbf{g} = \mathbf{0}$.
- **Initial State:** $\mathbf{x}_1 = (-20.0, 0.0), \mathbf{v}_1 = (10.0, 0.0)$; $\mathbf{x}_2 = (20.0, 0.0), \mathbf{v}_2 = (-10.0, 0.0)$.
- **Analytical Solution:**
  Velocity swap: $\mathbf{v}_1' = (-10.0, 0.0)$, $\mathbf{v}_2' = (10.0, 0.0)$.
  Total momentum: $\mathbf{P} = m_1 \mathbf{v}_1 + m_2 \mathbf{v}_2 = \mathbf{0}$.
  Kinetic energy: $E_{\text{kin}} = \frac{1}{2}m_1 v_1^2 + \frac{1}{2}m_2 v_2^2 = \text{const}$.
- **Verification Criterion:**
  Post-collision velocities must match $(-10, 0)$ and $(10, 0)$ within $10^{-4}$. Kinetic energy preserved to $99.99\%$.

#### Test Case TC04: 1D Unequal-Mass Central Elastic Collision
- **Configuration:** $N = 2$, $r_1 = 2.0 \implies m_1 = 4\pi$; $r_2 = 6.0 \implies m_2 = 36\pi$. $e = 1.0$, $\mathbf{g} = \mathbf{0}$.
- **Initial State:** $\mathbf{v}_1 = (10.0, 0.0)$, $\mathbf{v}_2 = (0.0, 0.0)$.
- **Analytical Solution:**
  $$v_1' = \frac{m_1 - m_2}{m_1 + m_2} v_1 = \frac{4 - 36}{40} (10) = -8.0\,\text{units/s}$$
  $$v_2' = \frac{2 m_1}{m_1 + m_2} v_1 = \frac{8}{40}(10) = +2.0\,\text{units/s}$$
- **Verification Criterion:**
  $$|v_1' - (-8.0)| < 10^{-3}, \quad |v_2' - 2.0| < 10^{-3}$$
  *(Explicitly catches the legacy CUDA defect where particle masses were ignored).*

#### Test Case TC05: Inelastic Collision with Variable Restitution $e$
- **Configuration:** Parameterized sweep for $e \in \{0.0, 0.25, 0.5, 0.75, 1.0\}$, $\mathbf{g} = \mathbf{0}$.
- **Analytical Truth:**
  Relative normal separation velocity:
  $$v_{\text{rel}}' = -e \, v_{\text{rel}}$$
- **Conservation & Invariant Criteria:**
  - Total linear momentum $\mathbf{P} = m_1 \mathbf{v}_1 + m_2 \mathbf{v}_2$ must be conserved: $\Delta \mathbf{P} = 0$.
  - Measured separation ratio $\left|\frac{v_{1,x}' - v_{2,x}'}{v_{1,x} - v_{2,x}}\right|$ must equal $e \pm 0.01$.
  - **IMPORTANT:** Energy conservation is **not** required or expected for $e < 1.0$; kinetic energy must dissipate strictly by $\Delta E_k = -\frac{1}{2}\left(\frac{m_1 m_2}{m_1 + m_2}\right)(1 - e^2) v_{\text{rel}}^2$.

#### Test Case TC06: Circular Boundary Reflection
- **Configuration:** Circular wall radius $R = 100$, particle $r = 5$, wall restitution $e_b = 1.0$, frictionless $\mu_b = 1.0$.
- **Analytical Truth:** Specular reflection: Angle of incidence $\theta_i$ equals angle of reflection $\theta_r$. Energy conserved.
- **Verification Criterion:** $|\theta_r - \theta_i| < 0.1^\circ$, $|\|\mathbf{v}'\| - \|\mathbf{v}\|\| / \|\mathbf{v}\| < 10^{-4}$.

---

## 3. Multi-Particle Invariant Verification

#### Test Case TC08: Isolated N-Body Momentum Conservation
- **Configuration:** $N = 1,000$ particles in an elastic bounding box, $\mathbf{g} = \mathbf{0}$, $e = 1.0$.
- **Duration:** 10,000 integration steps.
- **Invariant Monitored:** Total linear momentum $\mathbf{P}(t) = \sum_{i=1}^N m_i \mathbf{v}_i(t)$.
- **Acceptance Threshold:**
  $$\frac{\|\mathbf{P}(t) - \mathbf{P}(0)\|}{\|\mathbf{P}(0)\| + 10^{-6}} \le 10^{-5} \quad \forall t \in [0, t_{\text{end}}]$$
  *(Catches OpenMP data races and asymmetric CUDA atomic updates).*

#### Test Case TC09: Symplectic Hamiltonian Energy Drift (Conservative Regime Only)
- **Configuration:** $N = 500$ particles in an elastic circular container, $e = 1.0, e_b = 1.0, \mu = 0$, $\mathbf{g} = (0, -980)$.
- **Duration:** 50,000 integration steps.
- **Acceptance Threshold:**
  No secular drift in total energy $E(t) = E_{\text{kin}} + E_{\text{pot}}$:
  $$\left|\frac{\langle E \rangle_{\text{final}} - E(0)}{E(0)}\right| \le 0.5\%$$
  Energy must oscillate within a bounded symplectic envelope.

---

## 4. Numerical Stability & Degeneracy Stress Tests

#### Test Case TC10: Exact Coincident Particle Spawn ($\mathbf{x}_1 = \mathbf{x}_2$)
- **Configuration:** Two particles initialized at the exact same coordinates: $\mathbf{x}_1 = \mathbf{x}_2 = (200.0, 200.0)$.
- **Acceptance Threshold:**
  1. Zero `NaN` or `Inf` generated in position or velocity buffers.
  2. Particles separate stably along the fallback perturbation normal.

#### Test Case TC11: High-Velocity Tunneling Stress Test
- **Configuration:** High-velocity particle ($v = 10 \times r / \Delta t_{\text{frame}}$) directed at a wall under substepping $S = 8$.
- **Acceptance Threshold:** Particle position strictly remains inside inner container margin ($d_b \le R - r$).

---

## 5. Convergence Analysis Protocol

### Empirical Order of Convergence (EOC)
1. Run a reference 2-body orbit problem across a sequence of diminishing timesteps:
   $$\Delta t_k = \frac{\Delta t_0}{2^k}, \quad k \in \{0, 1, 2, 3, 4\}$$
2. Compute global position error $E(\Delta t_k) = \|\mathbf{x}_{\Delta t_k}(T) - \mathbf{x}_{\text{exact}}(T)\|_{L_2}$.
3. Calculate empirical order:
   $$\text{EOC}_k = \log_2 \left( \frac{E(\Delta t_{k-1})}{E(\Delta t_k)} \right)$$
4. **Validation Target:** For Substepped Semi-Implicit Euler, $\text{EOC} \approx 1.0$ across non-smooth contact collisions.

---

## 6. Pre-Generated Deterministic Test Datasets

To ensure reproducible cross-backend verification, the validation suite includes canonical binary test fixtures:
- `data/test_fixtures/fixture_2body_elastic.bin`: 2 particles colliding head-on (equal and unequal masses).
- `data/test_fixtures/fixture_gas_100.bin`: 100 particles in free dispersion without gravity.
- `data/test_fixtures/fixture_settling_5000.bin`: 5,000 particles settling into a dense bed under gravity.

All backends (CPU, OpenMP, and CUDA) load these fixtures and verify identical kinematic trajectories to within $10^{-4}$ tolerance.
