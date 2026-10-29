# Film Study status

Updated: 2026-10-29. Source of truth for scope: [PRD](PRD.md).

| Milestone | Status | Evidence or gap |
| --- | --- | --- |
| M0 upstream baseline | complete locally | OBS 33.0.0 peeled to the required commit; recorded submodules initialized |
| M0 product branch and docs | complete on task branch | `filmstudy/main` and `feat/filmstudy-foundation` are on origin; Draft PR #1 targets the product branch |
| M0 schema contract | partial | Appendix A SQL, native SQLite schema 2 opener and manual capture state API are in the module; 6 local Python checks pass; v1 migration and full services remain |
| M0 JSON format 2 | WIP | Schema and PRD example fixture added; 3 local schema checks pass. Exporter now constructs a frozen manifest in code; native CI and complete cross-field checks remain |
| M0 CI | partial at export WIP head | Run `40832401606` at PR merge SHA `8fa308a6`: data contract and macOS arm64 build plus export smoke passed; Windows x64 build passed but export smoke failed on path separator comparison. A local fix awaits the next CI run. Native app startup, preview playback and real recording remain unverified |
| M0 Windows x64 build | build passed, export smoke failed | Native AMD64 runner confirmed in run `40832401606`; local host has no CMake or Visual Studio. The path containment fix is unvalidated |
| M0 macOS arm64 build | build and export smoke passed | Native arm64 runner confirmed in run `40832401606`; synthetic export passed. Pinned Qt and FFmpeg runtime paths supplied to standalone smoke |
| M1 capture to MP4 | WIP, awaiting native CI | Tools menu entry and OBS recording callbacks are coded; recording hides the Film Study window, then imports the completed MP4 with FFmpeg probing and SHA-256 verification on a worker thread. Native compilation, app startup and real capture are unverified |
| M1 preview, two analyses, query, export | WIP partial flow | Tools menu has ready-only OBS media-source preview, add-analysis, narrow search and export dialogs. Catalog stores independent analyses and same-row filter IDs. Export freezes version 2 JSON plus item identities before verified MP4 copies. Export compiled on both platforms; synthetic smoke passed on macOS arm64 and failed on Windows x64 due path comparison. Full editor, full filters, retry UI and real end-to-end validation remain |
| M2 remaining P0 | not implemented | See PRD section 4 and section 17 |

Build success, application startup, and real screen/audio capture are separate evidence. Neither platform has yet passed any of these product checks. The PRD's 53 database examples are design evidence only; repository tests must be run independently.
