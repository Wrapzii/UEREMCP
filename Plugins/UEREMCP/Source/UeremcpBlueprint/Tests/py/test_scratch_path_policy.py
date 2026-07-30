"""Table-driven scratch-path policy tests (Blueprint submit vs WS-11 RB-14)."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from scratch_path_policy import (
    is_blueprint_submit_scratch_path,
    is_unsupported_control_flow_type_id,
    is_ws11_safe_scratch_path,
    make_scratch_package_path,
)


FIXTURES_DIR = Path(__file__).resolve().parent.parent / "fixtures"


def load_policy_fixture() -> dict:
    data = json.loads((FIXTURES_DIR / "scratch_path_policy.fixture.json").read_text(encoding="utf-8"))
    data.pop("$comment", None)
    return data


class ScratchPathPolicyTests(unittest.TestCase):
    def test_blueprint_submit_allow_list(self) -> None:
        fixture = load_policy_fixture()
        for path in fixture["blueprint_submit_allow"]:
            with self.subTest(path=path):
                self.assertTrue(is_blueprint_submit_scratch_path(path))

    def test_blueprint_submit_deny_list(self) -> None:
        fixture = load_policy_fixture()
        for path in fixture["blueprint_submit_deny"]:
            with self.subTest(path=path):
                self.assertFalse(is_blueprint_submit_scratch_path(path))

    def test_ws11_safe_allow_list(self) -> None:
        fixture = load_policy_fixture()
        for path in fixture["ws11_safe_allow"]:
            with self.subTest(path=path):
                self.assertTrue(is_ws11_safe_scratch_path(path))

    def test_ws11_safe_deny_list(self) -> None:
        fixture = load_policy_fixture()
        for path in fixture["ws11_safe_deny"]:
            with self.subTest(path=path):
                self.assertFalse(is_ws11_safe_scratch_path(path))

    def test_poc_root_allowed_for_blueprint_but_not_ws11(self) -> None:
        poc_path = "/Game/__UeremcpPoc/Fireball/BP_Fireball.BP_Fireball"
        self.assertFalse(is_blueprint_submit_scratch_path(poc_path))
        self.assertFalse(is_ws11_safe_scratch_path(poc_path))

    def test_make_scratch_package_path_examples(self) -> None:
        for example in load_policy_fixture()["make_path_examples"]:
            with self.subTest(suite=example["suite"], name=example["name"]):
                self.assertEqual(
                    make_scratch_package_path(example["suite"], example["name"]),
                    example["expected"],
                )

    def test_make_path_rejects_empty_name(self) -> None:
        with self.assertRaises(ValueError):
            make_scratch_package_path("Suite", "")

    def test_control_flow_type_id_markers(self) -> None:
        self.assertTrue(is_unsupported_control_flow_type_id("Utilities|FlowControl|Branch"))
        self.assertTrue(is_unsupported_control_flow_type_id("FlowControl|Sequence"))
        self.assertFalse(is_unsupported_control_flow_type_id("Development|PrintString"))


if __name__ == "__main__":
    unittest.main()
