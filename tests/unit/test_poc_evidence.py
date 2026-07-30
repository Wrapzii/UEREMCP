#!/usr/bin/env python3
"""Pure-logic tests for strict POC evidence acceptance."""

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = REPO_ROOT / "tests" / "poc_evidence.py"
SPEC = importlib.util.spec_from_file_location("poc_evidence", MODULE_PATH)
assert SPEC and SPEC.loader
poc_evidence = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(poc_evidence)


def valid_metrics() -> dict:
    return {
        "mcp_round_trips": 3,
        "internal_operations": 12,
        "tokens_total": 4200,
        "wall_clock_seconds": 7.5,
        "primitive_call_equivalent": 55,
    }


def valid_poc_a() -> dict:
    criteria = {f"A{index}": {"status": "pass"} for index in range(1, 12)}
    criteria["A6"].update(
        expected_nodes_present=True,
        expected_connections_present=True,
    )
    return {
        "schema_version": 1,
        "scenario": "poc_a",
        "run_id": "poc-a-test",
        "outcome": "pass",
        "criteria": criteria,
        "metrics": valid_metrics(),
    }


class EvidenceExtractionTest(unittest.TestCase):
    def test_last_valid_marker_wins(self):
        log = "\n".join(
            (
                'prefix UEREMCP_POC_EVIDENCE={"run_id":"first"}',
                "UEREMCP_POC_EVIDENCE=not-json",
                'log UEREMCP_POC_EVIDENCE={"run_id":"last","outcome":"skip"}',
            )
        )
        self.assertEqual(
            poc_evidence.extract_last_evidence(log)["run_id"],
            "last",
        )

    def test_pretty_printed_multiline_marker_is_extracted(self):
        log = "\n".join(
            (
                "prefix UEREMCP_POC_EVIDENCE={",
                '  "run_id": "restart",',
                '  "outcome": "pass",',
                '  "checkpoint": {"id": "cp-1"}',
                "}",
                "ordinary trailing log line",
            )
        )
        self.assertEqual(
            poc_evidence.extract_last_evidence(log)["checkpoint"]["id"],
            "cp-1",
        )

    def test_missing_marker_returns_none(self):
        self.assertIsNone(poc_evidence.extract_last_evidence("ordinary log line"))


class PocAEvidenceTest(unittest.TestCase):
    def test_complete_poc_a_evidence_passes(self):
        self.assertEqual(
            poc_evidence.validate_evidence(valid_poc_a(), "poc_a"),
            [],
        )

    def test_a6_requires_programmatic_node_and_connection_asserts(self):
        evidence = valid_poc_a()
        evidence["criteria"]["A6"]["expected_connections_present"] = False
        errors = poc_evidence.validate_evidence(evidence, "poc_a")
        self.assertIn("A6 expected_connections_present must be true", errors)

    def test_a9_rejects_more_than_three_round_trips(self):
        evidence = valid_poc_a()
        evidence["metrics"]["mcp_round_trips"] = 4
        errors = poc_evidence.validate_evidence(evidence, "poc_a")
        self.assertIn("A9 requires metrics.mcp_round_trips <= 3", errors)

    def test_pass_requires_every_criterion_and_metric(self):
        evidence = valid_poc_a()
        evidence["criteria"].pop("A8")
        evidence["metrics"].pop("tokens_total")
        errors = poc_evidence.validate_evidence(evidence, "poc_a")
        self.assertIn("A8 must have status=pass", errors)
        self.assertIn("metrics.tokens_total must be numeric", errors)

    def test_honest_skip_does_not_invent_unrun_criteria(self):
        evidence = {
            "schema_version": 1,
            "scenario": "poc_a",
            "run_id": "blocked",
            "outcome": "skip",
        }
        self.assertEqual(
            poc_evidence.validate_evidence(evidence, "poc_a"),
            [],
        )


class B8EvidenceTest(unittest.TestCase):
    def test_create_requires_durable_checkpoint(self):
        evidence = {
            "schema_version": 1,
            "scenario": "poc_b8_create",
            "run_id": "restart-test",
            "outcome": "pass",
            "checkpoint": {"id": "cp-1", "assets": []},
        }
        errors = poc_evidence.validate_evidence(evidence, "poc_b8_create")
        self.assertIn("checkpoint.assets must be a non-empty array", errors)

    def test_verify_requires_observed_restart_reread_and_metrics(self):
        evidence = {
            "schema_version": 1,
            "scenario": "poc_b8_verify",
            "run_id": "restart-test",
            "outcome": "pass",
            "restart_observed": True,
            "reread_after_restart": True,
            "criteria": {"B8": {"status": "pass"}},
            "checkpoint": {
                "id": "cp-1",
                "assets": ["/Game/__UeremcpPoc/Fireball"],
            },
            "metrics": valid_metrics(),
        }
        self.assertEqual(
            poc_evidence.validate_evidence(evidence, "poc_b8_verify"),
            [],
        )

    def test_runner_uses_two_editor_launches_for_restart_boundary(self):
        runner = (REPO_ROOT / "tests" / "run_poc_acceptance.ps1").read_text(
            encoding="utf-8"
        )
        self.assertIn("poc_b8_create", runner)
        self.assertIn("poc_b8_verify", runner)
        self.assertIn("UEREMCP_EDITOR_LOG=", runner)
        self.assertIn("$createId -ne $verifyId", runner)
        self.assertIn("restart verify checkpoint does not match create phase", runner)
        self.assertNotIn("retarget", runner.lower())


class PocBBundleTest(unittest.TestCase):
    def test_complete_honest_bundle_passes_with_transport_skip(self):
        bundle = {
            "schema_version": 1,
            "scenario": "poc_b",
            "tested_tip_sha": "a" * 40,
            "generated_at_utc": "2026-07-30T14:05:00Z",
            "overall_poc_b_claimed": False,
            "criteria": {
                f"B{index}": {
                    "status": "skip" if index == 1 else "pass",
                    "evidence": [{"path": f"tests/evidence/b{index}.log"}],
                }
                for index in range(1, 11)
            },
        }
        self.assertEqual(poc_evidence.validate_poc_b_bundle(bundle), [])

    def test_live_transport_b1_pass_preserves_partial_response_status(self):
        bundle = {
            "schema_version": 1,
            "scenario": "poc_b",
            "tested_tip_sha": "a" * 40,
            "generated_at_utc": "2026-07-30T14:20:00Z",
            "overall_poc_b_claimed": False,
            "transport": {
                "status": "pass",
                "response_status": "partially_completed",
            },
            "criteria": {
                f"B{index}": {
                    "status": "pass",
                    "scope": "MCP transport" if index == 1 else "editor evidence",
                    "evidence": [
                        {
                            "path": (
                                poc_evidence.POC_B_B1_LIVE_MCP_ARTIFACT
                                if index == 1
                                else f"tests/evidence/b{index}.log"
                            ),
                            "result": "pass",
                        }
                    ],
                }
                for index in range(1, 11)
            },
        }
        self.assertEqual(poc_evidence.validate_poc_b_bundle(bundle), [])

        bundle["transport"]["response_status"] = "created_and_validated"
        errors = poc_evidence.validate_poc_b_bundle(bundle)
        self.assertIn(
            "B1 PASS transport.response_status must preserve partially_completed",
            errors,
        )

    def test_complete_canonical_overall_claim_passes(self):
        bundle = {
            "schema_version": 1,
            "scenario": "poc_b",
            "tested_tip_sha": poc_evidence.POC_B_CLAIM_SHA,
            "generated_at_utc": "2026-07-30T14:50:00Z",
            "overall_poc_b_claimed": True,
            "claim": {
                "decision_document": poc_evidence.POC_B_CLAIM_DOCUMENT,
                "decision_sha": poc_evidence.POC_B_CLAIM_SHA,
                "scope": "poc_b_only",
            },
            "lineage": {
                "criterion_bundle_path": poc_evidence.POC_B_CRITERION_BUNDLE,
                "criterion_bundle_sha": poc_evidence.POC_B_CRITERION_BUNDLE_SHA,
                "metrics_sha": poc_evidence.POC_B_CLAIM_SHA,
            },
            "transport": {
                "status": "pass",
                "response_status": "partially_completed",
            },
            "criteria": {
                f"B{index}": {
                    "status": "pass",
                    "scope": "MCP transport" if index == 1 else "editor evidence",
                    "evidence": [
                        {
                            "path": (
                                poc_evidence.POC_B_B1_LIVE_MCP_ARTIFACT
                                if index == 1
                                else f"tests/evidence/b{index}.log"
                            ),
                            "result": "pass",
                        }
                    ],
                }
                for index in range(1, 11)
            },
            "metrics": {
                "mcp_round_trips": 1,
                "internal_operations": 46,
                "primitive_call_equivalent": 63,
                "primitive_trials_attempted": 3,
                "primitive_trials_usable": 3,
                "primitive_mean_wall_clock_seconds": 6.2771547,
                "response_status": "partially_completed",
                "tokens_total": None,
                "tokens_status": (
                    "unavailable: Cursor MCP caller exposes no per-call agent usage"
                ),
            },
            "other_poc_claims": {
                "poc_c": False,
                "poc_d": False,
                "poc_e": False,
            },
        }
        self.assertEqual(poc_evidence.validate_poc_b_bundle(bundle), [])

        bundle["metrics"]["tokens_total"] = 0
        errors = poc_evidence.validate_poc_b_bundle(bundle)
        self.assertIn(
            "claimed POC B requires metrics.tokens_total=None",
            errors,
        )

    def test_bundle_rejects_unsupported_incomplete_overall_claim(self):
        bundle = {
            "schema_version": 1,
            "scenario": "poc_b",
            "tested_tip_sha": "b" * 40,
            "generated_at_utc": "2026-07-30T14:05:00Z",
            "overall_poc_b_claimed": True,
            "criteria": {},
        }
        errors = poc_evidence.validate_poc_b_bundle(bundle)
        self.assertIn("claimed POC B requires claim object", errors)
        self.assertIn("B10 object is required", errors)


if __name__ == "__main__":
    unittest.main()
