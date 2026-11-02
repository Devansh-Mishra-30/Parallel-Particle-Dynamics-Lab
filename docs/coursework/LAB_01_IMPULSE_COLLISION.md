# Engineering Coursework: Laboratory Module 1
## Two-Particle Collision Dynamics, Restitution, and Kinetic Energy Partition

**Course:** Computational Dynamics & Mechanical Systems Simulation
**Platform:** Parallel Particle Dynamics Lab (`sim_lab` & `run_experiments`)
**Module Target:** Kinematics, Poisson Impulse Formulation, Energy Dissipation

---

### 1. Educational Objectives

By completing this laboratory assignment, students will:
1. **Derive and verify** the algebraic equations governing central collision impulses between two disks of arbitrary masses ($m_1, m_2$) and kinematic restitution coefficient $e \in [0, 1]$.
2. **Quantify the kinetic energy partition** and prove that the theoretical kinetic energy dissipation $\Delta E_k = -\frac{1}{2}\mu(1 - e^2)v_{\text{rel}}^2$ matches numerical simulation measurements.
3. **Inspect momentum conservation** as an invariant of the impulse-momentum theorem ($|P_x(t) - P_x(0)| < 10^{-6}\text{ kg}\cdot\text{m/s}$).
4. **Compare computational solvers** (Sequential CPU vs OpenMP Multicore vs CUDA GPU) and demonstrate numerical invariance across computing architectures.

---

### 2. Theoretical Background & Governing Equations

#### 2.1 Conservation of Linear Momentum
For a closed two-particle system with masses $m_1 = \pi \rho r_1^2$ and $m_2 = \pi \rho r_2^2$ moving with pre-impact velocities $\mathbf{v}_1, \mathbf{v}_2$:
$$\mathbf{P} = m_1 \mathbf{v}_1 + m_2 \mathbf{v}_2 = \text{constant}$$

#### 2.2 Normal Impact Impulse
The relative normal approach velocity along the contact normal $\mathbf{n}_{12} = \frac{\mathbf{x}_2 - \mathbf{x}_1}{\|\mathbf{x}_2 - \mathbf{x}_1\|}$ is:
$$v_{\text{rel}}^- = (\mathbf{v}_1 - \mathbf{v}_2) \cdot \mathbf{n}_{12}$$

Under Poisson's restitution hypothesis, the normal impulse magnitude $j_n$ exchanged at contact is:
$$j_n = -(1 + e) \left(\frac{m_1 m_2}{m_1 + m_2}\right) v_{\text{rel}}^- = -(1 + e)\,\mu\, v_{\text{rel}}^-$$
where $\mu = \frac{m_1 m_2}{m_1 + m_2}$ is the reduced mass.

The post-impact velocities are:
$$\mathbf{v}_1^+ = \mathbf{v}_1^- - \frac{j_n}{m_1}\mathbf{n}_{12}, \qquad \mathbf{v}_2^+ = \mathbf{v}_2^- + \frac{j_n}{m_2}\mathbf{n}_{12}$$

#### 2.3 Kinetic Energy Dissipation
The continuous kinetic energy is $E_k = \frac{1}{2}m_1 \|\mathbf{v}_1\|^2 + \frac{1}{2}m_2 \|\mathbf{v}_2\|^2$. The exact loss of kinetic energy is:
$$\Delta E_k = E_k^+ - E_k^- = -\frac{1}{2}\mu\,(1 - e^2)\,(v_{\text{rel}}^-)^2$$
- For perfectly elastic collisions ($e = 1.0$): $\Delta E_k = 0$ (energy is strictly conserved).
- For perfectly plastic collisions ($e = 0.0$): $\Delta E_k = -\frac{1}{2}\mu (v_{\text{rel}}^-)^2$ (maximum kinetic energy dissipation).

---

### 3. Laboratory Procedure

#### Exercise 1: GUI Interactive Verification
1. Launch the interactive workbench:
   ```bash
   ./build/sim_lab
   ```
2. Select **Scenario: Two-Particle Impact (EXP_01)** in the left panel.
3. Observe the initial setup:
   - $r_1 = 0.05\text{ m}, r_2 = 0.05\text{ m} \implies m_1 = m_2 = 7.854\text{ kg}$ ($\rho = 1000\text{ kg/m}^2$).
   - $v_{1x} = +1.00\text{ m/s}, v_{2x} = -1.00\text{ m/s} \implies v_{\text{rel}}^- = 2.00\text{ m/s}$.
4. In the Controls panel, adjust the restitution slider $e$ to the following test values:
   - **Case A:** $e = 1.000$ (Elastic)
   - **Case B:** $e = 0.750$
   - **Case C:** $e = 0.528$ (Validation Benchmark)
   - **Case D:** $e = 0.250$
   - **Case E:** $e = 0.000$ (Plastic)
5. For each test value, click **Reset / Reload Preset**, press **Play/Pause**, and observe the collision.
6. Record the values reported in the **Analytical Comparison & Diagnostics** pane.

#### Exercise 2: Data Collection Table
Fill in the table below using measurements from the interactive GUI:

| Case | Restitution $e$ | Reduced Mass $\mu$ [kg] | Theoretical $\Delta E_k$ [J] | Measured Post $E_k$ [J] | Measured $\Delta E_k$ [J] | Relative Error [%] | Momentum Drift [kg$\cdot$m/s] |
|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| A | 1.000 | 3.9270 | 0.0000 | 7.8540 | 0.0000 | 0.00% | $< 10^{-7}$ |
| B | 0.750 | 3.9270 | -3.4361 | 4.4179 | -3.4361 | | |
| C | 0.528 | 3.9270 | -5.6644 | 2.1896 | -5.6644 | | |
| D | 0.250 | 3.9270 | -7.3631 | 0.4909 | -7.3631 | | |
| E | 0.000 | 3.9270 | -7.8540 | 0.0000 | -7.8540 | | |

#### Exercise 3: Backend Invariance Check
1. Under Case C ($e = 0.528$), switch between the three solver backends:
   - `[CPU Sequential]`
   - `[OpenMP Multicore]`
   - `[CUDA GPU (RTX 4060)]`
2. Verify that all three computing backends produce identical post-collision velocities ($v_{1x}^+ = -0.5280\text{ m/s}, v_{2x}^+ = +0.5280\text{ m/s}$) to within $10^{-5}\text{ m/s}$.

---

### 4. Analysis Questions

1. **Energy Dissipation Mechanism:** In Case C ($e=0.528$), where does the missing $5.6644\text{ J}$ of mechanical energy go in a physical mechanical system? Does the simulator include thermal or acoustic degrees of freedom?
2. **Action-Reaction Invariance:** Explain why total linear momentum $\mathbf{P}(t)$ remains conserved to machine precision roundoff ($\approx 10^{-16}$) while kinetic energy drops discontinuously at contact.
3. **Unequal Masses:** Set $r_2 = 0.10\text{ m}$ ($m_2 = 4 m_1 = 31.416\text{ kg}$). Derive the theoretical post-impact velocity $v_1^+$ for $e=1.0$ when $v_{1x}^- = 1.0\text{ m/s}, v_{2x}^- = 0.0\text{ m/s}$. Verify using the simulator.
