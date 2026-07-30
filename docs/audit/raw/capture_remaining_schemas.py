#!/usr/bin/env python3
"""Batch describe_toolset capture for WS-02 audit matrix. READ-ONLY MCP calls."""
from __future__ import annotations

import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

MCP_URL = "http://127.0.0.1:8001/mcp"
ROOT = Path(__file__).resolve().parent
LIST_PATH = ROOT / "runtime" / "list_toolsets.json"
SCHEMA_DIR = ROOT / "schemas"
MAX_RETRIES = 3
RETRY_DELAY_S = 2.0


def describe_toolset(name: str) -> dict:
    body = json.dumps(
        {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "tools/call",
            "params": {
                "name": "describe_toolset",
                "arguments": {"toolset_name": name},
            },
        }
    ).encode()
    headers = {
        "Content-Type": "application/json",
        "Accept": "application/json, text/event-stream",
    }
    req = urllib.request.Request(MCP_URL, data=body, headers=headers)
    with urllib.request.urlopen(req, timeout=120) as resp:
        payload = json.loads(resp.read().decode())
    if "error" in payload:
        raise RuntimeError(payload["error"])
    text = payload["result"]["content"][0]["text"]
    return json.loads(text)


def main() -> int:
    with LIST_PATH.open(encoding="utf-8") as f:
        toolsets = json.load(f)["toolsets"]
    SCHEMA_DIR.mkdir(parents=True, exist_ok=True)

    existing = {p.stem for p in SCHEMA_DIR.glob("*.json")}
    missing = [t for t in toolsets if t not in existing]
    print(f"total={len(toolsets)} existing={len(existing)} to_capture={len(missing)}")

    failures: list[dict[str, str]] = []
    captured: list[str] = []

    for name in missing:
        out_path = SCHEMA_DIR / f"{name}.json"
        for attempt in range(1, MAX_RETRIES + 1):
            try:
                schema = describe_toolset(name)
                out_path.write_text(
                    json.dumps(schema, indent=2, ensure_ascii=False) + "\n",
                    encoding="utf-8",
                )
                tool_count = len(schema.get("tools", []))
                print(f"OK  {name} ({tool_count} tools)")
                captured.append(name)
                break
            except Exception as exc:  # noqa: BLE001
                if attempt < MAX_RETRIES:
                    print(f"RETRY {attempt}/{MAX_RETRIES} {name}: {exc}")
                    time.sleep(RETRY_DELAY_S)
                else:
                    print(f"FAIL {name}: {exc}")
                    failures.append({"toolset": name, "error": str(exc)})

    summary = {
        "captured": captured,
        "failures": failures,
        "captured_count": len(captured),
        "failure_count": len(failures),
    }
    summary_path = ROOT / "runtime" / "capture-batch-summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, indent=2))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
