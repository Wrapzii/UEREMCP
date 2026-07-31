#!/usr/bin/env python
"""Targeted live MCP verification for deploy consolidation."""
from __future__ import annotations

import json
import re
import subprocess
import sys
import tempfile

URL = "http://127.0.0.1:8000/mcp"
_next = 200


def post(payload, session=None, timeout=120):
    hdr = tempfile.NamedTemporaryFile(suffix=".hdr", delete=False)
    hdr.close()
    cmd = [
        "curl.exe", "-s", "-D", hdr.name, "-m", str(timeout), "-X", "POST", URL,
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


def parse(body):
    body = re.sub(r"^event: .*$", "", body, flags=re.M)
    body = re.sub(r"^data: ", "", body, flags=re.M).strip()
    return json.loads(body) if body else None


class Client:
    def __init__(self):
        _, self.sid = post({
            "jsonrpc": "2.0", "id": 1, "method": "initialize",
            "params": {"protocolVersion": "2025-06-18", "capabilities": {},
                       "clientInfo": {"name": "deploy-verify", "version": "1"}}})
        if not self.sid:
            raise SystemExit("no MCP session on :8000")
        post({"jsonrpc": "2.0", "method": "notifications/initialized"}, self.sid)

    def call(self, name, args, timeout=180):
        global _next
        _next += 1
        body, _ = post({"jsonrpc": "2.0", "id": _next, "method": "tools/call",
                         "params": {"name": name, "arguments": args}}, self.sid, timeout)
        data = parse(body)
        if not isinstance(data, dict) or "result" not in data:
            return data
        out = []
        for chunk in data["result"].get("content", []):
            if chunk.get("type") == "text":
                text = chunk["text"]
                try:
                    text = json.loads(text)
                except ValueError:
                    pass
                out.append(text)
        return out[0] if len(out) == 1 else out


def main() -> int:
    c = Client()
    listing = c.call("list_toolsets", {})
    text = listing if isinstance(listing, str) else json.dumps(listing)
    names = re.findall(r"^-\s+([A-Za-z0-9_]+(?:\.[A-Za-z0-9_]+)+):", text, flags=re.M)
    required = [
        "UeremcpEnvironment.UeremcpEnvironmentToolset",
        "UeremcpSystems.UeremcpSystemsToolset",
        "UeremcpCore.UeremcpReferenceToolset",
        "UeremcpTemplates.UeremcpTemplatesToolset",
    ]
    print("list_toolsets count:", len(names))
    for req in required:
        ok = req in names
        print("  required", req, "OK" if ok else "MISSING")
        if not ok:
            return 1

    env = c.call("describe_toolset", {"toolset_name": "UeremcpEnvironment.UeremcpEnvironmentToolset"})
    if isinstance(env, str):
        try:
            env = json.loads(env)
        except ValueError:
            pass
    build = None
    for tool in (env or {}).get("tools", []):
        if tool.get("name", "").endswith(".BuildEnvironment"):
            build = tool
            break
    schema = (build or {}).get("inputSchema") or {}
    spec = (schema.get("properties") or {}).get("specification") or {}
    publishing = (
        (env or {}).get("x-ueremcp-schema-publishing")
        or schema.get("x-ueremcp-schema-publishing")
        or (spec.get("properties") or {}).get("terrain")
    )
    print("BuildEnvironment nested schema:", "OK" if publishing else "MISSING")
    if not publishing:
        return 2

    tmpl = c.call("describe_toolset", {"toolset_name": "UeremcpTemplates.UeremcpTemplatesToolset"})
    if isinstance(tmpl, str):
        try:
            tmpl = json.loads(tmpl)
        except ValueError:
            pass
    tmpl_tools = [t.get("name", "").split(".")[-1] for t in (tmpl or {}).get("tools", [])]
    print("Templates tools:", sorted(tmpl_tools))
    for t in ("CreateTemplate", "UpdateTemplate", "PromoteToTemplate"):
        print("  ", t, "OK" if t in tmpl_tools else "MISSING")

    create_tpl = c.call("call_tool", {
        "toolset_name": "UeremcpTemplates.UeremcpTemplatesToolset",
        "tool_name": "CreateTemplate",
        "arguments": {
            "requestJson": json.dumps({
                "protocol_version": "1.0",
                "action": "create_template",
                "request_id": "tpl-dry-1",
                "target": {"asset_path": "/Game/__UeremcpPoc/Templates/DeployVerify"},
                "options": {"dry_run": True, "validate": True},
                "specification": {
                    "template_id": "deploy.verify.v1",
                    "description": "deploy consolidation dry-run",
                    "domain": "niagara",
                    "operations": []
                }
            })
        }
    })
    if isinstance(create_tpl, str):
        try:
            create_tpl = json.loads(create_tpl)
        except ValueError:
            pass
    print("CreateTemplate dry-run status:", (create_tpl or {}).get("status") if isinstance(create_tpl, dict) else create_tpl)

    intent = c.call("call_tool", {
        "toolset_name": "UeremcpCore.UeremcpReferenceToolset",
        "tool_name": "ResolveIntent",
        "arguments": {
            "requestJson": json.dumps({
                "protocol_version": "1.0",
                "action": "resolve_intent",
                "request_id": "intent-snow-1",
                "specification": {"intent": "build a snowy icy mountain environment with hail"}
            })
        }
    })
    if isinstance(intent, str):
        try:
            intent = json.loads(intent)
        except ValueError:
            pass
    plan_tools = []
    if isinstance(intent, dict):
        for step in (intent.get("plan") or []):
            tool = step.get("tool") or step.get("qualified_tool") or ""
            plan_tools.append(tool)
    has_build_env = any("BuildEnvironment" in t for t in plan_tools)
    print("ResolveIntent snow/ice routes BuildEnvironment:", "OK" if has_build_env else "FAIL", plan_tools[:3])

    snow_dry = c.call("call_tool", {
        "toolset_name": "UeremcpEnvironment.UeremcpEnvironmentToolset",
        "tool_name": "BuildEnvironment",
        "arguments": {
            "requestJson": json.dumps({
                "protocol_version": "1.0",
                "action": "build_environment",
                "request_id": "env-snow-dry-1",
                "target": {"asset_path": "/Game/__UeremcpPoc/SnowIceHail"},
                "options": {"dry_run": True, "validate": True},
                "specification": {
                    "schema_version": 2,
                    "seed": 8801,
                    "terrain": {"profile": "mountains", "mountain_amplitude": 0.62},
                    "weather": [
                        {"phenomenon": "snow", "intensity": 0.75},
                        {"phenomenon": "hail", "intensity": 0.35}
                    ],
                    "structures": [{"kind": "ice_wall_ring", "count": 12, "height": 600}],
                    "lighting": {"preset": "blizzard"}
                }
            })
        }
    })
    if isinstance(snow_dry, str):
        try:
            snow_dry = json.loads(snow_dry)
        except ValueError:
            pass
    print("v2 SnowIceHail dry-run status:", (snow_dry or {}).get("status") if isinstance(snow_dry, dict) else snow_dry)
    return 0


if __name__ == "__main__":
    sys.exit(main())
