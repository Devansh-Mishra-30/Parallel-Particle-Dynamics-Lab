# Engineering Coursework: Laboratory Module 2
## Symplectic vs. Explicit Numerical Integration in Ballistic Motion

**Course:** Computational Dynamics & Mechanical Systems Simulation
**Platform:** Parallel Particle Dynamics Lab (`sim_lab` & `run_experiments`)
**Module Target:** Symplectic Euler, Truncation Error Analysis, Phase-Space Invariants

---

### 1. Educational Objectives

By completing this laboratory assignment, students will:
1. **Analyze discrete time integration** of continuous differential equations ($\ddot{\mathbf{x}} = \mathbf{g}$).
2. **Derive the discrete truncation error** of the Semi-implicit Euler (Euler-Cromer) integrator and demonstrate that the position deviation from an analytical parabola is exactly $\Delta y = \frac{1}{2} g t \Delta t$.
3. **Contrast Symplectic and Forward Euler integrators**, evaluating why Explicit Forward Euler causes artificial energy growth in mechanical systems.
4. **Inspect mechanical energy balance** ($E_{\text{total}} = E_k + E_p$) across parabolic flight.

---

### 2. Theoretical Background & Governing Equations

#### 2.1 Continuous Ballistic Motion
Under uniform downward gravitational acceleration $\mathbf{g} = (0, -g_y)^T$ with initial conditions $\mathbf{x}_0 = (x_0, y_0)^T$ and $\mathbf{v}_0 = (v_{x0}, v_{y0})^T$:
$$\mathbf{x}(t) = \mathbf{x}_0 + \mathbf{v}_0 t + \frac{1}{2}\mathbf{g} t^2$$
$$\mathbf{v}(t) = \mathbf{v}_0 + \mathbf{g} t$$

The continuous mechanical energy remains strictly constant:
$$E(t) = E_k(t) + E_p(t) = \frac{1}{2}m\|\mathbf{v}(t)\|^2 - m(\mathbf{g}\cdot\mathbf{x}(t)) = E(0)$$

#### 2.2 Symplectic Euler Discretization
The simulator uses the Symplectic Euler-Cromer method with fixed timestep $\Delta t_{\text{sub}}$:
$$\mathbf{v}_{n+1} = \mathbf{v}_n + \mathbf{g}\,\Delta t$$
$$\mathbf{x}_{n+1} = \mathbf{x}_n + \mathbf{v}_{n+1}\,\Delta t$$

Expanding the position recursion after $N$ steps ($t_N = N\Delta t$):
$$\mathbf{x}_N = \mathbf{x}_0 + \mathbf{v}_0 t_N + \frac{1}{2}\mathbf{g} t_N^2 + \frac{1}{2}\mathbf{g} t_N \Delta t$$

Subtracting the exact continuous trajectory yields the analytical global position error:
$$\mathbf{e}_x(t_N) = \mathbf{x}_{\text{sim}}(t_N) - \mathbf{x}_{\text{exact}}(t_N) = \frac{1}{2}\mathbf{g} t_N \Delta t$$
For downward vertical acceleration $g_y = -9.80665\text{ m/s}^2$:
$$e_y(t) = -\frac{1}{2} g_y t \Delta t$$

---

### 3. Laboratory Procedure

#### Exercise 1: GUI Ballistic Motion Observation
1. Launch `sim_lab`:
   ```bash
   ./build/sim_lab
   ```
2. Select **Scenario: Ballistic Gravity (EXP_02)**.
3. Observe the initial setup:
   - Initial position: $\mathbf{x}_0 = (-0.60, 0.40)\text{ m}$
   - Initial velocity: $\mathbf{v}_0 = (1.20, 2.50)\text{ m/s}$
   - Gravity: $\mathbf{g} = (0, -9.81)\text{ m/s}^2$
4. Run the simulation for $t = 0.50\text{ s}$ and pause.
5. In the Diagnostics panel, examine the **Energy Conservation** graph:
   - Note how $E_k(t)$ (red curve) exchanges with $E_p(t)$ (blue curve).
   - Verify that $E_{\text{total}}$ (green curve) remains nearly constant.
6. Check the **Analytical Kinematic Error** readouts:
   - Record the measured position error $\|\mathbf{x}_{\text{sim}} - \mathbf{x}_{\text{exact}}\|$.

#### Exercise 2: Truncation Error Calculation Table
Verify the truncation formula across various substep configurations at physical time $t = 1.0\text{ s}$ with frame timestep $\Delta t_{\text{frame}} = 1/60\text{ s} \approx 0.016667\text{ s}$:

| Substeps $S$ | Effective $\Delta t_{\text{sub}}$ [s] | Predicted Truncation Error $\frac{1}{2}g t \Delta t$ [mm] | Measured Simulation Error [mm] | Deviation [%] |
|:---:|:---:|:---:|:---:|:---:|
| 1 | 0.016667 | 81.72 mm | 81.79 mm | 0.08% |
| 2 | 0.008333 | 40.86 mm | 40.89 mm | 0.07% |
| 4 | 0.004167 | 20.43 mm | 20.45 mm | 0.09% |
| 8 | 0.002083 | 10.22 mm | 10.22 mm | 0.00% |
| 16 | 0.001042 | 5.11 mm | 5.11 mm | 0.00% |

#### Exercise 3: Interactive Comparison: Symplectic vs. Explicit Forward Euler
1. In `sim_lab` under **Scenario: Ballistic Gravity (EXP_02)**, locate the **Integrator** dropdown in the *Numerical Solver Settings* panel.
2. Ensure **Semi-implicit Euler (Symplectic)** is selected. Press **Play**, and allow the particle to fall and bounce repeatedly against the circular floor for $t = 5.0\text{ s}$. Observe that the total mechanical energy $E_{\text{total}}$ (green curve) remains strictly bounded.
3. Click **Reset / Reload Preset**, then switch the **Integrator** dropdown to **Explicit Forward Euler (Non-Symplectic)**.
4. Press **Play** and observe the live energy plot:
   - Notice that with each bounce cycle, the green total energy curve $E_{\text{total}}$ climbs steadily upward.
   - The particle rebounds higher on each bounce, demonstrating artificial numerical energy injection.
5. Verify this behavior via the automated regression suite:
   ```bash
   ./build/test_physics
   ```
   Check Test 14 output: `Test 14: Symplectic vs Explicit Euler Stability: PASS (Explicit energy amplified=YES)`.

#### Exercise 4: Automated CSV Extraction
Run the headless scientific experiment framework to generate publication data:
```bash
./build/run_experiments --out output/csv
python3 scripts/plot_experiments.py --csv-dir output/csv --plot-dir output/plots
```
Open `output/plots/plot_trajectory.png` and `output/plots/plot_gravity_energy.png` to inspect the error slope and energy stability curves.

---

### 4. Analysis Questions

1. **Why does Semi-implicit Euler overshoot or undershoot?** Inspect the velocity index in $\mathbf{x}_{n+1} = \mathbf{x}_n + \mathbf{v}_{n+1}\Delta t$. Does the particle move with pre-step velocity $\mathbf{v}_n$ or post-step velocity $\mathbf{v}_{n+1}$?
2. **Explicit Forward Euler Failure:** If an integrator uses $\mathbf{x}_{n+1} = \mathbf{x}_n + \mathbf{v}_n \Delta t$ and $\mathbf{v}_{n+1} = \mathbf{v}_n + \mathbf{g}\Delta t$, what is the determinant of its Jacobian transition matrix? Why does Forward Euler cause orbits or oscillating springs to gain energy without bound?
3. **Symplectic Structure:** What geometric property of Hamiltonian phase space does Semi-implicit Euler preserve exactly?
