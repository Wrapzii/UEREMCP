#!/usr/bin/env python3
"""POC-B metrics live-run harness (ready; do not contend with ws01 editor).

Measures client wall-clock around one CreateNiagaraEffect MCP call, parses the
response metrics, and records token accounting honesty.

Coordination required before use:
  - RE plugin junction must NOT be switched away from the active WS-07/ws01 tip
    unless WS-01/WS-07 explicitly clear the editor.
  - Prefer a dedicated editor on a separate project copy, or wait for junction
    handoff.

Usage (when coordinated)::

    python docs/reviews/metrics/run_poc_b_metrics.py \\
        --request schemas/domains/niagara/fixtures/poc_b_mcp_fireball_request.json \\
        --out docs/reviews/metrics/artifacts/poc_b_ueremcp_trial.json \\
        --transport cursor_mcp

Without --execute, prints the exact planned call and exits 0 (dry prepare).
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
_REVIEWS = str(HERE.parent)
if _REVIEWS not in sys.path:
    sys.path.insert(0, _REVIEWS)

from metrics.parse_response import assert_poc_b_b1_slice, extract_envelope_metrics, load_response  # noqa: E402
from metrics.token_accounting import resolve_token_accounting  # noqa: E402
from metrics.acceptance import wall_clock_from_client  # noqa: E402


_SCHEMA_REQUEST = REPO / "schemas/domains/niagara/fixtures/poc_b_mcp_fireball_request.json"
_MIRROR_REQUEST = HERE / "fixtures" / "poc_b_mcp_fireball_request.json"
DEFAULT_REQUEST = _SCHEMA_REQUEST if _SCHEMA_REQUEST.exists() else _MIRROR_REQUEST


def load_canonical_request(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    if "request" in data and "mcp_handoff" in data:
        return data
    raise ValueError(f"expected fixture with mcp_handoff+request: {path}")


def planned_call(fixture: dict) -> dict:
    handoff = fixture["mcp_handoff"]
    return {
        "toolset_name": handoff["toolset"],
        "tool_name": handoff["tool"],
        "arguments": {
            handoff.get("request_field", "RequestJson"): json.dumps(
                fixture["request"], separators=(",", ":")
            )
        },
        "notes": (
            "Invoke via Cursor MCP user-unreal-mcp.call_tool OR an HTTP MCP client "
            "that can return the raw tool result. Capture monotonic time immediately "
            "before and after the call."
        ),
    }


def run_execute_stub() -> None:
    raise SystemExit(
        "Live --execute is intentionally disabled in this worktree harness to avoid "
        "contending with the shared RE editor/junction (WS-07 on ws01). "
        "Re-run with coordination, or have WS-11 execute and drop the trial JSON "
        "into docs/reviews/metrics/artifacts/."
    )


def build_trial_record(
    *,
    fixture: dict,
    response_obj: dict | None,
    start_mono: float | None,
    end_mono: float | None,
    transport: str,
    usage: dict | None,
) -> dict:
    wall = wall_clock_from_client(start_mono, end_mono, evidence="run_poc_b_metrics.py")
    tokens = resolve_token_accounting(usage=usage, transport=transport)
    extracted = None
    b1_failures = ["no response captured"]
    if response_obj is not None:
        extracted = extract_envelope_metrics(response_obj)
        b1_failures = assert_poc_b_b1_slice(extracted)
    return {
        "scenario_id": "poc-b-fireball-ueremcp",
        "arm": "ueremcp",
        "canonical_request_id": fixture["request"].get("request_id"),
        "planned_call": planned_call(fixture),
        "wall_clock_seconds": wall.as_dict(),
        "tokens": tokens,
        "response_metrics": extracted,
        "b1_slice_failures": b1_failures,
        "server_side_lower_bound_seconds": {
            "status": "see_log_parse",
            "notes": "parse separately with parse_editor_log.measure_server_side_interval_file",
        },
    }


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--request", type=Path, default=DEFAULT_REQUEST)
    p.add_argument("--out", type=Path, default=HERE / "artifacts" / "poc_b_ueremcp_trial.json")
    p.add_argument("--transport", choices=("cursor_mcp", "http_mcp_wire"), default="cursor_mcp")
    p.add_argument("--response", type=Path, help="Optional already-captured response JSON to score")
    p.add_argument("--execute", action="store_true", help="Live MCP call (blocked unless coordinated)")
    p.add_argument(
        "--allow-live",
        action="store_true",
        help="Required together with --execute after junction coordination",
    )
    args = p.parse_args(argv)

    fixture = load_canonical_request(args.request)
    call = planned_call(fixture)

    if args.execute:
        if not args.allow_live:
            print(json.dumps({"error": "refuse_uncoordinated_live_run", "planned_call": call}, indent=2))
            run_execute_stub()
        run_execute_stub()

    response_obj = load_response(args.response) if args.response else None
    # When scoring a pre-captured response without client timestamps, wall clock stays unavailable.
    start = end = None
    record = build_trial_record(
        fixture=fixture,
        response_obj=response_obj,
        start_mono=start,
        end_mono=end,
        transport=args.transport,
        usage=None,
    )
    record["prepared"] = True
    record["execute_blocked_reason"] = (
        "Shared RE editor/junction owned by ws01/WS-07; harness prepared only"
    )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"wrote": str(args.out), "planned_call": call, "tokens": record["tokens"]}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
