# Film Study status

Updated: 2026-10-29. Scope is defined by [PRD](PRD.md). The current Windows tryout instructions are in [WINDOWS-TRIAL.md](WINDOWS-TRIAL.md).

| Milestone | Status | Evidence or gap |
| --- | --- | --- |
| M0 upstream and Git workflow | complete | OBS 33.0.0 resolves to `72e86655715d9a25d69ce446aa4dd44ba58e10d2`; recorded submodule commits are retained. `filmstudy/main` and `feat/filmstudy-foundation` are on origin; Draft PR #1 targets the product branch. |
| M0 data contract | partial | Schema 2 SQL, native catalog, JSON format 2 schema and query semantics 1 are implemented for the first flow. V1 migration and remaining PRD cases are open. |
| M0 CI | passed for task head `941e9f61c86028489bb4e65dac2481a7c82a62bd` | [Run 40892652246](https://github.com/MingLiu0613/OBS-filmstudy/actions/runs/40892652246) passed data contract, native Windows AMD64 and macOS arm64 OBS builds, catalog smokes and packaged Windows module-registration check. CI tested PR merge SHA `3509f1567c0f2fa1019b5042cf8dfb5086920de3`. |
| M1 Windows application startup | passed locally for packaged build | The unsigned Windows CI ZIP started on this Windows host. Its OBS log listed `filmstudy-ui.dll` under Loaded Modules; Tools → Basketball Film Study opened the catalog panel. The package has no manually copied plugin DLL. |
| M1 Windows record, analyze, filter, export | passed locally on an isolated test copy of the earlier build | A pure-color OBS source recorded a 14-second MP4 (10,897,871 bytes) after setting standard MP4 output. The ready Clip retained its stable ID. Two independent analyses were saved. A cross-analysis combination returned zero results; a same-analysis combination returned one Clip with one matched Analysis ID. Export produced one byte-identical MP4 and JSON v2 with both analyses and only the matching ID. Audio was muted for this nonprivate test. The isolated copy used a temporary side-load workaround for the then-missing module registration; the fixed packaged build above has only been checked for startup and panel opening. |
| M1 preview | partial native check | The test Clip opened in the in-app preview. Initial playback stayed black at 0:00; pressing Restart displayed the color footage and advanced playback. Initial autoplay and the remaining controls need correction and full validation. |
| M1 macOS arm64 | build and catalog smoke passed | Native arm64 CI runner compiled OBS and passed synthetic catalog/export checks. Application startup, screen/audio recording and preview on an Apple Silicon Mac have not been run. |
| M2 remaining P0 | open | Complete analysis editing and full filters/saved filters, replay-buffer flow, migration/recovery paths and the remaining PRD acceptance cases. |

Build success, application startup, screen recording, audio recording and end-to-end workflow are separate evidence. The local synthetic color recording establishes the Windows video path only. No real basketball footage or user database is committed. The PRD's 53 database examples are design evidence; repository and native tests were run separately.
