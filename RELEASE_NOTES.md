# Parallel Particle Dynamics Lab — consolidated v2 scientific workbench

**Publication state (November 6, 2026):** source and scientific evidence merged into `main` by [PR #3](https://github.com/Devansh-Mishra-30/cuda-particle-simulation/pull/3). This is an educational / portfolio engineering prototype, **not** a physically validated granular solver or a research-grade library. No formal GitHub Release/tag has been created.

## Delivered

- C++17 frictionless 2D discs, uniform gravity, circular boundaries, explicit and semi-implicit Euler integration, impulse contacts, and discrete penetration correction.
- Sequential CPU Gauss–Seidel-style reference, OpenMP uniform-grid Jacobi implementation, and optional NVIDIA CUDA grid/Jacobi backend.
- Native GLFW/ImGui/ImPlot interface with three investigation modes, five scenarios, parameter predictions, diagnostics, trajectory overlays, and experiment-specific validity flags.
- Automated collision and ballistic verification; timestep convergence; controlled 48-particle dense-contact sensitivity; 128–1024-particle CPU/OpenMP/CUDA scaling; CSV exports with methodological metadata.
- CI builds CPU/OpenMP headless and GUI-enabled configurations on Ubuntu 24.04. CUDA runtime verification is performed separately on NVIDIA hardware.

## Verified evidence (November 6, 2026)

| Evidence | Result and scope |
| --- | --- |
| Local CUDA-enabled CMake/CTest | **14/14** CTest suites passed on WSL2/RTX 4060 (compute architecture 89) |
| GitHub Actions headless CPU/OpenMP | **10/10** tests passed on `main` |
| GitHub Actions GUI-enabled CPU/OpenMP | **11/11** tests passed using Xvfb on `main` |
| Collision energy | Maximum analytical energy-change error approximately `1.43e-6 J` in the tested cases |
| Ballistic trajectory | Errors consistent with the first-order semi-implicit Euler error model for two accelerations before wall impact |
| Timestep sweep | Empirical convergence order approximately `1.00` at each refinement |
| Dense contacts | Four controlled finite-state configurations; decreasing observed penetration with increased resolution in the tested scene |
| CPU/OpenMP/CUDA study | Four deterministic collision-free workloads; exploratory 3-sample median times with differences in algorithms acknowledged |

See [Stage 4 verification](docs/audit/STAGE_4_VERIFICATION_REPORT.md), [immutable evidence snapshot](output/audit/stage4_preserved_2026-11-06_Ku9qXI/), and [successful main-branch CI run](https://github.com/Devansh-Mishra-30/cuda-particle-simulation/actions/runs/42302040951).

## Interpretation boundaries

- CPU reference uses all-pairs candidate search and in-place Gauss–Seidel updates; OpenMP and CUDA select spatial-grid Jacobi strategies. **Implementation speedups are not pure hardware speedups.**
- Identical results for a collision-free cloud do not establish contact-active many-body backend equivalence.
- Finite states and small per-step displacement do not guarantee accurate contact resolution, material validation, or continuous collision detection.
- The GPU result is hardware/toolchain-specific. `CMAKE_CUDA_ARCHITECTURES` is configurable; architecture `89` is the locally tested default, not a universal requirement.
- Desktop framebuffer rendering was reviewed at three resolutions. Mouse-driven experiment controls still require a short manual acceptance check; automated UI clicking was unavailable in the audit environment.
- Old SFML source and historical performance artifacts are retained as prior development. The current C++17 v2 pipeline does **not** use the historical SFML gitlinks.
- Historical fonts, binary artifacts, and profiling captures are not part of the v2 build, and their redistribution/provenance must be reviewed before creating a redistributed binary package.

## Remaining work outside the v2 software scope

- Optional media pass: improve screenshots, add GIF/video demo, and polish the visual README presentation.
- Optional formal version tag or packaged binary release after license/provenance review.

**Previous checkpoint:** The November 1 `v2.0-rc1` audit reported 10 CUDA, 7 desktop, and 6 CLI test suites and a fixed `sm_89` architecture. Those counts and configuration were superseded by the November 6 Stage 4/5 workbench verification and configurable CMake CUDA architecture.
