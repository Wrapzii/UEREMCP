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

    def test_create_vfx_material_status_gated_on_validate(self) -> None:
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


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
