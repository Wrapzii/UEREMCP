#!/usr/bin/env python3
"""Offline contract tests for Material validated-status reporting (WS-14 H-5).

Envelope rule: options.validate=false forfeits *_validated status
[VERIFIED: schemas/envelope/request.schema.json:79-82].

Usage::

    python schemas/domains/materials/test_validate_contract.py
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
MATERIAL_SERVICE = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialService.cpp"
MATERIAL_MASTER = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialMasterBuilder.cpp"
MATERIAL_ASSET_LOAD = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialAssetLoad.cpp"
PROCEDURAL_SERVICE = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpProceduralTextureService.cpp"
PROCEDURAL_HEADER = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Public/UeremcpProceduralTextureService.h"
ENVELOPE_REQUEST = REPO_ROOT / "schemas/envelope/request.schema.json"


class MaterialValidateContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.material_cpp = MATERIAL_SERVICE.read_text(encoding="utf-8")
        cls.procedural_cpp = PROCEDURAL_SERVICE.read_text(encoding="utf-8")
        cls.procedural_h = PROCEDURAL_HEADER.read_text(encoding="utf-8")
        cls.envelope = ENVELOPE_REQUEST.read_text(encoding="utf-8")

    def test_envelope_documents_validate_false_rule(self) -> None:
        self.assertIn("forfeits any *_validated status", self.envelope)
        self.assertIn("partially_completed", self.envelope)

    def test_master_builder_registers_fresh_assets(self) -> None:
        master_cpp = (
            REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialMasterBuilder.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("FAssetRegistryModule::AssetCreated", master_cpp)
        self.assertIn("MasterMaterial = Material", master_cpp)
        self.assertIn("master save unverified under automation", master_cpp)

    def test_create_vfx_material_caps_partial_when_proof_unavailable(self) -> None:
        self.assertIn("CapPartialWhenProofUnavailable", self.material_cpp)
        self.assertIn("UeremcpMaterialAssetLoad::ResolveMaterial", self.material_cpp)
        self.assertNotRegex(
            self.material_cpp,
            r"VerifyInstanceParameters[\s\S]{0,120}Result\.Status = TEXT\(\"failed_validation\"\)",
        )
        self.assertIn("ResolveMaterialSuccessStatus", self.material_cpp)
        self.assertRegex(
            self.material_cpp,
            r"if\s*\(\s*!bValidate\s*\)\s*\{\s*return\s+TEXT\(\"partially_completed\"\)",
        )
        self.assertNotRegex(
            self.material_cpp,
            r"parameters applied, parent recompiled, re-read verified",
        )

    def test_create_vfx_material_summary_conditional_on_validate(self) -> None:
        self.assertIn("BuildMaterialSuccessSummary", self.material_cpp)
        self.assertIn("options.validate=false", self.material_cpp)

    def test_modify_with_unimplemented_features_not_modified_and_validated(self) -> None:
        self.assertRegex(
            self.material_cpp,
            r"bHasUnimplementedFeatures[\s\S]*partially_completed",
        )

    def test_procedural_texture_propagates_bvalidate(self) -> None:
        self.assertIn("bValidate", self.procedural_h)
        self.assertIn("TextureRequest.bValidate = Request.bValidate", self.procedural_cpp)

    def test_procedural_texture_validate_false_is_partially_completed(self) -> None:
        self.assertRegex(
            self.procedural_cpp,
            r"if\s*\(\s*!Request\.bValidate\s*\)[\s\S]*TEXT\(\"partially_completed\"\)",
        )

    def test_create_vfx_material_saves_before_compile_gate(self) -> None:
        save_idx = self.material_cpp.find("SaveLoadedAssetPackage(Instance)")
        compile_idx = self.material_cpp.find("RecompileMaterial(MasterMaterial)")
        self.assertNotEqual(save_idx, -1)
        self.assertNotEqual(compile_idx, -1)
        self.assertLess(save_idx, compile_idx)

    def test_asset_load_gates_editor_subsystem_with_does_asset_exist(self) -> None:
        asset_load_cpp = MATERIAL_ASSET_LOAD.read_text(encoding="utf-8")
        self.assertIn("DoesAssetExist", asset_load_cpp)
        self.assertRegex(
            asset_load_cpp,
            r"if\s*\(\s*!AssetSubsystem->DoesAssetExist\(PackagePath\)\s*\)\s*\{\s*return nullptr;\s*\}",
        )
        master_cpp = MATERIAL_MASTER.read_text(encoding="utf-8")
        self.assertIn("UeremcpMaterialAssetLoad::ResolveMaterial", master_cpp)
        self.assertNotIn("LoadAsset(PackagePath)", master_cpp)
        self.assertNotIn("AssetSubsystem->LoadAsset", self.material_cpp)
        self.assertNotIn("AssetSubsystem->LoadAsset", self.procedural_cpp)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
