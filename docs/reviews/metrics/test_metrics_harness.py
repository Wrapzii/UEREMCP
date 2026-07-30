#!/usr/bin/env python3
"""Offline unit tests for WS-14 POC metrics harness.

Run from repo root::

    python docs/reviews/metrics/test_metrics_harness.py
"""

from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from metrics.acceptance import (  # noqa: E402
    POC_REQUIRED_FIELDS,
    MetricCell,
    metrics_completeness,
    server_side_lower_bound,
    wall_clock_from_client,
)
from metrics.parse_editor_log import measure_server_side_interval, parse_ue_timestamp  # noqa: E402
from metrics.parse_response import assert_poc_b_b1_slice, extract_envelope_metrics  # noqa: E402
from metrics.primitive_baseline import (  # noqa: E402
    comparability_audit,
    planned_known_minimum,
    summarize_trials,
)
from metrics.token_accounting import CURSOR_MCP_NO_USAGE, resolve_token_accounting  # noqa: E402


FIXTURES = HERE / "fixtures"


class TestAcceptanceFormula(unittest.TestCase):
    def test_required_fields_match_poc_acceptance(self):
        self.assertIn("mcp_round_trips", POC_REQUIRED_FIELDS)
        self.assertIn("internal_operations", POC_REQUIRED_FIELDS)
        self.assertIn("tokens_total", POC_REQUIRED_FIELDS)
        self.assertIn("wall_clock_seconds", POC_REQUIRED_FIELDS)
        self.assertIn("primitive_call_equivalent", POC_REQUIRED_FIELDS)

    def test_completeness_refuses_partial_claim(self):
        cells = {
            "mcp_round_trips": MetricCell(1, "measured", "response"),
            "internal_operations": MetricCell(46, "measured", "response"),
            "tokens_total": MetricCell(None, "unavailable", "harness", CURSOR_MCP_NO_USAGE),
            "wall_clock_seconds": MetricCell(None, "unavailable", "harness"),
            "primitive_call_equivalent": MetricCell(None, "open", "baseline"),
        }
        summary = metrics_completeness(cells)
        self.assertFalse(summary["may_claim_metrics_complete"])
        self.assertIn("primitive_call_equivalent", summary["incomplete_or_open"])

    def test_wall_clock_rejects_missing_client_timestamps(self):
        cell = wall_clock_from_client(None, None, evidence="test")
        self.assertEqual(cell.status, "unavailable")
        self.assertIsNone(cell.value)

    def test_wall_clock_measured(self):
        cell = wall_clock_from_client(10.0, 12.5, evidence="test")
        self.assertEqual(cell.status, "measured")
        self.assertEqual(cell.value, 2.5)

    def test_server_side_not_wall(self):
        cell = server_side_lower_bound(100.0, 102.319, evidence="log")
        self.assertEqual(cell.status, "measured")
        self.assertAlmostEqual(cell.value, 2.319)
        self.assertIn("not wall_clock", cell.notes)


class TestParseResponse(unittest.TestCase):
    def test_extract_from_fixture(self):
        data = json.loads((FIXTURES / "sample_poc_b_response.json").read_text(encoding="utf-8"))
        extracted = extract_envelope_metrics(data)
        self.assertEqual(extracted["mcp_round_trips"], 1)
        self.assertEqual(extracted["internal_operations"], 46)
        self.assertEqual(assert_poc_b_b1_slice(extracted), [])

    def test_wrapped_result(self):
        inner = json.loads((FIXTURES / "sample_poc_b_response.json").read_text(encoding="utf-8"))
        extracted = extract_envelope_metrics({"result": inner})
        self.assertEqual(extracted["mcp_round_trips"], 1)


class TestParseEditorLog(unittest.TestCase):
    def test_timestamp_and_interval(self):
        log = (FIXTURES / "sample_editor_interval.log").read_text(encoding="utf-8")
        first = log.splitlines()[0]
        self.assertIsNotNone(parse_ue_timestamp(first))
        result = measure_server_side_interval(log)
        self.assertEqual(result["status"], "measured")
        self.assertAlmostEqual(result["value_seconds"], 2.319, places=3)
        self.assertIn("not wall_clock", result["reason"])

    def test_missing_completion(self):
        log = "[2026.07.30-11.14.53:492][  0]LogUeremcpNiagara: CreateNiagaraEffect dispatch\n"
        result = measure_server_side_interval(log)
        self.assertEqual(result["status"], "unavailable")


class TestTokens(unittest.TestCase):
    def test_cursor_unavailable(self):
        cell = resolve_token_accounting(usage=None, transport="cursor_mcp")
        self.assertEqual(cell["status"], "unavailable")
        self.assertIsNone(cell["tokens_total"])
        self.assertIn("does not expose", cell["reason"])

    def test_measured_when_usage_present(self):
        cell = resolve_token_accounting(
            usage={"tokens_input": 100, "tokens_output": 50},
            transport="cursor_mcp",
        )
        self.assertEqual(cell["status"], "measured")
        self.assertEqual(cell["tokens_total"], 150)


class TestPrimitiveBaseline(unittest.TestCase):
    def test_planned_minimum_excludes_open_steps(self):
        plan = planned_known_minimum(include_optional_replace=False)
        self.assertEqual(plan["status"], "planned_partial")
        self.assertIn("materials", plan["open_steps"])
        self.assertIn("compile_poll", plan["open_steps"])
        # create(1)+emitters(6)+user_vars(1)+renderer(6)+save(1)+verify(2) = 17
        self.assertEqual(plan["known_minimum_primitive_ops"], 17)

    def test_comparability_audit(self):
        audit = comparability_audit(46)
        self.assertFalse(audit["comparable_to_mcp_round_trips"])
        self.assertFalse(audit["comparable_to_epic_primitive_baseline"])

    def test_summarize_empty_is_open(self):
        summary = summarize_trials([])
        self.assertEqual(summary["status"], "open")
        self.assertIsNone(summary["primitive_ops"])

    def test_summarize_measured(self):
        trials = [
            {
                "primitive_ops_executed": 40,
                "mcp_round_trips": 1,
                "wall_clock_seconds": 5.0,
                "completed": True,
            },
            {
                "primitive_ops_executed": 42,
                "mcp_round_trips": 1,
                "wall_clock_seconds": 6.0,
                "completed": True,
            },
        ]
        summary = summarize_trials(trials)
        self.assertEqual(summary["status"], "measured")
        self.assertEqual(summary["n"], 2)
        self.assertEqual(summary["primitive_ops"]["min"], 40)


class TestPrepareScript(unittest.TestCase):
    def test_prepare_writes_artifact_without_execute(self):
        from metrics.run_poc_b_metrics import main

        schema_req = HERE.parents[2] / "schemas/domains/niagara/fixtures/poc_b_mcp_fireball_request.json"
        req = schema_req if schema_req.exists() else FIXTURES / "poc_b_mcp_fireball_request.json"
        if not req.exists():
            self.skipTest("canonical fixture not present in this worktree tip")
        with tempfile.TemporaryDirectory() as td:
            out = Path(td) / "trial.json"
            rc = main(["--request", str(req), "--out", str(out), "--response", str(FIXTURES / "sample_poc_b_response.json")])
            self.assertEqual(rc, 0)
            data = json.loads(out.read_text(encoding="utf-8"))
            self.assertTrue(data["prepared"])
            self.assertEqual(data["tokens"]["status"], "unavailable")
            self.assertEqual(data["wall_clock_seconds"]["status"], "unavailable")
            self.assertEqual(data["response_metrics"]["internal_operations"], 46)


if __name__ == "__main__":
    unittest.main()
