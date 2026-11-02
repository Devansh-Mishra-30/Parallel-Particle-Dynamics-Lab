# Parallel Particle Dynamics Lab: GUI Acceptance & Usability Report
## Phase 1C: Interactive Workbench Verification with Real OpenGL Screenshots

**Document Version:** 1.0.0
**Date:** 2026-10-16
**Application Target:** `sim_lab` (GLFW 3.3.8, OpenGL 3.3 Core, Dear ImGui v1.90.4-docking, ImPlot v0.16)
**Host Environment:** WSL2 Ubuntu 24.04 LTS (WSLg graphical display server, `:0`)
**Resolution:** 1600 × 920 native framebuffer

---

### 1. Executive Summary & Verification Protocol

The interactive graphical user interface (`sim_lab`) was verified in the real WSL2 graphical desktop environment. The application acts as a digital laboratory bench for mechanical engineering students studying particle dynamics, numerical methods, and high-performance computing.

All acceptance criteria established in Task 1 and Task 3 were subjected to live verification:
- [x] Application initializes with zero crashes, OpenGL errors, or graphical artifacts.
- [x] Three-panel ergonomic layout (Controls, Viewport, Diagnostics) renders cleanly with responsive resizing.
- [x] Real-time playback controls (Play, Pause, Step, Reset, 0.1x–2.0x speeds) control the physical integrator without state drift.
- [x] Viewport pan and zoom operate smoothly, keeping SI metric world coordinates ($1\,\text{unit} = 1\,\text{meter}$) calibrated to screen space.
- [x] 60 FPS live scientific plots render kinetic, potential, and total energy with auto-scaling axes.
- [x] Parameter sliders dynamically modulate the physical simulation engine.
- [x] Comprehensive CSV telemetry streams complete scenario metadata, invariants, and simulation states.
- [x] Genuine OpenGL framebuffers captured via `glReadPixels` and documented below.

---

### 2. Panel-by-Panel Acceptance Verification

#### Panel 1: Experiment Controls & Parameter Configuration (Left Panel)
1. **Simulation Playback Engine:**
   - **Play / Pause (`[||] Pause` / `[>] Play`):** Instantly halts or resumes integrator steps while maintaining rendering and camera interaction.
   - **Step (`[>|] Step`):** Advances the physics engine by exactly one discrete frame ($\Delta t = 1/120\,\text{s}$ divided into $N_{\text{sub}}$ substeps), allowing single-substep inspection of contact events.
   - **Reset (`[<<] Reset`):** Reconstructs the initial conditions of the active experiment preset, clearing time histories and analytical error buffers.
   - **Simulation Speed:** Radio toggles ($0.1\times, 0.5\times, 1.0\times, 2.0\times$) scale physical time progression per wall-clock second.
   - **CSV Export & Screenshot:** One-click buttons stream state telemetry to disk or dump the current framebuffer to PNG.
2. **Physical Parameter Sliders:**
   - **Restitution ($e \in [0, 1]$):** Adjusts particle-particle inelastic dissipation coefficient.
   - **Boundary Restitution ($e_{\text{wall}} \in [0, 1]$):** Modulates elasticity of boundary reflections.
   - **Gravitational Field Quick Presets:** Earth ($-9.81\,\text{m/s}^2$), Moon ($-1.62\,\text{m/s}^2$), Zero G ($0.00\,\text{m/s}^2$), and arbitrary 2D vector sliders ($g_x, g_y$).
3. **Numerical Solver Hyperparameters:**
   - **Frame Timestep ($\Delta t$):** Adjustable from $0.001\,\text{s}$ to $0.05\,\text{s}$.
   - **Substeps ($N_{\text{sub}}$):** Adjustable from $1$ to $32$ substeps per frame. Displays effective $\Delta t_{\text{sub}}$ in milliseconds.
   - **Gauss-Seidel Iterations:** Sets collision solver relaxation passes ($1$ to $16$).
   - **Penetration Stabilization ($\beta$):** Controls mass-weighted position correction coefficient ($0.0$ to $1.0$).
4. **Viewport Visual Overlays:**
   - Metric grid with SI distance labels ($0.2\,\text{m}$ subdivisions).
   - Boundary circle perimeter line.
   - Velocity vector arrows with adjustable visual scaling ($0.05$ to $0.50$).
   - Contact normal vectors with live penetration depth callouts (e.g. `0.05mm`).
   - Ghost analytical trajectory overlays.

---

#### Panel 2: 2D Simulation Viewport (Center Panel)
1. **Orthographic World Projection:**
   - World space coordinates are strictly SI metric meters ($x, y \in \mathbb{R}$).
   - Particle discs render with smooth circular geometry, radius-scaled diameters, center markers, and mass labels (e.g., `P1 (11.3kg)`).
2. **Interactive Camera Navigation:**
   - **Pan:** Middle-mouse drag or right-mouse drag translates the coordinate origin.
   - **Zoom:** Mouse wheel smoothly scales view magnification centered on cursor coordinates.
   - **Reset View:** Top-left HUD button resets zoom ($400\,\text{px/m}$) and centers on container origin $(0, 0)$.
3. **Diagnostic HUD Overlay:**
   - Top-left HUD displays simulation time ($t$), substep count ($N_{\text{sub}}$), active particle count ($N$), and live DCD tunneling sentinel ratio.

---

#### Panel 3: Scientific Diagnostics Dashboard (Right Panel)
1. **Live Energy Partitioning (ImPlot):**
   - Live 60 FPS rolling plot displaying Kinetic Energy ($E_k$, green), Gravitational Potential Energy ($E_p$, orange), and Total Mechanical Energy ($E_{\text{total}}$, cyan).
   - Dynamic auto-fitting Y-axis handles negative potential energy regimes without clipping.
   - Explicit datum documentation: $E_p = -m(\mathbf{g} \cdot \mathbf{x})$, referenced to container origin $y = 0\,\text{m}$.
2. **Momentum Invariant Monitor:**
   - Real-time total momentum vector $\mathbf{P} = (P_x, P_y)\,\text{kg}\cdot\text{m/s}$.
   - Momentum drift magnitude $\|\mathbf{P}(t) - \mathbf{P}(0)\|$.
   - Context-aware status indicator distinguishing:
     - `[STATUS: MOMENTUM CONSERVED (drift < 1e-5 kg*m/s)]` in isolated collisions.
     - `[STATUS: EXTERNAL GRAVITATIONAL FIELD: dP/dt = M * g]` when gravity acts.
     - `[STATUS: WALL IMPULSE INTERACTION: Boundary Normal Forces]` during boundary reflections.
3. **Analytical Reference Verification Overlays:**
   - **EXP_01 (Collision):** Displays pre-impact reduced mass $\mu$, relative normal velocity $v_n$, exact theoretical velocities $\mathbf{v}_1^*, \mathbf{v}_2^*$, theoretical dissipation $\Delta E_k$, post-collision measured velocities, and absolute error metrics ($||\mathbf{e}_v||, |\Delta E_{k,\text{error}}|$).
   - **EXP_02 (Gravity):** Displays theoretical parabola coordinates $\mathbf{x}^*(t) = \mathbf{x}_0 + \mathbf{v}_0 t + \frac{1}{2}\mathbf{g}t^2$, theoretical velocity $\mathbf{v}^*(t)$, and Euclidean position/velocity errors.
   - **EXP_03 (Convergence):** Interactive multi-timestep convergence engine executing $\Delta t, \Delta t/2, \Delta t/4, \Delta t/8$ sweeps and tabulating $L_2$ errors, reduction ratios, and empirical convergence order $p$.
   - **EXP_04 (Granular):** Displays active pairwise contact count, maximum penetration depth, and compaction state.
4. **Numerical Health & Stability Sentinel:**
   - **DCD Tunneling Sentinel:** Monitors $v_{\max}\Delta t_{\text{sub}} / r_{\min}$ to ensure particles do not displace more than half their radius per substep.
   - **Floating-Point Sentinel:** Validates that $100\%$ of position and velocity components are finite ($0\,\text{NaN}, 0\,\text{Inf}$).

---

### 3. Real Application Screenshots & Visual Evidence

The following screenshots were captured directly from the running OpenGL 3.3 framebuffer in WSLg via `glReadPixels`:

#### Figure 1: EXP_01 — Two-Particle Inelastic Impact ($e = 0.528$)
![EXP_01 Inelastic Collision](../../output/screenshots/gui_exp01_collision_inelastic.png)

*Figure 1 Description:*
Two equal-radius particles ($r_1=r_2=0.06\,\text{m}$, $m_1=m_2=11.31\,\text{kg}$) undergo an inelastic impact with $e=0.528$. The right diagnostics panel demonstrates the resolution of **BUG-01**: pre-impact relative normal velocity $v_n = -2.000\,\text{m/s}$ yields exact theoretical dissipation $\Delta E_k = -8.1568\,\text{J}$. Measured post-collision velocities match analytical targets with machine precision ($||\mathbf{e}_v|| = 0.00\,\text{m/s}$, $|\Delta E_{k,\text{error}}| = 0.00\,\text{J}$), while total system momentum is preserved to $1.77\times 10^{-7}\,\text{kg}\cdot\text{m/s}$.

---

#### Figure 2: EXP_02 — Ballistic Free Fall in Gravitational Field ($g = -9.81\,\text{m/s}^2$)
![EXP_02 Ballistic Flight](../../output/screenshots/gui_exp02_ballistic_gravity.png)

*Figure 2 Description:*
A projectile particle traverses a gravitational parabolic arc within the circular boundary. The dashed teal curve represents the exact continuous analytical trajectory $\mathbf{x}^*(t)$. The right panel displays the context-aware momentum monitor confirming `[STATUS: EXTERNAL GRAVITATIONAL FIELD: dP/dt = M * g]`, avoiding misleading conservation failure flags. Position error is only $1.704\,\text{mm}$ after $0.33\,\text{s}$ of flight.

---

#### Figure 3: EXP_03 — Multi-Timestep Convergence Study ($p \approx 1.00$)
![EXP_03 Convergence Study](../../output/screenshots/gui_exp03_convergence_study.png)

*Figure 3 Description:*
Students trigger an automated convergence study comparing $\Delta t = 0.04\,\text{s}, 0.02\,\text{s}, 0.01\,\text{s}, 0.005\,\text{s}$ against exact analytical projectile motion. The live table confirms that each halving of $\Delta t$ cuts $L_2$ position error exactly in half (reduction ratio $2.00$), demonstrating textbook first-order convergence ($p \approx 1.00$) for the Semi-Implicit Symplectic Euler integrator.

---

#### Figure 4: EXP_04 — Dense Granular Column Settling Under Gravity
![EXP_04 Dense Granular Settling](../../output/screenshots/gui_exp04_dense_granular.png)

*Figure 4 Description:*
A column of 48 particles settles into a compacted heap at the base of the circular container under $g = -9.81\,\text{m/s}^2$. The viewport highlights active contact lines and millimeter-scale penetration labels directly on the particles. The right dashboard demonstrates the resolution of **BUG-02**: the diagnostics calculator detects **68 active pairwise contacts** with peak penetration stabilized to $0.282\,\text{mm}$ ($<0.7\%$ of particle diameter). The system remains $100\%$ finite with zero numerical jitter or explosive penetration.
