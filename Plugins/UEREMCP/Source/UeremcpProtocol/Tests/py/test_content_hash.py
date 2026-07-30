"""Unit tests for content_hash semantics."""

from __future__ import annotations

import copy
import unittest

from ueremcp_protocol.content_hash import content_hash


def _sample_graph():
    return {
        "asset_path": "/Game/BP_Test",
        "graph_id": "EventGraph",
        "graph_type": "BlueprintEventGraph",
        "schema_version": "1.0",
        "content_hash": "sha256:deadbeef",
        "revision": "sha256:deadbeef",
        "retrieved_at": "2026-07-29T00:00:00Z",
        "nodes": [
            {
                "node_id": "guid-aaaa",
                "semantic_id": "event.begin_play",
                "node_class": "K2Node_Event",
                "semantic_type": "event_begin_play",
                "title": "BeginPlay",
                "position": [100.0, 200.0],
                "properties": {"bOverrideFunction": True},
                "input_pins": [],
                "output_pins": [
                    {
                        "pin_id": "pin-guid-1",
                        "name": "then",
                        "direction": "output",
                        "pin_type": {"category": "exec"},
                        "default_value": None,
                        "links": [{"node_id": "guid-bbbb", "pin_id": "pin-guid-2"}],
                    }
                ],
            },
            {
                "node_id": "guid-bbbb",
                "semantic_id": "print_string",
                "node_class": "K2Node_CallFunction",
                "semantic_type": "call_function",
                "title": "Print String",
                "position": [400.0, 200.0],
                "properties": {"FunctionReference": "PrintString"},
                "input_pins": [
                    {
                        "pin_id": "pin-guid-2",
                        "name": "execute",
                        "direction": "input",
                        "pin_type": {"category": "exec"},
                        "links": [{"node_id": "guid-aaaa", "pin_id": "pin-guid-1"}],
                    },
                    {
                        "pin_id": "pin-guid-3",
                        "name": "InString",
                        "direction": "input",
                        "pin_type": {"category": "string"},
                        "default_value": "hello",
                        "links": [],
                    },
                ],
                "output_pins": [],
            },
        ],
        "links": [
            {
                "from_node": "guid-aaaa",
                "from_pin": "pin-guid-1",
                "to_node": "guid-bbbb",
                "to_pin": "pin-guid-2",
                "kind": "exec",
            }
        ],
    }


class ContentHashTests(unittest.TestCase):
    def test_stable_across_cosmetic_churn(self):
        a = _sample_graph()
        b = copy.deepcopy(a)
        # Reorder nodes, jitter positions, regenerate GUIDs / ids / timestamps.
        b["nodes"] = list(reversed(b["nodes"]))
        b["nodes"][0]["position"] = [999.0, -1.0]
        b["nodes"][1]["position"] = [0.0, 0.0]
        b["nodes"][0]["node_id"] = "brand-new-guid-1"
        b["nodes"][1]["node_id"] = "brand-new-guid-2"
        b["nodes"][0]["input_pins"][0]["pin_id"] = "new-pin-a"
        b["nodes"][0]["input_pins"][1]["pin_id"] = "new-pin-b"
        b["nodes"][1]["output_pins"][0]["pin_id"] = "new-pin-c"
        b["links"][0] = {
            "from_node": "brand-new-guid-2",
            "from_pin": "new-pin-c",
            "to_node": "brand-new-guid-1",
            "to_pin": "new-pin-a",
            "kind": "exec",
        }
        b["retrieved_at"] = "2099-01-01T00:00:00Z"
        b["content_hash"] = "sha256:ffff"
        b["revision"] = "sha256:ffff"

        # Fix pin link endpoints inside nodes to new ids for realism — hash
        # uses top-level links + pin names, so cosmetic pin link ids should not
        # matter when semantic_id + pin names + defaults match.
        self.assertEqual(content_hash(a), content_hash(b))

    def test_sensitive_to_pin_default_change(self):
        a = _sample_graph()
        b = copy.deepcopy(a)
        b["nodes"][1]["input_pins"][1]["default_value"] = "goodbye"
        self.assertNotEqual(content_hash(a), content_hash(b))

    def test_sensitive_to_connection_change(self):
        a = _sample_graph()
        b = copy.deepcopy(a)
        b["links"] = []
        self.assertNotEqual(content_hash(a), content_hash(b))

    def test_format(self):
        h = content_hash(_sample_graph())
        self.assertTrue(h.startswith("sha256:"))
        self.assertEqual(len(h), len("sha256:") + 64)
        self.assertEqual(h, h.lower())


if __name__ == "__main__":
    unittest.main()
