#!/usr/bin/env python
"""Dump the live MCP tool registry to tools/registry_snapshot.json.

Ground truth for what tools actually exist. Everything else that claims to know
the tool surface -- docs, indexes, agent briefs -- should be checked against this
rather than hand-maintained, because hand-maintained lists go stale silently and
an agent that trusts a stale list burns a call and then starts guessing.

Requires a running editor with the MCP server on :8000 (Epic's
ModelContextProtocol plugin). Run it after any toolset change:

    python tools/dump_tool_registry.py
    python tools/check_tool_names.py        # now has fresh ground truth
    python tools/gen_tool_index.py          # regenerate docs/TOOL_INDEX.md
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
from datetime import datetime, timezone

from check_tool_names import discover_source_tools, source_surface_fingerprint

URL = os.environ.get("UEREMCP_MCP_URL", "http://127.0.0.1:8000/mcp")
HERE = os.path.dirname(os.path.abspath(__file__))
SNAPSHOT = os.path.join(HERE, "registry_snapshot.json")

_next_id = [100]


def _post(payload, session=None, timeout=120):
    """curl-backed POST. urllib returns an empty body against this SSE server."""
    hdr = tempfile.NamedTemporaryFile(suffix=".hdr", delete=False)
    hdr.close()
    cmd = ["curl", "-s", "-D", hdr.name, "-m", str(timeout), "-X", "POST", URL,
           "-H", "Content-Type: application/json",
           "-H", "Accept: application/json, text/event-stream"]
    if session:
        cmd += ["-H", "Mcp-Session-Id: " + session]
    cmd += ["--data-binary", "@-"]
    proc = subprocess.run(cmd, input=json.dumps(payload).encode(), capture_output=True)
    sid = None
    with open(hdr.name, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if line.lower().startswith("mcp-session-id:"):
                sid = line.split(":", 1)[1].strip()
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
                       "clientInfo": {"name": "uneremcp-registry-dump", "version": "1"}}})
        if not self.sid:
            raise SystemExit("no MCP session -- is the editor running with the MCP server on :8000?")
        _post({"jsonrpc": "2.0", "method": "notifications/initialized"}, self.sid)

    def call(self, name, args, timeout=180):
        _next_id[0] += 1
        body, _ = _post({"jsonrpc": "2.0", "id": _next_id[0], "method": "tools/call",
                         "params": {"name": name, "arguments": args}}, self.sid, timeout)
        data = _parse(body)
        if not isinstance(data, dict) or "result" not in data:
            return None
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
    client = Client()

    listing = client.call("list_toolsets", {})
    if not isinstance(listing, str):
        listing = json.dumps(listing)

    # list_toolsets returns "- <name>: <description>" lines.
    names = re.findall(r"^-\s+([A-Za-z0-9_.]+):", listing, flags=re.M)
    print("toolsets: %d" % len(names))

    toolsets = {}
    for i, name in enumerate(sorted(names), 1):
        detail = client.call("describe_toolset", {"toolset_name": name})
        if not isinstance(detail, dict):
            print("  [%d/%d] %-60s SKIP (no schema)" % (i, len(names), name))
            toolsets[name] = {"error": "describe_toolset returned no object"}
            continue

        tools = {}
        for tool in detail.get("tools", []):
            full = tool.get("name", "")
            short = full.split(".")[-1]
            schema = tool.get("inputSchema") or {}
            tools[short] = {
                "qualified_name": full,
                "description": (tool.get("description") or "").strip(),
                "required": schema.get("required", []),
                "properties": sorted((schema.get("properties") or {}).keys()),
            }
        toolsets[name] = {
            "description": (detail.get("description") or "").strip(),
            "version": detail.get("version"),
            "tools": tools,
        }
        print("  [%d/%d] %-60s %d tools" % (i, len(names), name, len(tools)))

    source_tools = discover_source_tools()
    live_names = {
        "%s.%s" % (toolset_name, tool_name)
        for toolset_name, toolset in toolsets.items()
        for tool_name in (toolset.get("tools") or {})
    }
    missing_source_tools = sorted(set(source_tools) - live_names)
    if missing_source_tools:
        print("\nrefusing to overwrite ground truth: live registry is missing %d source callable(s)"
              % len(missing_source_tools), file=sys.stderr)
        for missing in missing_source_tools:
            print("  " + missing, file=sys.stderr)
        print("Rebuild/redeploy, ensure one editor owns :8000, then dump again.", file=sys.stderr)
        return 3

    snapshot = {
        "snapshot_schema_version": 2,
        "generated_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "source": "live MCP registry via list_toolsets + describe_toolset",
        "source_surface_fingerprint": source_surface_fingerprint(source_tools),
        "toolset_count": len(toolsets),
        "tool_count": sum(len(t.get("tools", {})) for t in toolsets.values()),
        "toolsets": toolsets,
    }
    with open(SNAPSHOT, "w", encoding="utf-8") as fh:
        json.dump(snapshot, fh, indent=1, sort_keys=True)
        fh.write("\n")
    print("\nwrote %s (%d toolsets, %d tools)"
          % (SNAPSHOT, snapshot["toolset_count"], snapshot["tool_count"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
