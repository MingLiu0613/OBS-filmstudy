# Film Study module

Use `docs/filmstudy/PRD.md` chapter 11 and appendices A/B when changing schema, queries, capture identity, or export. SQLite is the editable source of truth; JSON is a frozen export. Preserve Clip, Asset, Location, and Analysis as distinct identities. Add a new numbered migration for every schema change; never edit a migration after it has shipped. Enforce same-Analysis matching and retain every matched analysis ID when grouping clips. Keep file hashing and copying outside database transactions, and associate OBS callbacks with an explicit operation and path.
