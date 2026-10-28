"""Focused executable checks for the Film Study schema contract."""

import sqlite3
import unittest
from pathlib import Path


SCHEMA = Path(__file__).resolve().parents[2] / "frontend/filmstudy/migrations/0002_initial.sql"
NOW = "2026-10-28T00:00:00.000Z"


def new_catalog():
    db = sqlite3.connect(":memory:")
    db.execute("PRAGMA foreign_keys=ON")
    db.executescript(SCHEMA.read_text(encoding="utf-8"))
    return db


def seed_ready_clip(db):
    db.execute(
        "INSERT INTO games(id,name,created_at,updated_at) VALUES (?,?,?,?)",
        ("game-1", "Example game", NOW, NOW),
    )
    db.execute(
        "INSERT INTO clips(id,game_id,capture_mode,capture_state,created_at,updated_at) "
        "VALUES (?,?,?,?,?,?)",
        ("clip-1", "game-1", "manual", "processing", NOW, NOW),
    )
    db.execute(
        "INSERT INTO media_assets(id,clip_id,asset_kind,container,video_codec,width,height,"
        "fps_num,fps_den,duration_ms,size_bytes,sha256,created_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
        ("asset-1", "clip-1", "original", "mp4", "h264", 1920, 1080, 60, 1,
         1000, 100, "a" * 64, NOW),
    )
    db.execute(
        "INSERT INTO media_locations(id,asset_id,relative_path,path_key,availability,"
        "is_preferred,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?)",
        ("location-1", "asset-1", "media/clip-1.mp4", "media/clip-1.mp4",
         "available", 1, NOW, NOW),
    )
    db.execute(
        "UPDATE clips SET primary_asset_id='asset-1',capture_state='ready' WHERE id='clip-1'"
    )


class SchemaContractTests(unittest.TestCase):
    def setUp(self):
        self.db = new_catalog()

    def tearDown(self):
        self.db.close()

    def test_schema_version_and_table_count(self):
        self.assertGreaterEqual(sqlite3.sqlite_version_info, (3, 38, 0))
        self.assertEqual(self.db.execute("PRAGMA user_version").fetchone()[0], 2)
        tables = self.db.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'"
        ).fetchall()
        self.assertEqual(len(tables), 19)
        self.assertEqual(self.db.execute("PRAGMA foreign_key_check").fetchall(), [])

    def test_original_and_location_are_unique(self):
        seed_ready_clip(self.db)
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(
                "INSERT INTO media_assets(id,clip_id,asset_kind,container,video_codec,width,"
                "height,fps_num,fps_den,duration_ms,size_bytes,sha256,created_at) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)",
                ("asset-2", "clip-1", "original", "mp4", "h264", 1920, 1080,
                 60, 1, 1000, 100, "b" * 64, NOW),
            )
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(
                "INSERT INTO media_locations(id,asset_id,relative_path,path_key,"
                "availability,is_preferred,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?)",
                ("location-2", "asset-1", "media/other.mp4", "media/other.mp4",
                 "available", 1, NOW, NOW),
            )

    def test_complete_analysis_requires_core_fields_and_same_row_match(self):
        seed_ready_clip(self.db)
        for topic_id in ("topic-1", "topic-2"):
            self.db.execute(
                "INSERT INTO topics(id,name,name_key,created_at,updated_at) "
                "VALUES (?,?,?,?,?)",
                (topic_id, topic_id, topic_id, NOW, NOW),
            )
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(
                "INSERT INTO analyses(id,clip_id,status,created_at,updated_at) "
                "VALUES (?,?,?,?,?)",
                ("invalid", "clip-1", "complete", NOW, NOW),
            )
        for analysis_id, perspective, topic_id in (
            ("analysis-1", "offense", "topic-1"),
            ("analysis-2", "defense", "topic-2"),
        ):
            self.db.execute(
                "INSERT INTO analyses(id,clip_id,reference_asset_id,status,perspective,"
                "topic_id,assessment,created_at,updated_at) VALUES (?,?,?,?,?,?,?,?,?)",
                (analysis_id, "clip-1", "asset-1", "complete", perspective,
                 topic_id, "positive", NOW, NOW),
            )
        query = (
            "SELECT a.id FROM v_clip_primary_media v JOIN analyses a "
            "ON a.clip_id=v.clip_id AND a.reference_asset_id=v.asset_id "
            "WHERE v.capture_state='ready' AND v.availability='available' "
            "AND a.status='complete' AND a.perspective=? AND a.topic_id=?"
        )
        self.assertEqual(
            self.db.execute(query, ("offense", "topic-2")).fetchall(), []
        )
        self.assertEqual(
            self.db.execute(query, ("offense", "topic-1")).fetchall(),
            [("analysis-1",)],
        )

    def test_capture_identity_and_busy_guard(self):
        self.db.execute(
            "INSERT INTO games(id,name,created_at,updated_at) VALUES (?,?,?,?)",
            ("game-1", "Example game", NOW, NOW),
        )
        self.db.execute(
            "INSERT INTO capture_sessions(id,game_id,mode,state,source_snapshot_json,"
            "created_at,updated_at) VALUES (?,?,'manual','starting','{}',?,?)",
            ("session-1", "game-1", NOW, NOW),
        )
        self.db.execute(
            "INSERT INTO clips(id,game_id,capture_mode,capture_state,created_at,updated_at) "
            "VALUES (?,?,'manual','starting',?,?)",
            ("clip-1", "game-1", NOW, NOW),
        )
        self.db.execute(
            "INSERT INTO capture_operations(id,session_id,clip_id,phase,intended_final_relpath,"
            "created_at,updated_at) VALUES (?,?,?,'starting',?,?,?)",
            ("operation-1", "session-1", "clip-1", "media/clip-1.mp4", NOW, NOW),
        )
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute(
                "INSERT INTO capture_sessions(id,game_id,mode,state,source_snapshot_json,"
                "created_at,updated_at) VALUES (?,?,'manual','starting','{}',?,?)",
                ("session-2", "game-1", NOW, NOW),
            )
        self.db.execute("UPDATE capture_operations SET phase='failed' WHERE id='operation-1'")
        self.db.execute("UPDATE capture_sessions SET state='failed' WHERE id='session-1'")
        self.db.execute("UPDATE clips SET capture_state='failed' WHERE id='clip-1'")
        self.db.execute(
            "INSERT INTO capture_sessions(id,game_id,mode,state,source_snapshot_json,"
            "created_at,updated_at) VALUES (?,?,'manual','starting','{}',?,?)",
            ("session-2", "game-1", NOW, NOW),
        )
        self.assertEqual(
            self.db.execute("SELECT id FROM clips WHERE game_id='game-1'").fetchall(),
            [("clip-1",)],
        )

    def test_asset_content_is_immutable(self):
        seed_ready_clip(self.db)
        with self.assertRaises(sqlite3.IntegrityError):
            self.db.execute("UPDATE media_assets SET sha256=? WHERE id='asset-1'", ("b" * 64,))
        self.assertEqual(
            self.db.execute("SELECT sha256 FROM media_assets WHERE id='asset-1'").fetchone()[0],
            "a" * 64,
        )


if __name__ == "__main__":
    unittest.main()
