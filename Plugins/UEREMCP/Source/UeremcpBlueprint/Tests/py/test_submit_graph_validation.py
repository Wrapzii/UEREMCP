"""Offline tests for submit_graph pre-flight validation (mirrors C++ writer)."""

from __future__ import annotations

import copy
import json
import unittest
from pathlib import Path

from submit_graph_validation import SubmitGraphValidationError, validate_submitted_graph_for_replace


FIXTURES_DIR = Path(__file__).resolve().parent.parent / "fixtures"


def load_fixture(name: str) -> dict:
    data = json.loads((FIXTURES_DIR / name).read_text(encoding="utf-8"))
    if isinstance(data, dict):
        data.pop("$comment", None)
    return data


class SubmitGraphValidationTests(unittest.TestCase):
    def test_valid_replace_fixture_passes(self) -> None:
        graph = load_fixture("submit_graph_replace.fixture.json")
        validate_submitted_graph_for_replace(
            graph,
            graph["asset_path"],
            graph["graph_id"],
        )

    def test_wrong_graph_type_rejected(self) -> None:
        graph = load_fixture("submit_graph_replace.fixture.json")
        graph = copy.deepcopy(graph)
        graph["graph_type"] = "AnimBlueprintGraph"
        with self.assertRaises(SubmitGraphValidationError) as ctx:
            validate_submitted_graph_for_replace(graph, graph["asset_path"], graph["graph_id"])
        self.assertIn("submit_graph.unsupported_graph_type", ctx.exception.capability_notes)

    def test_missing_fidelity_rejected(self) -> None:
        graph = load_fixture("submit_graph_replace.fixture.json")
        graph = copy.deepcopy(graph)
        del graph["fidelity"]
        with self.assertRaises(SubmitGraphValidationError) as ctx:
            validate_submitted_graph_for_replace(graph, graph["asset_path"], graph["graph_id"])
        self.assertIn("fidelity", str(ctx.exception).lower())

    def test_asset_path_mismatch_rejected(self) -> None:
        graph = load_fixture("submit_graph_replace.fixture.json")
        with self.assertRaises(SubmitGraphValidationError) as ctx:
            validate_submitted_graph_for_replace(
                graph,
                "/Game/__UeremcpTests/Other/BP_Other.BP_Other",
                graph["graph_id"],
            )
        self.assertIn("submit_graph.asset_path_mismatch", ctx.exception.capability_notes)

    def test_graph_id_mismatch_rejected(self) -> None:
        graph = load_fixture("submit_graph_replace.fixture.json")
        with self.assertRaises(SubmitGraphValidationError) as ctx:
            validate_submitted_graph_for_replace(graph, graph["asset_path"], "ConstructionScript")
        self.assertIn("submit_graph.graph_id_mismatch", ctx.exception.capability_notes)

    def test_unresolvable_dsl_rejected(self) -> None:
        graph = load_fixture("translate_branch_if.fixture.json")
        bare = {k: v for k, v in graph.items() if k != "extensions"}
        bare["fidelity"] = {"round_trip_supported": False}
        with self.assertRaises(SubmitGraphValidationError) as ctx:
            validate_submitted_graph_for_replace(
                bare,
                bare["asset_path"],
                bare["graph_id"],
            )
        self.assertIn("submit_graph.dsl_required", ctx.exception.capability_notes)

    def test_unresolvable_dsl_allowed_for_noop_preflight(self) -> None:
        graph = load_fixture("translate_branch_if.fixture.json")
        bare = {k: v for k, v in graph.items() if k != "extensions"}
        bare["fidelity"] = {"round_trip_supported": False}
        validate_submitted_graph_for_replace(
            bare,
            bare["asset_path"],
            bare["graph_id"],
            require_write_dsl=False,
        )

    def test_missing_nodes_rejected(self) -> None:
        graph = load_fixture("submit_graph_replace.fixture.json")
        graph = copy.deepcopy(graph)
        del graph["nodes"]
        with self.assertRaises(SubmitGraphValidationError):
            validate_submitted_graph_for_replace(graph, graph["asset_path"], graph["graph_id"])


if __name__ == "__main__":
    unittest.main()
