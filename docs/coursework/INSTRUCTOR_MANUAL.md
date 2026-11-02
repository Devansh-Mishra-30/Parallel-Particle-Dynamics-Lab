# Engineering Coursework: Instructor Solutions Manual & Grading Guide
## Parallel Particle Dynamics Lab — Educational Release v2.0

**Course:** Computational Dynamics & Mechanical Systems Simulation
**Platform:** Parallel Particle Dynamics Lab (`sim_lab`, `sim_core`, `sim_omp_v2`, `sim_cuda_v2`)
**Audience:** Course Instructors, Teaching Assistants, Lab Evaluators

---

### 1. Coursework Architecture Overview

The lab modules are structured around the progressive transition from continuous physics to high-performance parallel computation:
1. **Lab 1:** Continuous Mechanics, Inelastic Impulses, and Conservation Laws.
2. **Lab 2:** Discrete Numerical Integrators, Truncation Error, and Symplectic Phase Space.
3. **Lab 3:** Grid/Timestep Refinement, Error Norms, and Empirical Convergence ($p = 1.00$).
4. **Lab 4:** Discrete Collision Detection, High-Speed Tunneling, and Contact Crowding.
5. **Lab 5:** Multicore CPU & CUDA GPU Acceleration, Amdahl's Law, and Algorithmic Disentanglement.

---

### 2. Solutions & Key Discussion Points

#### Module 1: Two-Particle Impact Dynamics
- **Exercise 2 Table:** Theoretical values are derived from $\Delta E_k = -\frac{1}{2}\mu(1 - e^2)v_{\text{rel}}^2$ where $\mu = \frac{m_1 m_2}{m_1 + m_2} = 3.9270\text{ kg}$ and $v_{\text{rel}} = 2.0\text{ m/s}$:
  - $e = 1.000 \implies \Delta E_k = 0.0000\text{ J}$
  - $e = 0.750 \implies \Delta E_k = -0.5 \times 3.927 \times (1 - 0.5625) \times 4.0 = -3.4361\text{ J}$
  - $e = 0.528 \implies \Delta E_k = -0.5 \times 3.927 \times (1 - 0.278784) \times 4.0 = -5.6644\text{ J}$
  - $e = 0.250 \implies \Delta E_k = -0.5 \times 3.927 \times (1 - 0.0625) \times 4.0 = -7.3631\text{ J}$
  - $e = 0.000 \implies \Delta E_k = -0.5 \times 3.927 \times (1 - 0) \times 4.0 = -7.8540\text{ J}$
- **Analysis Question 1:** The lost kinetic energy represents plastic mechanical deformation, microscopic lattice vibrations, and thermal dissipation. The simulator does not model internal thermo-mechanical variables, so this energy is removed algebraically via the normal impulse.
- **Analysis Question 2:** Linear momentum is an exact algebraic invariant because normal contact forces obey Newton's Third Law ($\mathbf{F}_{ij} = -\mathbf{F}_{ji}$); impulses applied to particle 1 and particle 2 are strictly equal in magnitude and opposite in direction ($j_n \mathbf{n}_{12} - j_n \mathbf{n}_{12} = 0$).
- **Analysis Question 3:** For $m_2 = 4m_1$ and $e=1.0$:
  $$v_1^+ = \left(\frac{m_1 - m_2}{m_1 + m_2}\right) v_1^- = \left(\frac{1 - 4}{1 + 4}\right)(1.0) = -0.60\text{ m/s}$$
  $$v_2^+ = \left(\frac{2 m_1}{m_1 + m_2}\right) v_1^- = \left(\frac{2}{5}\right)(1.0) = +0.40\text{ m/s}$$
  Students should confirm this matches `sim_lab` and `tests/test_physics.cpp`.

#### Module 2: Integrator Stability & Truncation Error
- **Exercise 2 Table:** Truncation error of Semi-implicit Euler is $\Delta y = \frac{1}{2} g t \Delta t$.
  At $t = 1.0\text{ s}$, $g = 9.80665\text{ m/s}^2$:
  - $\Delta t = 0.016667\text{ s} \implies \Delta y = 0.5 \times 9.80665 \times 1.0 \times 0.016667 \approx 81.72\text{ mm}$.
  - Every halving of $\Delta t$ cuts the truncation error by exactly half.
- **Analysis Question 1:** Semi-implicit Euler updates velocity first: $\mathbf{v}_{n+1} = \mathbf{v}_n + \mathbf{g}\Delta t$, then uses $\mathbf{v}_{n+1}$ to step position. Therefore, over each step, position advances using the higher post-step downward speed rather than the average mid-step speed, producing an $O(\Delta t)$ systematic offset.
- **Analysis Question 2:** Forward Euler transition matrix has determinant $\det(J) = 1 + \omega^2 \Delta t^2 > 1$. Because the phase space volume expands by a factor greater than 1 at each cycle, any oscillatory or orbital mode experiences exponential energy growth.
- **Analysis Question 3:** Semi-implicit Euler is a *symplectic integrator*: it preserves the differential 2-form $d\mathbf{p} \wedge d\mathbf{q}$ in Hamiltonian phase space ($\det(J) = 1.000000$), guaranteeing that energy oscillations remain bounded on an invariant shadow Hamiltonian.

#### Module 3: Timestep Refinement & Convergence
- **Exercise 2 Table:** The empirical order of convergence:
  $$p = \frac{\ln(R)}{\ln(2)} \approx \frac{\ln(2.000)}{\ln(2)} = 1.000$$
  Confirms that Semi-implicit Euler is asymptotically 1st-order accurate.
- **Analysis Question 1:** Local truncation error is $O(\Delta t^2)$ per step. Integrating over $N = T/\Delta t$ steps accumulates $N \times O(\Delta t^2) = \frac{T}{\Delta t} O(\Delta t^2) = O(\Delta t)$, yielding global order $p=1$.
- **Analysis Question 2:** Velocity Verlet has local error $O(\Delta t^3)$ and global error $O(\Delta t^2)$ ($p=2$). Halving $\Delta t$ cuts error by $2^2 = 4\times$ ($R = 4.0$). The log-log plot slope would be $2.0$.
- **Analysis Question 3:** In discrete collision detection, the contact event occurs between steps ($t_{\text{impact}} \in (t_n, t_{n+1})$). The exact collision instant has a temporal uncertainty of $O(\Delta t)$, which introduces an irregular $O(\Delta t)$ perturbation into the post-collision state that prevents smooth monotonic asymptotic convergence across multiple collisions.

#### Module 4: Contact Mechanics & Tunneling
- **Analysis Question 1:** Continuous Collision Detection (CCD) performs continuous swept-volume or time-of-impact (TOI) root finding between $t_n$ and $t_{n+1}$, finding the exact time $t^* \in [t_n, t_{n+1}]$ when distance becomes zero. However, CCD requires iterative root finding (expensive polynomial or iterative solvers) and introduces difficult synchronization barriers across thousands of parallel GPU threads. DCD with substepping is lightweight, highly parallelizable, and perfectly suited for GPU SIMT execution as long as $\text{CFL} < 0.5$.
- **Analysis Question 2:** Normalizing displacements by contact count ($1/K_i$) treats contacts as soft springs with variable stiffness depending on how many particles touch. In tall granular columns, particles at the bottom have 4–6 contacts while particles above have 2–3. The bottom particles experience reduced upward restitution, allowing them to sag and sink through the container floor.
- **Analysis Question 3:** Direct Jacobi updates without multiple iterations cause overshooting because each contact calculates displacement assuming neighbors are stationary. When all neighbors push simultaneously, the particle receives multiple displacement vectors and "shoots" outward, creating numerical explosions. Using Baumgarte relaxation $\beta \in [0.2, 0.4]$ and multiple iterations ($K=8$) dampens overshooting.

#### Module 5: HPC Parallel Computing & Scaling
- **Analysis Question 1:** The GPU overhead floor ($\approx 1.4\text{ ms}$) consists of CUDA API driver overhead, PCIe kernel dispatch latency, device memory stream synchronization, and Thrust radix sort initialization. For small $N$, the GPU spends $>95\%$ of its time waiting for launch and transfer overheads rather than performing arithmetic.
- **Analysis Question 2:** At $N=100$, the entire particle working set is only $3.2\text{ KB}$, fitting inside the CPU L1 data cache ($48\text{ KB}$). The CPU processes particles with zero memory wait states at clock speeds up to $4.90\text{ GHz}$.
- **Analysis Question 3:** At $N=10,000$:
  - Sequential CPU: $37.21\text{ ms} \implies 26.8\text{ FPS}$ ($0.27\text{ million particles/sec}$).
  - NVIDIA RTX 4060 GPU: $2.05\text{ ms} \implies 488\text{ FPS}$ ($4.88\text{ million particles/sec}$).
  - The GPU delivers an **$18.16\times$ speedup** over optimized serial grid and preserves smooth interactive rendering ($>400\text{ FPS}$).

---

### 3. Grading Rubric (100 Points Total)

| Section | Criteria | Points |
|:---|:---|:---:|
| **Lab 1: Impact Mechanics** | Correct theoretical $\Delta E_k$ derivations, accurate table entries, clear explanation of momentum conservation and kinetic energy dissipation. | 20 pts |
| **Lab 2: Integrator Stability** | Correct derivation of $\frac{1}{2}gt\Delta t$, accurate table values, explanation of symplectic phase-space preservation vs Forward Euler blowup. | 20 pts |
| **Lab 3: Convergence Study** | Correct calculation of reduction ratios $R \approx 2.00$ and empirical orders $p \approx 1.00$, discussion of local vs global truncation error. | 20 pts |
| **Lab 4: Contact & Tunneling** | Explanation of DCD tunneling, demonstration of the CFL heuristic ($v\Delta t/r$), critique of contact-count normalization. | 20 pts |
| **Lab 5: HPC Parallel Scaling** | Correct speedup and efficiency calculations, Amdahl's Law analysis, explanation of the CPU-GPU crossover and algorithmic vs hardware acceleration. | 20 pts |
