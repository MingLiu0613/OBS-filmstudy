# Film Study status

Updated: 2026-10-28. Source of truth for scope: [PRD](PRD.md).

| Milestone | Status | Evidence or gap |
| --- | --- | --- |
| M0 upstream baseline | complete locally | OBS 33.0.0 peeled to the required commit; recorded submodules initialized |
| M0 product branch and docs | in progress | `filmstudy/main` exists on origin; PRD, rules, handoff, and decision are on the foundation task branch |
| M0 schema contract | partial | Appendix A SQL is in `frontend/filmstudy/migrations/0002_initial.sql`; 3 local checks pass; application migration runner and v1 upgrade remain |
| M0 CI | configured, not run | Scoped workflow added for Windows x64, macOS arm64, and schema; upstream publication triggers removed on task branch |
| M0 Windows x64 build | not run | This Windows host has no CMake or Visual Studio toolchain detected |
| M0 macOS arm64 build | not run | No macOS host in this session |
| M1 capture to MP4 | not implemented | OBS integration pending |
| M1 preview, two analyses, query, export | not implemented | Product services and UI pending |
| M2 remaining P0 | not implemented | See PRD section 4 and section 17 |

Build success, application startup, and real screen/audio capture are separate evidence. Neither platform has yet passed any of these product checks. The PRD's 53 database examples are design evidence only; repository tests must be run independently.
