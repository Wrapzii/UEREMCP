#!/usr/bin/env python3
"""Generate Tests/golden expected outputs from the Python protocol package.

The goldens are the cross-language contract. C++ AutomationTests must match
these expected files using FUeremcp* production code — not a second mirror.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "py"))

from ueremcp_protocol.content_hash import canonicalise_for_hash, content_hash  # noqa: E402
from ueremcp_protocol.dependency_order import topological_sort  # noqa: E402
from ueremcp_protocol.envelope import parse_request, serialize_response  # noqa: E402
from ueremcp_protocol.ref_resolve import resolve_refs  # noqa: E402

GOLDEN = HERE


def write_json(path: Path, obj) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text if text.endswith("\n") else text + "\n", encoding="utf-8")


def main() -> None:
    graph = {
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
                "position": [100, 200],
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
                "position": [400, 200],
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
    write_json(GOLDEN / "content_hash" / "graph.in.json", graph)
    write_text(GOLDEN / "content_hash" / "hash.expected.txt", content_hash(graph))
    write_json(
        GOLDEN / "content_hash" / "canonical.expected.json",
        canonicalise_for_hash(graph),
    )

    # Cosmetic churn: reorder nodes, jitter positions, regenerate retrieval-local
    # ids, rewrite top-level links to the new ids. Pin-level links are ignored by
    # the hasher; semantic_id + pin names keep the hash stable.
    cosmetic = json.loads(json.dumps(graph))
    cosmetic["nodes"] = list(reversed(cosmetic["nodes"]))
    # After reverse: [0]=print_string, [1]=begin_play
    cosmetic["nodes"][0]["position"] = [999, -1]
    cosmetic["nodes"][1]["position"] = [0, 0]
    cosmetic["nodes"][0]["node_id"] = "brand-new-guid-1"
    cosmetic["nodes"][1]["node_id"] = "brand-new-guid-2"
    cosmetic["nodes"][0]["input_pins"][0]["pin_id"] = "new-pin-a"
    cosmetic["nodes"][0]["input_pins"][1]["pin_id"] = "new-pin-b"
    cosmetic["nodes"][1]["output_pins"][0]["pin_id"] = "new-pin-c"
    cosmetic["links"] = [
        {
            "from_node": "brand-new-guid-2",
            "from_pin": "new-pin-c",
            "to_node": "brand-new-guid-1",
            "to_pin": "new-pin-a",
            "kind": "exec",
        }
    ]
    cosmetic["retrieved_at"] = "2099-01-01T00:00:00Z"
    cosmetic["content_hash"] = "sha256:ffff"
    write_json(GOLDEN / "content_hash" / "graph_cosmetic.in.json", cosmetic)
    assert content_hash(cosmetic) == content_hash(graph), (
        content_hash(cosmetic),
        content_hash(graph),
    )

    req = {
        "protocol_version": "1.0",
        "request_id": "3f1c2e40-8a11-4c2b-9b7e-2e5d1a0c9f31",
        "action": "create_niagara_effect",
        "project": {
            "path": "$UEREMCP_LEGACY_PROJECT/RE.uproject",
            "engine_version": "5.8",
        },
        "target": {"asset_path": "/Game/VFX/Spells/NS_Fireball"},
        "mode": "create_or_update",
        "specification": {"name": "Fireball"},
        "options": {
            "dry_run": False,
            "atomic": True,
            "response_detail": "summary",
            "timeout_ms": 120000,
        },
        "expected_revision": None,
        "idempotency_key": "poc-b-fireball-001",
    }
    write_json(GOLDEN / "envelope" / "request.in.json", req)
    parsed = parse_request(json.dumps(req))
    write_json(
        GOLDEN / "envelope" / "request.parsed.expected.json",
        {
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
        },
    )

    resp_in = {
        "request_id": "3f1c2e40-8a11-4c2b-9b7e-2e5d1a0c9f31",
        "status": "no_change_required",
        "summary": "Echoed request for action create_niagara_effect.",
        "understood_action": "create_niagara_effect",
        "understood_target": "/Game/VFX/Spells/NS_Fireball",
        "primary_asset": "/Game/VFX/Spells/NS_Fireball",
        "metrics": {"mcp_round_trips": 1, "internal_operations": 0},
    }
    write_json(GOLDEN / "envelope" / "response_fields.in.json", resp_in)
    write_json(
        GOLDEN / "envelope" / "response.out.expected.json",
        json.loads(serialize_response(resp_in)),
    )

    spec = {
        "name": "Fireball",
        "core_material": {"$ref": "material.result.primary_asset"},
        "trail_material": "$material",
        "effects": [{"$ref": "fx.result.primary_asset"}, "literal"],
    }
    completed = {
        "material": {
            "result": {"primary_asset": "/Game/VFX/Materials/MI_Core"},
            "status": "created_and_validated",
        },
        "fx": {"result": {"primary_asset": "/Game/VFX/Spells/NS_Fireball"}},
    }
    write_json(GOLDEN / "ref" / "spec.in.json", spec)
    write_json(GOLDEN / "ref" / "completed.in.json", completed)
    write_json(GOLDEN / "ref" / "resolved.expected.json", resolve_refs(spec, completed))

    nodes = [
        {"id": "material", "depends_on": []},
        {"id": "trail", "depends_on": []},
        {"id": "fx", "depends_on": ["material", "trail"]},
        {"id": "ability", "depends_on": ["fx"]},
    ]
    write_json(GOLDEN / "topo" / "nodes.in.json", nodes)
    write_json(GOLDEN / "topo" / "order.expected.json", topological_sort(nodes))

    print(f"Wrote goldens under {GOLDEN}")
    print(f"content_hash = {content_hash(graph)}")


if __name__ == "__main__":
    main()
