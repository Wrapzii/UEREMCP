"""Parse UEREMCP MCP response JSON for metrics fields."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


def load_response(path_or_obj: str | Path | dict[str, Any]) -> dict[str, Any]:
    if isinstance(path_or_obj, dict):
        return path_or_obj
    text = Path(path_or_obj).read_text(encoding="utf-8")
    return json.loads(text)


def extract_envelope_metrics(response: dict[str, Any]) -> dict[str, Any]:
    """Extract metrics.* from a response envelope (or a wrapped MCP tool result)."""
    root = response
    # Common wrappers: {"result": {...}}, {"content":[{"text":"{...}"}]}
    if "metrics" not in root and isinstance(root.get("result"), dict):
        root = root["result"]
    if "metrics" not in root and isinstance(root.get("ResponseJson"), str):
        root = json.loads(root["ResponseJson"])
    metrics = root.get("metrics")
    if not isinstance(metrics, dict):
        raise ValueError("response missing metrics object")

    out: dict[str, Any] = {
        "mcp_round_trips": metrics.get("mcp_round_trips"),
        "internal_operations": metrics.get("internal_operations"),
        "timing_ms": metrics.get("timing_ms"),
        "replayed": metrics.get("replayed"),
        "status": root.get("status"),
        "poc_b_gates": None,
    }
    extra = root.get("extra") or root.get("result") or {}
    if isinstance(extra, dict):
        gates = extra.get("poc_b_gates")
        if isinstance(gates, dict):
            out["poc_b_gates"] = gates
    return out


def assert_poc_b_b1_slice(extracted: dict[str, Any]) -> list[str]:
    """Return list of failures for the B1 metrics slice (not overall POC B)."""
    failures: list[str] = []
    if extracted.get("mcp_round_trips") != 1:
        failures.append(f"mcp_round_trips expected 1, got {extracted.get('mcp_round_trips')}")
    ops = extracted.get("internal_operations")
    if not isinstance(ops, (int, float)) or ops < 1:
        failures.append(f"internal_operations expected >=1, got {ops}")
    gates = extracted.get("poc_b_gates") or {}
    if gates.get("B1_single_request_complete") is False:
        failures.append("poc_b_gates.B1_single_request_complete is false")
    return failures
