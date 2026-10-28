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

## LESSON-20261028-schema-lines: Hash canonical migration bytes across platforms

- Date: 2026-10-28
- Tags: sqlite, windows, ci, line-endings
- Applicable scenarios: Generated source or migration checksums consumed on Windows and Unix hosts.
- Severity: medium
- Source: Film Study CI run `40653155498`, data-contract job.
- Problem: Regenerating `SchemaV2.generated.hpp` on Linux changed only the SQL SHA-256 constant.
- Root cause: Windows checked out the SQL with CRLF, while the Linux runner used LF; the generator hashed raw checkout bytes.
- Fix: Normalize CRLF to LF before hashing and generating the embedded SQL.
- Prevention: Define a canonical byte representation for persisted migration identity and verify generated files on CI after cross-platform checkout.
