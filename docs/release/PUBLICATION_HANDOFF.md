# HPC v2 — publication, reproducibility, and career handoff

## Publication state

The scientific workbench was merged into `main` by [PR #3](https://github.com/Devansh-Mishra-30/cuda-particle-simulation/pull/3) on November 6, 2026. The [GitHub Actions run on `main`](https://github.com/Devansh-Mishra-30/cuda-particle-simulation/actions/runs/42302040951) completed successfully: 10 headless CPU/OpenMP tests and 11 GUI-enabled CPU/OpenMP tests. Separately, the CUDA-enabled WSL2/RTX 4060 build passed 14 local CTest suites. No GitHub Release/tag has been created.

The main sources of truth are the [README](../../README.md), [Stage 4 verification report](../audit/STAGE_4_VERIFICATION_REPORT.md), [preserved experiment evidence](../../output/audit/stage4_preserved_2026-11-06_Ku9qXI/), and [release notes](../../RELEASE_NOTES.md).

## Engineering narrative

**Central question:** How do physical assumptions, numerical integration choices, contact resolution, and CPU/OpenMP/CUDA implementation strategies trade accuracy, robustness, and computational cost?

**Portfolio description:** Developed a C++17 numerical particle-dynamics and HPC workbench spanning sequential CPU, OpenMP, and CUDA solvers, with analytical collision/ballistic verification, first-order convergence, dense-contact sensitivity, deterministic performance studies, interactive ImGui diagnostics, automated regression tests, and reproducible CSV evidence.

### Resume-ready technical points

- Developed a modular C++17 2D particle simulator implementing impulse-based frictionless collision response, circular confinement, gravity, and first-order Euler integration, with analytical collision and ballistic verification.
- Built sequential CPU, OpenMP, and CUDA implementations with distinct Gauss–Seidel/Jacobi and broad-phase strategies; characterized implementation-level performance across 128–1024 particles while documenting workload and algorithmic differences.
- Implemented reproducible scientific experiments and CI: convergence-order verification, 48-particle contact sensitivity, exported numerical evidence, 14 local CUDA-enabled test suites, and 10/11 hosted CPU/OpenMP test suites.

Avoid universal fidelity, speedup, numerical equivalence, real-material validation, and performance portability claims.

## Reproduce the released workbench

From a fresh Linux or WSL2 checkout (Bash; repository root):

```bash
cmake -S . -B build_cli -DCMAKE_BUILD_TYPE=Release -DENABLE_CUDA=OFF -DBUILD_LAB_GUI=OFF
cmake --build build_cli -j8
ctest --test-dir build_cli --output-on-failure
```

For GUI support, use `-DBUILD_LAB_GUI=ON` and the documented desktop dependencies. For NVIDIA CUDA hardware, use `-DENABLE_CUDA=ON` and set `-DCMAKE_CUDA_ARCHITECTURES=<compatible architecture>`.

## Final user-operated checks

1. Review live native GUI selection and resize behavior, automated HPC/contact study buttons, post-wall analytical-warning behavior, and CSV export. Native rendering and underlying studies already have automated evidence; click automation is the remaining limitation.
2. Decide when to replace the single embedded screenshot with a richer media sequence. This is presentation work, not prerequisite physics development.
3. Review historical font/binary/profiler licensing and redistribution provenance before shipping a packaged executable. Do not rewrite Git history or remove user-owned local artifacts as part of a routine documentation update.
4. Keep historical exploratory numbers separate from the November 6 Stage 4 measured protocol.

## Local WSL repository hygiene

If the former feature worktree contains uncommitted CSVs, figures, drafts, and one-time migration scripts, **preserve that worktree**. Open a separate clean Git worktree at `origin/main` for new development or release hygiene. Do not run `git clean`, `git reset --hard`, or wholesale staging merely to obtain a clean status. The old artifacts can be archived or curated independently.
