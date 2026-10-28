# Film Study status

Updated: 2026-10-28. Source of truth for scope: [PRD](PRD.md).

| Milestone | Status | Evidence or gap |
| --- | --- | --- |
| M0 upstream baseline | complete locally | OBS 33.0.0 peeled to the required commit; recorded submodules initialized |
| M0 product branch and docs | complete on task branch | `filmstudy/main` and `feat/filmstudy-foundation` are on origin; Draft PR #1 targets the product branch |
| M0 schema contract | partial | Appendix A SQL and a native SQLite schema 2 opener are in the module; 3 Python checks pass; C++ build, v1 migration and catalog service tests remain |
| M0 CI | running | Data job passed before generator was added; a CRLF hash fix is pushed. Both native builds found the locked Qt bundle lacks Qt6Sql; the catalog now uses pinned SQLite 3.46.1, awaiting CI |
| M0 Windows x64 build | failed in CI | Native x64 runner confirmed; CMake could not find Qt6Sql in the locked Qt bundle. Local host has no CMake or Visual Studio |
| M0 macOS arm64 build | failed in CI | Native arm64 runner confirmed; first run hit Xcode CAS root error, next CMake run could not find Qt6Sql. Both mitigations await CI |
| M1 capture to MP4 | not implemented | OBS integration pending |
| M1 preview, two analyses, query, export | not implemented | Product services and UI pending |
| M2 remaining P0 | not implemented | See PRD section 4 and section 17 |

Build success, application startup, and real screen/audio capture are separate evidence. Neither platform has yet passed any of these product checks. The PRD's 53 database examples are design evidence only; repository tests must be run independently.
