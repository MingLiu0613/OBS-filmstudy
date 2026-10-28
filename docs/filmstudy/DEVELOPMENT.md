# Development and computer handoff

## Branches and remotes

`origin` is `MingLiu0613/OBS-filmstudy`; `upstream` is `obsproject/obs-studio`. `filmstudy/main` starts at the locked OBS commit. Use task branches (`feat/filmstudy-*`, `fix/filmstudy-*`) for both operating systems. Open Draft PRs against `filmstudy/main`; mark ready only after the stated checks pass. Do not force push, rewrite shared history, or merge without the project's review decision.

## Start on either computer

1. Inspect `git status --short --branch`, `git remote -v`, and `git branch -vv`. Preserve any local work.
2. `git fetch origin --prune`. Inspect the tracking branch with `git log --oneline --left-right HEAD...@{upstream}`. If clean and only behind, `git merge --ff-only @{upstream}`; inspect divergence before resolving it.
3. Read the PRD, STATUS, and the task handoff. Run `git submodule update --init --recursive` at recorded commits.
4. Check the native build toolchain. Keep `build_x64` and `build_macos` local to each platform; never copy CMake caches across computers.

## Build and validation

Use the locked root `CMakePresets.json` and upstream build scripts as the source of tool versions. Windows CI uses `.github/scripts/Build-Windows.ps1 -Target x64`; macOS CI uses `.github/scripts/build-macos --config RelWithDebInfo --target macos-arm64`. Local builds may use matching `cmake --preset windows-x64` / `cmake --preset macos` and `cmake --build` commands after dependencies are installed. Run `python tests/filmstudy/test_schema.py` for the data contract. Build and run `filmstudy-catalog-smoke` to verify catalog creation, single-writer locking and reopening on each native runner. The macOS CI preset disables Xcode's compilation cache after a runner CAS root failure; this changes no release tool version. Record build, app startup, and real capture as independent results. CI must print `runner.os` and native CPU architecture.

## Finish or switch computers

Commit code, migrations, tests, documentation, and the task handoff together; label unfinished commits WIP. Push the task branch, verify the remote ref and PR, and record commit SHA, checks, failures, and next step in `docs/filmstudy/handoffs/`. A local stash or untracked file is not a handoff. If push fails, name the local-only commits in STATUS and do not claim synchronization.

Catalog databases and WAL/SHM files, match footage, export packages, build caches, credentials, and signing material remain outside Git. Keep local machine paths in untracked local configuration.
