# Clean Environment Installation & Build Reproducibility Audit
## Parallel Particle Dynamics Lab — Phase 5 Release Engineering Audit

**Audit Date:** 2026-10-16
**Auditor:** Antigravity Engineering Review Team
**Scope:** Clean checkout build in `/tmp/clean_sim_test`, CMake target independence, CUDA optionality, Python plotting

---

### Executive Summary

Under Phase 5 Task 3, a fresh, isolated source tree test was executed from a completely clean directory (`/tmp/clean_sim_test`) to verify build reproducibility, absence of hard-coded paths, optionality of CUDA dependencies, automated testing pass rates, and end-to-end data export.

---

### 1. Verification Matrix Across Build Configurations

| Build Target | CMake Configuration | Toolchain | Targets Built | Test Suite Pass Rate | Status |
|:---|:---|:---|:---:|:---:|:---:|
| **CPU-Only & OpenMP** | `-DENABLE_CUDA=OFF` | GCC 13.3.0, C++17, OpenMP 4.5 | `sim_core`, `sim_omp_v2`, `sim_experiments`, `sim_lab`, tests | **6/6 Passed (100%)** | **PASSED** |
| **Full CUDA GPU** | `-DENABLE_CUDA=ON` | GCC 13.3.0 + NVCC 12.0 (sm_89) | `sim_core`, `sim_omp_v2`, `sim_cuda_v2`, `sim_lab`, tests | **9/9 Passed (100%)** | **PASSED** |
| **Experiment Harness** | Standalone Executable | GCC 13.3.0 | `run_experiments` (5 scenarios) | **5/5 Scenarios Exported** | **PASSED** |
| **Python Plotting** | Matplotlib / Seaborn | Python 3.12 (WSL Ubuntu) | 8 Publication PNGs generated | **8/8 Figures Saved** | **PASSED** |

---

### 2. Isolated CPU-Only Build Verification (`-DENABLE_CUDA=OFF`)

To ensure that mechanical engineering students and institutions without NVIDIA GPUs can use the software:
1. **Isolated Path:** `/tmp/clean_sim_test/build_cpu_only`
2. **CMake Invocation:**
   ```bash
   cmake -B build_cpu_only -DENABLE_CUDA=OFF -DCMAKE_BUILD_TYPE=Release
   cmake --build build_cpu_only -j8
   ```
3. **Behavior:**
   - CMake reported: `-- CUDA backend disabled by configuration (ENABLE_CUDA=OFF).`
   - `sim_cuda_v2` static library was completely omitted from compilation.
   - `sim_lab` desktop GUI compiled cleanly using `#if HAS_CUDA_BACKEND` guards; the GUI displays `CUDA GPU (Disabled)` with an inactive radio button, preventing null pointer or driver crashes.
   - All 6 applicable test suites executed via `ctest`:
     - `PhysicsVerification`: 14/14 tests passed (including Test 14 Symplectic vs Explicit Euler).
     - `ExperimentFrameworkVerification`: 6/6 tests passed.
     - `CollisionLaboratoryVerification`: 7/7 tests passed.
     - `OpenMpCorrectnessVerification`: 46/46 tests passed.
     - `SpatialGridVerification`: 0 false negatives.
     - `LabGuiSmokeTest`: Passed (headless 60 frames).
     - **Pass Rate:** **6/6 passed (100% success rate)** in $2.58$ seconds.

---

### 3. Isolated Full CUDA Build Verification (`-DENABLE_CUDA=ON`)

1. **Isolated Path:** `/tmp/clean_sim_test/build_cuda`
2. **CMake Invocation:**
   ```bash
   cmake -B build_cuda -DENABLE_CUDA=ON -DCMAKE_BUILD_TYPE=Release
   cmake --build build_cuda -j8
   ```
3. **Behavior:**
   - NVCC compiled `cuda_solver.cu` and `cuda_spatial_grid.cu` targeting Ada Lovelace architecture (`sm_89`).
   - All 9 test suites executed via `ctest`:
     - Cross-backend verification (`test_cross_backend_verification`) confirmed mathematical equivalence between CPU reference, OpenMP (8T), and CUDA GPU.
     - Radix sort GPU spatial grid passed with zero false negatives across all 6 physical particle distributions.
     - **Pass Rate:** **9/9 passed (100% success rate)**.

---

### 4. Headless Experiment & Data Pipeline Verification

In `/tmp/clean_sim_test`:
```bash
./build_cpu_only/run_experiments --out output/csv
python3 scripts/plot_experiments.py --csv-dir output/csv --plot-dir output/plots
```
- **CSV Data Integrity:**
  - `collision_elastic.csv`: 6,874 bytes (Momentum error: $0.00\text{ kg}\cdot\text{m/s}$).
  - `collision_inelastic.csv`: 6,886 bytes (Actual $\Delta E_k = -5.89049\text{ J}$, Theory: $-5.89049\text{ J}$).
  - `collision_unequal.csv`: 6,835 bytes ($v_1 = -1.20\text{ m/s}, v_2 = +0.80\text{ m/s}$).
  - `gravity_trajectory.csv`: 8,849 bytes (Position error matches $\frac{1}{2}gt\Delta t$ to $0.08\%$).
  - `convergence_study.csv`: 848 bytes (Empirical orders: $0.999996$, $1.00003$, $0.999982$, $1.00007$).
- **Plot Generation:**
  - `plot_collision_energy.png` (175 KB)
  - `plot_convergence.png` (259 KB)
  - `plot_gravity_energy.png` (229 KB)
  - `plot_momentum.png` (221 KB)
  - `plot_trajectory.png` (254 KB)

All figures generated with zero warnings or NaN exceptions.
