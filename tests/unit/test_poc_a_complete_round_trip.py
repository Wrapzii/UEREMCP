from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

TESTS_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TESTS_ROOT))

import run_poc_a_complete_round_trip as complete_round_trip


class CompleteRoundTripParsingTest(unittest.TestCase):
    def test_extracts_nested_return_value(self) -> None:
        envelope = {
            "protocol_version": "1.0",
            "status": "no_change_required",
        }
        result = {
            "content": [
                {
                    "type": "text",
                    "text": json.dumps(
                        {"returnValue": json.dumps(envelope)}
                    ),
                }
            ]
        }
        self.assertEqual(
            complete_round_trip._extract_return_envelope(result),
            envelope,
        )

    def test_complete_graph_requires_all_context(self) -> None:
        graph = {
            "nodes": [
                {
                    "defaults": {},
                    "input_pins": [{"pin_type": {"category": "bool"}}],
                    "output_pins": [],
                }
            ],
            "links": [],
            "variables": [],
            "entry_points": [],
            "dependencies": [],
            "fidelity": {"lossy_areas": []},
        }
        self.assertEqual(
            complete_round_trip._complete_graph_errors(graph),
            [],
        )
        del graph["dependencies"]
        self.assertIn(
            "graph.dependencies must be an array",
            complete_round_trip._complete_graph_errors(graph),
        )

    def test_expected_branch_requires_both_exec_links(self) -> None:
        graph = {
            "nodes": [
                {"node_id": "event", "node_class": "K2Node_Event"},
                {"node_id": "branch", "node_class": "K2Node_IfThenElse"},
                {"node_id": "print", "node_class": "K2Node_CallFunction"},
            ],
            "links": [
                {
                    "from_node": "event",
                    "from_pin": "then",
                    "to_node": "branch",
                    "to_pin": "execute",
                },
                {
                    "from_node": "branch",
                    "from_pin": "then",
                    "to_node": "print",
                    "to_pin": "execute",
                },
            ],
        }
        self.assertEqual(
            complete_round_trip._has_expected_branch(graph),
            (True, True),
        )


if __name__ == "__main__":
    unittest.main()
