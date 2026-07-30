"""Offline tests for submit_graph hash/dry_run scenario fixtures."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

from graph_json_to_dsl import resolve_write_dsl
from test_submit_graph_writer_policy import is_scratch_asset_path


FIXTURES_DIR = Path(__file__).resolve().parent.parent / "fixtures"
PROTO_TESTS = Path(__file__).resolve().parents[3] / "UeremcpProtocol" / "Tests" / "py"
sys.path.insert(0, str(PROTO_TESTS))
from ueremcp_protocol.content_hash import content_hash  # noqa: E402


def load_fixture(name: str) -> dict:
    data = json.loads((FIXTURES_DIR / name).read_text(encoding="utf-8"))
    if isinstance(data, dict):
        data.pop("$comment", None)
    return data


def load_scenarios() -> list[dict]:
    raw = json.loads(
        (FIXTURES_DIR / "submit_graph_hash_scenarios.fixture.json").read_text(encoding="utf-8")
    )
    return raw["scenarios"]


class SubmitGraphHashScenarioTests(unittest.TestCase):
    def test_scenario_fixture_has_six_cases(self) -> None:
        scenarios = load_scenarios()
        self.assertEqual(len(scenarios), 6)
        ids = {s["id"] for s in scenarios}
        self.assertIn("changed_replace_dry_run", ids)
        self.assertIn("changed_replace_live_validated", ids)

    def test_dry_run_scenario_never_claims_validated(self) -> None:
        scenario = next(s for s in load_scenarios() if s["id"] == "changed_replace_dry_run")
        self.assertEqual(scenario["expected_status"], "partially_completed")
        self.assertFalse(scenario["validated_claim"])
        self.assertTrue(scenario["dry_run"])
        self.assertEqual(scenario["revision_after"], "unchanged")

    def test_live_validated_scenario_documents_editor_not_verified(self) -> None:
        scenario = next(s for s in load_scenarios() if s["id"] == "changed_replace_live_validated")
        self.assertTrue(scenario["validated_claim"])
        self.assertFalse(scenario["editor_verified"])

    def test_hash_diff_implies_changed_submission(self) -> None:
        current = load_fixture("translate_begin_play_print.fixture.json")
        changed = load_fixture("translate_begin_play_print.fixture.json")
        for node in changed["nodes"]:
            if node.get("semantic_type") == "call_function":
                node.setdefault("defaults", {})["InString"] = "dry_run probe"
        self.assertNotEqual(content_hash(current), content_hash(changed))
        _dsl, _lossy = resolve_write_dsl(changed)
        # Offline: hash differs; dry_run would leave revision unchanged (documented in fixture).
        dry_run = next(s for s in load_scenarios() if s["id"] == "changed_replace_dry_run")
        self.assertFalse(dry_run["submitted_equals_current_hash"])

    def test_non_scratch_scenario_matches_path_guard(self) -> None:
        scenario = next(s for s in load_scenarios() if s["id"] == "non_scratch_changed_replace")
        path = scenario["asset_path"]
        self.assertFalse(is_scratch_asset_path(path))


if __name__ == "__main__":
    unittest.main()
