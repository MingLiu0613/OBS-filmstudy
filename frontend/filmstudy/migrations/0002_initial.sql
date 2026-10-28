-- Basketball Film Study OBS schema 2
-- Connection setup must run before migration BEGIN.
PRAGMA foreign_keys = ON;
-- Production migration runner owns BEGIN IMMEDIATE, COMMIT and rollback.

CREATE TABLE catalog_info (
  singleton INTEGER NOT NULL PRIMARY KEY CHECK(singleton = 1),
  catalog_id TEXT NOT NULL UNIQUE,
  normalization_version INTEGER NOT NULL DEFAULT 1 CHECK(normalization_version >= 1),
  min_reader_schema INTEGER NOT NULL DEFAULT 2 CHECK(min_reader_schema >= 1),
  content_revision INTEGER NOT NULL DEFAULT 0 CHECK(content_revision >= 0),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
) STRICT;

CREATE TABLE schema_migrations (
  version INTEGER NOT NULL PRIMARY KEY CHECK(version >= 1),
  migration_name TEXT NOT NULL,
  script_sha256 TEXT NOT NULL CHECK(length(script_sha256) = 64 AND script_sha256 NOT GLOB '*[^0-9a-f]*'),
  applied_at TEXT NOT NULL,
  application_version TEXT NOT NULL
) STRICT;

CREATE TABLE games (
  id TEXT NOT NULL PRIMARY KEY,
  name TEXT NOT NULL CHECK(length(trim(name)) > 0),
  game_date TEXT,
  season TEXT,
  competition TEXT,
  home_team_name TEXT,
  away_team_name TEXT,
  default_source_name TEXT,
  default_source_url TEXT,
  default_source_note TEXT NOT NULL DEFAULT '',
  is_archived INTEGER NOT NULL DEFAULT 0 CHECK(is_archived IN (0,1)),
  revision INTEGER NOT NULL DEFAULT 1 CHECK(revision >= 1),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
) STRICT;

CREATE TABLE players (
  id TEXT NOT NULL PRIMARY KEY,
  name TEXT NOT NULL CHECK(length(trim(name)) > 0),
  name_key TEXT NOT NULL,
  identity_note TEXT NOT NULL DEFAULT '',
  external_provider TEXT,
  external_id TEXT,
  is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0,1)),
  revision INTEGER NOT NULL DEFAULT 1 CHECK(revision >= 1),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  CHECK((external_provider IS NULL AND external_id IS NULL) OR
        (external_provider IS NOT NULL AND external_id IS NOT NULL)),
  UNIQUE(external_provider, external_id)
) STRICT;

CREATE TABLE player_aliases (
  id TEXT NOT NULL PRIMARY KEY,
  player_id TEXT NOT NULL REFERENCES players(id) ON DELETE RESTRICT,
  alias TEXT NOT NULL CHECK(length(trim(alias)) > 0),
  alias_key TEXT NOT NULL,
  created_at TEXT NOT NULL,
  UNIQUE(player_id, alias_key)
) STRICT;

CREATE TABLE topics (
  id TEXT NOT NULL PRIMARY KEY,
  name TEXT NOT NULL CHECK(length(trim(name)) > 0),
  name_key TEXT NOT NULL UNIQUE,
  definition TEXT NOT NULL DEFAULT '',
  is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0,1)),
  revision INTEGER NOT NULL DEFAULT 1 CHECK(revision >= 1),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
) STRICT;

CREATE TABLE result_types (
  id TEXT NOT NULL PRIMARY KEY,
  name TEXT NOT NULL CHECK(length(trim(name)) > 0),
  name_key TEXT NOT NULL UNIQUE,
  definition TEXT NOT NULL DEFAULT '',
  is_active INTEGER NOT NULL DEFAULT 1 CHECK(is_active IN (0,1)),
  revision INTEGER NOT NULL DEFAULT 1 CHECK(revision >= 1),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
) STRICT;

CREATE TABLE clips (
  id TEXT NOT NULL PRIMARY KEY,
  game_id TEXT NOT NULL REFERENCES games(id) ON DELETE RESTRICT,
  title TEXT NOT NULL DEFAULT '',
  capture_mode TEXT NOT NULL CHECK(capture_mode IN ('manual','replay')),
  capture_state TEXT NOT NULL CHECK(capture_state IN
    ('created','starting','recording','saving','processing','ready',
     'failed','recovery_required','cancelled')),
  primary_asset_id TEXT,
  source_name TEXT,
  source_url TEXT,
  source_note TEXT NOT NULL DEFAULT '',
  period_code TEXT,
  game_clock_ms INTEGER CHECK(game_clock_ms IS NULL OR game_clock_ms >= 0),
  revision INTEGER NOT NULL DEFAULT 1 CHECK(revision >= 1),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  deleted_at TEXT,
  CHECK(capture_state <> 'ready' OR primary_asset_id IS NOT NULL),
  FOREIGN KEY(primary_asset_id, id) REFERENCES media_assets(id, clip_id)
    ON DELETE RESTRICT DEFERRABLE INITIALLY DEFERRED
) STRICT;

CREATE TABLE media_assets (
  id TEXT NOT NULL PRIMARY KEY,
  clip_id TEXT NOT NULL REFERENCES clips(id) ON DELETE RESTRICT,
  asset_kind TEXT NOT NULL CHECK(asset_kind IN ('original','proxy','excerpt')),
  parent_asset_id TEXT,
  parent_start_ms INTEGER,
  parent_end_ms INTEGER,
  container TEXT NOT NULL CHECK(length(container) > 0),
  video_codec TEXT NOT NULL CHECK(length(video_codec) > 0),
  audio_codec TEXT,
  width INTEGER NOT NULL CHECK(width > 0),
  height INTEGER NOT NULL CHECK(height > 0),
  fps_num INTEGER NOT NULL CHECK(fps_num > 0),
  fps_den INTEGER NOT NULL CHECK(fps_den > 0),
  duration_ms INTEGER NOT NULL CHECK(duration_ms > 0),
  size_bytes INTEGER NOT NULL CHECK(size_bytes > 0),
  sha256 TEXT NOT NULL CHECK(length(sha256) = 64 AND sha256 NOT GLOB '*[^0-9a-f]*'),
  verification_state TEXT NOT NULL DEFAULT 'verified'
    CHECK(verification_state IN ('verified','quarantined')),
  created_at TEXT NOT NULL,
  UNIQUE(id, clip_id),
  FOREIGN KEY(parent_asset_id, clip_id) REFERENCES media_assets(id, clip_id) ON DELETE RESTRICT,
  CHECK((asset_kind = 'original' AND parent_asset_id IS NULL AND container = 'mp4') OR
        (asset_kind IN ('proxy','excerpt') AND parent_asset_id IS NOT NULL)),
  CHECK((asset_kind IN ('original','proxy') AND parent_start_ms IS NULL AND parent_end_ms IS NULL) OR
        (asset_kind = 'excerpt' AND parent_start_ms IS NOT NULL AND parent_end_ms IS NOT NULL
         AND parent_start_ms >= 0 AND parent_end_ms > parent_start_ms))
) STRICT;

CREATE TABLE media_locations (
  id TEXT NOT NULL PRIMARY KEY,
  asset_id TEXT NOT NULL REFERENCES media_assets(id) ON DELETE RESTRICT,
  relative_path TEXT NOT NULL CHECK(length(relative_path) > 0 AND
    substr(relative_path,1,1) <> '/' AND instr(relative_path,char(92)) = 0),
  path_key TEXT NOT NULL UNIQUE,
  availability TEXT NOT NULL DEFAULT 'unverified'
    CHECK(availability IN ('unverified','available','missing','invalid')),
  is_preferred INTEGER NOT NULL DEFAULT 0 CHECK(is_preferred IN (0,1)),
  last_verified_at TEXT,
  observed_size_bytes INTEGER CHECK(observed_size_bytes IS NULL OR observed_size_bytes >= 0),
  observed_mtime_ns INTEGER CHECK(observed_mtime_ns IS NULL OR observed_mtime_ns >= 0),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  UNIQUE(id, asset_id)
) STRICT;

CREATE TABLE analyses (
  id TEXT NOT NULL PRIMARY KEY,
  clip_id TEXT NOT NULL REFERENCES clips(id) ON DELETE RESTRICT,
  reference_asset_id TEXT,
  status TEXT NOT NULL DEFAULT 'draft' CHECK(status IN ('draft','complete')),
  perspective TEXT CHECK(perspective IN ('offense','defense')),
  topic_id TEXT REFERENCES topics(id) ON DELETE RESTRICT,
  assessment TEXT CHECK(assessment IN ('positive','negative')),
  result_id TEXT REFERENCES result_types(id) ON DELETE RESTRICT,
  range_start_ms INTEGER,
  range_end_ms INTEGER,
  note TEXT NOT NULL DEFAULT '',
  revision INTEGER NOT NULL DEFAULT 1 CHECK(revision >= 1),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  deleted_at TEXT,
  UNIQUE(id, clip_id),
  FOREIGN KEY(reference_asset_id, clip_id) REFERENCES media_assets(id, clip_id) ON DELETE RESTRICT,
  CHECK(status = 'draft' OR
    (perspective IS NOT NULL AND topic_id IS NOT NULL AND assessment IS NOT NULL)),
  CHECK((range_start_ms IS NULL AND range_end_ms IS NULL) OR
    (range_start_ms IS NOT NULL AND range_end_ms IS NOT NULL
     AND reference_asset_id IS NOT NULL
     AND range_start_ms >= 0 AND range_end_ms > range_start_ms))
) STRICT;

CREATE TABLE analysis_players (
  analysis_id TEXT NOT NULL REFERENCES analyses(id) ON DELETE RESTRICT,
  player_id TEXT NOT NULL REFERENCES players(id) ON DELETE RESTRICT,
  role TEXT NOT NULL DEFAULT 'focus' CHECK(role IN ('focus','context')),
  created_at TEXT NOT NULL,
  PRIMARY KEY(analysis_id, player_id)
) STRICT;

CREATE TABLE capture_sessions (
  id TEXT NOT NULL PRIMARY KEY,
  game_id TEXT NOT NULL REFERENCES games(id) ON DELETE RESTRICT,
  mode TEXT NOT NULL CHECK(mode IN ('manual','replay')),
  state TEXT NOT NULL CHECK(state IN
    ('starting','active','stopping','stopped','failed','cancelled','recovery_required')),
  source_snapshot_json TEXT NOT NULL CHECK(json_valid(source_snapshot_json) AND
    json_type(source_snapshot_json) = 'object'),
  snapshot_version INTEGER NOT NULL DEFAULT 1 CHECK(snapshot_version >= 1),
  requested_buffer_ms INTEGER CHECK(requested_buffer_ms IS NULL OR requested_buffer_ms > 0),
  started_at TEXT,
  stopped_at TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  CHECK(mode = 'replay' OR requested_buffer_ms IS NULL)
) STRICT;

CREATE TABLE capture_operations (
  id TEXT NOT NULL PRIMARY KEY,
  session_id TEXT NOT NULL REFERENCES capture_sessions(id) ON DELETE RESTRICT,
  clip_id TEXT NOT NULL UNIQUE REFERENCES clips(id) ON DELETE RESTRICT,
  phase TEXT NOT NULL CHECK(phase IN
    ('created','starting','recording','saving','processing','ready',
     'failed','recovery_required','cancelled')),
  attempt_count INTEGER NOT NULL DEFAULT 1 CHECK(attempt_count >= 1),
  raw_relpath TEXT,
  intended_final_relpath TEXT NOT NULL,
  actual_output_relpath TEXT,
  final_asset_id TEXT,
  error_code TEXT,
  error_message TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  UNIQUE(id, session_id),
  FOREIGN KEY(final_asset_id, clip_id) REFERENCES media_assets(id, clip_id) ON DELETE RESTRICT
) STRICT;

CREATE TABLE operation_events (
  id TEXT NOT NULL PRIMARY KEY,
  session_id TEXT NOT NULL REFERENCES capture_sessions(id) ON DELETE RESTRICT,
  operation_id TEXT,
  dedupe_key TEXT NOT NULL,
  event_type TEXT NOT NULL CHECK(length(event_type) > 0),
  attempt_no INTEGER CHECK(attempt_no IS NULL OR attempt_no >= 1),
  payload_json TEXT NOT NULL DEFAULT '{}' CHECK(json_valid(payload_json) AND
    json_type(payload_json) = 'object'),
  payload_version INTEGER NOT NULL DEFAULT 1 CHECK(payload_version >= 1),
  created_at TEXT NOT NULL,
  UNIQUE(session_id, dedupe_key),
  FOREIGN KEY(operation_id, session_id) REFERENCES capture_operations(id, session_id) ON DELETE RESTRICT
) STRICT;

CREATE TABLE saved_filters (
  id TEXT NOT NULL PRIMARY KEY,
  name TEXT NOT NULL CHECK(length(trim(name)) > 0),
  query_json TEXT NOT NULL CHECK(json_valid(query_json) AND json_type(query_json) = 'object'),
  query_version INTEGER NOT NULL DEFAULT 1 CHECK(query_version >= 1),
  revision INTEGER NOT NULL DEFAULT 1 CHECK(revision >= 1),
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  deleted_at TEXT
) STRICT;

CREATE TABLE export_jobs (
  id TEXT NOT NULL PRIMARY KEY,
  catalog_id TEXT NOT NULL REFERENCES catalog_info(catalog_id) ON DELETE RESTRICT,
  state TEXT NOT NULL CHECK(state IN
    ('preparing','copying','publishing','succeeded','failed','cancelled')),
  destination_root TEXT NOT NULL,
  package_name TEXT NOT NULL,
  staging_name TEXT NOT NULL,
  query_json TEXT NOT NULL CHECK(json_valid(query_json) AND json_type(query_json) = 'object'),
  query_version INTEGER NOT NULL DEFAULT 1 CHECK(query_version >= 1),
  metadata_schema_version INTEGER NOT NULL DEFAULT 2 CHECK(metadata_schema_version >= 1),
  snapshot_catalog_revision INTEGER NOT NULL CHECK(snapshot_catalog_revision >= 0),
  manifest_snapshot_json TEXT CHECK(manifest_snapshot_json IS NULL OR
    (json_valid(manifest_snapshot_json) AND json_type(manifest_snapshot_json) = 'object')),
  snapshot_frozen_at TEXT,
  clip_count INTEGER NOT NULL DEFAULT 0 CHECK(clip_count >= 0),
  analysis_count INTEGER NOT NULL DEFAULT 0 CHECK(analysis_count >= 0),
  manifest_sha256 TEXT CHECK(manifest_sha256 IS NULL OR
    (length(manifest_sha256) = 64 AND manifest_sha256 NOT GLOB '*[^0-9a-f]*')),
  error_code TEXT,
  error_message TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  completed_at TEXT,
  CHECK(snapshot_frozen_at IS NULL OR
    (manifest_snapshot_json IS NOT NULL AND clip_count > 0)),
  CHECK(state NOT IN ('copying','publishing','succeeded') OR snapshot_frozen_at IS NOT NULL),
  CHECK(state <> 'succeeded' OR
    (completed_at IS NOT NULL AND manifest_sha256 IS NOT NULL))
) STRICT;

CREATE TABLE export_items (
  export_id TEXT NOT NULL REFERENCES export_jobs(id) ON DELETE RESTRICT,
  clip_id TEXT NOT NULL REFERENCES clips(id) ON DELETE RESTRICT,
  asset_id TEXT NOT NULL,
  source_location_id TEXT NOT NULL,
  snapshot_clip_revision INTEGER NOT NULL CHECK(snapshot_clip_revision >= 1),
  position INTEGER NOT NULL CHECK(position >= 0),
  destination_relpath TEXT NOT NULL,
  expected_size_bytes INTEGER NOT NULL CHECK(expected_size_bytes > 0),
  expected_sha256 TEXT NOT NULL CHECK(length(expected_sha256) = 64 AND
    expected_sha256 NOT GLOB '*[^0-9a-f]*'),
  copy_state TEXT NOT NULL DEFAULT 'pending'
    CHECK(copy_state IN ('pending','copying','verified','failed','cancelled')),
  bytes_copied INTEGER NOT NULL DEFAULT 0 CHECK(bytes_copied >= 0),
  copied_at TEXT,
  error_code TEXT,
  PRIMARY KEY(export_id, clip_id),
  UNIQUE(export_id, position),
  UNIQUE(export_id, destination_relpath),
  FOREIGN KEY(asset_id, clip_id) REFERENCES media_assets(id, clip_id) ON DELETE RESTRICT,
  FOREIGN KEY(source_location_id, asset_id) REFERENCES media_locations(id, asset_id) ON DELETE RESTRICT,
  CHECK(bytes_copied <= expected_size_bytes),
  CHECK(copy_state <> 'verified' OR
    (bytes_copied = expected_size_bytes AND copied_at IS NOT NULL))
) STRICT;

CREATE TABLE export_item_matches (
  export_id TEXT NOT NULL,
  clip_id TEXT NOT NULL,
  analysis_id TEXT NOT NULL,
  snapshot_analysis_revision INTEGER NOT NULL CHECK(snapshot_analysis_revision >= 1),
  PRIMARY KEY(export_id, clip_id, analysis_id),
  FOREIGN KEY(export_id, clip_id) REFERENCES export_items(export_id, clip_id) ON DELETE RESTRICT,
  FOREIGN KEY(analysis_id, clip_id) REFERENCES analyses(id, clip_id) ON DELETE RESTRICT
) STRICT;

CREATE UNIQUE INDEX ux_original_asset ON media_assets(clip_id) WHERE asset_kind = 'original';
CREATE UNIQUE INDEX ux_preferred_location ON media_locations(asset_id) WHERE is_preferred = 1;
CREATE UNIQUE INDEX ux_active_capture_session ON capture_sessions((1))
  WHERE state IN ('starting','active','stopping');
CREATE UNIQUE INDEX ux_busy_capture_operation ON capture_operations((1))
  WHERE phase IN ('created','starting','recording','saving','processing');
CREATE INDEX ix_games_date ON games(game_date, id);
CREATE INDEX ix_players_name ON players(name_key);
CREATE INDEX ix_player_alias_key ON player_aliases(alias_key, player_id);
CREATE INDEX ix_clips_recent ON clips(created_at DESC, id) WHERE deleted_at IS NULL;
CREATE INDEX ix_clips_game ON clips(game_id, created_at DESC, id) WHERE deleted_at IS NULL;
CREATE INDEX ix_clips_primary_asset ON clips(primary_asset_id);
CREATE INDEX ix_assets_clip ON media_assets(clip_id);
CREATE INDEX ix_assets_parent ON media_assets(parent_asset_id, clip_id);
CREATE INDEX ix_assets_sha256 ON media_assets(sha256);
CREATE INDEX ix_locations_asset ON media_locations(asset_id, availability);
CREATE INDEX ix_analyses_clip ON analyses(clip_id, created_at, id) WHERE deleted_at IS NULL;
CREATE INDEX ix_analysis_topic ON analyses(topic_id, perspective, assessment, clip_id)
  WHERE status = 'complete' AND deleted_at IS NULL;
CREATE INDEX ix_analysis_result ON analyses(result_id, clip_id)
  WHERE status = 'complete' AND deleted_at IS NULL AND result_id IS NOT NULL;
CREATE INDEX ix_analysis_reference ON analyses(reference_asset_id, clip_id);
CREATE INDEX ix_players_analysis ON analysis_players(player_id, analysis_id);
CREATE INDEX ix_sessions_game ON capture_sessions(game_id, created_at);
CREATE INDEX ix_operations_session ON capture_operations(session_id, created_at);
CREATE INDEX ix_operations_asset ON capture_operations(final_asset_id, clip_id);
CREATE INDEX ix_operations_recovery ON capture_operations(phase, updated_at);
CREATE INDEX ix_events_operation ON operation_events(operation_id, created_at);
CREATE INDEX ix_filters_recent ON saved_filters(updated_at DESC) WHERE deleted_at IS NULL;
CREATE INDEX ix_export_state ON export_jobs(state, created_at);
CREATE INDEX ix_export_catalog ON export_jobs(catalog_id);
CREATE INDEX ix_export_items_clip ON export_items(clip_id, export_id);
CREATE INDEX ix_export_items_asset ON export_items(asset_id, clip_id);
CREATE INDEX ix_export_items_location ON export_items(source_location_id, asset_id);
CREATE INDEX ix_export_matches_analysis ON export_item_matches(analysis_id, clip_id);

CREATE TRIGGER tr_primary_asset_immutable
BEFORE UPDATE OF primary_asset_id ON clips
WHEN OLD.primary_asset_id IS NOT NULL AND NEW.primary_asset_id IS NOT OLD.primary_asset_id
BEGIN SELECT RAISE(ABORT, 'primary asset identity is immutable'); END;

CREATE TRIGGER tr_clip_ready_insert
BEFORE INSERT ON clips WHEN NEW.capture_state = 'ready'
BEGIN
  SELECT CASE WHEN NOT EXISTS(
    SELECT 1 FROM media_assets m JOIN media_locations l ON l.asset_id = m.id
    WHERE m.id = NEW.primary_asset_id AND m.clip_id = NEW.id
      AND m.asset_kind = 'original' AND m.verification_state = 'verified'
      AND l.is_preferred = 1 AND l.availability = 'available'
  ) THEN RAISE(ABORT, 'ready clip requires verified original and available preferred location') END;
  SELECT CASE WHEN EXISTS(SELECT 1 FROM analyses a WHERE a.clip_id = NEW.id
    AND a.deleted_at IS NULL AND a.reference_asset_id IS NULL)
    THEN RAISE(ABORT, 'bind legacy analyses before publishing clip') END;
END;

CREATE TRIGGER tr_clip_ready_update
BEFORE UPDATE OF capture_state, primary_asset_id ON clips WHEN NEW.capture_state = 'ready'
BEGIN
  SELECT CASE WHEN NOT EXISTS(
    SELECT 1 FROM media_assets m JOIN media_locations l ON l.asset_id = m.id
    WHERE m.id = NEW.primary_asset_id AND m.clip_id = NEW.id
      AND m.asset_kind = 'original' AND m.verification_state = 'verified'
      AND l.is_preferred = 1 AND l.availability = 'available'
  ) THEN RAISE(ABORT, 'ready clip requires verified original and available preferred location') END;
  SELECT CASE WHEN EXISTS(SELECT 1 FROM analyses a WHERE a.clip_id = NEW.id
    AND a.deleted_at IS NULL AND a.reference_asset_id IS NULL)
    THEN RAISE(ABORT, 'bind legacy analyses before publishing clip') END;
END;

CREATE TRIGGER tr_asset_bytes_immutable
BEFORE UPDATE OF clip_id, asset_kind, parent_asset_id, parent_start_ms, parent_end_ms,
  container, video_codec, audio_codec, width, height, fps_num, fps_den,
  duration_ms, size_bytes, sha256 ON media_assets
BEGIN SELECT RAISE(ABORT, 'media asset content is immutable; create a derivative asset'); END;

CREATE TRIGGER tr_asset_parent_range
BEFORE INSERT ON media_assets WHEN NEW.parent_asset_id IS NOT NULL
BEGIN
  SELECT CASE WHEN NOT EXISTS(SELECT 1 FROM media_assets p
    WHERE p.id = NEW.parent_asset_id AND p.clip_id = NEW.clip_id
      AND (NEW.parent_end_ms IS NULL OR NEW.parent_end_ms <= p.duration_ms))
    THEN RAISE(ABORT, 'invalid derivative parent or range') END;
END;

CREATE TRIGGER tr_location_asset_immutable
BEFORE UPDATE OF asset_id ON media_locations WHEN NEW.asset_id IS NOT OLD.asset_id
BEGIN SELECT RAISE(ABORT, 'location asset identity is immutable'); END;

CREATE TRIGGER tr_analysis_range_insert
BEFORE INSERT ON analyses
BEGIN
  SELECT CASE WHEN NOT EXISTS(
    SELECT 1 FROM clips c LEFT JOIN media_assets m ON m.id = NEW.reference_asset_id
    WHERE c.id = NEW.clip_id AND c.deleted_at IS NULL AND (
      (NEW.reference_asset_id IS NULL AND c.primary_asset_id IS NULL
        AND c.capture_state = 'recovery_required'
        AND NEW.range_start_ms IS NULL AND NEW.range_end_ms IS NULL)
      OR (NEW.reference_asset_id IS NOT NULL AND c.primary_asset_id = m.id
        AND m.clip_id = c.id AND m.verification_state = 'verified'
        AND c.capture_state = 'ready'
        AND (NEW.range_end_ms IS NULL OR NEW.range_end_ms <= m.duration_ms)))
  ) THEN RAISE(ABORT, 'analysis requires same clip original and valid range') END;
END;

CREATE TRIGGER tr_analysis_range_update
BEFORE UPDATE OF clip_id, reference_asset_id, range_start_ms, range_end_ms ON analyses
BEGIN
  SELECT CASE WHEN NOT EXISTS(
    SELECT 1 FROM clips c LEFT JOIN media_assets m ON m.id = NEW.reference_asset_id
    WHERE c.id = NEW.clip_id AND c.deleted_at IS NULL AND (
      (NEW.reference_asset_id IS NULL AND c.primary_asset_id IS NULL
        AND c.capture_state = 'recovery_required'
        AND NEW.range_start_ms IS NULL AND NEW.range_end_ms IS NULL)
      OR (NEW.reference_asset_id IS NOT NULL AND c.primary_asset_id = m.id
        AND m.clip_id = c.id AND m.verification_state = 'verified'
        AND c.capture_state IN ('processing','ready')
        AND (NEW.range_end_ms IS NULL OR NEW.range_end_ms <= m.duration_ms)))
  ) THEN RAISE(ABORT, 'analysis requires same clip original and valid range') END;
END;

CREATE TRIGGER tr_events_no_update BEFORE UPDATE ON operation_events
BEGIN SELECT RAISE(ABORT, 'operation events are append only'); END;
CREATE TRIGGER tr_events_no_delete BEFORE DELETE ON operation_events
BEGIN SELECT RAISE(ABORT, 'operation events are append only'); END;

CREATE TRIGGER tr_export_snapshot_immutable
BEFORE UPDATE OF catalog_id, query_json, query_version, metadata_schema_version,
  snapshot_catalog_revision, manifest_snapshot_json, snapshot_frozen_at,
  clip_count, analysis_count ON export_jobs
WHEN OLD.snapshot_frozen_at IS NOT NULL
BEGIN SELECT RAISE(ABORT, 'export snapshot is immutable'); END;

CREATE TRIGGER tr_export_starts_preparing
BEFORE INSERT ON export_jobs
WHEN NEW.state <> 'preparing' OR NEW.snapshot_frozen_at IS NOT NULL
BEGIN SELECT RAISE(ABORT, 'export must start preparing without a frozen snapshot'); END;

CREATE TRIGGER tr_export_success_terminal
BEFORE UPDATE OF state ON export_jobs
WHEN OLD.state = 'succeeded' AND NEW.state <> 'succeeded'
BEGIN SELECT RAISE(ABORT, 'successful export is terminal'); END;

CREATE TRIGGER tr_export_freeze_counts
BEFORE UPDATE OF snapshot_frozen_at ON export_jobs
WHEN OLD.snapshot_frozen_at IS NULL AND NEW.snapshot_frozen_at IS NOT NULL
BEGIN
  SELECT CASE WHEN NEW.clip_count <> (SELECT count(*) FROM export_items WHERE export_id = NEW.id)
    OR NEW.clip_count <> coalesce(json_array_length(NEW.manifest_snapshot_json,'$.clips'),-1)
    OR NEW.clip_count <> coalesce(json_extract(NEW.manifest_snapshot_json,'$.clip_count'),-1)
    OR NEW.analysis_count <> coalesce(json_extract(NEW.manifest_snapshot_json,'$.analysis_count'),-1)
    OR NEW.metadata_schema_version <> coalesce(json_extract(NEW.manifest_snapshot_json,'$.schema_version'),-1)
    OR NEW.analysis_count <> coalesce((SELECT sum(json_array_length(value,'$.analyses'))
      FROM json_each(NEW.manifest_snapshot_json,'$.clips')),0)
    THEN RAISE(ABORT, 'export snapshot count or version mismatch') END;
END;

CREATE TRIGGER tr_export_success_requires_files
BEFORE UPDATE OF state ON export_jobs WHEN NEW.state = 'succeeded'
BEGIN
  SELECT CASE WHEN NEW.clip_count < 1 OR
    NEW.clip_count <> (SELECT count(*) FROM export_items WHERE export_id = NEW.id)
    OR EXISTS(SELECT 1 FROM export_items WHERE export_id = NEW.id AND copy_state <> 'verified')
    THEN RAISE(ABORT, 'all export items must be verified before success') END;
END;

CREATE TRIGGER tr_export_items_insert
BEFORE INSERT ON export_items
BEGIN
  SELECT CASE WHEN EXISTS(
    SELECT 1 FROM export_jobs WHERE id = NEW.export_id AND snapshot_frozen_at IS NOT NULL
  ) THEN RAISE(ABORT, 'cannot add items to frozen export') END;
  SELECT CASE WHEN NOT EXISTS(
    SELECT 1 FROM media_assets m JOIN clips c ON c.id = m.clip_id
    WHERE m.id = NEW.asset_id AND m.clip_id = NEW.clip_id
      AND c.primary_asset_id = m.id AND c.revision = NEW.snapshot_clip_revision
      AND m.sha256 = NEW.expected_sha256 AND m.size_bytes = NEW.expected_size_bytes
  ) THEN RAISE(ABORT, 'export item does not match original asset snapshot') END;
END;

CREATE TRIGGER tr_export_item_identity_immutable
BEFORE UPDATE OF export_id, clip_id, asset_id, source_location_id, snapshot_clip_revision,
  position, destination_relpath, expected_size_bytes, expected_sha256 ON export_items
BEGIN SELECT RAISE(ABORT, 'export item identity is immutable'); END;

CREATE TRIGGER tr_export_items_no_delete
BEFORE DELETE ON export_items
WHEN EXISTS(SELECT 1 FROM export_jobs WHERE id = OLD.export_id AND snapshot_frozen_at IS NOT NULL)
BEGIN SELECT RAISE(ABORT, 'cannot delete frozen export items'); END;

CREATE TRIGGER tr_export_verified_item_terminal
BEFORE UPDATE OF copy_state ON export_items
WHEN NEW.copy_state <> 'verified' AND EXISTS(
  SELECT 1 FROM export_jobs WHERE id = OLD.export_id AND state = 'succeeded')
BEGIN SELECT RAISE(ABORT, 'cannot invalidate successful export item state'); END;

CREATE TRIGGER tr_export_matches_insert
BEFORE INSERT ON export_item_matches
BEGIN
  SELECT CASE WHEN EXISTS(
    SELECT 1 FROM export_jobs WHERE id = NEW.export_id AND snapshot_frozen_at IS NOT NULL
  ) THEN RAISE(ABORT, 'cannot add matches to frozen export') END;
  SELECT CASE WHEN NOT EXISTS(
    SELECT 1 FROM analyses a WHERE a.id = NEW.analysis_id AND a.clip_id = NEW.clip_id
      AND a.revision = NEW.snapshot_analysis_revision
      AND a.status = 'complete' AND a.deleted_at IS NULL
  ) THEN RAISE(ABORT, 'invalid matched analysis snapshot') END;
END;
CREATE TRIGGER tr_export_matches_no_update BEFORE UPDATE ON export_item_matches
BEGIN SELECT RAISE(ABORT, 'export match snapshot is immutable'); END;
CREATE TRIGGER tr_export_matches_no_delete BEFORE DELETE ON export_item_matches
WHEN EXISTS(SELECT 1 FROM export_jobs WHERE id = OLD.export_id AND snapshot_frozen_at IS NOT NULL)
BEGIN SELECT RAISE(ABORT, 'cannot delete frozen export matches'); END;

CREATE VIEW v_clip_primary_media AS
SELECT c.id AS clip_id, c.game_id, c.title, c.capture_mode, c.capture_state,
  c.revision AS clip_revision, c.created_at, c.updated_at,
  m.id AS asset_id, m.duration_ms, m.size_bytes, m.sha256, m.container,
  m.video_codec, m.audio_codec, m.width, m.height, m.fps_num, m.fps_den,
  l.id AS location_id, l.relative_path, l.availability, l.last_verified_at
FROM clips c
JOIN media_assets m ON m.id = c.primary_asset_id AND m.clip_id = c.id
LEFT JOIN media_locations l ON l.asset_id = m.id AND l.is_preferred = 1
WHERE c.deleted_at IS NULL AND m.verification_state = 'verified';

PRAGMA user_version = 2;

