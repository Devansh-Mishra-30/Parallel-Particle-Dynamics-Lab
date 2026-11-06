# HPC v2 consolidation — evidence-based release checklist

**Repository:** `Devansh-Mishra-30/cuda-particle-simulation`

**Current integration branch:** `main` after PR #3, merged 2026-11-06
**Release classification:** **PASS WITH LIMITATIONS** for the documented educational/portfolio prototype; no tagged binary release has been issued.

| Check | Status | Evidence / limitation |
| --- | --- | --- |
| Workbench source and tests merged to `main` | **PASS** | [PR #3](https://github.com/Devansh-Mishra-30/cuda-particle-simulation/pull/3) |
| CUDA-enabled local build and regression tests | **PASS** | 14/14 CTest suites; verified RTX 4060/WSL2 |
| GitHub Actions CPU/OpenMP headless | **PASS** | 10/10 CTest suites on `main` |
| GitHub Actions CPU/OpenMP GUI build | **PASS** | 11/11 CTest suites under Xvfb on `main` |
| Test runs leave source checkout unchanged | **PASS** | Build-tree output isolation and CI clean-tree checks |
| Analytical collision and smooth-flight verification | **PASS WITH SCOPE** | Two restitution cases, two gravity cases before first boundary interaction |
| First-order convergence | **PASS WITH SCOPE** | Ballistic four-point timestep refinement; p approximately 1.00 |
| Dense-contact sensitivity | **PASS WITH SCOPE** | One 48-particle scene at four numerical resolutions; not material validation |
| HPC scaling and reproducibility | **PASS WITH SCOPE** | Four cloud sizes, 3-sample medians, mixed algorithms and hardware |
| Native GUI rendering | **PASS WITH SCOPE** | Three resolutions inspected; interactive click automation was unavailable |
| Manual mouse-driven study acceptance | **OWNER CHECK** | Confirm selection controls, both automated study buttons, trajectories, and CSV export |
| Historical SFML gitlink hygiene | **FOLLOW-UP PR** | Remove two broken gitlink pointers without rewriting history; v2 does not depend on them |
| Historical font/binary/profiler provenance | **DOCUMENTED LIMITATION** | Review before binary distribution; existing Git history preserved |
| Updated screenshots / video demo | **DEFERRED** | Separate media pass; not a scientific build blocker |

The old October 16 checklist included stronger statements about universal numerical equivalence, TSan, performance and GUI behavior. Treat that as a historical audit rather than evidence of universal correctness. See the [Stage 4 report](../audit/STAGE_4_VERIFICATION_REPORT.md), [README](../../README.md), and [current release notes](../../RELEASE_NOTES.md).

**Publication conclusion:** The code, evidence, and portable CPU CI constitute a defensible portfolio source release for the explicitly tested scope. No broad physical-validation or cross-hardware performance guarantee is claimed.
