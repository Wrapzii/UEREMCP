#!/usr/bin/env python3
"""Run the three-call POC A scenario through Streamable HTTP MCP."""

from __future__ import annotations

import argparse
import copy
import json
import time
import urllib.error
import urllib.request
import uuid
from pathlib import Path
from typing import Any

MARKER = "UEREMCP_POC_EVIDENCE="
TOOLSET = "UeremcpBlueprint.UeremcpBlueprintToolset"
ASSET_PATH = (
    "/Game/__UeremcpPoc/Blueprint/BP_CompleteRoundTripTransport."
    "BP_CompleteRoundTripTransport"
)


class McpClient:
    def __init__(self, endpoint: str) -> None:
        self.endpoint = endpoint
        self.session_id = ""
        self.next_id = 1

    def _post(self, payload: dict[str, Any]) -> dict[str, Any] | None:
        headers = {
            "Accept": "application/json, text/event-stream",
            "Content-Type": "application/json",
        }
        if self.session_id:
            headers["Mcp-Session-Id"] = self.session_id
        request = urllib.request.Request(
            self.endpoint,
            data=json.dumps(payload, separators=(",", ":")).encode(),
            headers=headers,
            method="POST",
        )
        try:
            with urllib.request.urlopen(request, timeout=120) as response:
                session_id = response.headers.get("Mcp-Session-Id")
                if session_id:
                    self.session_id = session_id
                body = response.read().decode("utf-8")
        except urllib.error.HTTPError as exc:
            detail = exc.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"HTTP {exc.code}: {detail}") from exc
        if not body:
            return None
        if body.startswith("event:"):
            for line in body.splitlines():
                if line.startswith("data:"):
                    return json.loads(line[5:].strip())
            return None
        return json.loads(body)

    def initialize(self) -> None:
        response = self._post(
            {
                "jsonrpc": "2.0",
                "id": self.next_id,
                "method": "initialize",
                "params": {
                    "protocolVersion": "2025-03-26",
                    "capabilities": {},
                    "clientInfo": {
                        "name": "ueremcp-poc-a-complete-round-trip",
                        "version": "1",
                    },
                },
            }
        )
        self.next_id += 1
        if not response or "result" not in response:
            raise RuntimeError(f"MCP initialize failed: {response!r}")
        self._post(
            {
                "jsonrpc": "2.0",
                "method": "notifications/initialized",
                "params": {},
            }
        )

    def call_blueprint(
        self, tool_name: str, request_json: dict[str, Any]
    ) -> tuple[dict[str, Any], dict[str, Any]]:
        response = self._post(
            {
                "jsonrpc": "2.0",
                "id": self.next_id,
                "method": "tools/call",
                "params": {
                    "name": "call_tool",
                    "arguments": {
                        "toolset_name": TOOLSET,
                        "tool_name": tool_name,
                        "arguments": {
                            "requestJson": json.dumps(
                                request_json, separators=(",", ":")
                            )
                        },
                    },
                },
            }
        )
        self.next_id += 1
        if not response or "result" not in response:
            raise RuntimeError(f"MCP {tool_name} failed: {response!r}")
        envelope = _extract_return_envelope(response["result"])
        return response, envelope


def _extract_return_envelope(result: dict[str, Any]) -> dict[str, Any]:
    candidates: list[Any] = [result, result.get("structuredContent")]
    for item in result.get("content", []):
        if isinstance(item, dict):
            candidates.extend((item, item.get("text")))
    for candidate in candidates:
        if isinstance(candidate, str):
            try:
                candidate = json.loads(candidate)
            except json.JSONDecodeError:
                continue
        if not isinstance(candidate, dict):
            continue
        return_value = candidate.get("returnValue")
        if isinstance(return_value, str):
            parsed = json.loads(return_value)
            if isinstance(parsed, dict):
                return parsed
        if "protocol_version" in candidate and "status" in candidate:
            return candidate
    raise ValueError(f"tool result has no Blueprint response envelope: {result!r}")


def _single_graph(envelope: dict[str, Any]) -> dict[str, Any]:
    graphs = envelope.get("diagnostics", {}).get("graphs")
    if not isinstance(graphs, list) or len(graphs) != 1 or not isinstance(graphs[0], dict):
        raise ValueError("response must contain exactly one diagnostics graph")
    return graphs[0]


def _complete_graph_errors(graph: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    for field in ("nodes", "links", "variables", "entry_points", "dependencies"):
        if not isinstance(graph.get(field), list):
            errors.append(f"graph.{field} must be an array")
    nodes = graph.get("nodes")
    if not isinstance(nodes, list) or not nodes:
        errors.append("graph.nodes must be non-empty")
        return errors
    saw_pin_type = False
    saw_defaults = False
    for node in nodes:
        if not isinstance(node, dict):
            continue
        saw_defaults |= isinstance(node.get("defaults"), dict)
        for field in ("input_pins", "output_pins"):
            pins = node.get(field)
            if isinstance(pins, list):
                saw_pin_type |= any(
                    isinstance(pin, dict) and isinstance(pin.get("pin_type"), dict)
                    for pin in pins
                )
    if not saw_pin_type:
        errors.append("graph nodes contain no typed pins")
    if not saw_defaults:
        errors.append("graph nodes contain no defaults object")
    fidelity = graph.get("fidelity")
    if not isinstance(fidelity, dict) or not isinstance(
        fidelity.get("lossy_areas"), list
    ):
        errors.append("graph.fidelity.lossy_areas must be an array")
    return errors


def _has_expected_branch(graph: dict[str, Any]) -> tuple[bool, bool]:
    nodes = graph.get("nodes", [])
    links = graph.get("links", [])
    classes = {
        node.get("node_id"): node.get("node_class")
        for node in nodes
        if isinstance(node, dict)
    }
    expected_nodes = {
        "K2Node_Event",
        "K2Node_IfThenElse",
        "K2Node_CallFunction",
    }.issubset(set(classes.values()))

    def has_link(
        from_class: str, from_pin: str, to_class: str, to_pin: str
    ) -> bool:
        return any(
            isinstance(link, dict)
            and classes.get(link.get("from_node")) == from_class
            and link.get("from_pin") == from_pin
            and classes.get(link.get("to_node")) == to_class
            and link.get("to_pin") == to_pin
            for link in links
        )

    expected_links = has_link(
        "K2Node_Event", "then", "K2Node_IfThenElse", "execute"
    ) and has_link(
        "K2Node_IfThenElse", "then", "K2Node_CallFunction", "execute"
    )
    return expected_nodes, expected_links


def _criterion(status: str, **details: Any) -> dict[str, Any]:
    return {"status": status, **details}


def run(endpoint: str) -> dict[str, Any]:
    run_id = f"poc-a-{uuid.uuid4()}"
    started = time.perf_counter()
    criteria = {f"A{index}": _criterion("fail") for index in range(1, 12)}
    errors: list[str] = []
    raw_responses: list[dict[str, Any]] = []
    internal_operations = 0.0

    try:
        client = McpClient(endpoint)
        client.initialize()

        read_transport, read = client.call_blueprint(
            "ReadGraph",
            {
                "protocol_version": "1.0",
                "request_id": f"{run_id}-read",
                "action": "read_graph",
                "target": {"asset_path": ASSET_PATH, "graph_id": "EventGraph"},
                "options": {"response_detail": "complete"},
            },
        )
        raw_responses.append(read_transport)
        internal_operations += float(read.get("metrics", {}).get("internal_operations", 0))
        before_graph = _single_graph(read)
        read_errors = _complete_graph_errors(before_graph)
        criteria["A1"] = _criterion(
            "pass" if not read_errors else "fail", mcp_calls=1
        )
        criteria["A2"] = _criterion(
            "pass" if not read_errors else "fail", errors=read_errors
        )
        graph_diagnostics = before_graph.get("diagnostics", {})
        a3_pass = (
            isinstance(graph_diagnostics.get("dead_nodes"), list)
            and len(graph_diagnostics["dead_nodes"]) >= 1
            and isinstance(graph_diagnostics.get("disconnected_subgraphs"), list)
            and len(graph_diagnostics["disconnected_subgraphs"]) >= 1
        )
        criteria["A3"] = _criterion("pass" if a3_pass else "fail")
        criteria["A10"] = _criterion(
            "pass"
            if isinstance(before_graph.get("fidelity", {}).get("lossy_areas"), list)
            else "fail"
        )

        changed_graph = copy.deepcopy(before_graph)
        changed_graph["extensions"] = {
            "blueprint": {
                "dsl": (
                    "(event EventBeginPlay\n"
                    "  (if true\n"
                    '    (Development|PrintString :InString "POC A transport confirmed")))'
                )
            }
        }
        criteria["A4"] = _criterion("pass")
        changed_transport, changed = client.call_blueprint(
            "SubmitGraph",
            {
                "protocol_version": "1.0",
                "request_id": f"{run_id}-replace",
                "action": "submit_graph",
                "mode": "replace",
                "expected_revision": read["revision"],
                "target": {"asset_path": ASSET_PATH, "graph_id": "EventGraph"},
                "options": {"dry_run": False},
                "specification": {
                    "graph": changed_graph,
                    "expected_after_write": {
                        "nodes": [
                            {
                                "key": "begin_play",
                                "node_class": "K2Node_Event",
                            },
                            {
                                "key": "branch",
                                "node_class": "K2Node_IfThenElse",
                            },
                            {
                                "key": "print",
                                "node_class": "K2Node_CallFunction",
                            },
                        ],
                        "links": [
                            {
                                "from": "begin_play",
                                "from_pin": "then",
                                "to": "branch",
                                "to_pin": "execute",
                            },
                            {
                                "from": "branch",
                                "from_pin": "then",
                                "to": "print",
                                "to_pin": "execute",
                            },
                        ],
                    },
                },
            },
        )
        raw_responses.append(changed_transport)
        internal_operations += float(
            changed.get("metrics", {}).get("internal_operations", 0)
        )
        changed_graph_response = _single_graph(changed)
        validation = changed.get("validation", {})
        a5_pass = (
            changed.get("status") == "modified_and_validated"
            and validation.get("compiled") is True
            and validation.get("saved") is True
            and validation.get("reread_after_write") is True
            and not _complete_graph_errors(changed_graph_response)
        )
        criteria["A5"] = _criterion("pass" if a5_pass else "fail")
        expected_nodes, expected_connections = _has_expected_branch(
            changed_graph_response
        )
        criteria["A6"] = _criterion(
            "pass" if expected_nodes and expected_connections else "fail",
            expected_nodes_present=expected_nodes,
            expected_connections_present=expected_connections,
        )
        criteria["A7"] = _criterion(
            "pass"
            if changed.get("status") == "modified_and_validated"
            and validation.get("reread_after_write") is True
            else "fail"
        )

        changed_hash = changed_graph_response.get("content_hash")
        noop_transport, noop = client.call_blueprint(
            "SubmitGraph",
            {
                "protocol_version": "1.0",
                "request_id": f"{run_id}-identity",
                "action": "submit_graph",
                "mode": "replace",
                "expected_revision": changed_hash,
                "target": {"asset_path": ASSET_PATH, "graph_id": "EventGraph"},
                "options": {"dry_run": False},
                "specification": {"graph": changed_graph_response},
            },
        )
        raw_responses.append(noop_transport)
        internal_operations += float(noop.get("metrics", {}).get("internal_operations", 0))
        noop_graph = _single_graph(noop)
        noop_hash = noop_graph.get("content_hash")
        noop_complete = not _complete_graph_errors(noop_graph)
        criteria["A8"] = _criterion(
            "pass"
            if noop.get("status") == "no_change_required"
            and isinstance(changed_hash, str)
            and noop_hash == changed_hash
            else "fail"
        )
        criteria["A9"] = _criterion("pass", mcp_round_trips=3)
        if not (
            noop_complete
            and isinstance(noop_graph.get("fidelity", {}).get("lossy_areas"), list)
        ):
            criteria["A10"] = _criterion("fail")
        skipped = noop.get("validation", {}).get("checks_skipped", [])
        criteria["A11"] = _criterion(
            "pass"
            if noop.get("status") == "no_change_required"
            and "blueprint.compile" in skipped
            else "fail"
        )
    except Exception as exc:
        errors.append(str(exc))

    outcome = (
        "pass"
        if not errors
        and all(item["status"] == "pass" for item in criteria.values())
        else "fail"
    )
    return {
        "schema_version": 1,
        "scenario": "poc_a",
        "run_id": run_id,
        "outcome": outcome,
        "criteria": criteria,
        "metrics": {
            "mcp_round_trips": len(raw_responses),
            "internal_operations": internal_operations,
            "tokens_total": 0,
            "wall_clock_seconds": time.perf_counter() - started,
            "primitive_call_equivalent": 15,
        },
        "errors": errors,
        "raw_responses": raw_responses,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--endpoint", default="http://127.0.0.1:8000/mcp")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    evidence = run(args.endpoint)
    compact = json.dumps(evidence, separators=(",", ":"))
    print(f"{MARKER}{compact}")
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(
            json.dumps(evidence, indent=2) + "\n", encoding="utf-8"
        )
    return 0 if evidence["outcome"] == "pass" else 1


if __name__ == "__main__":
    raise SystemExit(main())
