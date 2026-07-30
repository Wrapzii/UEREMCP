"""POC-B primitive baseline sequence accounting (WS-07 sequence).

This module encodes the *accepted planned sequence* from
`docs/proposals/ws-07-poc-b-primitive-baseline-sequence.md` and computes a
machine-checkable **planned minimum** for known steps.

It does NOT invent measured wall-clock or a measured primitive count. Materials
inner primitives and compile-poll iterations remain OPEN until a live trial.
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

# Semantic roles from poc_b_mcp_fireball_request.json / WS-07 sequence.
FIREBALL_ROLES = (
    "core",
    "flame_shell",
    "sparks",
    "smoke",
    "ribbon_trail",
    "impact_burst",
)

# Ordered minimum from WS-07 proposal. Counts are planned, not measured.
SEQUENCE_STEPS: list[dict[str, Any]] = [
    {
        "id": "optional_replace",
        "required": False,
        "planned_primitive_ops": {"min": 0, "max": 1},
        "tools": ["AssetTools.Delete", "script_helper"],
        "notes": "Delete existing baseline asset if present",
    },
    {
        "id": "create_system",
        "required": True,
        "planned_primitive_ops": {"min": 1, "max": 1},
        "tools": ["CreateNiagaraSystem"],
        "notes": "From MinimalLightweight template",
    },
    {
        "id": "add_emitters",
        "required": True,
        "planned_primitive_ops": {"min": 6, "max": 6},
        "tools": ["AddEmitter"],
        "roles": list(FIREBALL_ROLES),
        "notes": "One AddEmitter per role",
    },
    {
        "id": "user_variables",
        "required": True,
        "planned_primitive_ops": {"min": 1, "max": 4},
        "tools": ["AddUserVariables", "SetVariable"],
        "notes": "colour (primary/secondary), scale, intensity — exact Epic split TBD at trial",
    },
    {
        "id": "set_renderer_data",
        "required": True,
        "planned_primitive_ops": {"min": 6, "max": 6},
        "tools": ["SetRendererData"],
        "roles": list(FIREBALL_ROLES),
        "notes": "Material binding per role with mesh renderer",
    },
    {
        "id": "materials",
        "required": True,
        "planned_primitive_ops": {"min": None, "max": None},
        "tools": ["MaterialTools", "MaterialInstanceTools", "create_vfx_material"],
        "roles": list(FIREBALL_ROLES),
        "notes": "OPEN — each inner material primitive counts; do not invent",
        "status": "open",
    },
    {
        "id": "compile_poll",
        "required": True,
        "planned_primitive_ops": {"min": None, "max": None},
        "tools": ["GetSystemCompileState"],
        "notes": "OPEN — poll loop until UpToDate; iteration count is runtime-dependent",
        "status": "open",
    },
    {
        "id": "save",
        "required": True,
        "planned_primitive_ops": {"min": 1, "max": None},
        "tools": ["SaveAsset", "AssetTools.Save"],
        "notes": "Niagara system + material packages; package count measured at trial",
    },
    {
        "id": "structural_verify",
        "required": True,
        "planned_primitive_ops": {"min": 2, "max": None},
        "tools": ["GetSystemSummary", "GetEmitterTopology"],
        "notes": "B7-equivalent re-read",
    },
]


def planned_known_minimum(*, include_optional_replace: bool = False) -> dict[str, Any]:
    """Sum planned mins for steps whose min is known; leave open steps listed."""
    total = 0
    known_steps: list[str] = []
    open_steps: list[str] = []
    for step in SEQUENCE_STEPS:
        if step["id"] == "optional_replace" and not include_optional_replace:
            continue
        lo = step["planned_primitive_ops"]["min"]
        if lo is None:
            open_steps.append(step["id"])
            continue
        total += int(lo)
        known_steps.append(step["id"])
    return {
        "status": "planned_partial",  # never "measured"
        "known_minimum_primitive_ops": total,
        "known_steps": known_steps,
        "open_steps": open_steps,
        "mcp_round_trips_planned_batched": {
            "script": 1,
            "materials_if_separate": "OPEN — 0 if inside script, else tally separate MCP calls",
        },
        "notes": (
            "Planned minimum from WS-07 sequence for known Niagara steps only. "
            "Not a measured baseline. Materials + compile polls remain OPEN. "
            "Do not equate to UEREMCP internal_operations."
        ),
    }


def comparability_audit(ueremcp_internal_operations: int | None = 46) -> dict[str, Any]:
    """Audit whether internal_operations is comparable to the primitive baseline."""
    return {
        "ueremcp_internal_operations": ueremcp_internal_operations,
        "comparable_to_mcp_round_trips": False,
        "reason_not_round_trips": (
            "internal_operations counts domain-internal editor steps inside one MCP call; "
            "mcp_round_trips counts agent-visible MCP hops. Conflating them erases the "
            "headline ratio (defs.schema.json metrics description)."
        ),
        "comparable_to_epic_primitive_baseline": False,
        "reason_not_epic_baseline": (
            "UEREMCP increments InternalOperations on its own create/bind/material/inspect "
            "grain (UeremcpNiagaraCreate / MaterialBinding / Inspect). The WS-07 baseline "
            "counts Epic call_tool invocations inside execute_tool_script plus material "
            "chain primitives. Same semantic goal, different counter grain — measure the "
            "baseline separately; do not substitute 46."
        ),
        "allowed_ratio_narrative": (
            "Once baseline primitive_ops and mcp_round_trips are measured, report "
            "UEREMCP mcp_round_trips vs baseline mcp_round_trips, and optionally "
            "UEREMCP internal_operations vs baseline primitive_ops as a secondary "
            "grain-matched comparison only if the trial log proves matching grain."
        ),
    }


def load_trial_record(path: str | Path) -> dict[str, Any]:
    """Load a measured baseline trial JSON produced by the live harness."""
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    required = ("primitive_ops_executed", "mcp_round_trips", "wall_clock_seconds", "completed")
    missing = [k for k in required if k not in data]
    if missing:
        raise ValueError(f"trial record missing fields: {missing}")
    return data


def summarize_trials(trials: list[dict[str, Any]]) -> dict[str, Any]:
    """Aggregate measured trials; refuses to invent values when list is empty."""
    if not trials:
        return {
            "status": "open",
            "n": 0,
            "primitive_ops": None,
            "mcp_round_trips": None,
            "wall_clock_seconds": None,
            "notes": "no measured baseline trials yet",
        }
    ops = [int(t["primitive_ops_executed"]) for t in trials]
    trips = [int(t["mcp_round_trips"]) for t in trials]
    walls = [float(t["wall_clock_seconds"]) for t in trials]
    return {
        "status": "measured",
        "n": len(trials),
        "primitive_ops": {"min": min(ops), "max": max(ops), "mean": sum(ops) / len(ops)},
        "mcp_round_trips": {"min": min(trips), "max": max(trips), "mean": sum(trips) / len(trips)},
        "wall_clock_seconds": {
            "min": min(walls),
            "max": max(walls),
            "mean": sum(walls) / len(walls),
        },
        "completion_rate": sum(1 for t in trials if t.get("completed")) / len(trials),
    }
