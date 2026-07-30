#!/usr/bin/env python3
"""Validate WS-04 transport job handoff JSON and job-model invariants.

Runs without Unreal. C++ parity: UeremcpTransport::CapabilityFlagsToJson.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HANDOFF = ROOT / "constraints" / "transport_job_handoff.json"
PLUGIN_SOURCE = ROOT.parent
REGISTRY_HEADER = PLUGIN_SOURCE / "UeremcpProtocol" / "Public" / "UeremcpJobRegistry.h"
REFERENCE_TOOLSET_HEADER = (
    PLUGIN_SOURCE / "UeremcpCore" / "Public" / "UeremcpReferenceToolset.h"
)
BUILD_RULES = ROOT / "UeremcpTransport.Build.cs"
AUTOMATION_TESTS = ROOT / "Private" / "Tests" / "UeremcpTransportAutomationTests.cpp"
SCHEDULER_HEADER = ROOT / "Public" / "UeremcpJobScheduler.h"
SCHEDULER_SOURCE = ROOT / "Private" / "UeremcpJobScheduler.cpp"

REQUIRED_JOB_CAPABILITIES = {
    "In-process job registry with stable job_id (envelope job block)",
    "get_job_result poll tool/action",
    "Cooperative cancellation wired from MCP cancel to domain work",
    "Semantic progress mapping (engine heartbeat is not percent-complete)",
    "timeout_ms enforcement returning partially_completed + job handle",
    "Crash recovery is out of scope for Wave 1 — jobs are in-memory only",
}

REQUIRED_REGISTRY_SYMBOLS = {
    "FUeremcpJobRegistry",
    "CreateJob(",
    "CompleteJob(",
    "CancelJob(",
    "GetTimeoutResponse(",
    "GetJobResult(",
}


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

    timeout_values = [
        defaults.get("min_timeout_ms"),
        defaults.get("default_timeout_ms"),
        defaults.get("max_timeout_ms"),
    ]
    if all(isinstance(value, int) for value in timeout_values):
        minimum, default, maximum = timeout_values
        if not 0 < minimum <= default <= maximum:
            errors.append(
                "job timeout bounds must satisfy 0 < min_timeout_ms "
                "<= default_timeout_ms <= max_timeout_ms"
            )

    ws05 = data.get("ws05_constraints", {})
    required_ws05_values = {
        "dispatch_inline_when_timeout_ms_zero": True,
        "dispatch_poll_when_timeout_ms_positive": True,
        "never_hold_mcp_sse_open_past_client_timeout": True,
        "job_id_scope": "per-editor-process",
        "mcp_round_trips_metric_includes_polls": True,
    }
    for key, expected in required_ws05_values.items():
        if ws05.get(key) != expected:
            errors.append(f"ws05_constraints.{key} must be {expected!r}")

    declared_job_capabilities = data.get("ueremcp_must_build")
    if not isinstance(declared_job_capabilities, list):
        errors.append("ueremcp_must_build must be an array")
    else:
        missing_capabilities = REQUIRED_JOB_CAPABILITIES.difference(declared_job_capabilities)
        for capability in sorted(missing_capabilities):
            errors.append(f"ueremcp_must_build missing: {capability}")

    gate_status = validate_unskip_gate(errors)

    if errors:
        for err in errors:
            print(f"FAIL: {err}", file=sys.stderr)
        return 1

    print(f"OK: {HANDOFF.name} ({data.get('handoff_version')})")
    print(f"OK: JobRegistry unskip gate {gate_status}")
    return 0


def validate_unskip_gate(errors: list[str]) -> str:
    """Prevent landed, callable registry symbols from silently retaining SKIPs."""
    for path in (BUILD_RULES, AUTOMATION_TESTS):
        if not path.is_file():
            errors.append(f"missing Transport source required by unskip gate: {path}")
            return "invalid"

    automation_source = AUTOMATION_TESTS.read_text(encoding="utf-8")
    skip_count = automation_source.count("UeremcpTransportTest::SkipMissingApi(")
    required_pending_tests = {
        '"UEREMCP.Transport.JobRegistry.Poll"',
        '"UEREMCP.Transport.JobRegistry.Cancel"',
    }
    for test_name in sorted(required_pending_tests):
        if test_name not in automation_source:
            errors.append(f"missing Transport registry test path: {test_name}")

    if "FUeremcpTransportTimeoutPartialResponseTest" not in automation_source:
        errors.append(
            "Timeout.PartiallyCompleted must retain its active response-contract test "
            "while lifecycle assertions await the registry"
        )

    if not REGISTRY_HEADER.is_file():
        if skip_count != 2:
            errors.append(
                "registry is not landed, so exactly two explicit JobRegistry SKIPs "
                f"must remain (found {skip_count})"
            )
        return "pending (registry header not landed; 2 explicit SKIPs required)"

    registry_source = REGISTRY_HEADER.read_text(encoding="utf-8")
    missing_symbols = sorted(
        symbol for symbol in REQUIRED_REGISTRY_SYMBOLS if symbol not in registry_source
    )
    if missing_symbols:
        return "pending (registry header present but incomplete: " + ", ".join(missing_symbols) + ")"

    build_source = BUILD_RULES.read_text(encoding="utf-8")
    if '"UeremcpProtocol"' not in build_source:
        if skip_count != 2:
            errors.append(
                "registry is not callable from Transport, so exactly two explicit "
                f"SKIPs must remain (found {skip_count})"
            )
        return "pending (registry landed but UeremcpProtocol dependency missing)"

    if skip_count:
        errors.append(
            "callable FUeremcpJobRegistry surface is present but Transport still has "
            f"{skip_count} explicit JobRegistry SKIP bodies"
        )

    validate_wrapper_and_residual_scope(automation_source, errors)
    return "ready (scheduler active; notification cancellation residual tracked)"


def validate_wrapper_and_residual_scope(
    automation_source: str, errors: list[str]
) -> None:
    """Keep active wrapper coverage and remaining transport limitations explicit."""
    if not REFERENCE_TOOLSET_HEADER.is_file():
        errors.append(f"missing AICallable wrapper header: {REFERENCE_TOOLSET_HEADER}")
        return

    wrapper_source = REFERENCE_TOOLSET_HEADER.read_text(encoding="utf-8")
    for wrapper in ("GetJobResult", "CancelJob"):
        pattern = (
            r"UFUNCTION\(meta\s*=\s*\(AICallable\).*?\)\s*"
            rf"static\s+FString\s+{wrapper}\s*\("
        )
        if not re.search(pattern, wrapper_source, re.DOTALL):
            errors.append(f"{wrapper} must remain an AICallable ReferenceToolset wrapper")

        invocation = f"UUeremcpReferenceToolset::{wrapper}("
        if invocation not in automation_source:
            errors.append(f"Transport tests must invoke active wrapper: {invocation}")

    residual_markers = {
        "MCP notifications/cancelled is not mapped": "cancellation notification",
    }
    for marker, residual_name in residual_markers.items():
        if marker not in automation_source:
            errors.append(f"missing honest {residual_name} residual marker")

    residual_count = automation_source.count("SKIP residual:")
    if residual_count != len(residual_markers):
        errors.append(
            "Transport must carry exactly the verified notification-cancellation "
            f"residual SKIP; found {residual_count}"
        )

    for path in (SCHEDULER_HEADER, SCHEDULER_SOURCE):
        if not path.is_file():
            errors.append(f"missing production timeout scheduler source: {path}")
    if SCHEDULER_HEADER.is_file() and "FUeremcpJobScheduler" not in SCHEDULER_HEADER.read_text(
        encoding="utf-8"
    ):
        errors.append("production timeout scheduler header missing FUeremcpJobScheduler")
    if "FUeremcpJobScheduler::Dispatch(" not in automation_source:
        errors.append("Timeout.PartiallyCompleted must invoke the production scheduler")

    poll_start = automation_source.find("bool FUeremcpTransportJobRegistryPollTest::RunTest")
    cancel_start = automation_source.find("bool FUeremcpTransportJobRegistryCancelTest::RunTest")
    if poll_start < 0 or cancel_start < 0:
        errors.append("cannot locate active Poll/Cancel Transport test bodies")
    elif "SKIP residual:" in automation_source[poll_start:cancel_start]:
        errors.append("JobRegistry.Poll must remain fully active with no residual SKIP")


if __name__ == "__main__":
    raise SystemExit(main())
