"""Unit tests for WS-11 visual acceptance + semantic eval telemetry."""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests" / "visual"))

from mountain_river_rain_harness import (  # noqa: E402
    evaluate_evidence_dir,
    evaluate_structure,
)

SCHEMA_PATH = ROOT / "tests" / "schemas" / "semantic_eval_report.schema.json"


class MountainRiverAcceptanceTests(unittest.TestCase):
    def test_structure_pass(self):
        gates = evaluate_structure(
            {
                "terrain_elevation_range_uu": 1500,
                "valley_shoulder_drop_uu": 320,
                "river_centerline_samples": [[0, 0, 0], [10, 0, 0]],
                "river_dry_gap_count": 0,
                "foliage_channel_intersections": 0,
                "trees_both_banks_sections": 3,
                "rain": {
                    "camera_follow_world_error_pct": 5.0,
                    "local_offset_delta_uu": 40.0,
                },
                "compile_ok": True,
            }
        )
        statuses = {g["gate"]: g["status"] for g in gates}
        self.assertEqual(statuses["landscape_valley"], "PASS")
        self.assertEqual(statuses["river_continuity"], "PASS")
        self.assertEqual(statuses["foliage_exclusion"], "PASS")
        self.assertEqual(statuses["camera_follow_rain"], "PASS")
        self.assertEqual(statuses["compile_renderer"], "PASS")

    def test_missing_evidence_blocked(self):
        with tempfile.TemporaryDirectory() as tmp:
            report = evaluate_evidence_dir(Path(tmp))
        self.assertEqual(report["overall"], "BLOCKED")
        self.assertEqual(report["visual_policy"].count("supplemental"), 1)

    def test_foliage_intersection_fails(self):
        gates = evaluate_structure(
            {
                "terrain_elevation_range_uu": 1500,
                "valley_shoulder_drop_uu": 320,
                "river_centerline_samples": [[0, 0, 0], [10, 0, 0]],
                "river_dry_gap_count": 0,
                "foliage_channel_intersections": 2,
                "trees_both_banks_sections": 3,
                "rain": {
                    "camera_follow_world_error_pct": 1.0,
                    "local_offset_delta_uu": 1.0,
                },
                "compile_ok": True,
            }
        )
        foliage = next(g for g in gates if g["gate"] == "foliage_exclusion")
        self.assertEqual(foliage["status"], "FAIL")


class SemanticEvalReportSchemaTests(unittest.TestCase):
    def test_schema_loads(self):
        schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
        self.assertEqual(schema["title"], "Semantic operation evaluation report")
        self.assertIn("telemetry", schema["properties"])
        visual = schema["properties"]["evidence"]["properties"]["visual"]
        self.assertTrue(visual["properties"]["supplemental_only"]["const"])

    def test_example_report_shape(self):
        report = {
            "schema_version": "1.0",
            "operation": "capture_world_frames",
            "request_id": "unit-1",
            "overall": "PASS_WITH_LIMITATIONS",
            "telemetry": {
                "mcp_round_trips": 1,
                "internal_operations": 40,
                "call_count": 1,
                "statuses": ["no_change_required"],
            },
            "gates": [
                {"name": "png_reread", "status": "PASS", "detail": "2/2"},
                {"name": "structural_world", "status": "PASS"},
            ],
            "evidence": {
                "structural": {"actor_count": 12},
                "visual": {
                    "supplemental_only": True,
                    "png_paths": ["Saved/UEREMCP/WorldCapture/x/y/world_frame_00.png"],
                },
            },
            "limitations": ["Beauty not judged"],
        }
        schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
        for key in schema["required"]:
            self.assertIn(key, report)
        self.assertTrue(report["evidence"]["visual"]["supplemental_only"])


class CaptureProtocolDocTests(unittest.TestCase):
    def test_protocols_exist(self):
        visual = ROOT / "tests" / "visual"
        self.assertTrue((visual / "MATERIAL_CAPTURE_PROTOCOL.md").is_file())
        self.assertTrue((visual / "ANIMATION_CAPTURE_PROTOCOL.md").is_file())
        self.assertTrue((visual / "MOUNTAIN_RIVER_RAIN_ACCEPTANCE.md").is_file())
        self.assertTrue((visual / "mountain_river_rain_harness.py").is_file())


if __name__ == "__main__":
    unittest.main()
