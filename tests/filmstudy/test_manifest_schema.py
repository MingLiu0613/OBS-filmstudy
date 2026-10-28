"""Validate the PRD's version 2 manifest example and critical boundaries."""

import copy
import json
import unittest
from pathlib import Path

from jsonschema import Draft202012Validator, FormatChecker, ValidationError


ROOT = Path(__file__).resolve().parents[2]
SCHEMA = json.loads((ROOT / "docs/filmstudy/manifest-v2.schema.json").read_text("utf-8"))
EXAMPLE = json.loads(
    (ROOT / "tests/filmstudy/fixtures/manifest-v2-example.json").read_text("utf-8")
)
VALIDATOR = Draft202012Validator(SCHEMA, format_checker=FormatChecker())


class ManifestSchemaTests(unittest.TestCase):
    def test_schema_is_valid_and_prd_example_conforms(self):
        Draft202012Validator.check_schema(SCHEMA)
        VALIDATOR.validate(EXAMPLE)
        self.assertEqual(EXAMPLE["clip_count"], len(EXAMPLE["clips"]))
        self.assertEqual(
            EXAMPLE["analysis_count"], sum(len(c["analyses"]) for c in EXAMPLE["clips"])
        )

    def test_wrong_version_or_nonportable_path_is_rejected(self):
        changed = copy.deepcopy(EXAMPLE)
        changed["schema_version"] = 1
        with self.assertRaises(ValidationError):
            VALIDATOR.validate(changed)
        changed = copy.deepcopy(EXAMPLE)
        changed["clips"][0]["file"]["path"] = "C:\\recordings\\clip.mp4"
        with self.assertRaises(ValidationError):
            VALIDATOR.validate(changed)

    def test_complete_analysis_needs_grouped_fields(self):
        changed = copy.deepcopy(EXAMPLE)
        changed["clips"][0]["analyses"][0]["topic"] = None
        with self.assertRaises(ValidationError):
            VALIDATOR.validate(changed)


if __name__ == "__main__":
    unittest.main()
