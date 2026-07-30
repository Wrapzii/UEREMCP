"""Assert Python protocol package against Tests/golden/ vectors (C-2).

These goldens are the cross-language contract. C++ AutomationTests under
Private/Tests/UeremcpProtocolGoldenTests.cpp must exercise FUeremcp* production
code against the same files. Python green alone is NOT C++ parity.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from ueremcp_protocol.content_hash import content_hash
from ueremcp_protocol.dependency_order import topological_sort
from ueremcp_protocol.envelope import parse_request, serialize_response
from ueremcp_protocol.ref_resolve import resolve_refs

GOLDEN = Path(__file__).resolve().parent.parent / "golden"


def load_json(rel: str):
    return json.loads((GOLDEN / rel).read_text(encoding="utf-8"))


def load_text(rel: str) -> str:
    return (GOLDEN / rel).read_text(encoding="utf-8").strip()


class GoldenVectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (GOLDEN / "content_hash" / "graph.in.json").is_file():
            raise unittest.SkipTest(f"golden root missing: {GOLDEN}")

    def test_content_hash_matches_golden(self):
        graph = load_json("content_hash/graph.in.json")
        expected = load_text("content_hash/hash.expected.txt")
        self.assertEqual(content_hash(graph), expected)

    def test_content_hash_cosmetic_stable(self):
        expected = load_text("content_hash/hash.expected.txt")
        cosmetic = load_json("content_hash/graph_cosmetic.in.json")
        self.assertEqual(content_hash(cosmetic), expected)

    def test_envelope_request_parse(self):
        raw = (GOLDEN / "envelope/request.in.json").read_text(encoding="utf-8")
        parsed = parse_request(raw)
        expected = load_json("envelope/request.parsed.expected.json")
        got = {
            "protocol_version": parsed["protocol_version"],
            "request_id": parsed["request_id"],
            "action": parsed["action"],
            "mode": parsed["mode"],
            "target_asset_path": parsed["target_asset_path"],
            "engine_version": parsed["engine_version"],
            "dry_run": parsed["dry_run"],
            "atomic": parsed["atomic"],
            "response_detail": parsed["response_detail"],
            "timeout_ms": parsed["timeout_ms"],
            "has_expected_revision": parsed["has_expected_revision"],
            "idempotency_key": parsed["idempotency_key"],
            "specification_name": (parsed["specification"] or {}).get("name"),
        }
        self.assertEqual(got, expected)

    def test_envelope_response_serialize(self):
        fields = load_json("envelope/response_fields.in.json")
        expected = load_json("envelope/response.out.expected.json")
        got = json.loads(serialize_response(fields))
        self.assertEqual(got, expected)

    def test_ref_both_forms(self):
        spec = load_json("ref/spec.in.json")
        completed = load_json("ref/completed.in.json")
        expected = load_json("ref/resolved.expected.json")
        self.assertEqual(resolve_refs(spec, completed), expected)

    def test_topo_sort(self):
        nodes = load_json("topo/nodes.in.json")
        expected = load_json("topo/order.expected.json")
        self.assertEqual(topological_sort(nodes), expected)


if __name__ == "__main__":
    unittest.main()
