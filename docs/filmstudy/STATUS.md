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
| M1 capture to MP4 | WIP, awaiting native CI | Tools menu entry and OBS recording callbacks are coded; recording hides the Film Study window, then imports the completed MP4 with FFmpeg probing and SHA-256 verification on a worker thread. Native compilation, app startup and real capture are unverified |
| M1 preview, two analyses, query, export | WIP partial flow | Tools menu has add-analysis and narrow search dialogs; catalog stores independent drafts/completed analyses and same-row filter preserves matching IDs. Native compilation and behavior are unverified. Preview, full editor, full filters and frozen export pending |
| M2 remaining P0 | not implemented | See PRD section 4 and section 17 |

Build success, application startup, and real screen/audio capture are separate evidence. Neither platform has yet passed any of these product checks. The PRD's 53 database examples are design evidence only; repository tests must be run independently.
