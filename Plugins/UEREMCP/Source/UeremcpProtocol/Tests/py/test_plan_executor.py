"""Offline execute_plan interpreter tests (mirrors C++ PlanExecutor automation)."""

from __future__ import annotations

import json
import unittest

from ueremcp_protocol.plan_executor import PlanExecutor, PlanExecutorError


def _ok(status: str, summary: str, **extra) -> str:
    body = {
        "protocol_version": "1.0",
        "status": status,
        "summary": summary,
        "metrics": {"mcp_round_trips": 1, "internal_operations": extra.pop("internal", 1)},
    }
    body.update(extra)
    return json.dumps(body)


class PlanExecutorTests(unittest.TestCase):
    def setUp(self) -> None:
        self.exe = PlanExecutor()

    def tearDown(self) -> None:
        self.exe.clear()

    def test_missing_handler_rejects_before_begin(self) -> None:
        began = False

        def begin():
            nonlocal began
            began = True
            return True, ""

        self.exe.set_transaction_callbacks(
            begin, lambda: (True, ""), lambda: (True, "")
        )
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "request_id": "preflight",
                    "action": "execute_plan",
                    "specification": {
                        "operations": [{"id": "missing", "action": "missing_action"}]
                    },
                }
            )
        )
        self.assertTrue(ok)
        response = json.loads(response_json)
        self.assertEqual(response["status"], "rejected")
        self.assertIn("missing_action", response["summary"])
        self.assertFalse(began)

    def test_atomic_requires_transaction_callbacks(self) -> None:
        self.exe.register_action(
            "noop",
            lambda _req: (True, _ok("no_change_required", "unused"), ""),
        )
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "operations": [{"id": "a", "action": "noop"}],
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(json.loads(response_json)["status"], "rejected")

    def test_topo_ref_and_metrics(self) -> None:
        order: list[str] = []
        began = committed = rolled = False

        def begin():
            nonlocal began
            began = True
            return True, ""

        def commit():
            nonlocal committed
            committed = True
            return True, ""

        def rollback():
            nonlocal rolled
            rolled = True
            return True, ""

        self.exe.set_transaction_callbacks(begin, commit, rollback)

        def produce(_req: str):
            order.append("producer")
            return (
                True,
                _ok(
                    "created_and_validated",
                    "producer ok",
                    internal=3,
                    result={
                        "primary_asset": "/Game/Test/A",
                        "created_assets": [{"asset_path": "/Game/Test/A"}],
                    },
                    changes=[{"asset_path": "/Game/Test/A", "kind": "created"}],
                    revision="sha256:producer",
                ),
                "",
            )

        def consume(req: str):
            order.append("consumer")
            nested = json.loads(req)
            self.assertEqual(
                nested["specification"]["input_asset"], "/Game/Test/A"
            )
            self.assertEqual(nested["options"]["timeout_ms"], 0)
            return (
                True,
                _ok(
                    "modified_and_validated",
                    "consumer ok",
                    internal=5,
                    result={
                        "primary_asset": "/Game/Test/B",
                        "modified_assets": [{"asset_path": "/Game/Test/B"}],
                    },
                    changes=[{"asset_path": "/Game/Test/B", "kind": "modified"}],
                    revision="sha256:consumer",
                ),
                "",
            )

        self.exe.register_action("produce_asset", produce)
        self.exe.register_action("consume_asset", consume)
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "request_id": "plan-dispatch",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {
                            "atomic": True,
                            "rollback_on_failure": True,
                            "compile_policy": "at_boundaries",
                            "validate_policy": "at_end",
                        },
                        "operations": [
                            {
                                "id": "consumer",
                                "action": "consume_asset",
                                "depends_on": ["producer"],
                                "specification": {
                                    "input_asset": {
                                        "$ref": "producer.result.primary_asset"
                                    }
                                },
                            },
                            {
                                "id": "producer",
                                "action": "produce_asset",
                                "specification": {},
                            },
                        ],
                        "on_failure": "rollback_all",
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(order, ["producer", "consumer"])
        self.assertTrue(began and committed and not rolled)
        response = json.loads(response_json)
        self.assertEqual(response["status"], "modified_and_validated")
        self.assertEqual(response["revision"], "sha256:consumer")
        self.assertEqual(len(response["result"]["operations"]), 2)
        self.assertEqual(response["metrics"]["internal_operations"], 8)
        self.assertEqual(response["metrics"]["mcp_round_trips"], 1)
        self.assertEqual(len(response["changes"]), 2)

    def test_required_failure_rolls_back(self) -> None:
        rolled = False

        def rollback():
            nonlocal rolled
            rolled = True
            return True, ""

        self.exe.set_transaction_callbacks(
            lambda: (True, ""), lambda: (True, ""), rollback
        )
        self.exe.register_action(
            "fail_action",
            lambda _req: (
                True,
                _ok("failed_validation", "expected failure", internal=2),
                "",
            ),
        )
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "request_id": "plan-rollback",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {
                            "atomic": True,
                            "rollback_on_failure": True,
                        },
                        "operations": [{"id": "failure", "action": "fail_action"}],
                        "on_failure": "rollback_all",
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertTrue(rolled)
        response = json.loads(response_json)
        self.assertEqual(response["status"], "rolled_back")
        self.assertTrue(response["rollback"]["performed"])

    def test_asset_condition_rejects_preflight(self) -> None:
        self.exe.register_action(
            "condition_action",
            lambda _req: (True, _ok("no_change_required", "unused"), ""),
        )
        self.exe.set_transaction_callbacks(
            lambda: (True, ""), lambda: (True, ""), lambda: (True, "")
        )
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "operations": [
                            {
                                "id": "condition",
                                "action": "condition_action",
                                "condition": {"asset_exists": "/Game/A"},
                            }
                        ]
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(json.loads(response_json)["status"], "rejected")

    def test_continue_independent_skips_dependents(self) -> None:
        ran: list[str] = []

        def fail(_req: str):
            ran.append("fail")
            return True, _ok("failed_validation", "boom", internal=1), ""

        def independent(_req: str):
            ran.append("independent")
            return True, _ok("no_change_required", "ok", internal=1), ""

        def dependent(_req: str):
            ran.append("dependent")
            return True, _ok("no_change_required", "should not run", internal=1), ""

        self.exe.register_action("fail_op", fail)
        self.exe.register_action("independent_op", independent)
        self.exe.register_action("dependent_op", dependent)
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {"atomic": False, "rollback_on_failure": False},
                        "operations": [
                            {"id": "fail", "action": "fail_op"},
                            {"id": "indep", "action": "independent_op"},
                            {
                                "id": "dep",
                                "action": "dependent_op",
                                "depends_on": ["fail"],
                            },
                        ],
                        "on_failure": "continue_independent",
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(ran, ["fail", "independent"])
        response = json.loads(response_json)
        self.assertEqual(response["status"], "partially_completed")
        by_id = {op["id"]: op for op in response["result"]["operations"]}
        self.assertEqual(by_id["dep"]["status"], "partially_completed")
        self.assertIn("dependency", by_id["dep"]["skipped_reason"])

    def test_optional_failure_does_not_stop_plan(self) -> None:
        ran: list[str] = []

        def optional_fail(_req: str):
            ran.append("optional")
            return True, _ok("failed_validation", "optional boom", internal=1), ""

        def later(_req: str):
            ran.append("later")
            return True, _ok("created_and_validated", "later ok", internal=2), ""

        self.exe.register_action("optional_fail", optional_fail)
        self.exe.register_action("later_op", later)
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {"atomic": False, "rollback_on_failure": False},
                        "operations": [
                            {
                                "id": "opt",
                                "action": "optional_fail",
                                "optional": True,
                            },
                            {"id": "later", "action": "later_op"},
                        ],
                        "on_failure": "stop",
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(ran, ["optional", "later"])
        response = json.loads(response_json)
        self.assertEqual(response["status"], "partially_completed")
        self.assertEqual(response["metrics"]["internal_operations"], 3)

    def test_duplicate_registration_rejected(self) -> None:
        self.exe.register_action(
            "dup", lambda _req: (True, _ok("no_change_required", "x"), "")
        )
        with self.assertRaises(PlanExecutorError):
            self.exe.register_action(
                "dup", lambda _req: (True, _ok("no_change_required", "y"), "")
            )

    def test_cycle_rejected(self) -> None:
        self.exe.register_action(
            "a_op", lambda _req: (True, _ok("no_change_required", "x"), "")
        )
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {"atomic": False},
                        "operations": [
                            {"id": "a", "action": "a_op", "depends_on": ["b"]},
                            {"id": "b", "action": "a_op", "depends_on": ["a"]},
                        ],
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(json.loads(response_json)["status"], "rejected")
        self.assertIn("cycle", json.loads(response_json)["summary"].lower())

    def test_on_failure_stop_skips_remaining(self) -> None:
        ran: list[str] = []

        def fail(_req: str):
            ran.append("fail")
            return True, _ok("failed_validation", "boom", internal=1), ""

        def later(_req: str):
            ran.append("later")
            return True, _ok("no_change_required", "should not run", internal=1), ""

        self.exe.register_action("fail_op", fail)
        self.exe.register_action("later_op", later)
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {"atomic": False, "rollback_on_failure": False},
                        "operations": [
                            {"id": "fail", "action": "fail_op"},
                            {"id": "later", "action": "later_op"},
                        ],
                        "on_failure": "stop",
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(ran, ["fail"])
        response = json.loads(response_json)
        self.assertEqual(response["status"], "partially_completed")
        by_id = {op["id"]: op for op in response["result"]["operations"]}
        self.assertEqual(by_id["later"]["status"], "partially_completed")
        self.assertIn("stopped", by_id["later"]["skipped_reason"])

    def test_optional_failure_skips_dependents(self) -> None:
        ran: list[str] = []

        def optional_fail(_req: str):
            ran.append("optional")
            return True, _ok("failed_validation", "optional boom", internal=1), ""

        def dependent(_req: str):
            ran.append("dependent")
            return True, _ok("no_change_required", "should not run", internal=1), ""

        self.exe.register_action("optional_fail", optional_fail)
        self.exe.register_action("dependent_op", dependent)
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {"atomic": False, "rollback_on_failure": False},
                        "operations": [
                            {
                                "id": "opt",
                                "action": "optional_fail",
                                "optional": True,
                            },
                            {
                                "id": "dep",
                                "action": "dependent_op",
                                "depends_on": ["opt"],
                            },
                        ],
                        "on_failure": "stop",
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(ran, ["optional"])
        response = json.loads(response_json)
        self.assertEqual(response["status"], "partially_completed")
        by_id = {op["id"]: op for op in response["result"]["operations"]}
        self.assertEqual(by_id["dep"]["status"], "partially_completed")
        self.assertIn("dependency", by_id["dep"]["skipped_reason"])

    def test_created_with_warnings_allows_ref(self) -> None:
        def produce(_req: str):
            return (
                True,
                _ok(
                    "created_with_warnings",
                    "producer warned",
                    internal=1,
                    result={"primary_asset": "/Game/Test/Warned"},
                ),
                "",
            )

        def consume(req: str):
            nested = json.loads(req)
            self.assertEqual(
                nested["specification"]["input_asset"], "/Game/Test/Warned"
            )
            return True, _ok("modified_and_validated", "consumer ok", internal=1), ""

        self.exe.register_action("produce_asset", produce)
        self.exe.register_action("consume_asset", consume)
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {"atomic": False, "rollback_on_failure": False},
                        "operations": [
                            {"id": "producer", "action": "produce_asset"},
                            {
                                "id": "consumer",
                                "action": "consume_asset",
                                "depends_on": ["producer"],
                                "specification": {
                                    "input_asset": {
                                        "$ref": "producer.result.primary_asset"
                                    }
                                },
                            },
                        ],
                        "on_failure": "stop",
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(json.loads(response_json)["status"], "modified_and_validated")

    def test_operation_status_condition_skip(self) -> None:
        ran: list[str] = []

        def first(_req: str):
            ran.append("first")
            return True, _ok("created_with_warnings", "warned", internal=1), ""

        def gated(_req: str):
            ran.append("gated")
            return True, _ok("no_change_required", "should not run", internal=1), ""

        self.exe.register_action("first_op", first)
        self.exe.register_action("gated_op", gated)
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {"atomic": False, "rollback_on_failure": False},
                        "operations": [
                            {"id": "first", "action": "first_op"},
                            {
                                "id": "gated",
                                "action": "gated_op",
                                "depends_on": ["first"],
                                "condition": {
                                    "operation_status": {
                                        "id": "first",
                                        "is": ["created_and_validated"],
                                    }
                                },
                            },
                        ],
                        "on_failure": "stop",
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(ran, ["first"])
        response = json.loads(response_json)
        by_id = {op["id"]: op for op in response["result"]["operations"]}
        self.assertEqual(by_id["gated"]["status"], "no_change_required")
        self.assertIn("condition not met", by_id["gated"]["skipped_reason"])

    def test_non_atomic_without_txn_callbacks(self) -> None:
        self.exe.register_action(
            "noop",
            lambda _req: (True, _ok("no_change_required", "ok", internal=1), ""),
        )
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {"atomic": False, "rollback_on_failure": False},
                        "operations": [{"id": "a", "action": "noop"}],
                        "on_failure": "stop",
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(json.loads(response_json)["status"], "no_change_required")

    def test_atomic_stop_without_rollback_flag_commits(self) -> None:
        rolled = committed = False

        def commit():
            nonlocal committed
            committed = True
            return True, ""

        def rollback():
            nonlocal rolled
            rolled = True
            return True, ""

        self.exe.set_transaction_callbacks(
            lambda: (True, ""), commit, rollback
        )
        self.exe.register_action(
            "fail_action",
            lambda _req: (
                True,
                _ok("failed_validation", "expected failure", internal=1),
                "",
            ),
        )
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {
                            "atomic": True,
                            "rollback_on_failure": False,
                        },
                        "operations": [{"id": "failure", "action": "fail_action"}],
                        "on_failure": "stop",
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertTrue(committed and not rolled)
        response = json.loads(response_json)
        self.assertEqual(response["status"], "partially_completed")
        self.assertNotIn("rollback", response)

    def test_handler_bool_false_is_failure(self) -> None:
        self.exe.register_action(
            "crash_op", lambda _req: (False, "", "handler unavailable")
        )
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {"atomic": False, "rollback_on_failure": False},
                        "operations": [{"id": "crash", "action": "crash_op"}],
                        "on_failure": "stop",
                    },
                }
            )
        )
        self.assertTrue(ok)
        response = json.loads(response_json)
        self.assertEqual(response["status"], "partially_completed")
        by_id = {op["id"]: op for op in response["result"]["operations"]}
        self.assertEqual(by_id["crash"]["status"], "error")
        self.assertIn("unavailable", by_id["crash"]["summary"])

    def test_dollar_string_ref_resolves(self) -> None:
        def produce(_req: str):
            return (
                True,
                _ok(
                    "created_and_validated",
                    "producer ok",
                    internal=1,
                    result={
                        "primary_asset": "/Game/Test/A",
                        "created_assets": [{"asset_path": "/Game/Test/A"}],
                    },
                ),
                "",
            )

        def consume(req: str):
            nested = json.loads(req)
            self.assertEqual(nested["specification"]["trail_material"], "/Game/Test/A")
            return True, _ok("modified_and_validated", "consumer ok", internal=1), ""

        self.exe.register_action("produce_asset", produce)
        self.exe.register_action("consume_asset", consume)
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {"atomic": False, "rollback_on_failure": False},
                        "operations": [
                            {"id": "producer", "action": "produce_asset"},
                            {
                                "id": "consumer",
                                "action": "consume_asset",
                                "depends_on": ["producer"],
                                "specification": {"trail_material": "$producer"},
                            },
                        ],
                        "on_failure": "stop",
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(json.loads(response_json)["status"], "modified_and_validated")

    def test_unregister_removes_handler(self) -> None:
        self.exe.register_action(
            "temp_op",
            lambda _req: (True, _ok("no_change_required", "ok", internal=1), ""),
        )
        self.exe.unregister_action("temp_op")
        ok, response_json = self.exe.execute_request(
            json.dumps(
                {
                    "protocol_version": "1.0",
                    "action": "execute_plan",
                    "specification": {
                        "transaction": {"atomic": False},
                        "operations": [{"id": "a", "action": "temp_op"}],
                    },
                }
            )
        )
        self.assertTrue(ok)
        self.assertEqual(json.loads(response_json)["status"], "rejected")
        self.assertIn("temp_op", json.loads(response_json)["summary"])


if __name__ == "__main__":
    unittest.main()
