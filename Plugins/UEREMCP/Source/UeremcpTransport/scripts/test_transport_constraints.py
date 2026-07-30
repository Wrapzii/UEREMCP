#!/usr/bin/env python3
"""Validate WS-04 transport job handoff JSON and job-model invariants.

Runs without Unreal. C++ parity: UeremcpTransport::CapabilityFlagsToJson.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HANDOFF = ROOT / "constraints" / "transport_job_handoff.json"


def main() -> int:
    if not HANDOFF.is_file():
        print(f"missing handoff file: {HANDOFF}", file=sys.stderr)
        return 1

    data = json.loads(HANDOFF.read_text(encoding="utf-8"))
    errors: list[str] = []

    caps = data.get("capabilities", {})
    required_bools = [
        "http_transport_only",
        "streamable_http_sse",
        "stdio_transport",
        "mcp_resources",
        "mcp_progress_notifications",
        "mcp_cancellation_notification",
        "toolset_registry_cancel_wired",
        "persistent_server_push",
        "engine_job_ids",
        "engine_auth",
        "origin_localhost_guard",
    ]
    for key in required_bools:
        if key not in caps:
            errors.append(f"capabilities.{key} missing")

    if caps.get("stdio_transport") is not False:
        errors.append("stdio_transport must be false (HTTP-only substrate)")

    if caps.get("engine_job_ids") is not False:
        errors.append("engine_job_ids must be false (UEREMCP owns job registry)")

    if caps.get("toolset_registry_cancel_wired") is not False:
        errors.append("toolset_registry_cancel_wired must be false until Epic wires CancelAsync")

    defaults = data.get("job_defaults", {})
    if defaults.get("poll_action") != "get_job_result":
        errors.append("job_defaults.poll_action must be get_job_result")

    for bound in ("default_timeout_ms", "min_timeout_ms", "max_timeout_ms"):
        if not isinstance(defaults.get(bound), int):
            errors.append(f"job_defaults.{bound} must be int")

    ws05 = data.get("ws05_constraints", {})
    if ws05.get("dispatch_inline_when_timeout_ms_zero") is not True:
        errors.append("ws05_constraints.dispatch_inline_when_timeout_ms_zero must be true")

    if errors:
        for err in errors:
            print(f"FAIL: {err}", file=sys.stderr)
        return 1

    print(f"OK: {HANDOFF.name} ({data.get('handoff_version')})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
