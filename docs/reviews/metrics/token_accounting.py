"""Token accounting for POC metrics.

Cursor's MCP caller and REAgentTools' HTTP client do not expose billed agent
tokens. Record unavailable with a precise reason — never 0, never wire_bytes/4
as total agent tokens.
"""

from __future__ import annotations

from typing import Any


CURSOR_MCP_NO_USAGE = (
    "unavailable (Cursor MCP caller does not expose per-call agent usage; "
    "wire_bytes/4 is a payload-token proxy only, not total agent tokens)"
)

HTTP_MCP_WIRE_ONLY = (
    "unavailable (HTTP MCP client measures wire bytes only; "
    "Cursor Usage / cache-read tokens not exposed to script — "
    "see REAgentTools/Docs/BENCHMARK_REPORT.md)"
)


def resolve_token_accounting(
    *,
    usage: dict[str, Any] | None = None,
    transport: str = "cursor_mcp",
) -> dict[str, Any]:
    """Return a machine-checkable token cell.

    If `usage` contains input/output/total integers from an instrumented harness,
    status is measured. Otherwise status is unavailable with a transport-specific reason.
    """
    if usage is not None:
        total = usage.get("tokens_total")
        if total is None and "tokens_input" in usage and "tokens_output" in usage:
            total = int(usage["tokens_input"]) + int(usage["tokens_output"])
        if isinstance(total, (int, float)) and total >= 0:
            return {
                "status": "measured",
                "tokens_input": usage.get("tokens_input"),
                "tokens_output": usage.get("tokens_output"),
                "tokens_total": int(total),
                "reason": "harness-reported usage",
            }
        return {
            "status": "unavailable",
            "tokens_input": None,
            "tokens_output": None,
            "tokens_total": None,
            "reason": "usage object present but missing numeric tokens_total/input+output",
        }

    reason = CURSOR_MCP_NO_USAGE if transport == "cursor_mcp" else HTTP_MCP_WIRE_ONLY
    if transport not in ("cursor_mcp", "http_mcp_wire"):
        reason = f"unavailable (unknown transport {transport!r}; no usage exposed)"
    return {
        "status": "unavailable",
        "tokens_input": None,
        "tokens_output": None,
        "tokens_total": None,
        "reason": reason,
    }
