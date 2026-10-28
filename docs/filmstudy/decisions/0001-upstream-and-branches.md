# 0001: OBS baseline and branch topology

Date: 2026-10-28

Use OBS Studio `33.0.0` at commit `72e86655715d9a25d69ce446aa4dd44ba58e10d2`. Its tag resolves to that exact commit and its original Git history and licenses remain intact. The personal repository is `origin`; official OBS is `upstream` for verification only. Integrate platform work into `filmstudy/main` through task branches. Keep submodules at recorded commits.

This avoids platform forks and allows a Windows or Mac agent to resume from pushed task commits. Any upstream upgrade requires a new decision with dependency, migration, and validation impact.
