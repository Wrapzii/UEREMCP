"""Unit tests for ADR-0009 job model helpers."""

from __future__ import annotations

import json
import unittest

from ueremcp_protocol.envelope import EnvelopeError, serialize_response
from ueremcp_protocol.job import (
    CLIENT_SSE_RISK_MS,
    DEFAULT_TIMEOUT_MS,
    POLL_ACTION,
    make_job_timeout_response,
    normalise_timeout_ms,
    should_dispatch_inline,
    validate_job,
)


class JobModelTests(unittest.TestCase):
    def test_inline_when_timeout_zero(self):
        self.assertTrue(should_dispatch_inline(0))
        self.assertFalse(should_dispatch_inline(120000))

    def test_defaults_match_handoff(self):
        self.assertEqual(DEFAULT_TIMEOUT_MS, 120000)
        self.assertEqual(CLIENT_SSE_RISK_MS, 30000)
        self.assertEqual(POLL_ACTION, "get_job_result")
        self.assertEqual(normalise_timeout_ms(0), 0)
        self.assertEqual(normalise_timeout_ms(500), 1000)
        self.assertEqual(normalise_timeout_ms(999999), 600000)

    def test_timeout_response_shape(self):
        resp = make_job_timeout_response("rid-1", "job-uuid-1", "Compiling…")
        self.assertEqual(resp["status"], "partially_completed")
        self.assertEqual(resp["job"]["state"], "running")
        self.assertEqual(resp["job"]["poll_action"], "get_job_result")
        self.assertIs(resp["job"]["cancellable"], False)
        text = serialize_response(resp)
        obj = json.loads(text)
        self.assertEqual(obj["job"]["job_id"], "job-uuid-1")
        self.assertIn("mcp_round_trips", obj["metrics"])

    def test_inflight_job_requires_partial_status(self):
        with self.assertRaises(EnvelopeError):
            serialize_response(
                {
                    "status": "created_and_validated",
                    "summary": "nope",
                    "job": {
                        "job_id": "j1",
                        "state": "running",
                        "poll_action": "get_job_result",
                    },
                    "metrics": {"mcp_round_trips": 1, "internal_operations": 0},
                }
            )

    def test_progress_bounds(self):
        with self.assertRaises(Exception):
            validate_job({"job_id": "j", "state": "running", "progress": 1.5})
        validate_job({"job_id": "j", "state": "running", "progress": 0.5})


if __name__ == "__main__":
    unittest.main()
