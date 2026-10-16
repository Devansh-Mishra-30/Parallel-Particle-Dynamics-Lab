# Numerical Verification Report: Phase 1A Sequential Reference Solver

**Target System:** Simulator V2 Sequential CPU Reference Solver (`sim_core`)
**Workspace:** `/home/devansh/simulator-workspaces/cuda-particle-simulation-v2`
**Branch:** `feature/simulator-v2`
**Test Harness:** `tests/test_physics.cpp` (Target `test_physics`)
**Date of Execution:** October 2026
**Status:** 13/13 Passed (100% Pass Rate)

---

## 1. Executive Summary

Phase 1A implemented the standalone, modular shared physics core (`sim_core`) and the sequential CPU reference solver (`CpuReferenceSolver`). An automated validation test suite comprising 13 analytical and numerical test cases was executed to scientifically verify the physics implementation before extending it to parallel OpenMP and CUDA backends.

All 13 tests passed within mathematically justified tolerances.

---

## 2. Test Execution Results Matrix

```
========================================================================================================================
SIMULATOR V2: PHYSICAL & NUMERICAL VERIFICATION RESULTS MATRIX (PHASE 1A)
========================================================================================================================
ID   Test Description                          Category                 Measured Error   Tolerance      Status
------------------------------------------------------------------------------------------------------------------------
TC01 Constant Velocity Motion                  Analytical Verification  5.0576e-06 m     1.0000e-05 m   PASS
TC02 Constant Acceleration Gravity Free Fall   Analytical Verification  8.1062e-06 m     1.0000e-04 m   PASS
TC03 Two-Particle Equal-Mass Elastic Collision Analytical Verification  0.0000e+00 m/s   1.0000e-04 m/s PASS
TC04 Two-Particle Inelastic Collision (e=0.5)  Physical Model Valid.    0.0000e+00       1.0000e-04     PASS
TC05 Unequal-Mass Elastic Collision (m2=4*m1)  Analytical Verification  0.0000e+00 m/s   1.0000e-04 m/s PASS
TC06 Multi-Particle Momentum Conservation      Physical Model Valid.    1.5497e-06 kg*m/s 1.0000e-05 kg*m/s PASS
TC07 Kinetic Energy Monotonicity (Inelastic)   Physical Model Valid.    3.1621e-07       1.0000e-03     PASS
TC08 Specular Circular Boundary Reflection     Analytical Verification  1.1921e-07 m/s   1.0000e-04 m/s PASS
TC09 Zero-Distance Overlap Regularization      Numerical Consistency    0 NaNs (sep>0)   0 NaNs         PASS
TC10 Finite Position & Velocity Invariants     Numerical Consistency    0 NaNs / 2400 sub 0 NaNs        PASS
TC11 Timestep Refinement & Convergence         Numerical Consistency    1.5259e-05       2.0000e-01     PASS
TC12 Three-Particle Symmetric Contact          Physical Model Valid.    0.0000e+00 m/s   1.0000e-03 m/s PASS
TC13 Dense Contact Stability (Granular Column) Physical Model Valid.    4.1027e-04 m     5.0000e-03 m   PASS
========================================================================================================================
TOTAL: 13 PASSED, 0 FAILED (100% SUCCESS)
========================================================================================================================
```

---

## 3. Detailed Case-by-Case Numerical Analysis

### TC01: Constant Velocity (Free Ballistic Motion)
- **Analytical Solution:** $\mathbf{x}(t) = \mathbf{x}_0 + \mathbf{v}_0 t$
- **Measured Position:** $(1.499995, -0.799999)\,\text{m}$ vs exact $(1.500000, -0.800000)\,\text{m}$
- **Measured Error:** $5.06 \times 10^{-6}\,\text{m}$ (bounded by float32 accumulated rounding across 480 substeps).
- **Outcome:** PASS.

### TC02: Constant Acceleration Gravity Free Fall
- **Analytical Solution:** Velocity exact discrete $v_y = g t$; position discrete exact $y_n = y_0 + v_0 t + \frac{1}{2}gt^2 + \frac{1}{2}gt\Delta t_{\text{sub}}$.
- **Measured Velocity:** $-4.903333\,\text{m/s}$ vs exact $-4.903325\,\text{m/s}$ (error $8.11 \times 10^{-6}\,\text{m/s}$).
- **Measured Position:** $3.769059\,\text{m}$ vs discrete exact $3.769061\,\text{m}$ (error $2.0 \times 10^{-6}\,\text{m}$).
- **Outcome:** PASS.

### TC03: Two-Particle Equal-Mass Elastic Collision
- **Analytical Solution:** Head-on symmetric 1D collision ($v_1 = +1.0, v_2 = -1.0\,\text{m/s}$). Exact velocity swap ($v_1' = -1.0, v_2' = +1.0\,\text{m/s}$).
- **Measured Post-Collision Velocities:** $v_1' = -1.000000\,\text{m/s}$, $v_2' = +1.000000\,\text{m/s}$.
- **Kinetic Energy Error:** $\Delta E_k = 0.000000\,\text{J}$.
- **Momentum Error:** $\|\mathbf{P}\| = 0.000000\,\text{kg}\cdot\text{m/s}$.
- **Outcome:** PASS.

### TC04: Two-Particle Inelastic Collision ($e = 0.5$)
- **Analytical Solution:** $v_{\text{rel}}' = -e \, v_{\text{rel}} \implies v_1' = -0.5, v_2' = +0.5\,\text{m/s}$.
  Theoretical energy dissipation: $\Delta E_k = -0.5 \, m_{\text{eff}} (1 - e^2) v_{\text{rel}}^2 = -5.890487\,\text{J}$.
- **Measured Post-Collision Velocities:** $v_1' = -0.500000\,\text{m/s}$, $v_2' = +0.500000\,\text{m/s}$.
- **Measured Restitution:** $e_{\text{measured}} = 0.500000$ (exact).
- **Measured Energy Loss:** $\Delta E_k = -5.890487\,\text{J}$ (matches theoretical dissipation to 6 decimal places).
- **Outcome:** PASS.

### TC05: Unequal-Mass Elastic Collision ($m_2 = 4 m_1$)
- **Analytical Solution:** $v_1 = +2.0\,\text{m/s}$, $v_2 = 0\,\text{m/s}$.
  $v_1' = \frac{m_1 - m_2}{m_1 + m_2} v_1 = -1.2\,\text{m/s}$.
  $v_2' = \frac{2 m_1}{m_1 + m_2} v_1 = +0.8\,\text{m/s}$.
- **Measured Post-Collision Velocities:** $v_1' = -1.200000\,\text{m/s}$, $v_2' = +0.800000\,\text{m/s}$.
- **Kinetic Energy Error:** $\Delta E_k = 0.000000\,\text{J}$.
- **Linear Momentum Error:** $\Delta \mathbf{P} = 0.000000\,\text{kg}\cdot\text{m/s}$.
- **Outcome:** PASS.

### TC06: Multi-Particle Momentum Conservation
- **Setup:** 16 energetic particles colliding internally for 120 frames (960 substeps).
- **Measured Maximum Momentum Drift:** $\|\Delta \mathbf{P}\| = 1.55 \times 10^{-6}\,\text{kg}\cdot\text{m/s} \le 10^{-5}$.
- **Outcome:** PASS.

### TC07: Kinetic Energy Behavior (Elastic vs Inelastic)
- **Setup:** 6 inward colliding particles under $e = 1.0$ vs $e = 0.7$.
- **Elastic Relative Energy Drift:** $3.16 \times 10^{-7} \le 10^{-3}$.
- **Inelastic Energy Dissipation:** $15.0796\,\text{J} \to 4.7257\,\text{J}$ (monotonic energy loss confirmed).
- **Outcome:** PASS.

### TC08: Specular Circular Boundary Reflection
- **Setup:** Incident speed $1.5\,\text{m/s}$ at $30^\circ$ inside circular wall $R = 1.0\,\text{m}$.
- **Post-Bounce Speed:** $1.500000\,\text{m/s}$ (speed error $1.19 \times 10^{-7}\,\text{m/s}$).
- **Position Boundedness:** Maximum particle radius distance $0.651567\,\text{m} \le 0.95\,\text{m}$.
- **Outcome:** PASS.

### TC09: Zero-Distance Overlap Regularization
- **Setup:** Two particles initialized at identical coordinates $(0.2, -0.1)\,\text{m}$.
- **NaNs Detected:** 0.
- **Post-Regularization Separation:** $0.1000\,\text{m}$ (clean separation without numerical singularity).
- **Outcome:** PASS.

### TC10: Finite Position & Velocity Invariants
- **Setup:** 20 interacting particles under gravity and circular boundaries for 2,400 substeps.
- **NaN Count:** 0. All state variables remained strictly finite.
- **Outcome:** PASS.

### TC11: Timestep Convergence Order
- **Setup:** Global error evaluated at $\Delta t, \Delta t/2, \Delta t/4$.
- **Measured Convergence Ratio:** $\frac{E(\Delta t)}{E(\Delta t/2)} = 2.000015$ (theoretical expectation for 1st order Semi-implicit Euler is $2.00$).
- **Outcome:** PASS.

### TC12: Three-Particle Symmetric Contact
- **Setup:** Central resting particle struck simultaneously by left ($+1\,\text{m/s}$) and right ($-1\,\text{m/s}$) particles.
- **Central Particle Velocity:** $0.000000\,\text{m/s}$ (remained stationary by symmetry).
- **Reflected Velocities:** Left particle $-1.000000\,\text{m/s}$, Right particle $+1.000000\,\text{m/s}$.
- **Outcome:** PASS.

### TC13: Dense Contact Stability (Granular Column)
- **Setup:** 5 particles resting in a vertical stack under gravity ($g = -9.80665\,\text{m/s}^2, e = 0.0$) for 1,920 substeps.
- **Settled Maximum Speed:** $0.0581\,\text{m/s}$.
- **Maximum Inter-Particle Overlap:** $0.410\,\text{mm}$ ($< 5.0\,\text{mm}$ tolerance).
- **NaNs:** 0.
- **Outcome:** PASS.
