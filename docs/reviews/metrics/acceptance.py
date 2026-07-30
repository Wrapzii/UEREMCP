"""Acceptance formula for POC metrics — derived only from frozen docs.

Sources (do not invent thresholds):
  docs/POC_ACCEPTANCE.md global rules (lines ~17–25) and E7
  docs/WHY.md "Measure it" (three numbers per scenario)
  schemas/common/defs.schema.json#/$defs/metrics
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Any


# Required by docs/POC_ACCEPTANCE.md global rules — report numbers, not adjectives.
POC_REQUIRED_FIELDS = (
    "mcp_round_trips",
    "internal_operations",
    "tokens_total",
    "wall_clock_seconds",
    "primitive_call_equivalent",
)

# docs/WHY.md — three numbers per scenario (completion rate is separate from envelope metrics).
WHY_REQUIRED_NUMBERS = (
    "tool_calls_to_complete_goal",  # == mcp_round_trips for the UEREMCP arm
    "tokens_total_including_failures_retries",
    "completion_rate_verified",
)

# Explicitly NOT wall_clock_seconds (WS-11 handoff / WS-07 timing_ms.server_total).
SERVER_SIDE_LOWER_BOUND_FIELD = "server_side_lower_bound_seconds"


@dataclass(frozen=True)
class MetricCell:
    """One measured or honestly unavailable metric cell."""

    value: Any
    status: str  # measured | unavailable | open | not_applicable
    evidence: str
    notes: str = ""

    def as_dict(self) -> dict[str, Any]:
        return {
            "value": self.value,
            "status": self.status,
            "evidence": self.evidence,
            "notes": self.notes,
        }


def classify_tokens(harness_exposes_usage: bool, reason_if_unavailable: str) -> MetricCell:
    """Token accounting: measured only if the harness exposes usage; else unavailable."""
    if harness_exposes_usage:
        raise ValueError("caller must supply the measured token count when usage is exposed")
    return MetricCell(
        value=None,
        status="unavailable",
        evidence="harness_probe",
        notes=reason_if_unavailable,
    )


def wall_clock_from_client(
    start_monotonic_s: float | None,
    end_monotonic_s: float | None,
    *,
    evidence: str,
) -> MetricCell:
    """Client-observed wall clock only. Editor log intervals are not wall_clock_seconds."""
    if start_monotonic_s is None or end_monotonic_s is None:
        return MetricCell(
            value=None,
            status="unavailable",
            evidence=evidence,
            notes="client monotonic start/end not captured; do not substitute editor log interval",
        )
    if end_monotonic_s < start_monotonic_s:
        raise ValueError("end_monotonic_s < start_monotonic_s")
    return MetricCell(
        value=round(end_monotonic_s - start_monotonic_s, 6),
        status="measured",
        evidence=evidence,
        notes="client monotonic elapsed around the single MCP request",
    )


def server_side_lower_bound(
    dispatch_epoch_s: float | None,
    completion_epoch_s: float | None,
    *,
    evidence: str,
) -> MetricCell:
    """Log-derived server interval — must never be copied into wall_clock_seconds."""
    if dispatch_epoch_s is None or completion_epoch_s is None:
        return MetricCell(
            value=None,
            status="unavailable",
            evidence=evidence,
            notes="dispatch/completion markers missing from log",
        )
    return MetricCell(
        value=round(completion_epoch_s - dispatch_epoch_s, 6),
        status="measured",
        evidence=evidence,
        notes="server-side lower bound only; not wall_clock_seconds",
    )


def metrics_completeness(cells: dict[str, MetricCell]) -> dict[str, Any]:
    """Machine-checkable completeness against POC_ACCEPTANCE required fields."""
    missing = []
    for field in POC_REQUIRED_FIELDS:
        cell = cells.get(field)
        if cell is None or cell.status not in ("measured", "unavailable"):
            missing.append(field)
        elif field == "tokens_total" and cell.status == "unavailable":
            # unavailable with reason satisfies honesty; does NOT close E7 as complete.
            pass
    measured = [k for k, v in cells.items() if v.status == "measured"]
    return {
        "poc_required_fields": list(POC_REQUIRED_FIELDS),
        "measured_fields": measured,
        "incomplete_or_open": missing,
        "e7_record_exists": True,  # file may exist while values remain open
        "may_claim_metrics_complete": (
            all(
                cells.get(f) is not None and cells[f].status == "measured"
                for f in POC_REQUIRED_FIELDS
            )
        ),
    }
