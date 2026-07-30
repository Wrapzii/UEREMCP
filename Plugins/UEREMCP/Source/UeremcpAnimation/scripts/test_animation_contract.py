"""Fast source/schema checks for WS-10 animation scaffolding."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


MODULE = Path(__file__).resolve().parents[1]
REPO = MODULE.parents[3]


class AnimationContractTests(unittest.TestCase):
    def test_inspect_montage_spec_is_complete_state_request(self) -> None:
        schema = json.loads(
            (REPO / "schemas/domains/animation/inspect_montage.schema.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(schema["type"], "object")
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(schema["properties"], {})

    def test_service_serializes_adjacent_montage_state(self) -> None:
        source = (
            MODULE / "Private/UeremcpAnimationService.cpp"
        ).read_text(encoding="utf-8")
        for field in (
            '"skeleton"',
            '"slots"',
            '"segments"',
            '"sections"',
            '"notifies"',
            '"content_hash"',
            '"revision"',
        ):
            self.assertIn(field, source)
        self.assertIn("GetAnimationNotifyEvents", source)

    def test_tool_reports_response_schema_blocker_honestly(self) -> None:
        source = (
            MODULE / "Private/UeremcpAnimationToolset.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn('Response.Status = TEXT("partially_completed")', source)
        self.assertIn("frozen response envelope", source)
        self.assertNotIn('SetObjectField(TEXT("asset_state")', source)


if __name__ == "__main__":
    unittest.main()
