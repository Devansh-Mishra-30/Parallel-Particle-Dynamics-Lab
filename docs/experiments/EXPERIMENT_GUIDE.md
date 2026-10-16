# Parallel Particle Dynamics Lab: Scientific Experiment Guide
## Connecting Physical Theory, Numerical Modeling, and Quantitative Verification

**Platform:** Parallel Particle Dynamics Lab (Simulator V2)
**Target Audience:** Mechanical Engineering Students, Computational Scientists, and Dynamics Instructors
**Milestone:** Phase 1A.5 — Scientific Experiment Foundation
**Reference Codebase:** `sim_core/` (Sequential C++17 Reference Solver)

---

## 1. Pedagogical Mission and Philosophy

In classical engineering curricula, students frequently encounter physics as purely analytical equations (e.g., $m_1 v_1 + m_2 v_2 = m_1 v_1' + m_2 v_2'$) or as pre-compiled "black-box" commercial software (e.g., ANSYS, Abaqus). This division conceals the crucial numerical transition layer where continuous differential equations are transformed into discrete algorithms executed on discrete computing hardware.

The **Parallel Particle Dynamics Lab** bridges this gap:
$$\text{PHYSICAL THEORY} \quad\longrightarrow\quad \text{NUMERICAL MODEL} \quad\longrightarrow\quad \text{SIMULATION RUNTIME} \quad\longrightarrow\quad \text{QUANTITATIVE VERIFICATION}$$

Students do not merely observe animations; they inspect conservation laws, calculate truncation errors, observe symplectic energy stability, quantify non-smooth collision impact dynamics, and derive empirical orders of convergence from structured numerical data.

---

## 2. Experiment 1: Two-Particle Impact Dynamics (`EXP_01_TWO_PARTICLE_COLLISION`)

### 2.1 Physical Theory and Governing Equations
Consider two spherical/circular particles of radii $r_1, r_2$, densities $\rho_1 = \rho_2 = \rho$, and masses $m_i = \pi \rho r_i^2$ moving along a central line of collision with initial velocities $\mathbf{v}_1, \mathbf{v}_2$.

In the absence of external forces, the total linear momentum is strictly conserved across the impact:
$$\mathbf{P} = m_1 \mathbf{v}_1 + m_2 \mathbf{v}_2 = \text{constant}$$

The normal impact impulse $\mathbf{J}_n$ exchanged between the particles is governed by Poisson's / Newton's hypothesis of restitution:
$$v_{\text{rel}}^- = (\mathbf{v}_1 - \mathbf{v}_2) \cdot \mathbf{n}_{12}$$
$$v_{\text{rel}}^+ = -e\, v_{\text{rel}}^-$$
where $\mathbf{n}_{12} = \frac{\mathbf{x}_2 - \mathbf{x}_1}{\|\mathbf{x}_2 - \mathbf{x}_1\|}$ is the contact unit normal pointing from particle 1 to particle 2, and $e \in [0, 1]$ is the kinematic coefficient of restitution.

The scalar impulse magnitude $j_n$ acting along $\mathbf{n}_{12}$ is:
$$j_n = \frac{-(1 + e)\, v_{\text{rel}}^-}{\frac{1}{m_1} + \frac{1}{m_2}} = -(1 + e)\,\mu\, v_{\text{rel}}^-$$
where $\mu = \frac{m_1 m_2}{m_1 + m_2}$ is the reduced mass of the two-particle system.

The post-collision velocities are given analytically by:
$$\mathbf{v}_1' = \mathbf{v}_1 - \frac{j_n}{m_1} \mathbf{n}_{12} = \left(\frac{m_1 - e\,m_2}{m_1 + m_2}\right)\mathbf{v}_1 + \left(\frac{(1 + e)m_2}{m_1 + m_2}\right)\mathbf{v}_2$$
$$\mathbf{v}_2' = \mathbf{v}_2 + \frac{j_n}{m_2} \mathbf{n}_{12} = \left(\frac{(1 + e)m_1}{m_1 + m_2}\right)\mathbf{v}_1 + \left(\frac{m_2 - e\,m_1}{m_1 + m_2}\right)\mathbf{v}_2$$

### 2.2 Kinetic Energy Dissipation
The total kinetic energy before impact is:
$$E_{k,0} = \frac{1}{2}m_1 \|\mathbf{v}_1\|^2 + \frac{1}{2}m_2 \|\mathbf{v}_2\|^2$$

The analytical change in kinetic energy $\Delta E_k$ during the collision is:
$$\Delta E_k = E_{k,\text{post}} - E_{k,0} = -\frac{1}{2}\mu (1 - e^2)\,(v_{\text{rel}}^-)^2$$
- When $e = 1.0$ (perfectly elastic), $\Delta E_k = 0$ (exact kinetic energy conservation).
- When $e < 1.0$ (inelastic), $\Delta E_k < 0$ (exact physical dissipation due to internal mechanical deformation/heat).

### 2.3 Numerical Implementation in `sim_core`
The numerical contact solver operates discretely:
1. **Contact Detection:** A contact pair is detected when penetration depth $\delta = (r_1 + r_2) - \|\mathbf{x}_2 - \mathbf{x}_1\| > 0$.
2. **Velocity Update:** When approaching ($v_{\text{rel}}^- < 0$), normal impulse $\mathbf{J}_n = j_n \mathbf{n}_{12}$ updates particle velocities:
   $$\mathbf{v}_1 \leftarrow \mathbf{v}_1 - \frac{j_n}{m_1}\mathbf{n}_{12}, \quad \mathbf{v}_2 \leftarrow \mathbf{v}_2 + \frac{j_n}{m_2}\mathbf{n}_{12}$$
3. **Penetration Stabilization:** Mass-weighted position displacement resolves geometric overlap without injecting spurious kinetic energy:
   $$\Delta \mathbf{x}_1 = -\beta \left(\frac{m_2}{m_1 + m_2}\right) \delta \mathbf{n}_{12}, \quad \Delta \mathbf{x}_2 = +\beta \left(\frac{m_1}{m_1 + m_2}\right) \delta \mathbf{n}_{12}$$
   where $\beta \in [0.2, 0.4]$ is the Baumgarte-style stabilization factor.

### 2.4 Interpreting Experimental Plots
- **Momentum Plot (`plot_momentum.png`):**
  - Top subplot: Total $P_x(t)$ remains perfectly flat for equal-mass elastic ($0\,\text{kg}\cdot\text{m/s}$), inelastic ($0\,\text{kg}\cdot\text{m/s}$), and unequal-mass ($2.513\,\text{kg}\cdot\text{m/s}$) collisions.
  - Bottom subplot: Numerical momentum drift $|P_x(t) - P_x(0)| \le 10^{-6}\,\text{kg}\cdot\text{m/s}$ (machine precision roundoff). Total linear momentum is an exact algebraic invariant of the impulse formulation.
- **Collision Energy Plot (`plot_collision_energy.png`):**
  - For $e=1.0$: Kinetic energy line is strictly horizontal before and after collision ($7.854\,\text{J}$).
  - For $e=0.5$: Sharp step drop occurs at $t \approx 0.15\,\text{s}$ from $7.854\,\text{J}$ to $1.963\,\text{J}$, exactly matching the theoretical $\Delta E_k = -5.89049\,\text{J}$.

---

## 3. Experiment 2: Ballistic Gravity and Energy Drift (`EXP_02_GRAVITY_AND_FREE_MOTION`)

### 3.1 Physical Theory and Continuous Solution
A particle in a uniform gravitational field $\mathbf{g} = (0, -g_y)^T$ obeys Newton's Second Law:
$$\ddot{\mathbf{x}}(t) = \mathbf{g}$$

Given initial conditions $\mathbf{x}(0) = \mathbf{x}_0 = (x_0, y_0)^T$ and $\dot{\mathbf{x}}(0) = \mathbf{v}_0 = (v_{x0}, v_{y0})^T$, the exact continuous analytical trajectory is:
$$\mathbf{x}(t) = \mathbf{x}_0 + \mathbf{v}_0 t + \frac{1}{2}\mathbf{g} t^2$$
$$\mathbf{v}(t) = \mathbf{v}_0 + \mathbf{g} t$$

The total continuous mechanical energy is:
$$E_{\text{total}}(t) = E_k(t) + E_p(t) = \frac{1}{2}m\|\mathbf{v}(t)\|^2 - m(\mathbf{g} \cdot \mathbf{x}(t)) = \text{constant}$$

### 3.2 Discrete Numerical Integrator: Semi-Implicit Euler
The simulator implements the symplectic Euler-Cromer (Semi-implicit Euler) method:
$$\mathbf{v}_{n+1} = \mathbf{v}_n + \mathbf{g} \Delta t$$
$$\mathbf{x}_{n+1} = \mathbf{x}_n + \mathbf{v}_{n+1} \Delta t = \mathbf{x}_n + \mathbf{v}_n \Delta t + \mathbf{g}(\Delta t)^2$$

Notice the subtle discrete shift:
$$\mathbf{x}_N = \mathbf{x}_0 + \mathbf{v}_0 t_N + \frac{1}{2}\mathbf{g} t_N^2 + \frac{1}{2}\mathbf{g} t_N \Delta t$$
Because $\mathbf{v}_{n+1}$ is evaluated *before* updating $\mathbf{x}_{n+1}$, the discrete position differs from the continuous analytical parabola by an $O(\Delta t)$ truncation term:
$$\mathbf{x}_{\text{sim}}(t) - \mathbf{x}_{\text{exact}}(t) = \frac{1}{2}\mathbf{g} t \Delta t$$
For vertical free fall with $g_y = -9.80665\,\text{m/s}^2, t = 1.0\,\text{s}, \Delta t = 0.016667 / 8 = 0.0020833\,\text{s}$:
$$\text{Error}_y = \frac{1}{2}(9.80665)(1.0)(0.0020833) \approx 0.010215\,\text{m} = 10.2\,\text{mm}$$
The simulation output (`gravity_trajectory.csv`) records $\text{Error} = 0.0102239\,\text{m}$, matching analytical truncation error to $0.08\%$.

### 3.3 Interpreting Experimental Plots
- **Trajectory Plot (`plot_trajectory.png`):**
  - Top subplot: Parabolic $y(t)$ drops from $2.0\,\text{m}$ to $-2.9\,\text{m}$; horizontal $x(t)$ advances linearly with constant velocity $v_x = 1.0\,\text{m/s}$.
  - Bottom subplot: Position error $\|\mathbf{x}_{\text{sim}} - \mathbf{x}_{\text{exact}}\|$ grows linearly with time $t$ in exact accordance with $\frac{1}{2}g t \Delta t$.
- **Gravity Energy Plot (`plot_gravity_energy.png`):**
  - Kinetic energy $E_k(t)$ increases as potential energy $E_p(t)$ decreases.
  - The total mechanical energy $E_{\text{tot}} = E_k + E_p$ remains nearly constant.
  - Inset plot: Symplectic energy drift $\Delta E / E_0$ is tiny ($< 0.5\%$ over $1.0\,\text{s}$) and does not blow up exponentially, unlike Explicit Forward Euler.

---

## 4. Experiment 3: Timestep Refinement & Convergence (`EXP_03_TIMESTEP_CONVERGENCE`)

### 4.1 Theoretical Convergence Formulation
Let $x_{\Delta t}(t)$ denote the numerical solution at step size $\Delta t$, and $x_{\text{exact}}(t)$ the analytical solution. The global error norm at final time $T$ is defined as:
$$L_\infty = \max_{1 \le n \le N} \|\mathbf{x}_n - \mathbf{x}_{\text{exact}}(t_n)\|$$
$$L_2 = \sqrt{\frac{1}{N} \sum_{n=1}^N \|\mathbf{x}_n - \mathbf{x}_{\text{exact}}(t_n)\|^2}$$

For an integrator of order $p$, the error scales asymptotically as:
$$L_\infty(\Delta t) \approx C\,(\Delta t)^p$$

When refining the timestep by a factor of 2 ($\Delta t_2 = \Delta t_1 / 2$), the error reduction ratio is:
$$R = \frac{L_\infty(\Delta t_1)}{L_\infty(\Delta t_2)} \approx 2^p$$

The empirical order of convergence (EOC) is calculated from two consecutive simulation runs:
$$p = \frac{\ln(L_\infty(\Delta t_1) / L_\infty(\Delta t_2))}{\ln(\Delta t_1 / \Delta t_2)}$$

### 4.2 Measured Results across Refinement Levels
The convergence experiment sweeps $\Delta t \in \{0.04, 0.02, 0.01, 0.005, 0.0025\}\,\text{s}$:

| $\Delta t$ [s] | Steps $N$ | $L_2$ Error [m] | $L_\infty$ Error [m] | Reduction Ratio $R$ | Empirical Order $p$ | Compute Time [$\mu$s] |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|
| 0.0400 | 5 | $2.602 \times 10^{-2}$ | $3.923 \times 10^{-2}$ | 1.0000 | — | 0.18 |
| 0.0200 | 10 | $1.217 \times 10^{-2}$ | $1.961 \times 10^{-2}$ | 1.9999 | **0.999996** | 0.11 |
| 0.0100 | 20 | $5.874 \times 10^{-3}$ | $9.807 \times 10^{-3}$ | 2.0000 | **1.000026** | 0.23 |
| 0.0050 | 40 | $2.884 \times 10^{-3}$ | $4.903 \times 10^{-3}$ | 1.9999 | **0.999982** | 0.30 |
| 0.0025 | 80 | $1.429 \times 10^{-3}$ | $2.452 \times 10^{-3}$ | 2.0001 | **1.000070** | 0.56 |

### 4.3 Interpreting the Convergence Plot (`plot_convergence.png`)
- Left subplot (Log-Log Scale): Both $L_\infty$ and $L_2$ error lines lie exactly parallel to the theoretical slope $1.0$ reference line. Every halving of $\Delta t$ cuts the error by exactly $50\%$.
- Right subplot: Wall-clock execution time scales inversely with $\Delta t$ (linearly with step count $N$).

---

## 5. Critical Pedagogical Distinctions & Pitfalls

### 5.1 Verification vs. Validation
Engineering students must learn to distinguish these two scientific terms:
1. **Verification ("Solving the equations right"):** Ensuring the computer code accurately implements the chosen mathematical model. Tested by comparing numerical results to closed-form analytical solutions, checking invariant drift (momentum, energy), and calculating empirical convergence orders.
2. **Validation ("Solving the right equations"):** Ensuring the mathematical model accurately represents real physical phenomena. Requires physical laboratory experiments (e.g., high-speed cameras, load cells, air tables).

> **Important Rule:** Comparing a simulation against an analytical formula is **numerical verification**, NOT physical validation.

### 5.2 Pitfalls in Discrete Collision Detection (DCD)
While smooth ballistic motion demonstrates clean $O(\Delta t)$ convergence, collision-dominated dynamics do **not** exhibit smooth asymptotic convergence:
- **Impact Time Jitter:** In fixed-step discrete collision detection, an impact is detected only at discrete steps $t_n, t_{n+1}$. The collision occurs between steps ($t_{\text{impact}} \in (t_n, t_{n+1})$), causing an uncontrolled temporal discretization error of $O(\Delta t)$ in the collision response.
- **High-Speed Tunneling:** If particle speed $v$ is high relative to particle radius $r$ ($v \Delta t > 2r$), particles can pass entirely through one another in a single step without collision detection triggering. Substepping is required to satisfy the Courant-Friedrichs-Lewy (CFL) condition: $\text{CFL} = \frac{v_{\max}\Delta t_{\text{sub}}}{r_{\min}} < 1.0$.

### 5.3 Why Explicit Forward Euler Fails in Contact Dynamics
In naive Forward Euler:
$$\mathbf{x}_{n+1} = \mathbf{x}_n + \mathbf{v}_n \Delta t$$
$$\mathbf{v}_{n+1} = \mathbf{v}_n + \mathbf{a}_n \Delta t$$
The integration phase space area is not preserved: $\det\left(\frac{\partial(\mathbf{x}_{n+1}, \mathbf{v}_{n+1})}{\partial(\mathbf{x}_n, \mathbf{v}_n)}\right) = 1 + \omega^2 (\Delta t)^2 > 1$. Every cycle or contact artificially amplifies mechanical energy, causing simulated particles to "explode." Semi-implicit Euler preserves symplectic phase-space volume: $\det = 1.0$.

### 5.4 Why Contact-Count Normalization ($1/K$) Destroys Contact Physics
In early naive implementations, developers divided penetration displacements by the number of contacts $K_i$:
$$\Delta \mathbf{x}_i = \frac{1}{K_i} \sum_j \delta_{ij}\mathbf{n}_{ij}$$
This heuristic:
1. Destroys contact stiffness in dense piles (bottom particles sink through floors).
2. Violates Newton's Third Law when $K_i \ne K_j$ ($\Delta \mathbf{x}_i \ne -\Delta \mathbf{x}_j$).
3. Creates artificial damping that dissipates momentum.

In `sim_core`, sequential pairwise Gauss-Seidel resolution preserves physical contact stiffness and exact action-reaction balance.

---

## 6. How Students Run Experiments and Replicate Results

### 6.1 Building and Executing the Framework
```bash
# 1. Build the experiment suite and regression tests
cmake -B build
cmake --build build

# 2. Run the regression test suites (100% pass required)
./build/test_physics
./build/test_experiments

# 3. Execute all experiments and export structured CSV datasets
./build/run_experiments --out output/csv

# 4. Generate publication-grade verification plots
python3 scripts/plot_experiments.py --csv-dir output/csv --plot-dir output/plots --dpi 300
```

### 6.2 Output File Structure
- `output/csv/collision_elastic.csv`: Frame-by-frame kinematics, total momentum, kinetic energy for $e=1.0$.
- `output/csv/collision_inelastic.csv`: Kinematics and dissipative $\Delta E_k$ for $e=0.5$.
- `output/csv/collision_unequal.csv`: Asymmetric mass kinematics ($m_2 = 4m_1$).
- `output/csv/gravity_trajectory.csv`: Parabolic flight, analytical comparison, symplectic energy components.
- `output/csv/convergence_study.csv`: Error norms, reduction ratios, and empirical orders across timestep sweep.
- `output/plots/*.png`: Publication-ready figures matching this guide.
