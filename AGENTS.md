# Film Study development

For changes to the Basketball Film Study product, read `docs/filmstudy/PRD.md`, `docs/filmstudy/STATUS.md`, and `docs/filmstudy/DEVELOPMENT.md` before editing. The PRD chapter 11 and appendices A/B define the database and query contract.

Start by checking `git status`, the current branch and tracking ref, then fetch `origin`. Preserve uncommitted work. Use `filmstudy/main` as the integration branch and a separate `feat/filmstudy-*` or `fix/filmstudy-*` branch for each task. Push a task branch and update its handoff under `docs/filmstudy/handoffs/` before changing computers. Never force push or advance the OBS upstream baseline implicitly.

Keep OBS history, licenses, and recorded submodule commits. `origin` is the personal repository; `upstream` is read-only for baseline verification. Keep Film Study logic in `frontend/filmstudy/`; changes to OBS frontend integration should be minimal. Record build, app startup, and real capture separately for Windows x64 and macOS arm64. Do not commit media, catalog databases, build output, credentials, or signing files.

For user-visible behavior, maintain scenarios in `tests/filmstudy/features/`. Run the relevant tests and record actual results in `docs/filmstudy/STATUS.md`; do not infer platform validation from a workflow definition.
