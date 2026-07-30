#!/usr/bin/env python3
"""Offline parity tests for element_presets.v1.json vs loader/parser expectations.

Usage::

    python schemas/domains/materials/test_element_presets_loader.py
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
PRESETS_JSON = REPO_ROOT / "schemas/domains/materials/element_presets.v1.json"
LOADER_CPP = (
    REPO_ROOT
    / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialElementPresetsLoader.cpp"
)
FEATURES_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatures.cpp"
FEATURE_GRAPH_CPP = (
    REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatureGraph.cpp"
)


class ElementPresetsLoaderParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.data = json.loads(PRESETS_JSON.read_text(encoding="utf-8"))
        cls.loader_cpp = LOADER_CPP.read_text(encoding="utf-8")
        cls.features_cpp = FEATURES_CPP.read_text(encoding="utf-8")
        cls.graph_cpp = FEATURE_GRAPH_CPP.read_text(encoding="utf-8")

    def test_json_purpose_defaults_cover_ws15_purposes(self) -> None:
        defaults = self.data["purpose_default_features"]
        for purpose in (
            "elemental_projectile_core",
            "elemental_projectile_trail",
            "fireball_core",
            "fireball_ribbon_trail",
        ):
            self.assertIn(purpose, defaults)
            self.assertGreaterEqual(len(defaults[purpose]), 4)

    def test_loader_parses_json_element_fields(self) -> None:
        for field in (
            "particle_color",
            "color_secondary",
            "emissive_scale",
            "flow_speed",
            "turbulence",
            "soft_edge",
            "depth_fade",
        ):
            self.assertIn(field, self.loader_cpp)
        self.assertIn('"elements"', self.loader_cpp)

    def test_fire_json_values_match_documented_defaults(self) -> None:
        fire = self.data["elements"]["fire"]
        self.assertEqual(fire["emissive_scale"], 8.0)
        self.assertEqual(fire["particle_color"][:3], [1.0, 0.35, 0.05])
        self.assertEqual(fire["depth_fade"], 120.0)

    def test_features_cpp_implements_distortion_and_flipbook(self) -> None:
        for token in ("distortion", "flipbook_subuv"):
            self.assertIn(token, self.features_cpp)
        self.assertIn("MaterialExpressionBumpOffset", self.graph_cpp)
        self.assertIn("MaterialExpressionTextureSampleParameterSubUV", self.graph_cpp)

    def test_resolve_path_candidates_include_repo_schemas(self) -> None:
        self.assertIn("schemas/domains/materials/element_presets.v1.json", self.loader_cpp)
        self.assertIn("Resources/Materials/element_presets.v1.json", self.loader_cpp)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
