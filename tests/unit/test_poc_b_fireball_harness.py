#!/usr/bin/env python3
"""Contract tests for the single-create POC B fireball proof scaffold."""

from __future__ import annotations

import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


class FireballHarnessContractTest(unittest.TestCase):
    def test_runner_preserves_pass_fail_skip_and_two_fixtures(self):
        runner = (REPO_ROOT / "tests" / "run_poc_b_fireball.ps1").read_text(
            encoding="utf-8"
        )
        for token in (
            "UEREMCP.Niagara.POCB.FireballInlineMaterials",
            "poc_b_editor_gate_scaffold.json",
            "poc_b_fireball_materials.json",
            "-PocBScaffold",
            "-PocBMaterials",
            "UEREMCP_POC_B_FIREBALL_OUTCOME",
            '"skipped"',
            '"failed"',
            "not MCP transport or overall POC B proof",
        ):
            self.assertIn(token, runner)

    def test_filter_enforces_single_call_manifest_and_binding_proof(self):
        source = (
            REPO_ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpValidation"
            / "Private"
            / "Tests"
            / "NiagaraPocBFireballMaterials.spec.cpp"
        ).read_text(encoding="utf-8")
        self.assertEqual(source.count("CreateNiagaraEffect(RequestJson)"), 1)
        for token in (
            "/Game/__UeremcpPoc/NS_POCB_Fireball",
            'TEXT("materials")',
            'TEXT("created_assets")',
            'TEXT("reused_assets")',
            'TEXT("B2_created_assets_reported")',
            'TEXT("B2_reused_assets_reported")',
            'TEXT("B4_material_bindings_verified")',
            'TEXT("material_bindings_verified")',
            "renderer_bindings_verified",
            "POC material asset remains under POC root",
            "this is not transport/MCP proof",
        ):
            self.assertIn(token, source)

    def test_editor_runner_forwards_material_fixture(self):
        runner = (REPO_ROOT / "tests" / "run_editor_tests.ps1").read_text(
            encoding="utf-8"
        )
        self.assertIn('[string]$PocBMaterials = ""', runner)
        self.assertIn("UeremcpPocBMaterials=", runner)
        self.assertIn("UEREMCP_POC_B_FIREBALL_OUTCOME", runner)


if __name__ == "__main__":
    unittest.main()
