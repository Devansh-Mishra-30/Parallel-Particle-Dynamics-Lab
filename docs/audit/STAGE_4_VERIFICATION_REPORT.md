# Stage 4 Verification and Engineering Audit

**Project:** Parallel Particle Dynamics Lab  
**Repository:** `/home/devansh/simulator-workspaces/cuda-particle-simulation-v2`  
**Audit date:** 2026-11-06  
**Branch:** `feature/experiment-workbench-v1`  
**Final classification:** **PASS WITH LIMITATIONS**

## A. Executive Summary

Stage 4 is scientifically correct within its documented scope and is ready for final consolidation before Stage 5. The audit executed the implemented collision, ballistic, convergence, contact-sensitivity, CPU/OpenMP/CUDA scaling, configuration-integrity, export, and native rendering paths. No fundamental physics algorithm was changed.

The initial repository already contained extensive uncommitted Stage 1-4 work and modified generated evidence. All pre-existing changes were treated as user-owned; no reset, clean, commit, merge, push, or deletion was performed.

The audit found and fixed five integration/verification defects: wall contact was inferred from proximity instead of an actual numerical boundary impulse; convergence evidence could leak across scenario changes; screenshot mode did not advance paused simulations; CLI-selected GUI scenarios retained collision labels/baselines; and compact windows clipped fixed-width actions. Evidence output was also extended to record requested/generated counts, sample counts, total time, normalized time, speedups, CUDA availability, and both position and velocity differences.

The release remains limited by exploratory timing sample counts, mixed backend algorithms, a collision-free scaling comparison window, and unavailable desktop input injection. The native GLFW/ImGui application was genuinely launched and visually inspected at three resolutions, but dropdown/button clicking could not be automated because both available Windows automation runtimes failed during initialization.

## B. Build and Automated Tests

Commands executed from the repository root:

```text
cmake --build build -j8
ctest --test-dir build --output-on-failure
git diff --check
```

Initial baseline:

- Build: passed.
- CTest: 13/13 passed in 7.18 s.
- `git diff --check`: passed with no output.

Final regression run after fixes:

- Build: passed, including `sim_lab` and `test_stage4_verification`.
- CTest: 14/14 passed in 3.40 s.
- New suite: `Stage4EndToEndVerification` passed.
- `git diff --check`: passed with no output.

The new verifier is registered in `CMakeLists.txt`, has a 180-second timeout, and writes only to the dedicated audit directory documented below.

## C. Physics Verification

### Isolated two-particle collision

Both cases used the same initial geometry, velocities, masses, boundary, timestep, and substeps. The post-impact state was captured on the solver's first reported collision impulse. Both particles remained more than 0.1 m from the boundary exclusion limit, so an external wall impulse did not invalidate the analytical comparison.

| Restitution | Frames to impulse | Max velocity error (m/s) | Measured dEk (J) | Analytical dEk (J) | Energy error (J) | Momentum drift (kg m/s) |
|---:|---:|---:|---:|---:|---:|---:|
| 0.528 | 29 | 0 | -8.156761169 | -8.156761169 | 0 | 1.7745e-7 |
| 0.800 | 29 | 0 | -4.071505070 | -4.071503639 | 1.4305e-6 | 4.1438e-7 |

Acceptance tolerances were 2e-5 m/s per particle, 2e-4 J energy error, and 2e-5 kg m/s momentum drift. Both cases passed meaningful numerical assertions.

### Gravity and ballistic validity

Pre-contact measurements used 36 frames (`t = 0.300000 s`), frame `dt = 1/120 s`, eight substeps, and semi-implicit Euler. The expected first-order position error is approximately `0.5 |g| t h`, where `h = dt/substeps`.

| Gravity y (m/s²) | Measured position error (m) | Expected Euler error (m) | Velocity error (m/s) | First detected boundary impulse (s) |
|---:|---:|---:|---:|---:|
| -9.80665 | 0.001533182 | 0.001532289 | 3.9637e-6 | 0.7249996 |
| -3.70000 | 0.000581253 | 0.000578125 | 1.2159e-5 | 1.3833324 |

The former proximity/analytical-crossing heuristic was replaced by a comparison between the actual solver step and the same integrator advanced without a boundary. The application now invalidates free-flight error only when the numerical result diverges because a boundary impulse actually occurred. After that event it continues the simulation while reporting the analytical error as not applicable.

Trajectory data is stored separately from the dashed analytical path, reset by scenario/run resets, and capped at 1,200 samples. Rendered gravity evidence shows the simulated amber trace and analytical cyan dashed path as distinct curves.

## D. Numerical Accuracy

### Timestep convergence

The GUI and headless audit now call one shared convergence implementation. The table and plot therefore consume the same `ConvergenceDataPoint` vector; plot axes are `dt (s)` and `L2 position error (m)`.

| dt (s) | Position error (m) | Reduction ratio | Empirical order |
|---:|---:|---:|---:|
| 0.040 | 0.078453183 | — | — |
| 0.020 | 0.039226547 | 2.000002 | 1.000002 |
| 0.010 | 0.019613251 | 2.000002 | 1.000002 |
| 0.005 | 0.009806395 | 2.000047 | 1.000034 |

Errors decrease monotonically and confirm first-order convergence. Single-run sub-microsecond timings in this sweep are retained only as descriptive UI data and are not interpreted as reliable performance benchmarks.

### Controlled contact sensitivity

All four configurations used the identical 48-particle snapshot, physical and boundary parameters, frame timestep, and 180 frames (`3.000000156 s`). Timing includes per-frame diagnostics sampling.

| Config | Substeps | Iterations | Final penetration (m) | Peak penetration (m) | Active contacts | Final Ek (J) | Total CPU (ms) | ms/frame | Finite |
|:---:|---:|---:|---:|---:|---:|---:|---:|---:|:---:|
| A | 4 | 2 | 0.018261585 | 0.018265855 | 72 | 4.360576 | 3.712608 | 0.020626 | yes |
| B | 4 | 8 | 0.002754331 | 0.010635406 | 69 | 1.244693 | 13.652736 | 0.075849 | yes |
| C | 12 | 2 | 0.002771452 | 0.004779652 | 72 | 0.719445 | 11.020232 | 0.061224 | yes |
| D | 12 | 8 | 0.000258677 | 0.002823591 | 68 | 0.190286 | 41.355613 | 0.229753 | yes |

Increasing either substeps or solver iterations increased average cost as expected. This particular controlled setup also reduced final penetration, but the regression does not impose universal monotonic penetration behavior. These results establish numerical-resolution response and finite-state stability; they do not constitute real-material validation.

## E. HPC Performance

Protocol: deterministic golden-angle disk; 4 substeps; 2 contact iterations; 4 OpenMP threads; one backend-specific warm-up; three fresh solver measurements per workload; median milliseconds per simulation step; final state materialized before stopping the timer, which synchronizes CUDA and includes final host transfer. CPU uses all-pairs Gauss-Seidel while OpenMP/CUDA use spatial-grid Jacobi.

| N requested/generated | Steps | Simulated time (s) | CPU ms/step | OpenMP ms/step | CUDA ms/step | OpenMP speedup | CUDA speedup |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 128/128 | 25 | 0.208333 | 0.086374 | 0.061270 | 0.561828 | 1.410x | 0.154x |
| 256/256 | 25 | 0.208333 | 0.343500 | 0.071631 | 0.489744 | 4.795x | 0.701x |
| 512/512 | 10 | 0.083333 | 1.343733 | 0.122562 | 0.494987 | 10.964x | 2.715x |
| 1024/1024 | 10 | 0.083333 | 5.432116 | 0.230826 | 0.782169 | 23.533x | 6.945x |

CUDA was compiled and available; all CPU, OpenMP, and CUDA states remained finite. Requested and generated counts matched exactly. Different step counts for larger workloads are recorded explicitly and comparisons use normalized ms/step rather than total duration.

The measured position and velocity differences were zero for this selected time window on all workloads. No numerical-equivalence claim is made from that observation: the window is collision-free, and the backend contact-update algorithms differ. CUDA is not required to outperform CPU/OpenMP; it was slower at N=128 and N=256, then faster than CPU at N=512 and N=1024.

## F. GUI Verification

The native GLFW/OpenGL/ImGui/ImPlot executable was launched under WSLg and captured from the real framebuffer, not a browser mockup. Visually inspected cases:

- Collision after 40 frames at 1600x920.
- Gravity after 60 frames at 1600x920 and 1280x720.
- Convergence table/plot source state at 1600x920.
- Dense contacts after 120 frames at 1600x920 and 1920x1080.

Verified visually:

- Root workspace fills the framebuffer.
- Scenario/investigation labels match the actual CLI-loaded setup after the fix.
- Main controls and both compact-window action buttons are readable.
- The simulation viewport remains usable at all tested sizes.
- Collision, ballistic trajectory, dense-particle contacts, energy plots, diagnostic metrics, and convergence table render.
- The compact layout assigns 30/40/30 weights to setup/simulation/evidence below 1400 px and 25/50/25 otherwise.

Interactive desktop injection was attempted through both available Windows automation runtimes. Both failed during initialization with `CreateProcessSecurityEnvironment failed (HRESULT = 0x80070001)`. Consequently, automated clicks on investigation/scenario dropdowns, Custom Settings, and study buttons were not claimed. Their underlying actions were executed headlessly, and scenario/render integration was exercised through the native CLI render path.

At 1280x720, long one-line diagnostic explanations still truncate horizontally; quantitative sections remain visible or vertically scrollable. This is cosmetic, not a scientific blocker.

## G. Export and Reproducibility

Dedicated evidence directory:

`output/audit/stage4_verification_2026-11-06/`

Generated and programmatically reopened:

- `regular_run.csv`: 1 data row plus full experiment, backend, integrator, timestep, substeps, solver iterations, restitution, gravity, particle count, units, and initial-particle metadata.
- `backend_scaling.csv`: 4 rows plus protocol, configuration, threads, repeats, requested/generated counts, step counts, normalized timings, speedups, availability, differences, and finite-state flag.
- `contact_sensitivity.csv`: 4 rows plus fixed-duration protocol and full numerical/physical provenance.
- `convergence_verification.csv`: 4 rows with timestep, substeps, position error, reduction ratio, order, and descriptive runtime.
- `collision_verification.csv`: 2 rows.
- `gravity_verification.csv`: 2 rows.
- `summary.json`: machine-readable audit counts and CUDA availability.

For every CSV, the audit ignored comment metadata, matched header/data column counts, verified expected row counts, and parsed every data value numerically. Study parameter edits and scenario changes invalidate prior results, preventing old evidence from being relabeled under a new configuration.

Previously modified tracked outputs present at audit start were not deleted or cleaned. The audit's new evidence uses the dedicated directory above. Existing CTest behavior still rewrites its pre-existing tracked smoke export; that file was already modified in the initial working-tree baseline.

## H. Defects Found and Fixed

1. **False wall-contact validity transition** — `lab_gui/src/app.cpp`, new `lab_gui/include/lab_gui/ballistic_reference.h`. Replaced proximity/analytical crossing with actual numerical free-flight deviation detection.
2. **Convergence evidence leaked across scenario selection** — `lab_gui/include/lab_gui/experiment_presets.h`. Scenario loading now clears convergence results.
3. **Screenshot acceptance mode did not advance simulations** — `lab_gui/src/main.cpp`. Screenshot mode now unpauses for the requested frames and pauses before rendering.
4. **CLI GUI scenarios were mislabeled as modified collision runs** — `lab_gui/src/main.cpp`. CLI setup now uses workbench scenario selection and captures the correct baseline.
5. **Compact-window action clipping** — `lab_gui/src/app.cpp`, `lab_gui/src/controls_panel.cpp`. Added compact column weights and available-width action buttons.
6. **Incomplete scaling evidence** — `lab_gui/include/lab_gui/workbench_evidence.h`, `lab_gui/include/lab_gui/workbench_study_runner.h`, `lab_gui/src/controls_panel.cpp`. Added requested/generated counts, sample counts, total time, speedups, CUDA availability, and velocity differences.
7. **GUI-only convergence implementation** — `lab_gui/include/lab_gui/workbench_study_runner.h`, `lab_gui/src/diagnostics_panel.cpp`. Refactored to one GUI/headless study function.
8. **Missing end-to-end Stage 4 regression** — `tests/test_stage4_verification.cpp`, `CMakeLists.txt`. Added executable physics, numerical, performance, selection, export, and invariant assertions.

## I. Outstanding Issues

### Critical

- None found.

### Moderate

- Desktop input automation is blocked by the host security-environment failure, so real mouse-driven interaction remains a manual acceptance item on this machine.
- Scaling uses only three samples per workload. This is sufficient to catch gross instability, not to publish statistically rigorous performance claims.
- CPU and parallel backends use different broadphase/contact-update algorithms. Speedups combine algorithm and hardware effects.
- The scaling interval is collision-free, so zero state differences do not validate dense-contact backend equivalence.

### Cosmetic / low risk

- Some long diagnostic prose truncates at 1280x720; core metrics and controls remain usable.
- Convergence runtimes are too small and noisy for performance interpretation.
- The Stage 4 audit evidence directory is deterministic and is replaced on a rerun of the new CTest; historical runs should be copied or timestamped before repeated release audits if retention is required.

## J. Final Release Readiness

**PASS WITH LIMITATIONS**

All existing and newly introduced tests pass. Physics verification, gravity validity, first-order convergence, contact sensitivity, CPU/OpenMP/CUDA scaling, configuration integrity, CSV reproducibility, and native rendering were executed with recorded evidence. Confirmed scientific-validity and integration defects were fixed with regression coverage.

Stage 4 is ready for final consolidation. Before a public performance claim or Stage 5 handoff, perform one manual mouse-driven GUI pass on a host where desktop automation works, retain multiple timestamped benchmark runs, and add a deliberately contact-active cross-backend scaling case if numerical backend equivalence is to be claimed.
