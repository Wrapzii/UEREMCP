"""Offline validation of Blueprint read_graph fixtures against graph.schema.json."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from schema_registry import graph_schema_validator, repo_root


FIXTURES_DIR = Path(__file__).resolve().parent.parent / "fixtures"
EXPECTED_LOSSY = {
    "multigate_no_dsl_roundtrip",
    "timeline_special_spawn",
    "math_expression_unproven",
    "dsl_bind_elision",
    "reroute_knots_elided",
    "node_guid_not_preserved",
    "positions_not_semantic",
    "collapsed_composite_subgraphs_unproven",
    "project_custom_k2_nodes_unknown",
}


def load_fixture(name: str) -> dict:
    path = FIXTURES_DIR / name
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, dict):
        data.pop("$comment", None)
    return data


def validation_errors(instance: dict) -> list[str]:
    validator = graph_schema_validator()
    errors = sorted(validator.iter_errors(instance), key=lambda e: list(e.path))
    return [f"{'/'.join(str(p) for p in err.path) or '<root>'}: {err.message}" for err in errors]


class GraphSchemaFixtureTests(unittest.TestCase):
    def test_read_graph_event_graph_fixture_validates(self) -> None:
        graph = load_fixture("read_graph_event_graph.fixture.json")
        errors = validation_errors(graph)
        self.assertEqual(errors, [], msg="\n".join(errors))

    def test_fixture_has_required_blueprint_fields(self) -> None:
        graph = load_fixture("read_graph_event_graph.fixture.json")
        for key in ("asset_path", "graph_id", "graph_type", "schema_version"):
            self.assertIn(key, graph)
        self.assertEqual(graph["graph_type"], "BlueprintEventGraph")
        self.assertTrue(graph["content_hash"].startswith("sha256:"))

    def test_fixture_diagnostics_a3_shape(self) -> None:
        graph = load_fixture("read_graph_event_graph.fixture.json")
        diagnostics = graph.get("diagnostics", {})
        dead = diagnostics.get("dead_nodes", [])
        islands = diagnostics.get("disconnected_subgraphs", [])
        self.assertGreaterEqual(len(dead), 1, "expected at least one dead node")
        self.assertGreaterEqual(len(islands), 1, "expected at least one disconnected island")
        for island in islands:
            self.assertIsInstance(island, list)
            self.assertGreaterEqual(len(island), 1)

    def test_fixture_fidelity_lossy_areas_complete(self) -> None:
        graph = load_fixture("read_graph_event_graph.fixture.json")
        fidelity = graph["fidelity"]
        self.assertFalse(fidelity["round_trip_supported"])
        lossy = set(fidelity.get("lossy_areas", []))
        self.assertTrue(EXPECTED_LOSSY.issubset(lossy))

    def test_missing_required_field_fails_schema(self) -> None:
        graph = load_fixture("read_graph_event_graph.fixture.json")
        del graph["graph_type"]
        errors = validation_errors(graph)
        self.assertTrue(any("graph_type" in err for err in errors))

    def test_schemas_dir_exists_from_repo_root(self) -> None:
        root = repo_root()
        self.assertTrue((root / "schemas" / "graph" / "graph.schema.json").is_file())


if __name__ == "__main__":
    unittest.main()
