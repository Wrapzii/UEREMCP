#!/usr/bin/env python3
"""Offline guards for MaterialFunction composition proposal and flipbook scaffold (WS-08).

Usage::

    python schemas/domains/materials/test_material_function_composition.py
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
PROPOSAL = REPO_ROOT / "docs/proposals/ws-08-material-function-composition.md"
COMPOSITION_JSON = REPO_ROOT / "schemas/domains/materials/feature_composition.v1.json"
PROCEDURAL_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpProceduralTextureService.cpp"
CAPABILITY_H = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Public/UeremcpMaterialCapabilityNotes.h"
PROCEDURAL_SCHEMA = REPO_ROOT / "schemas/domains/materials/create_procedural_texture.schema.json"


class MaterialFunctionCompositionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.proposal = PROPOSAL.read_text(encoding="utf-8")
        cls.composition = json.loads(COMPOSITION_JSON.read_text(encoding="utf-8"))
        cls.procedural_cpp = PROCEDURAL_CPP.read_text(encoding="utf-8")
        cls.capability_h = CAPABILITY_H.read_text(encoding="utf-8")
        cls.procedural_schema = json.loads(PROCEDURAL_SCHEMA.read_text(encoding="utf-8"))

    def test_proposal_exists_with_verified_api_tags(self) -> None:
        self.assertTrue(PROPOSAL.is_file())
        for marker in (
            "MaterialExpressionMaterialFunctionCall",
            "MaterialEditingLibrary::UpdateMaterialFunction",
            "MaterialFunctionFactoryNew",
            "[VERIFIED:",
        ):
            self.assertIn(marker, self.proposal)

    def test_proposal_status_is_not_implemented(self) -> None:
        self.assertIn("not implemented", self.proposal.lower())

    def test_composition_json_covers_implemented_features(self) -> None:
        features_cpp = (
            REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatures.cpp"
        ).read_text(encoding="utf-8")
        import re

        implemented = set(re.findall(r'TEXT\("([^"]+)"\)', re.search(
            r"static const TSet<FString> Implemented = \{(.*?)\};",
            features_cpp,
            re.DOTALL,
        ).group(1)))
        composition_keys = set(self.composition["features"].keys())
        missing = implemented - composition_keys
        self.assertFalse(missing, f"feature_composition.v1.json missing: {sorted(missing)}")

    def test_capability_notes_material_function_internals(self) -> None:
        self.assertIn("material_function_internals", self.capability_h)
        self.assertIn("MaterialFunction", self.capability_h)

    def test_flipbook_atlas_generator_wired_in_service(self) -> None:
        enum_values = set(self.procedural_schema["properties"]["generate"]["enum"])
        self.assertIn("flipbook_atlas", enum_values)
        self.assertIn("GenerateFlipbookAtlasPixels", self.procedural_cpp)
        generator_cpp = (
            REPO_ROOT
            / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpProceduralTextureGenerator.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("divide evenly by columns and rows", generator_cpp)
        self.assertIn("FlipbookColumns", self.procedural_cpp)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
