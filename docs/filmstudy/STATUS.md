# Film Study status

Updated: 2026-10-29. Source of truth for scope: [PRD](PRD.md).

| Milestone | Status | Evidence or gap |
| --- | --- | --- |
| M0 upstream baseline | complete locally | OBS 33.0.0 peeled to the required commit; recorded submodules initialized |
| M0 product branch and docs | complete on task branch | `filmstudy/main` and `feat/filmstudy-foundation` are on origin; Draft PR #1 targets the product branch |
| M0 schema contract | partial | Appendix A SQL, native SQLite schema 2 opener and manual capture state API are in the module; 6 local Python checks pass; v1 migration and full services remain |
| M0 JSON format 2 | partial | Schema and PRD fixture added; 4 local checks pass with an example supplied as the generated manifest. Both native CI jobs validated an actual synthetic export against Schema v2 and basic matched-ID consistency. Real MP4 and full cross-field checks remain |
| M0 CI | passed at `cd3e383ef` | Run `40836138489` at PR merge SHA `c8312ce2`: data contract and both native OBS builds, synthetic export smokes, saved-clip listing and generated-manifest JSON Schema checks passed. Native app startup, preview playback and real recording remain unverified |
| M0 Windows x64 build | build and catalog smoke passed | Native AMD64 runner confirmed in run `40836138489`; local host has no CMake or Visual Studio |
| M0 macOS arm64 build | build and catalog smoke passed | Native arm64 runner confirmed in run `40836138489`; pinned Qt and FFmpeg runtime paths supplied to standalone smoke |
| M1 capture to MP4 | WIP, native compilation passed | Tools menu entry and OBS recording callbacks are coded; recording hides the Film Study window, then imports the completed MP4 with FFmpeg probing and SHA-256 verification on a worker thread. App startup and real capture are unverified |
| M1 preview, two analyses, query, export | WIP partial flow | Tools menu has ready-only OBS media-source preview, saved-clip selector, add-analysis, narrow search and export dialogs. Catalog stores independent analyses and same-row filter IDs. Export freezes version 2 JSON plus item identities before verified MP4 copies. Synthetic export and persisted saved-clip listing passed on both native platforms at `cd3e383ef`. Full editor, full filters, retry UI and real end-to-end validation remain |
| M2 remaining P0 | not implemented | See PRD section 4 and section 17 |

Build success, application startup, and real screen/audio capture are separate evidence. Both platform builds passed; neither platform has an application startup, preview playback, or real recording pass. The PRD's 53 database examples are design evidence only; repository tests were run independently.
