# Film Study status

Updated: 2026-10-29. Source of truth for scope: [PRD](PRD.md).

| Milestone | Status | Evidence or gap |
| --- | --- | --- |
| M0 upstream baseline | complete locally | OBS 33.0.0 peeled to the required commit; recorded submodules initialized |
| M0 product branch and docs | complete on task branch | `filmstudy/main` and `feat/filmstudy-foundation` are on origin; Draft PR #1 targets the product branch |
| M0 schema contract | partial | Appendix A SQL, native SQLite schema 2 opener and manual capture state API are in the module; 5 Python checks pass; C++ smoke, v1 migration and full services remain |
| M0 JSON format 2 | partial | Schema and PRD example fixture added; 3 local schema checks pass; exporter and cross-field integrity checks remain |
| M0 CI | partial | Run `40656776827`: data contract and Windows x64 build/smoke passed. macOS build passed, smoke execution lacked QtCore runtime path; CI step adjusted, awaiting rerun |
| M0 Windows x64 build | build and catalog smoke passed | Native x64 runner confirmed in run `40656776827`; local host has no CMake or Visual Studio |
| M0 macOS arm64 build | build passed, smoke runtime failed | Native arm64 runner confirmed in run `40656776827`; smoke executable linked but dyld could not find bundled QtCore framework |
| M1 capture to MP4 | partial, awaiting native CI | Stable Clip/session/operation IDs and state transitions; completed-output path handling and FFmpeg-probed, SHA-256-verified media copy are coded. OBS event bridge and real capture remain pending |
| M1 preview, two analyses, query, export | not implemented | Product services and UI pending |
| M2 remaining P0 | not implemented | See PRD section 4 and section 17 |

Build success, application startup, and real screen/audio capture are separate evidence. Neither platform has yet passed any of these product checks. The PRD's 53 database examples are design evidence only; repository tests must be run independently.
