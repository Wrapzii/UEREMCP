#!/usr/bin/env python3
"""One-shot BuildEnvironment live verification against MCP."""
from __future__ import annotations

import json
import re
import subprocess
import sys
import tempfile

URL = "http://127.0.0.1:8000/mcp"
_next_id = [200]


def _post(payload, session=None, timeout=180):
    hdr = tempfile.NamedTemporaryFile(suffix=".hdr", delete=False)
    hdr.close()
    cmd = [
        "curl", "-s", "-D", hdr.name, "-m", str(timeout), "-X", "POST", URL,
        "-H", "Content-Type: application/json",
        "-H", "Accept: application/json, text/event-stream",
    ]
    if session:
        cmd += ["-H", "Mcp-Session-Id: " + session]
    cmd += ["--data-binary", "@-"]
    proc = subprocess.run(cmd, input=json.dumps(payload).encode(), capture_output=True)
    sid = None
    with open(hdr.name, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if line.lower().startswith("mcp-session-id:"):
                sid = line.split(":", 1)[1].strip()
    import os
    os.unlink(hdr.name)
    return proc.stdout.decode("utf-8", "replace"), sid


def _parse(body):
    body = re.sub(r"^event: .*$", "", body, flags=re.M)
    body = re.sub(r"^data: ", "", body, flags=re.M).strip()
    return json.loads(body) if body else None


class Client:
    def __init__(self):
        _, self.sid = _post({
            "jsonrpc": "2.0", "id": 1, "method": "initialize",
            "params": {"protocolVersion": "2025-06-18", "capabilities": {},
                       "clientInfo": {"name": "ws16-verify", "version": "1"}}})
        if not self.sid:
            raise SystemExit("no MCP session")
        _post({"jsonrpc": "2.0", "method": "notifications/initialized"}, self.sid)

    def call_tool(self, request_json: str) -> dict:
        _next_id[0] += 1
        body, _ = _post({
            "jsonrpc": "2.0", "id": _next_id[0], "method": "tools/call",
            "params": {
                "name": "call_tool",
                "arguments": {
                    "toolset_name": "UeremcpEnvironment.UeremcpEnvironmentToolset",
                    "tool_name": "BuildEnvironment",
                    "arguments": {"RequestJson": request_json},
                },
            },
        }, self.sid)
        data = _parse(body)
        text = data["result"]["content"][0]["text"]
        outer = json.loads(text)
        inner = outer.get("returnValue", text)
        if isinstance(inner, str):
            inner = json.loads(inner)
        return inner


def tech_names(resp: dict) -> list[str]:
    tech = resp.get("real_vs_approximated", {}).get("technologies", [])
    return [t.get("name", "?") for t in tech]


def main() -> int:
    client = Client()
    cases = [
        ("seed_only", {
            "protocol_version": "1.0",
            "action": "build_environment",
            "request_id": "verify-seed-only",
            "target": {"asset_path": "/Game/__UeremcpPoc/SeedOnlyDryRun"},
            "options": {"dry_run": True, "validate": True},
            "specification": {"seed": 1},
        }),
        ("rain_only", {
            "protocol_version": "1.0",
            "action": "build_environment",
            "request_id": "verify-rain-only",
            "target": {"asset_path": "/Game/__UeremcpPoc/RainOnly"},
            "options": {"dry_run": True, "validate": True},
            "specification": {
                "seed": 99,
                "include": {"rain": True},
                "weather": {"follow": "player_camera"},
                "fallback_policy": "prefer_real",
            },
        }),
        ("full_scene", {
            "protocol_version": "1.0",
            "action": "build_environment",
            "request_id": "verify-full-scene",
            "target": {"asset_path": "/Game/__UeremcpPoc/MountainRiverRain"},
            "options": {"dry_run": True, "validate": True},
            "specification": {
                "seed": 42,
                "include": {
                    "terrain": True, "river": True, "forest": True,
                    "rain": True, "lighting": True,
                },
                "terrain": {"mountain_amplitude": 0.55, "valley_depth": 0.12},
                "river": {"width": 600},
                "biome": {"forest_bank_width": 3500, "max_foliage_instances": 800},
                "weather": {"follow": "player_camera"},
                "fallback_policy": "prefer_real",
            },
        }),
    ]
    results = {}
    for label, req in cases:
        resp = client.call_tool(json.dumps(req))
        results[label] = {
            "status": resp.get("status"),
            "summary": resp.get("summary"),
            "technologies": tech_names(resp),
            "full": resp,
        }
    print(json.dumps(results, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
