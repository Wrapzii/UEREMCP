#!/usr/bin/env python3
"""Offline honesty and drift guards for create_vfx_material execute_plan handler (WS-08).

Mirrors WS-07 plan-handler hardening: dry-run surfaces no_change_required, mutating
create stays partially_completed, and responses never claim *_validated without proof.

Usage::

    python schemas/domains/materials/test_plan_handlers.py
"""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
MATERIALS_DIR = REPO_ROOT / "schemas/domains/materials"
FIXTURES_DIR = MATERIALS_DIR / "fixtures"

PLAN_HANDLERS_H = (
    REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Public/UeremcpMaterialPlanHandlers.h"
)
PLAN_HANDLERS_CPP = (
    REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialPlanHandlers.cpp"
)
MODULE_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialModule.cpp"
PLAN_TESTS_CPP = (
    REPO_ROOT
    / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/Tests/UeremcpMaterialPlanHandlersTests.cpp"
)
TOOLSET_TESTS_CPP = (
    REPO_ROOT
    / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/Tests/UeremcpMaterialToolsetTests.cpp"
)
MATERIAL_SERVICE = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialService.cpp"
README = MATERIALS_DIR / "README.md"
DRY_FIXTURE = FIXTURES_DIR / "execute_plan_create_vfx_material_dry.json"
MUTATING_FIXTURE = FIXTURES_DIR / "create_vfx_material_honesty_probe.json"

REGISTERED_ACTION = "create_vfx_material"
NEVER_CLAIMS = frozenset({"created_and_validated", "modified_and_validated"})


class MaterialPlanHandlerHonestyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.handlers_h = PLAN_HANDLERS_H.read_text(encoding="utf-8")
        cls.handlers_cpp = PLAN_HANDLERS_CPP.read_text(encoding="utf-8")
        cls.module_cpp = MODULE_CPP.read_text(encoding="utf-8")
        cls.plan_tests_cpp = PLAN_TESTS_CPP.read_text(encoding="utf-8")
        cls.toolset_tests_cpp = TOOLSET_TESTS_CPP.read_text(encoding="utf-8")
        cls.material_service = MATERIAL_SERVICE.read_text(encoding="utf-8")
        cls.readme = README.read_text(encoding="utf-8")
        cls.dry_fixture = json.loads(DRY_FIXTURE.read_text(encoding="utf-8"))
        cls.mutating_fixture = json.loads(MUTATING_FIXTURE.read_text(encoding="utf-8"))

    def test_registered_action_name_is_create_vfx_material(self) -> None:
        self.assertIn("RegisteredActionName()", self.handlers_h)
        self.assertIn(f'TEXT("{REGISTERED_ACTION}")', self.handlers_cpp)

    def test_handler_delegates_to_toolset_create_vfx_material(self) -> None:
        self.assertIn("UUeremcpMaterialToolset::CreateVfxMaterial", self.handlers_cpp)
        self.assertIn("create_vfx_material returned an empty response", self.handlers_cpp)

    def test_unregister_only_owned_action(self) -> None:
        self.assertIn("UnregisterAction(RegisteredActionName())", self.handlers_cpp)
        self.assertNotIn("ClearActionHandlers", self.handlers_cpp)
        self.assertNotIn("create_procedural_texture", self.handlers_cpp)

    def test_module_registers_and_unregisters_plan_handler(self) -> None:
        self.assertIn("FUeremcpMaterialPlanHandlers::Register", self.module_cpp)
        self.assertIn("FUeremcpMaterialPlanHandlers::Unregister", self.module_cpp)
        register_idx = self.module_cpp.index("FUeremcpMaterialPlanHandlers::Register")
        unregister_idx = self.module_cpp.index("FUeremcpMaterialPlanHandlers::Unregister")
        toolset_register_idx = self.module_cpp.index("RegisterToolsetClass")
        toolset_unregister_idx = self.module_cpp.index("UnregisterToolsetClass")
        self.assertGreater(register_idx, toolset_register_idx)
        self.assertLess(unregister_idx, toolset_unregister_idx)

    def test_editor_tests_cover_dry_run_honesty(self) -> None:
        self.assertIn("ResponseNeverClaimsValidated", self.plan_tests_cpp)
        self.assertIn('TEXT("no_change_required")', self.plan_tests_cpp)
        self.assertIn("UEREMCP.Material.PlanHandlers.Registration", self.plan_tests_cpp)
        self.assertIn("UEREMCP.Material.PlanHandlers.AtomicPreflightBlocked", self.plan_tests_cpp)
        self.assertIn("UEREMCP.Material.PlanHandlers.CreateHonestyDryRun", self.plan_tests_cpp)
        self.assertIn("ExpectDiskAssetWhenValidated", self.toolset_tests_cpp)
        self.assertIn("CapPartialWhenProofUnavailable", self.material_service)
        self.assertIn("dry-run aggregate status", self.plan_tests_cpp)
        self.assertIn("dry-run operation status", self.plan_tests_cpp)
        self.assertIn("direct dry-run status", self.plan_tests_cpp)
        self.assertIn("internal_operations present for plan executor", self.plan_tests_cpp)
        for forbidden in NEVER_CLAIMS:
            self.assertNotIn(f'TestEqual(TEXT("status"), Status, FString(TEXT("{forbidden}")))', self.plan_tests_cpp)

    def test_material_service_dry_run_is_no_change_required(self) -> None:
        self.assertRegex(
            self.material_service,
            r"if\s*\(\s*Request\.bDryRun\s*\)[\s\S]*Result\.Status\s*=\s*TEXT\(\"no_change_required\"\)",
        )

    def test_material_service_mutating_default_is_partially_completed(self) -> None:
        self.assertIn("ResolveMaterialSuccessStatus", self.material_service)
        self.assertRegex(
            self.material_service,
            r"if\s*\(\s*!bValidate\s*\)\s*\{\s*return\s+TEXT\(\"partially_completed\"\)",
        )

    def test_dry_run_execute_plan_fixture_expectations(self) -> None:
        expectations = self.dry_fixture["expectations"]
        self.assertEqual(self.dry_fixture["registered_action"], REGISTERED_ACTION)
        self.assertEqual(expectations["dry_run_operation_status"], "no_change_required")
        self.assertEqual(expectations["mutating_operation_status"], "partially_completed")
        self.assertEqual(set(expectations["validation_never_claims"]), set(NEVER_CLAIMS))
        self.assertTrue(self.dry_fixture["plan_request"]["options"]["dry_run"])

    def test_mutating_honesty_probe_fixture(self) -> None:
        expectations = self.mutating_fixture["expectations"]
        self.assertEqual(self.mutating_fixture["action"], REGISTERED_ACTION)
        self.assertFalse(self.mutating_fixture["request"]["options"]["dry_run"])
        self.assertEqual(expectations["honest_status"], "partially_completed")
        self.assertEqual(set(expectations["validation_never_claims"]), set(NEVER_CLAIMS))

    def test_readme_documents_execute_plan_honesty(self) -> None:
        self.assertIn("FUeremcpPlanExecutor", self.readme)
        self.assertIn("no_change_required", self.readme)
        self.assertIn("partially_completed", self.readme)
        self.assertIn("test_plan_handlers.py", self.readme)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
