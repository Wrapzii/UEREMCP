#!/usr/bin/env python3
"""Attempt and archive the canonical POC-B B1 request over live HTTP MCP."""

from __future__ import annotations

import argparse
import json
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

try:
    from .run_poc_b_primitive_live import McpClient, return_value
except ImportError:
    from run_poc_b_primitive_live import McpClient, return_value


def b1_failures(envelope: Any) -> list[str]:
    if not isinstance(envelope, dict):
        return ["returnValue is not an object"]
    failures: list[str] = []
    if envelope.get("metrics", {}).get("mcp_round_trips") != 1:
        failures.append("metrics.mcp_round_trips")
    validation = envelope.get("validation", {})
    gates = envelope.get("poc_b_gates", {})
    # Current envelopes serialize these extension payloads at top level. Accept
    # the older nested representation too so archived fixtures remain scorable.
    if validation.get("single_request_pipeline") is not True:
        validation = envelope.get("extra", {}).get("validation", {})
    if gates.get("B1_single_request_complete") is not True:
        gates = envelope.get("extra", {}).get("poc_b_gates", {})
    if validation.get("single_request_pipeline") is not True:
        failures.append("validation.single_request_pipeline")
    if gates.get("B1_single_request_complete") is not True:
        failures.append("poc_b_gates.B1_single_request_complete")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", default="http://127.0.0.1:8001/mcp")
    parser.add_argument("--request", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument(
        "--unique-request",
        action="store_true",
        help="Suffix transport identifiers to prevent idempotent replay caching",
    )
    args = parser.parse_args()

    fixture = json.loads(args.request.read_text(encoding="utf-8-sig"))
    request_payload = fixture["request"]
    if args.unique_request:
        suffix = str(time.time_ns())
        request_payload["request_id"] += f"-{suffix}"
        request_payload["idempotency_key"] += f"-{suffix}"
    client = McpClient(args.endpoint)
    client.initialize()
    started_utc = datetime.now(timezone.utc).isoformat()
    started = time.perf_counter()
    raw = client.call_tool(
        "UeremcpNiagara.UeremcpNiagaraToolset",
        "CreateNiagaraEffect",
        {"requestJson": json.dumps(request_payload, separators=(",", ":"))},
    )
    wall_clock_seconds = time.perf_counter() - started
    envelope = return_value(raw)
    failures = b1_failures(envelope)
    artifact = {
        "schema_version": 1,
        "scenario_id": "poc-b-fireball-b1-live-mcp",
        "transport": "streamable_http_mcp",
        "endpoint": args.endpoint,
        "started_utc": started_utc,
        "wall_clock_seconds": wall_clock_seconds,
        "outer_mcp_round_trips": 1,
        "canonical_request_with_unique_transport_identifiers": args.unique_request,
        "b1_transport_result": "PASS" if not failures else "FAIL",
        "b1_failures": failures,
        "response": envelope,
        "raw_mcp_response": raw,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(artifact, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(
        {
            "output": str(args.output),
            "b1_transport_result": artifact["b1_transport_result"],
            "wall_clock_seconds": wall_clock_seconds,
            "b1_failures": failures,
            "response_status": (
                envelope.get("status") if isinstance(envelope, dict) else None
            ),
        },
        indent=2,
    ))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
