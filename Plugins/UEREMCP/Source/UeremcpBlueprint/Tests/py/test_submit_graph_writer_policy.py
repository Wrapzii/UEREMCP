"""Offline policy tests for submit_graph replace writer (WS-06 P2)."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from schema_registry import graph_schema_validator


from scratch_path_policy import is_blueprint_submit_scratch_path


FIXTURES_DIR = Path(__file__).resolve().parent.parent / "fixtures"


def is_scratch_asset_path(asset_path: str) -> bool:
    """Mirror FUeremcpBlueprintGraphWriter::IsScratchAssetPath."""
    return is_blueprint_submit_scratch_path(asset_path)


def resolve_dsl_from_graph(graph: dict) -> str | None:
    """Mirror extensions.blueprint.dsl resolution."""
    blueprint_ext = graph.get("extensions", {}).get("blueprint", {})
    dsl = blueprint_ext.get("dsl")
    return dsl if isinstance(dsl, str) and dsl.strip() else None


def load_fixture(name: str) -> dict:
    path = FIXTURES_DIR / name
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, dict):
        data.pop("$comment", None)
    return data


class SubmitGraphWriterPolicyTests(unittest.TestCase):
    def test_scratch_path_guard_allows_tests_root(self) -> None:
        path = "/Game/__UeremcpTests/Blueprint_ReadGraph/BP_ReadGraph_Scratch.BP_ReadGraph_Scratch"
        self.assertTrue(is_scratch_asset_path(path))

    def test_scratch_path_guard_rejects_production_assets(self) -> None:
        self.assertFalse(is_scratch_asset_path("/Game/Characters/BP_Hero.BP_Hero"))
        self.assertFalse(is_scratch_asset_path("/Game/__UeremcpTestsFake/BP_X.BP_X"))

    def test_dsl_resolution_from_extensions(self) -> None:
        graph = load_fixture("submit_graph_replace.fixture.json")
        dsl = resolve_dsl_from_graph(graph)
        self.assertIsNotNone(dsl)
        assert dsl is not None
        self.assertIn("event EventBeginPlay", dsl)
        self.assertIn("PrintString", dsl)

    def test_dsl_missing_without_extensions(self) -> None:
        graph = load_fixture("read_graph_event_graph.fixture.json")
        self.assertIsNone(resolve_dsl_from_graph(graph))

    def test_submit_replace_fixture_validates_against_graph_schema(self) -> None:
        graph = load_fixture("submit_graph_replace.fixture.json")
        validator = graph_schema_validator()
        errors = sorted(validator.iter_errors(graph), key=lambda e: list(e.path))
        self.assertEqual(errors, [], msg="\n".join(f"{e.path}: {e.message}" for e in errors))

    def test_fidelity_lossy_areas_present_on_submit_fixture(self) -> None:
        graph = load_fixture("submit_graph_replace.fixture.json")
        lossy = set(graph["fidelity"]["lossy_areas"])
        self.assertIn("multigate_no_dsl_roundtrip", lossy)
        self.assertIn("node_guid_not_preserved", lossy)
        self.assertFalse(graph["fidelity"]["round_trip_supported"])


if __name__ == "__main__":
    unittest.main()
