#!/usr/bin/env python3
"""Offline tests for specification.features resolution (WS-08).

Validates element_presets.v1.json purpose defaults align with WS-15 elemental
projectile template and documents expected feature signatures for master naming.

Usage::

    python schemas/domains/materials/test_features.py
"""

from __future__ import annotations

import json
import unittest
import zlib
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
PRESETS_JSON = REPO_ROOT / "schemas" / "domains" / "materials" / "element_presets.v1.json"
FEATURES_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatures.cpp"
ELEMENTAL_TEMPLATE = REPO_ROOT / "templates" / "niagara" / "niagara.projectile.elemental.v1.json"


def feature_signature(features: list[str]) -> str:
    joined = ",".join(sorted(features))
    crc = zlib.crc32(joined.encode("utf-8")) & 0xFFFFFFFF
    return f"{crc:08X}"


class MaterialFeaturesTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.data = json.loads(PRESETS_JSON.read_text(encoding="utf-8"))
        cls.cpp_text = FEATURES_CPP.read_text(encoding="utf-8")

    def test_ws15_core_features_match_defaults(self) -> None:
        if not ELEMENTAL_TEMPLATE.is_file():
            self.skipTest("elemental template missing")
        template = json.loads(ELEMENTAL_TEMPLATE.read_text(encoding="utf-8"))
        core_spec = next(
            step["specification"]
            for step in template["construction_plan"]
            if step.get("id") == "core_material"
        )
        expected = set(self.data["purpose_default_features"]["elemental_projectile_core"])
        actual = set(core_spec.get("features", []))
        self.assertEqual(actual, expected)

    def test_ws15_trail_features_match_defaults(self) -> None:
        if not ELEMENTAL_TEMPLATE.is_file():
            self.skipTest("elemental template missing")
        template = json.loads(ELEMENTAL_TEMPLATE.read_text(encoding="utf-8"))
        trail_spec = next(
            step["specification"]
            for step in template["construction_plan"]
            if step.get("id") == "trail_material"
        )
        expected = set(self.data["purpose_default_features"]["elemental_projectile_trail"])
        actual = set(trail_spec.get("features", []))
        self.assertEqual(actual, expected)

    def test_feature_signature_stable(self) -> None:
        core = self.data["purpose_default_features"]["elemental_projectile_core"]
        sig_a = feature_signature(core)
        sig_b = feature_signature(list(reversed(core)))
        self.assertEqual(sig_a, sig_b)
        self.assertEqual(len(sig_a), 8)

    def test_master_base_names_in_cpp(self) -> None:
        for base in self.data["purpose_master_base"].values():
            self.assertIn(base, self.cpp_text)

    def test_implemented_features_documented_in_readme(self) -> None:
        readme = (PRESETS_JSON.parent / "README.md").read_text(encoding="utf-8")
        for token in (
            "radial_falloff",
            "animated_noise",
            "fresnel",
            "panning_textures",
            "flow_maps",
            "distortion",
            "flipbook_subuv",
            "depth_fade",
            "erosion",
        ):
            self.assertIn(token, readme)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
