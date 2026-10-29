# Locked OBS baseline

- Source: `https://github.com/obsproject/obs-studio.git`
- Release tag: `33.0.0`
- Annotated tag object: `02acc6a4cf08eba4ce2eab9342c5dd305b41e9e5`
- Peeled commit: `72e86655715d9a25d69ce446aa4dd44ba58e10d2`
- Verified with `git ls-remote upstream refs/tags/33.0.0 'refs/tags/33.0.0^{}'` on 2026-10-28.

Recorded direct submodule commits:

| Path | Commit |
| --- | --- |
| `deps/libdshowcapture/src` | `8878638324393815512f802640b0d5ce940161f1` |
| `plugins/obs-browser` | `85c7f2668936679b43329a9ee9f1e269d9bac812` |
| `plugins/obs-websocket` | `e28f84975984340e84fc4879cfc05741cc0afde1` |
| `deps/libdshowcapture/src/external/capture-device-support` | `fe9630974d47f51bf54826e72fb8b654e620aa93` |

Run `git submodule update --init --recursive`, without `--remote`. The root `CMakePresets.json` requires CMake 3.28 or newer and records the 2026-10-23 OBS prebuilt dependency and Qt 6 bundles and CEF 7871. Its Windows x64 preset targets Visual Studio and SDK 10.0.26100; the release notes specify Visual Studio 2026. The macOS preset uses Xcode and a macOS 13.0 deployment target. Keep dependency hashes and platform build directories from this exact commit.

The product branch does not follow upstream `master`. A future upstream upgrade requires a separate reviewed decision and migration assessment.

Film Study pins an additional SQLite 3.46.1 amalgamation in `frontend/filmstudy/third_party/sqlite/`; see its README and [decision 0002](decisions/0002-sqlite-dependency.md). The locked OBS Qt bundle omits Qt6Sql on both target platforms. This does not alter the upstream OBS dependency hashes.
