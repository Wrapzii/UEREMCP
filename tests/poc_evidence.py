#!/usr/bin/env python3
"""Validate machine-readable POC evidence emitted by Unreal Automation.

Domain-owned tests emit one compact JSON line prefixed with
``UEREMCP_POC_EVIDENCE=``. This module deliberately rejects incomplete PASS
claims; SKIP and FAIL evidence may omit criteria that could not run.
"""

from __future__ import annotations

import argparse
import json
import math
import re
from pathlib import Path
from typing import Any

MARKER = "UEREMCP_POC_EVIDENCE="
METRIC_FIELDS = (
    "mcp_round_trips",
    "internal_operations",
    "tokens_total",
    "wall_clock_seconds",
    "primitive_call_equivalent",
)
POC_A_CRITERIA = tuple(f"A{index}" for index in range(1, 12))
POC_B_CRITERIA = tuple(f"B{index}" for index in range(1, 11))


def extract_last_evidence(log_text: str) -> dict[str, Any] | None:
    """Return the last syntactically valid evidence object in a log."""
    evidence = None
    decoder = json.JSONDecoder()
    search_at = 0
    while (marker_at := log_text.find(MARKER, search_at)) >= 0:
        candidate = log_text[marker_at + len(MARKER) :].lstrip()
        try:
            parsed, _ = decoder.raw_decode(candidate)
        except json.JSONDecodeError:
            search_at = marker_at + len(MARKER)
            continue
        if isinstance(parsed, dict):
            evidence = parsed
        search_at = marker_at + len(MARKER)
    return evidence


def _validate_metrics(metrics: Any, errors: list[str]) -> None:
    if not isinstance(metrics, dict):
        errors.append("metrics object is required for PASS")
        return
    for field in METRIC_FIELDS:
        value = metrics.get(field)
        if not isinstance(value, (int, float)) or isinstance(value, bool):
            errors.append(f"metrics.{field} must be numeric")
        elif not math.isfinite(value) or value < 0:
            errors.append(f"metrics.{field} must be finite and non-negative")


def _criterion_passed(criteria: Any, name: str) -> bool:
    item = criteria.get(name) if isinstance(criteria, dict) else None
    return isinstance(item, dict) and item.get("status") == "pass"


def validate_evidence(
    evidence: dict[str, Any] | None, expected_scenario: str
) -> list[str]:
    """Return validation errors; an empty list means evidence is honest."""
    errors: list[str] = []
    if evidence is None:
        return ["evidence marker missing"]
    if evidence.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    if evidence.get("scenario") != expected_scenario:
        errors.append(
            f"scenario must be {expected_scenario!r}, got {evidence.get('scenario')!r}"
        )
    if not isinstance(evidence.get("run_id"), str) or not evidence["run_id"].strip():
        errors.append("run_id is required")

    outcome = evidence.get("outcome")
    if outcome not in {"pass", "fail", "skip"}:
        errors.append("outcome must be pass, fail, or skip")
        return errors
    if outcome != "pass":
        return errors

    if expected_scenario == "poc_a":
        criteria = evidence.get("criteria")
        for criterion in POC_A_CRITERIA:
            if not _criterion_passed(criteria, criterion):
                errors.append(f"{criterion} must have status=pass")
        a6 = criteria.get("A6", {}) if isinstance(criteria, dict) else {}
        if a6.get("expected_nodes_present") is not True:
            errors.append("A6 expected_nodes_present must be true")
        if a6.get("expected_connections_present") is not True:
            errors.append("A6 expected_connections_present must be true")
        metrics = evidence.get("metrics")
        _validate_metrics(metrics, errors)
        if isinstance(metrics, dict) and metrics.get("mcp_round_trips", 4) > 3:
            errors.append("A9 requires metrics.mcp_round_trips <= 3")

    elif expected_scenario == "poc_b8_create":
        checkpoint = evidence.get("checkpoint")
        if not isinstance(checkpoint, dict):
            errors.append("checkpoint object is required")
        else:
            if not checkpoint.get("id"):
                errors.append("checkpoint.id is required")
            if not isinstance(checkpoint.get("assets"), list) or not checkpoint["assets"]:
                errors.append("checkpoint.assets must be a non-empty array")

    elif expected_scenario == "poc_b8_verify":
        if evidence.get("restart_observed") is not True:
            errors.append("restart_observed must be true")
        if evidence.get("reread_after_restart") is not True:
            errors.append("reread_after_restart must be true")
        if not _criterion_passed(evidence.get("criteria"), "B8"):
            errors.append("B8 must have status=pass")
        checkpoint = evidence.get("checkpoint")
        if not isinstance(checkpoint, dict) or not checkpoint.get("id"):
            errors.append("checkpoint.id is required")
        elif not isinstance(checkpoint.get("assets"), list) or not checkpoint["assets"]:
            errors.append("checkpoint.assets must be a non-empty array")
        _validate_metrics(evidence.get("metrics"), errors)
    else:
        errors.append(f"unsupported scenario {expected_scenario!r}")
    return errors


def validate_poc_b_bundle(bundle: Any) -> list[str]:
    """Validate a criterion-indexed POC-B evidence summary."""
    errors: list[str] = []
    if not isinstance(bundle, dict):
        return ["bundle must be an object"]
    if bundle.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    if bundle.get("scenario") != "poc_b":
        errors.append("scenario must be 'poc_b'")
    if bundle.get("overall_poc_b_claimed") is not False:
        errors.append("overall_poc_b_claimed must be false")
    tested_tip = bundle.get("tested_tip_sha")
    if not isinstance(tested_tip, str) or not re.fullmatch(r"[0-9a-f]{40}", tested_tip):
        errors.append("tested_tip_sha must be a full lowercase Git SHA")
    if not isinstance(bundle.get("generated_at_utc"), str):
        errors.append("generated_at_utc is required")

    criteria = bundle.get("criteria")
    if not isinstance(criteria, dict):
        return errors + ["criteria object is required"]
    for name in POC_B_CRITERIA:
        criterion = criteria.get(name)
        if not isinstance(criterion, dict):
            errors.append(f"{name} object is required")
            continue
        if criterion.get("status") not in {"pass", "fail", "skip"}:
            errors.append(f"{name}.status must be pass, fail, or skip")
        evidence = criterion.get("evidence")
        if not isinstance(evidence, list) or not evidence:
            errors.append(f"{name}.evidence must be a non-empty array")
        elif any(
            not isinstance(item, dict)
            or not isinstance(item.get("path"), str)
            or not item["path"]
            for item in evidence
        ):
            errors.append(f"{name}.evidence entries require path")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario")
    parser.add_argument("--log", type=Path)
    parser.add_argument("--bundle", type=Path)
    args = parser.parse_args()

    if args.bundle:
        bundle = json.loads(args.bundle.read_text(encoding="utf-8-sig"))
        errors = validate_poc_b_bundle(bundle)
        print(json.dumps({"valid": not errors, "errors": errors}, separators=(",", ":")))
        return 0 if not errors else 1
    if not args.scenario or not args.log:
        parser.error("--scenario and --log are required unless --bundle is used")

    evidence = extract_last_evidence(args.log.read_text(encoding="utf-8", errors="replace"))
    errors = validate_evidence(evidence, args.scenario)
    print(
        json.dumps(
            {"valid": not errors, "errors": errors, "evidence": evidence},
            separators=(",", ":"),
        )
    )
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
