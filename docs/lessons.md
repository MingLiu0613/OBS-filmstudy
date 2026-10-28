# Development lessons

## LESSON-20261028-obs-tag: Preserve the upstream annotated tag in a new personal remote

- Date: 2026-10-28
- Tags: git, obs, ci, baseline
- Applicable scenarios: First push of an OBS product branch to an empty remote.
- Severity: medium
- Source: Film Study CI run `40651941002`, first attempt.
- Problem: Windows and macOS CMake configuration could not derive the expected version after checking out the new personal repository.
- Root cause: The OBS `33.0.0` annotated tag existed in the local upstream clone but was absent from `origin`; Actions fetched the branch with full history but could not see the tag.
- Fix: Push the verified annotated `33.0.0` tag to `origin`, then rerun the failed jobs.
- Prevention: Before enabling full source builds in a new OBS fork, verify that the exact upstream release tag is present on the personal remote. Preserve its original object and peeled commit; do not synthesize a replacement tag.
