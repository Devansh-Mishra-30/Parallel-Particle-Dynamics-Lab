# Parallel Particle Dynamics Lab: Interactive Workbench User Guide
## Phase 1B Desktop Scientific Verification Interface (`sim_lab`)

**Target Executable:** `build/sim_lab`
**Architecture:** Modern C++17, GLFW 3.3, OpenGL 3.3 Core Profile, Dear ImGui, ImPlot
**Status:** **PHASE 1B COMPLETE & VERIFIED** (100% CTest pass rate across all suites)

---

## 1. Quick Start

### 1.1 Launching the Interactive Desktop GUI
To launch the interactive GUI on Linux or Windows via WSL2 / WSLg:

```bash
# From workspace root
cd /home/devansh/simulator-workspaces/cuda-particle-simulation-v2

# Launch with WSLg display (or native X11/Wayland)
DISPLAY=:0 ./build/sim_lab
```

### 1.2 Running Automated Headless Verification
In headless server environments, remote CI workflows, or automated test pipelines without an active display server:

```bash
# Execute headless verification suite (100 frames per scenario)
./build/sim_lab --test-headless 100

# Or execute via CMake CTest
ctest --test-dir build --output-on-failure
```

---

## 2. Interface Architecture: The 3-Panel Scientific Workbench

```
+---------------------------------------------------------------------------------------------------+
|                                  PARALLEL PARTICLE DYNAMICS LAB                                   |
+--------------------------+------------------------------------------+-----------------------------+
|       LEFT PANEL         |               CENTER PANEL               |         RIGHT PANEL         |
|  Controls & Parameters   |       2D Simulation Viewport             |    Live Diagnostics         |
|                          |                                          |    & Verification Plots     |
| - Predefined Scenarios   | - Real-time SI Canvas (Meters)           | - Live Energy Partition     |
|   * EXP_01 Collision     | - Pan (RMB / MMB Drag), Zoom (Wheel)     |   (Ek, Ep, E_total via      |
|   * EXP_02 Gravity       | - Metric Grid with coordinate labels     |    ImPlot scrolling graph)  |
|   * EXP_03 Convergence   | - Velocity Vectors (direction & speed)   | - Momentum Drift Gauge      |
|   * EXP_04 Granular Heap | - Contact Normal Overlays & Penetration  |   (dP < 1e-5 status badge)  |
|   * EXP_CUSTOM           | - Analytical Trajectory Ghost Curve      | - Analytical Error Table    |
| - Physical Sliders       | - Circular Container Boundary            |   (v_exact, x_exact error)  |
|   (e, e_b, g, r, rho, m) | - 4 Color Modes:                         | - CFL Stability Sentinel    |
| - Numerical Controls     |   * Material Palette                     |   * CFL < 0.5 (Stable)      |
|   (dt, substeps, iters)  |   * Velocity Heatmap (Speed)             |   * CFL 0.5 - 1.0 (Warning) |
| - Playback & Export      |   * Contact State (Free/Clamped)         |   * CFL >= 1.0 (Tunneling)  |
|   (Play, Pause, Step,    |   * Mass Spectrum                        | - Non-Finite Sentinel       |
|    Reset, Export CSV)    | - Viewport HUD with quick nav buttons    | - Student Coursework Guides |
+--------------------------+------------------------------------------+-----------------------------+
```

---

## 3. Left Panel: Controls, Parameters & Playback

1. **Simulation Playback Controls:**
   - `[>] Play` / `[||] Pause`: Toggle real-time simulation advancement.
   - `[>|] Step`: Advance physical time by exactly one display frame ($N_{\text{sub}}$ integration substeps).
   - `[<<] Reset`: Instantly re-initialize the active scenario to its theoretical initial state.
   - **Sim Speed Multipliers:** Toggle playback rate between `0.1x`, `0.5x`, `1.0x`, and `2.0x`.
   - **Simulation Clock:** Live display of continuous physical time $t$ (seconds) and frame count.
   - **`Export Run to CSV`:** Generates high-precision CSV data in `output/csv/` complete with metadata comments and SI column headers.

2. **Scenario Selector:**
   - **EXP_01 (Two-Particle Collision):** Central and oblique impacts with adjustable restitution and mass ratios.
   - **EXP_02 (Ballistic Gravity):** Parabolic projectile motion with analytical trajectory ghost.
   - **EXP_03 (Timestep Convergence):** High-precision refinement benchmark comparing numerical drift.
   - **EXP_04 (Dense Granular Column):** 40+ particles settling under gravity into a static heap.
   - **EXP_CUSTOM:** Multi-particle playground for custom initial states.

3. **Physical Parameters:**
   - **Restitution ($e$):** Continuous slider $[0.00, 1.00]$. Alerts student if $e > 1.0$ (non-physical energy injection).
   - **Wall Restitution ($e_b$):** Boundary bounce coefficient.
   - **Gravity ($\mathbf{g}$):** Sliders for $g_x, g_y$ with presets for Earth ($-9.81\,\text{m/s}^2$), Moon ($-1.62\,\text{m/s}^2$), and Zero-G.
   - **Particle Radii & Masses:** Sliders for radii with automatic area-density mass calculation ($m = \pi \rho r^2$) and quick ratio presets ($m_2 = m_1$, $m_2 = 2m_1$, $m_2 = 4m_1$).

4. **Numerical Solver Controls:**
   - **Frame Timestep ($\Delta t$):** Selector from $0.001\,\text{s}$ to $0.050\,\text{s}$.
   - **Substeps per Frame ($N_{\text{sub}}$):** Slider $[1, 32]$.
   - **Effective Substep ($\Delta t_{\text{sub}}$):** Real-time calculated substep in milliseconds.
   - **Gauss-Seidel Iterations:** Contact solver stabilization passes $[1, 20]$.
   - **Baumgarte Factor ($\alpha$):** Positional penetration stabilization weight $[0.0, 1.0]$.

---

## 4. Center Panel: 2D Simulation Viewport

1. **Strict SI Coordinate System:**
   - All physical simulation calculations operate strictly in meters ($\text{m}$), seconds ($\text{s}$), and kilograms ($\text{kg}$).
   - World origin $(0, 0)\,\text{m}$ is centered in the container.
   - Pan by dragging the **Right Mouse Button** or **Middle Mouse Button**.
   - Zoom smoothly by scrolling the **Mouse Wheel** (centered on the cursor).
   - Quick navigation buttons (`[Reset View]`, `[+ Zoom]`, `[- Zoom]`) are accessible directly on the HUD.

2. **Visual Overlays:**
   - **Metric Grid:** Metric background grid with spacing ($0.2\,\text{m}$) and coordinate annotations along axes.
   - **Circular Container:** Outlined container of radius $R = 1.0\,\text{m}$ with radial tick marks.
   - **Analytical Trajectory Ghost:** In EXP_02, renders the exact mathematical parabola $\mathbf{x}_{\text{exact}}(t) = \mathbf{x}_0 + \mathbf{v}_0 t + \frac{1}{2}\mathbf{g} t^2$ as a dashed cyan curve.
   - **Velocity Vectors:** Directed arrows originating at particle centers with length proportional to speed $\|\mathbf{v}\|$.
   - **Contact Normal & Penetration:** During particle overlap, a red line connects centers with a contact midpoint disk and exact penetration annotation (e.g. `d=1.45mm`).

3. **Color Modes:**
   - **Material Palette:** Distinct recognizable color per particle ID.
   - **Velocity Heatmap:** Continuous gradient (Blue $\to$ Cyan $\to$ Green $\to$ Yellow $\to$ Red) proportional to instantaneous speed.
   - **Contact State:** Green for free particles, Orange for single contacts, Red for dense multi-contact clamping.
   - **Mass Spectrum:** Cyan brightness proportional to particle mass.

---

## 5. Right Panel: Live Quantitative Verification Dashboard

1. **Scrolling Energy Partition Plot (`ImPlot`):**
   - Live multi-curve scrolling time series displaying:
     - Kinetic Energy $E_k(t)$ (Green)
     - Potential Energy $E_p(t)$ (Orange)
     - Total Mechanical Energy $E_{\text{total}}(t) = E_k + E_p$ (Blue)
   - Real-time numerical readouts and percentage symplectic energy drift.

2. **Momentum Invariant Monitor:**
   - Real-time components $P_x(t), P_y(t)$ in $\text{kg}\cdot\text{m/s}$.
   - Drift gauge: $\Delta P = \|\mathbf{P}(t) - \mathbf{P}(0)\|$.
   - Status badge confirms momentum conservation ($< 10^{-5}\,\text{kg}\cdot\text{m/s}$) in isolated closed systems.

3. **Analytical Reference Verification Table:**
   - **Collision (EXP_01):** Displays exact theoretical 1D post-impact velocities ($v_{1,\text{exact}}, v_{2,\text{exact}}$) and theoretical kinetic energy drop $\Delta E_k$, side-by-side with measured simulation values and absolute error.
   - **Gravity (EXP_02):** Displays real-time exact position $\mathbf{x}_{\text{exact}}(t)$, velocity $\mathbf{v}_{\text{exact}}(t)$, and millimeter position error $\|\mathbf{x}_{\text{sim}} - \mathbf{x}_{\text{exact}}\|$.

4. **Numerical Health & Stability Sentinel:**
   - **CFL Ratio Indicator:**
     $$\text{CFL} = \frac{v_{\max}\Delta t_{\text{sub}}}{r_{\min}}$$
     - **Green ($\text{CFL} < 0.5$):** Stable, discrete collision detection reliable.
     - **Yellow ($0.5 \le \text{CFL} < 1.0$):** Marginal, recommend increasing substeps.
     - **Red ($\text{CFL} \ge 1.0$):** Unstable, high hazard of particle tunneling.
   - **Penetration Depth Monitor:** Displays $\delta_{\max}$ in millimeters.
   - **NaN / Inf Sentinel:** Guarantees 100% finite states.

5. **Student Coursework Modules:**
   - Embedded exercise descriptions for Modules 1 through 4 (elasticity, symplectic stability, empirical convergence order, and CFL tunneling).
