#!/usr/bin/env python3
"""Run and archive clean POC-B primitive-baseline trials over HTTP MCP."""

from __future__ import annotations

import argparse
import json
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


OUTPUT_PACKAGES = [
    "/Game/__UeremcpPoc/NS_POCB_Fireball_Baseline",
    "/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_core",
    "/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_flame_shell",
    "/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_sparks",
    "/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_smoke",
    "/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_ribbon_trail",
    "/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_Baseline_impact_burst",
]


class McpClient:
    def __init__(self, endpoint: str) -> None:
        self.endpoint = endpoint
        self.session_id = ""
        # The live bridge can replay a prior response when a new session reuses
        # the same JSON-RPC id. Seed ids uniquely so retries remain independent.
        self.next_id = time.time_ns()

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
            with urllib.request.urlopen(request, timeout=300) as response:
                session_id = response.headers.get("Mcp-Session-Id")
                if session_id:
                    self.session_id = session_id
                body = response.read().decode("utf-8")
        except urllib.error.HTTPError as error:
            detail = error.read().decode("utf-8", errors="replace")
            raise RuntimeError(f"HTTP {error.code}: {detail}") from error
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
                        "name": "ueremcp-ws14-poc-b-primitive-baseline",
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

    def call_tool(
        self, toolset_name: str, tool_name: str, arguments: dict[str, Any]
    ) -> dict[str, Any]:
        response = self._post(
            {
                "jsonrpc": "2.0",
                "id": self.next_id,
                "method": "tools/call",
                "params": {
                    "name": "call_tool",
                    "arguments": {
                        "toolset_name": toolset_name,
                        "tool_name": tool_name,
                        "arguments": arguments,
                    },
                },
            }
        )
        self.next_id += 1
        if not response or "result" not in response:
            raise RuntimeError(f"MCP {tool_name} failed: {response!r}")
        return response


def extract_wrapper(response: dict[str, Any]) -> dict[str, Any]:
    result = response["result"]
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
        if isinstance(candidate, dict) and "returnValue" in candidate:
            return candidate
    raise ValueError(f"MCP result has no call_tool returnValue: {response!r}")


def return_value(response: dict[str, Any]) -> Any:
    value = extract_wrapper(response)["returnValue"]
    if isinstance(value, str):
        try:
            return json.loads(value)
        except json.JSONDecodeError:
            return value
    return value


def clean_outputs(client: McpClient, execute_cleanup: bool) -> list[str]:
    packages_json = json.dumps(OUTPUT_PACKAGES)
    cleanup_nonce = time.time_ns()
    cleanup_script = f"""
import json

OUTPUT_PACKAGES = {packages_json}

def call(tool_name, arguments):
    return execute_tool(tool_name, json.dumps(arguments))["returnValue"]

def run():
    existing = [
        package
        for package in OUTPUT_PACKAGES
        if call("editor_toolset.toolsets.asset.AssetTools.exists", {{"path": package}})
    ]
    if existing and not {execute_cleanup!r}:
        return {{"status": "dry_run", "existing": existing, "removed": []}}
    dirty = [
        package
        for package in existing
        if call(
            "editor_toolset.toolsets.asset.AssetTools.is_dirty",
            {{"asset_path": package}},
        )
    ]
    if dirty:
        if not call(
            "editor_toolset.toolsets.asset.AssetTools.save_assets",
            {{"asset_paths": dirty}},
        ):
            raise RuntimeError("failed to save dirty controlled outputs")
    removed = []
    for package in existing:
        if not call("editor_toolset.toolsets.asset.AssetTools.delete", {{"path": package}}):
            fallback = (
                "/Game/__UeremcpPoc/__BenchmarkCleanup/"
                + package.rsplit("/", 1)[-1]
                + "_{cleanup_nonce}"
            )
            if not call(
                "editor_toolset.toolsets.asset.AssetTools.move",
                {{"path": package, "new_path": fallback}},
            ):
                raise RuntimeError("failed to move controlled output " + package)
            if not call(
                "editor_toolset.toolsets.asset.AssetTools.delete",
                {{"path": fallback}},
            ):
                raise RuntimeError("failed to delete moved controlled output " + fallback)
        removed.append(package)
    remaining = [
        package
        for package in OUTPUT_PACKAGES
        if call("editor_toolset.toolsets.asset.AssetTools.exists", {{"path": package}})
    ]
    if remaining:
        raise RuntimeError("controlled outputs remain after cleanup: " + json.dumps(remaining))
    return {{"status": "clean", "existing": existing, "removed": removed}}

# Unique outer request avoids transport replay of destructive calls.
# cleanup nonce: {cleanup_nonce}
"""
    cleanup = return_value(
        client.call_tool(
            "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
            "execute_tool_script",
            {"script": cleanup_script},
        )
    )
    if cleanup.get("status") == "dry_run":
        raise RuntimeError(
            "controlled outputs exist; rerun with --execute-cleanup: "
            + json.dumps(cleanup["existing"])
        )
    return cleanup["removed"]


def trial_is_usable(value: Any) -> tuple[bool, list[str]]:
    failures: list[str] = []
    if not isinstance(value, dict):
        return False, ["returnValue is not an object"]
    trace = value.get("primitive_trace")
    checks = [
        (value.get("status") == "created_and_validated", "status"),
        (value.get("completed") is True, "completed"),
        (isinstance(trace, list), "primitive_trace"),
        (
            isinstance(trace, list)
            and value.get("primitive_ops_executed") == len(trace),
            "primitive_ops_executed",
        ),
        (
            isinstance(trace, list)
            and all(isinstance(item, dict) and item.get("ok") is True for item in trace),
            "primitive_trace.ok",
        ),
        (len(value.get("emitters", [])) == 6, "emitters"),
        (len(value.get("user_variables", [])) == 4, "user_variables"),
        (
            len(value.get("renderer_bindings_verified", [])) >= 6,
            "renderer_bindings_verified",
        ),
        (
            value.get("compile_state", {}).get("bIsCompiling") is False,
            "compile_state.bIsCompiling",
        ),
        (
            value.get("compile_state", {}).get("bIsStale") is False,
            "compile_state.bIsStale",
        ),
        (
            value.get("compile_state", {}).get("bHasErrors") is False,
            "compile_state.bHasErrors",
        ),
    ]
    failures.extend(name for passed, name in checks if not passed)
    return not failures, failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", default="http://127.0.0.1:8001/mcp")
    parser.add_argument("--fixture", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--trials", type=int, default=3)
    parser.add_argument(
        "--execute-cleanup",
        action="store_true",
        help="Delete only the seven controlled POC-B baseline outputs before each trial.",
    )
    args = parser.parse_args()

    script = args.fixture.read_text(encoding="utf-8-sig")
    client = McpClient(args.endpoint)
    client.initialize()
    trials: list[dict[str, Any]] = []

    for index in range(1, args.trials + 1):
        removed = clean_outputs(client, args.execute_cleanup)
        started_utc = datetime.now(timezone.utc).isoformat()
        started = time.perf_counter()
        raw = client.call_tool(
            "editor_toolset.toolsets.programmatic.ProgrammaticToolset",
            "execute_tool_script",
            {
                "script": (
                    script
                    + f"\n# Unique outer request avoids transport replay.\n"
                    + f"# trial nonce: {time.time_ns()}\n"
                )
            },
        )
        wall_clock_seconds = time.perf_counter() - started
        value = return_value(raw)
        usable, failures = trial_is_usable(value)
        trials.append(
            {
                "trial": index,
                "started_utc": started_utc,
                "clean_state_removed": removed,
                "wall_clock_seconds": wall_clock_seconds,
                "usable": usable,
                "acceptance_failures": failures,
                "primitive_ops_executed": (
                    value.get("primitive_ops_executed")
                    if isinstance(value, dict)
                    else None
                ),
                "result": value,
                "raw_mcp_response": raw,
            }
        )

    artifact = {
        "schema_version": 1,
        "scenario_id": "poc-b-fireball-primitive-baseline",
        "arm": "epic_reagenttools_primitives",
        "transport": "streamable_http_mcp",
        "endpoint": args.endpoint,
        "outer_mcp_round_trips_per_trial": 1,
        "timer": "time.perf_counter immediately around execute_tool_script",
        "fixture": str(args.fixture),
        "trials_requested": args.trials,
        "trials_usable": sum(trial["usable"] for trial in trials),
        "trials": trials,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(artifact, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(
        {
            "output": str(args.output),
            "trials": [
                {
                    "trial": trial["trial"],
                    "usable": trial["usable"],
                    "primitive_ops_executed": trial["primitive_ops_executed"],
                    "wall_clock_seconds": trial["wall_clock_seconds"],
                    "acceptance_failures": trial["acceptance_failures"],
                }
                for trial in trials
            ],
        },
        indent=2,
    ))
    return 0 if all(trial["usable"] for trial in trials) else 1


if __name__ == "__main__":
    raise SystemExit(main())
