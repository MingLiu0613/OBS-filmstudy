# Film Study status

Updated: 2026-10-29. Source of truth for scope: [PRD](PRD.md).

| Milestone | Status | Evidence or gap |
| --- | --- | --- |
| M0 upstream baseline | complete locally | OBS 33.0.0 peeled to the required commit; recorded submodules initialized |
| M0 product branch and docs | complete on task branch | `filmstudy/main` and `feat/filmstudy-foundation` are on origin; Draft PR #1 targets the product branch |
| M0 schema contract | partial | Appendix A SQL, native SQLite schema 2 opener and manual capture state API are in the module; 5 Python checks pass; C++ smoke, v1 migration and full services remain |
| M0 JSON format 2 | WIP | Schema and PRD example fixture added; 3 local schema checks pass. Exporter now constructs a frozen manifest in code; native CI and complete cross-field checks remain |
| M0 CI | passed at current pushed head | Run `40805102636` at `ab0e1d102`: data contract, Windows x64 and macOS arm64 full builds and catalog smokes passed. Native app startup, preview playback and real recording remain unverified |
| M0 Windows x64 build | build and catalog smoke passed | Native x64 runner confirmed in run `40805102636`; local host has no CMake or Visual Studio |
| M0 macOS arm64 build | build and catalog smoke passed | Native arm64 runner confirmed in run `40805102636`; pinned Qt and FFmpeg runtime paths supplied to standalone smoke |
| M1 capture to MP4 | WIP, awaiting native CI | Tools menu entry and OBS recording callbacks are coded; recording hides the Film Study window, then imports the completed MP4 with FFmpeg probing and SHA-256 verification on a worker thread. Native compilation, app startup and real capture are unverified |
| M1 preview, two analyses, query, export | WIP partial flow | Tools menu has ready-only OBS media-source preview, add-analysis, narrow search and export dialogs. Catalog stores independent analyses and same-row filter IDs. Export freezes version 2 JSON plus item identities before verified MP4 copies. This export code is not yet natively compiled or run; full editor, full filters, retry UI and real end-to-end validation remain |
| M2 remaining P0 | not implemented | See PRD section 4 and section 17 |

Build success, application startup, and real screen/audio capture are separate evidence. Neither platform has yet passed any of these product checks. The PRD's 53 database examples are design evidence only; repository tests must be run independently.
