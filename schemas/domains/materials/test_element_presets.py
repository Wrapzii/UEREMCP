#!/usr/bin/env python3
"""Cross-check C++ element preset tables against element_presets.v1.json.

Usage::

    python schemas/domains/materials/test_element_presets.py
"""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
PRESETS_JSON = REPO_ROOT / "schemas" / "domains" / "materials" / "element_presets.v1.json"
PRESETS_CPP = (
    REPO_ROOT
    / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialElementPresets.cpp"
)
FEATURES_CPP = (
    REPO_ROOT
    / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatures.cpp"
)


class ElementPresetParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.data = json.loads(PRESETS_JSON.read_text(encoding="utf-8"))
        cls.cpp_text = PRESETS_CPP.read_text(encoding="utf-8")

    def test_json_elements_include_ws15_palette(self) -> None:
        for element in ("fire", "water", "wind", "earth", "ice"):
            self.assertIn(element, self.data["elements"])

    def test_purpose_master_base_matches_cpp(self) -> None:
        features_cpp = FEATURES_CPP.read_text(encoding="utf-8")
        for purpose, master in self.data["purpose_master_base"].items():
            self.assertIn(purpose, features_cpp)
            self.assertIn(master, features_cpp)

    def test_fire_defaults_present_in_cpp(self) -> None:
        fire = self.data["elements"]["fire"]
        # Spot-check literals mirrored in GetElementDefaults fire branch.
        self.assertIn("0.35f", self.cpp_text)
        self.assertIn("8.0f", self.cpp_text)
        self.assertIn(str(fire["depth_fade"]).replace(".0", ".0f").split(".")[0], self.cpp_text)
        pc = fire["particle_color"]
        self.assertIn(f"{pc[0]}f", self.cpp_text)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
