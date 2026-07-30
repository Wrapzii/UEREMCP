"""Unit tests for envelope parse/serialise/validate."""

from __future__ import annotations

import json
import unittest

from ueremcp_protocol.envelope import (
    PROTOCOL_VERSION,
    EnvelopeError,
    is_protocol_compatible,
    make_rejection,
    parse_request,
    serialize_response,
)


class EnvelopeTests(unittest.TestCase):
    def test_protocol_version(self):
        self.assertEqual(PROTOCOL_VERSION, "1.0")
        self.assertTrue(is_protocol_compatible("1.0"))
        self.assertTrue(is_protocol_compatible("1.9"))
        self.assertFalse(is_protocol_compatible("2.0"))
        self.assertFalse(is_protocol_compatible("nope"))

    def test_parse_minimal(self):
        req = parse_request('{"protocol_version":"1.0","action":"ping"}')
        self.assertEqual(req["action"], "ping")
        self.assertEqual(req["mode"], "create_or_update")
        self.assertTrue(req["atomic"])
        self.assertFalse(req["dry_run"])

    def test_parse_schema_example_shape(self):
        payload = {
            "protocol_version": "1.0",
            "request_id": "3f1c2e40-8a11-4c2b-9b7e-2e5d1a0c9f31",
            "action": "create_niagara_effect",
            "project": {
                "path": "$UEREMCP_LEGACY_PROJECT/RE.uproject",
                "engine_version": "5.8",
            },
            "target": {"asset_path": "/Game/VFX/Spells/NS_Fireball"},
            "mode": "create_or_update",
            "specification": {"name": "Fireball"},
            "options": {
                "dry_run": False,
                "atomic": True,
                "response_detail": "summary",
            },
            "expected_revision": None,
            "idempotency_key": "poc-b-fireball-001",
        }
        req = parse_request(json.dumps(payload))
        self.assertEqual(req["target_asset_path"], "/Game/VFX/Spells/NS_Fireball")
        self.assertEqual(req["idempotency_key"], "poc-b-fireball-001")
        self.assertFalse(req["has_expected_revision"])
        self.assertEqual(req["specification"]["name"], "Fireball")

    def test_reject_unknown_field(self):
        with self.assertRaises(EnvelopeError):
            parse_request(
                '{"protocol_version":"1.0","action":"ping","extra":true}'
            )

    def test_reject_bad_action(self):
        with self.assertRaises(EnvelopeError):
            parse_request('{"protocol_version":"1.0","action":"BadAction"}')

    def test_reject_bad_mode(self):
        with self.assertRaises(EnvelopeError):
            parse_request(
                '{"protocol_version":"1.0","action":"ping","mode":"upsert"}'
            )

    def test_serialize_and_rejection(self):
        text = make_rejection("rid-1", "Unsupported protocol_version")
        obj = json.loads(text)
        self.assertEqual(obj["status"], "rejected")
        self.assertEqual(obj["protocol_version"], "1.0")
        self.assertEqual(obj["request_id"], "rid-1")
        self.assertIn("mcp_round_trips", obj["metrics"])
        self.assertIn("internal_operations", obj["metrics"])

    def test_serialize_requires_metrics_and_summary(self):
        text = serialize_response(
            {
                "status": "no_change_required",
                "summary": "ok",
                "understood_action": "echo",
                "metrics": {"mcp_round_trips": 1, "internal_operations": 0},
            }
        )
        obj = json.loads(text)
        self.assertEqual(obj["understood"]["action"], "echo")


if __name__ == "__main__":
    unittest.main()
