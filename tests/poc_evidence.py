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
POC_B_B1_LIVE_MCP_ARTIFACT = (
    "docs/reviews/metrics/artifacts/poc_b_b1_live_mcp_20260730.json"
)
POC_B_CLAIM_DOCUMENT = "docs/proposals/ws-01-poc-b-acceptance-claim.md"
POC_B_CLAIM_SHA = "2aab525f3496465c9a4f62dd290ce5a31dd755d6"
POC_B_CRITERION_BUNDLE = (
    "tests/integration/_logs/poc_b_current_lineage_e85da3e.json"
)
POC_B_CRITERION_BUNDLE_SHA = "e85da3ea36c92dd8f2f61c6d29591f53094f9c63"


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
    overall_claimed = bundle.get("overall_poc_b_claimed")
    if not isinstance(overall_claimed, bool):
        errors.append("overall_poc_b_claimed must be boolean")
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

    b1 = criteria.get("B1")
    if isinstance(b1, dict) and b1.get("status") == "pass":
        if b1.get("scope") != "MCP transport":
            errors.append("B1 PASS requires scope='MCP transport'")
        evidence = b1.get("evidence")
        live_artifact = next(
            (
                item
                for item in evidence
                if isinstance(item, dict)
                and item.get("path") == POC_B_B1_LIVE_MCP_ARTIFACT
            ),
            None,
        ) if isinstance(evidence, list) else None
        if live_artifact is None:
            errors.append("B1 PASS requires the live MCP artifact")
        elif live_artifact.get("result") != "pass":
            errors.append("B1 live MCP artifact must have result=pass")

        transport = bundle.get("transport")
        if not isinstance(transport, dict) or transport.get("status") != "pass":
            errors.append("B1 PASS requires transport.status=pass")
        elif transport.get("response_status") != "partially_completed":
            errors.append(
                "B1 PASS transport.response_status must preserve partially_completed"
            )

    if overall_claimed:
        for name in POC_B_CRITERIA:
            if not _criterion_passed(criteria, name):
                errors.append(f"claimed POC B requires {name}.status=pass")

        claim = bundle.get("claim")
        if not isinstance(claim, dict):
            errors.append("claimed POC B requires claim object")
        else:
            if claim.get("decision_document") != POC_B_CLAIM_DOCUMENT:
                errors.append("claimed POC B requires the canonical decision document")
            if claim.get("decision_sha") != POC_B_CLAIM_SHA:
                errors.append("claimed POC B requires the canonical decision SHA")
            if claim.get("scope") != "poc_b_only":
                errors.append("claimed POC B scope must be poc_b_only")

        lineage = bundle.get("lineage")
        if not isinstance(lineage, dict):
            errors.append("claimed POC B requires lineage object")
        else:
            if lineage.get("criterion_bundle_path") != POC_B_CRITERION_BUNDLE:
                errors.append("claimed POC B requires the canonical criterion bundle")
            if lineage.get("criterion_bundle_sha") != POC_B_CRITERION_BUNDLE_SHA:
                errors.append("claimed POC B requires the criterion bundle SHA")
            if lineage.get("metrics_sha") != POC_B_CLAIM_SHA:
                errors.append("claimed POC B requires metrics SHA 2aab525")

        metrics = bundle.get("metrics")
        if not isinstance(metrics, dict):
            errors.append("claimed POC B requires metrics object")
        else:
            expected_metrics = {
                "mcp_round_trips": 1,
                "internal_operations": 46,
                "primitive_call_equivalent": 63,
                "primitive_trials_attempted": 3,
                "primitive_trials_usable": 3,
                "response_status": "partially_completed",
                "tokens_total": None,
            }
            for field, expected in expected_metrics.items():
                if metrics.get(field) != expected:
                    errors.append(
                        f"claimed POC B requires metrics.{field}={expected!r}"
                    )
            if metrics.get("primitive_mean_wall_clock_seconds") != 6.2771547:
                errors.append(
                    "claimed POC B requires primitive mean wall clock 6.2771547"
                )
            tokens_status = metrics.get("tokens_status")
            if not isinstance(tokens_status, str) or not tokens_status.startswith(
                "unavailable:"
            ):
                errors.append("claimed POC B must preserve unavailable token status")

        other_claims = bundle.get("other_poc_claims")
        if other_claims != {"poc_c": False, "poc_d": False, "poc_e": False}:
            errors.append("claimed POC B must explicitly leave POC C/D/E unclaimed")
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
