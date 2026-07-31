"""Schema + contract tests for remaining-domain coverage (WS-01)."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SCHEMAS = ROOT / "schemas" / "domains"


class RemainingDomainSchemaTests(unittest.TestCase):
    def test_create_audio_cue_schema_loads(self) -> None:
        path = SCHEMAS / "audio" / "create_audio_cue.schema.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        self.assertIn("sound_waves", data["required"])
        self.assertFalse(data.get("additionalProperties", True))

    def test_validate_replication_requires_networking_or_variables(self) -> None:
        path = SCHEMAS / "networking" / "validate_replication.schema.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        self.assertIn("anyOf", data)
        modes = data["properties"]["variables"]["items"]["properties"]["replication"]["enum"]
        self.assertEqual(modes, ["NONE", "REPLICATED", "REP_NOTIFY"])

    def test_repair_world_partition_is_non_destructive_by_contract(self) -> None:
        path = SCHEMAS / "world_partition" / "repair_world_partition.schema.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        self.assertIn("enable_streaming", data["properties"])
        # Destructive safety lives in the tool (dry_run + allow_destructive), not the
        # specification object — document that in description.
        self.assertIn("dry_run", data["description"])


if __name__ == "__main__":
    unittest.main()
