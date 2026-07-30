#!/usr/bin/env python3
"""Offline path-policy tests for Material scratch roots (WS-08).

Allowed write roots: /Game/__UeremcpTests/ and /Game/__UeremcpPoc/ (POC_ACCEPTANCE).

Usage::

    python schemas/domains/materials/test_path_policy.py
"""

from __future__ import annotations

import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
PATHS_HEADER = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Public/UeremcpMaterialPaths.h"
PATHS_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialPaths.cpp"
MATERIAL_SERVICE = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialService.cpp"
PROCEDURAL_SERVICE = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpProceduralTextureService.cpp"
MASTER_BUILDER = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialMasterBuilder.cpp"
NIAGARA_EXPORT = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialNiagaraExport.cpp"
TESTS_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/Tests/UeremcpMaterialToolsetTests.cpp"


class MaterialPathPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.paths_h = PATHS_HEADER.read_text(encoding="utf-8")
        cls.paths_cpp = PATHS_CPP.read_text(encoding="utf-8")
        cls.material_cpp = MATERIAL_SERVICE.read_text(encoding="utf-8")
        cls.procedural_cpp = PROCEDURAL_SERVICE.read_text(encoding="utf-8")
        cls.master_cpp = MASTER_BUILDER.read_text(encoding="utf-8")
        cls.niagara_cpp = NIAGARA_EXPORT.read_text(encoding="utf-8")
        cls.tests_cpp = TESTS_CPP.read_text(encoding="utf-8")

    def test_paths_header_declares_both_scratch_roots(self) -> None:
        self.assertIn('PocContentRoot = TEXT("/Game/__UeremcpPoc")', self.paths_h)
        self.assertIn("IsUnderAllowedScratchRoot", self.paths_h)
        self.assertIn("ResolveScratchContentRoot", self.paths_h)
        self.assertIn("MastersFolderForContentRoot", self.paths_h)

    def test_write_guards_use_allowed_scratch_root(self) -> None:
        self.assertIn("IsUnderAllowedScratchRoot(Request.TargetAssetPath)", self.material_cpp)
        self.assertIn("IsUnderAllowedScratchRoot(Request.TargetAssetPath)", self.procedural_cpp)
        self.assertIn("IsUnderAllowedScratchRoot(Request.MasterPackagePath)", self.master_cpp)
        self.assertNotIn("until WS-12 tier policy extends allowed roots", self.material_cpp)

    def test_master_and_textures_follow_target_scratch_root(self) -> None:
        self.assertIn("ResolveMasterPackagePath(Purpose, Features, ScratchContentRoot)", self.material_cpp)
        self.assertIn("TexturesFolderForContentRoot(ScratchContentRoot)", self.material_cpp)
        self.assertIn("MastersFolderForContentRoot", self.paths_cpp)
        features_cpp = (
            REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatures.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("MastersFolderForContentRoot(ScratchContentRoot)", features_cpp)

    def test_niagara_export_supports_poc_system_paths(self) -> None:
        self.assertIn("ResolveMaterialInstancePathForNiagaraSystem", self.niagara_cpp)
        self.assertIn("ExecuteCreateVfxMaterialForNiagaraSystem", self.niagara_cpp)
        self.assertIn("MaterialsFolderForContentRoot(ScratchContentRoot)", self.niagara_cpp)

    def test_editor_tests_cover_allow_and_deny(self) -> None:
        self.assertIn("Paths.AllowedScratchRoot", self.tests_cpp)
        self.assertIn("CreateVfxMaterial.PocPathPolicy", self.tests_cpp)
        self.assertIn("game content denied", self.tests_cpp)
        self.assertIn("bad root rejected", self.tests_cpp)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
