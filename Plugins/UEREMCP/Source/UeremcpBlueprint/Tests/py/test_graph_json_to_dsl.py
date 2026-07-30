"""Golden tests for graph JSON→DSL translator (mirrors C++ writer)."""

from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path

from graph_json_to_dsl import (
    GraphJsonToDslError,
    LOSSY_TRANSLATOR_NOTES,
    resolve_write_dsl,
    translate_graph_json_to_dsl,
)


FIXTURES_DIR = Path(__file__).resolve().parent.parent / "fixtures"
PROTO_TESTS = Path(__file__).resolve().parents[3] / "UeremcpProtocol" / "Tests" / "py"
sys.path.insert(0, str(PROTO_TESTS))
from ueremcp_protocol.content_hash import content_hash  # noqa: E402


def load_fixture(name: str) -> dict:
    data = json.loads((FIXTURES_DIR / name).read_text(encoding="utf-8"))
    if isinstance(data, dict):
        data.pop("$comment", None)
    return data


def load_golden_dsl(name: str) -> str:
    return (FIXTURES_DIR / name).read_text(encoding="utf-8").strip() + "\n"


class GraphJsonToDslTests(unittest.TestCase):
    def test_begin_play_print_matches_golden(self) -> None:
        graph = load_fixture("translate_begin_play_print.fixture.json")
        dsl, lossy = translate_graph_json_to_dsl(graph)
        self.assertEqual(dsl.strip() + "\n", load_golden_dsl("translate_begin_play_print.golden.dsl"))
        self.assertEqual(lossy, list(LOSSY_TRANSLATOR_NOTES))

    def test_linear_exec_chain_matches_golden(self) -> None:
        graph = load_fixture("translate_linear_exec_chain.fixture.json")
        dsl, _lossy = translate_graph_json_to_dsl(graph)
        self.assertEqual(dsl.strip() + "\n", load_golden_dsl("translate_linear_exec_chain.golden.dsl"))

    def test_read_graph_fixture_multi_event_translate(self) -> None:
        graph = load_fixture("read_graph_event_graph.fixture.json")
        dsl, lossy = translate_graph_json_to_dsl(graph)
        self.assertIn("(event EventBeginPlay", dsl)
        self.assertIn(':InString "hello"', dsl)
        self.assertIn("(event UeremcpOrphanEvent", dsl)
        self.assertEqual(len(lossy), 2)

    def test_extensions_dsl_preferred_over_translation(self) -> None:
        graph = load_fixture("submit_graph_replace.fixture.json")
        dsl, lossy = resolve_write_dsl(graph)
        self.assertIn("hello from submit", dsl)
        self.assertEqual(lossy, [])

    def test_missing_entry_points_raises(self) -> None:
        graph = load_fixture("translate_begin_play_print.fixture.json")
        del graph["entry_points"]
        with self.assertRaises(GraphJsonToDslError):
            translate_graph_json_to_dsl(graph)

    def test_semantic_hash_ignores_cosmetic_node_ids(self) -> None:
        base = load_fixture("translate_begin_play_print.fixture.json")
        h_base = content_hash(base)
        cosmetic = copy.deepcopy(base)
        for node in cosmetic["nodes"]:
            if node.get("node_id") == "n_event_begin_play":
                node["node_id"] = "n_renamed_event"
                node["position"] = [999, 999]
        cosmetic["links"][0]["from_node"] = "n_renamed_event"
        for pin in cosmetic["nodes"][0]["output_pins"]:
            for link in pin.get("links", []):
                link["node_id"] = cosmetic["nodes"][1]["node_id"]
        h_cosmetic = content_hash(cosmetic)
        self.assertEqual(h_base, h_cosmetic)

    def test_semantic_hash_changes_when_default_changes(self) -> None:
        a = load_fixture("translate_begin_play_print.fixture.json")
        b = copy.deepcopy(a)
        for node in b["nodes"]:
            if node.get("semantic_type") == "call_function":
                node.setdefault("defaults", {})["InString"] = "changed"
        self.assertNotEqual(content_hash(a), content_hash(b))


if __name__ == "__main__":
    unittest.main()
