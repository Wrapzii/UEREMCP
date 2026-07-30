"""Long-running job helpers — mirrors FUeremcpJob / ADR-0009."""

from __future__ import annotations

import uuid
from typing import Any


DEFAULT_TIMEOUT_MS = 120000
CLIENT_SSE_RISK_MS = 30000
MIN_TIMEOUT_MS = 1000
MAX_TIMEOUT_MS = 600000
POLL_ACTION = "get_job_result"

JOB_STATES = frozenset({"queued", "running", "completed", "failed", "cancelled"})


class JobError(ValueError):
    pass


def should_dispatch_inline(timeout_ms: int) -> bool:
    """timeout_ms == 0 → complete inline on MCP SSE (ADR-0009)."""
    return timeout_ms == 0


def normalise_timeout_ms(timeout_ms: int) -> int:
    if timeout_ms <= 0:
        return 0
    return max(MIN_TIMEOUT_MS, min(MAX_TIMEOUT_MS, timeout_ms))


def new_job_id() -> str:
    """UEREMCP UUID, per editor process — not the MCP JSON-RPC request id."""
    return str(uuid.uuid4())


def validate_job(job: dict[str, Any]) -> None:
    if not job.get("job_id"):
        raise JobError("job.job_id is required")
    state = job.get("state")
    if state not in JOB_STATES:
        raise JobError(f"invalid job.state '{state}'")
    if "progress" in job:
        progress = job["progress"]
        if not isinstance(progress, (int, float)) or progress < 0 or progress > 1:
            raise JobError("job.progress must be in [0, 1]")


def make_job_timeout_response(
    request_id: str,
    job_id: str,
    progress_message: str = "",
    mcp_round_trips: int = 1,
) -> dict[str, Any]:
    """partially_completed + running job handle. cancellable defaults false."""
    summary = progress_message or (
        "Operation exceeded timeout_ms; continuing in-process. Poll get_job_result."
    )
    return {
        "protocol_version": "1.0",
        "request_id": request_id,
        "status": "partially_completed",
        "summary": summary,
        "job": {
            "job_id": job_id,
            "state": "running",
            "progress_message": progress_message,
            "cancellable": False,
            "poll_action": POLL_ACTION,
        },
        "metrics": {
            "mcp_round_trips": max(1, mcp_round_trips),
            "internal_operations": 0,
        },
        "capability_notes": [
            "Long-running job: Epic MCP progress heartbeats are not percent-complete; "
            "use job.progress / job.progress_message."
        ],
    }
