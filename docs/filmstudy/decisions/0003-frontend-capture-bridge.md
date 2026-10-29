# Frontend capture bridge

Status: WIP implementation decision, 2026-10-29.

The first Film Study entry point is a small OBS frontend module under `frontend/filmstudy`. It uses the stable OBS frontend recording API and events rather than editing platform capture or encoder implementations. The SQLite catalog and FFmpeg media importer remain separate from UI code.

The user chooses a catalog and game before recording. The start command persists Clip, session, and operation IDs before requesting OBS to record. The Film Study window hides during recording; no Film Study dialog appears in event callbacks. After OBS signals stopped, the bridge retrieves the explicit final recording path and rejects an unchanged path. It probes and copies MP4 outside a database transaction, then publishes the immutable Asset and Location in one transaction. A failed import retains the operation and source path for recovery.

OBS's ordinary recording hotkey or button can stop the recording. The module currently requires the user to configure OBS's recording format as MP4 before Film Study capture. Native compilation, application startup, and a real capture must be checked separately. A recovery retry UI and automatic stale-operation scan remain to implement.
