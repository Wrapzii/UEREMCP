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

    def test_cap_partial_fails_when_primary_mi_absent(self) -> None:
        self.assertRegex(
            self.material_cpp,
            r"if\s*\(\s*!Instance\s*\)[\s\S]*Result\.bSuccess = false[\s\S]*TEXT\(\"failed_validation\"\)[\s\S]*PrimaryAsset\.Reset\(\)",
        )
        self.assertIn(
            "persisted master-only assets do not satisfy create_vfx_material",
            self.material_cpp,
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
        save_idx = self.material_cpp.find("SaveAssetObject(Instance, Request.TargetAssetPath")
        compile_idx = self.material_cpp.find("RecompileMaterial(MasterMaterial)")
        self.assertNotEqual(save_idx, -1)
        self.assertNotEqual(compile_idx, -1)
        self.assertLess(save_idx, compile_idx)

    def test_create_vfx_material_mi_uses_create_package_path(self) -> None:
        self.assertIn("CreateMaterialInstanceAtPath", self.material_cpp)
        self.assertIn("CreatePackage(*PackagePath)", self.material_cpp)
        self.assertIn("NewObject<UMaterialInstanceConstant>", self.material_cpp)
        self.assertIn("ReleasePackageForCreate", self.material_cpp)
        self.assertIn("TryLoadRegisteredMaterialInstance", self.material_cpp)
        self.assertIn("!Request.bSave", self.material_cpp)

    def test_master_create_uses_create_package_after_release(self) -> None:
        master_cpp = (
            REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialMasterBuilder.cpp"
        ).read_text(encoding="utf-8")
        asset_load_cpp = (
            REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialAssetLoad.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("ReleasePackageForCreate", asset_load_cpp)
        self.assertIn("NewObject<UMaterial>", master_cpp)
        self.assertIn("failed feature-graph verification; recreating package", master_cpp)

    def test_create_vfx_material_reports_mi_save_failure(self) -> None:
        self.assertIn("ReportMiSaveFailure", self.material_cpp)
        self.assertIn("MI save failed for", self.material_cpp)
        self.assertIn("UEditorLoadingAndSavingUtils::SavePackages", self.material_cpp)

    def test_master_builder_reuses_registry_not_stale_in_process(self) -> None:
        master_cpp = MATERIAL_MASTER.read_text(encoding="utf-8")
        self.assertIn("TryLoadRegisteredMaterial", master_cpp)
        self.assertNotIn("ResolveMaterial(PackagePath)", master_cpp)

    def test_master_builder_verifies_or_rebuilds_existing_master(self) -> None:
        master_cpp = MATERIAL_MASTER.read_text(encoding="utf-8")
        self.assertIn("VerifyFeatureGraph", master_cpp)
        self.assertIn("ReleasePackageForCreate", master_cpp)
        self.assertIn("failed feature-graph verification; recreating package", master_cpp)
        self.assertIn("Reused verified master", master_cpp)

    def test_incomplete_master_not_persisted_on_graph_failure(self) -> None:
        self.assertIn("Do not persist incomplete masters", self.material_cpp)
        self.assertNotRegex(
            self.material_cpp,
            r"!MasterResult\.bSuccess[\s\S]{0,400}TryPersistVfxAssets[\s\S]{0,120}Master material setup incomplete",
        )

    def test_create_vfx_material_save_uses_editor_asset_subsystem(self) -> None:
        self.assertIn("SaveAsset(PreferredPackagePath, false)", self.material_cpp)
        self.assertIn("SaveAsset(ActualPackagePath, false)", self.material_cpp)
        self.assertIn("UEditorLoadingAndSavingUtils::SavePackages", self.material_cpp)
        master_cpp = MATERIAL_MASTER.read_text(encoding="utf-8")
        self.assertIn("SaveAsset(PackagePath, false)", master_cpp)
        self.assertNotIn("SavePackages(PackagesToSave", master_cpp)

    def test_asset_load_gates_editor_subsystem_with_does_asset_exist(self) -> None:
        asset_load_cpp = MATERIAL_ASSET_LOAD.read_text(encoding="utf-8")
        self.assertIn("DoesAssetExist", asset_load_cpp)
        self.assertRegex(
            asset_load_cpp,
            r"if\s*\(\s*!AssetSubsystem->DoesAssetExist\(PackagePath\)\s*\)\s*\{\s*return nullptr;\s*\}",
        )
        master_cpp = MATERIAL_MASTER.read_text(encoding="utf-8")
        self.assertIn("TryLoadRegisteredMaterial", master_cpp)
        self.assertNotIn("LoadAsset(PackagePath)", master_cpp)
        self.assertNotIn("AssetSubsystem->LoadAsset", self.material_cpp)
        self.assertNotIn("AssetSubsystem->LoadAsset", self.procedural_cpp)

    def test_animated_noise_wires_float3_position_with_fallback_pins(self) -> None:
        feature_graph = (
            REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatureGraph.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("MaterialExpressionAppendVector", feature_graph)
        self.assertIn("ConnectToInputWithFallback", feature_graph)
        self.assertIn("World Position", feature_graph)
        self.assertIn("Failed to wire animated_noise.", feature_graph)

    def test_trail_depth_fade_wires_opacity_pin_with_fallback(self) -> None:
        feature_graph = (
            REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatureGraph.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("Failed to wire depth_fade.", feature_graph)
        self.assertIn("TEXT(\"Opacity\")", feature_graph)
        self.assertIn("Failed to wire panning_textures.", feature_graph)
        self.assertIn("TEXT(\"UVs\")", feature_graph)
        self.assertIn("Failed to connect UV chain to MainTexture.", feature_graph)

    def test_create_vfx_material_reports_reused_assets_for_master_reuse(self) -> None:
        self.assertIn("ReusedAssets", self.material_cpp)
        self.assertIn("Result.ReusedAssets.Add(ReusedMaster)", self.material_cpp)
        toolset_cpp = (
            REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialToolset.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("Response.ReusedAssets = CreateResult.ReusedAssets", toolset_cpp)
        tests_cpp = (
            REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/Tests/UeremcpMaterialToolsetTests.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("CreateVfxMaterial.MasterReuseManifest", tests_cpp)
        self.assertIn("reused_assets", tests_cpp)

    def test_procedural_texture_reports_reused_assets_on_idempotent_hit(self) -> None:
        self.assertIn("Result.ReusedAssets.Add(Reused)", self.procedural_cpp)
        toolset_cpp = (
            REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialToolset.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("Response.ReusedAssets = CreateResult.ReusedAssets", toolset_cpp)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
