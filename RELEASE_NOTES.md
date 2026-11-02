# Parallel Particle Dynamics Lab v2.0-rc1

`v2.0-rc1` is a release-candidate snapshot of an educational and portfolio-oriented 2D particle simulation workbench. It is suitable for publishing as a clearly labeled engineering prototype after owner review; it is not represented as a research-validated physics package.

## Included

- C++17 sequential CPU reference solver.
- OpenMP multicore solver with thread-local Jacobi contact accumulation.
- Optional CUDA solver and GPU spatial grid for the verified `sm_89` environment.
- Semi-implicit and explicit Euler integration experiments.
- Frictionless elastic and inelastic particle collisions.
- Circular boundary response and discrete penetration correction.
- Interactive GLFW, Dear ImGui, ImPlot, and OpenGL workbench.
- CSV export, plotting scripts, benchmark artifacts, coursework, and automated tests.

## Verification performed November 1, 2026

Environment: Ubuntu 24.04 under WSL2/WSLg; GCC 13.3.0; CMake 3.28.3; OpenMP 4.5; CUDA Toolkit 12.0; NVIDIA GeForce RTX 4060 Laptop GPU (compute capability 8.9).

| Configuration | Result |
|---|---:|
| Existing CUDA-enabled release build | 10/10 CTest suites passed |
| Clean CPU/OpenMP desktop build | 7/7 CTest suites passed |
| Clean CPU/OpenMP CLI-only build | 6/6 CTest suites passed |

Representative checks: core physics 14/14; collision acceptance 7/7; OpenMP correctness 46/46 across 1-16 threads; CUDA correctness 10/10; cross-backend verification 4/4; five non-empty experiment CSV outputs.

These checks verify the tested cases and configurations. They do not establish universal numerical accuracy, physical validation for real materials, or transferable performance results.

## Publication notes

- Existing benchmark data and plots are retained as machine-specific artifacts. Re-run them before quoting a speedup.
- CPU/OpenMP builds are supported with `-DENABLE_CUDA=OFF`.
- The current CUDA CMake path forces architecture 89; other GPU targets are not release-verified.
- The tracked GUI screenshots are genuine 1600x920 application captures.
- During the final WSLg audit, automated screenshot mode produced blank images; those audit-generated blank files were removed.
- Dense simultaneous contacts are expected to differ microscopically between sequential Gauss-Seidel and parallel Jacobi update ordering.

## Release boundary

No source-code defect was found that blocks publication as a prototype. Before publishing, the owner should review the complete diff, decide which internal audit drafts belong in the public repository, and explicitly authorize commit/push/tag actions. No commit, push, merge, tag, or GitHub release was performed during this audit.

See [README.md](README.md) for build and demonstration instructions and [docs/release/PUBLICATION_HANDOFF.md](docs/release/PUBLICATION_HANDOFF.md) for the checklist and career copy.
