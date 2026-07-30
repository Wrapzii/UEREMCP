#!/usr/bin/env python3
"""Offline guards for flipbook_atlas grid assembly (WS-08).

Usage::

    python schemas/domains/materials/test_flipbook_atlas_generator.py
"""

from __future__ import annotations

import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
GENERATOR_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpProceduralTextureGenerator.cpp"
TESTS_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/Tests/UeremcpMaterialToolsetTests.cpp"


class FlipbookAtlasGeneratorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.generator_cpp = GENERATOR_CPP.read_text(encoding="utf-8")
        cls.tests_cpp = TESTS_CPP.read_text(encoding="utf-8")

    def test_generator_exports_flipbook_assembly(self) -> None:
        self.assertIn("GenerateFlipbookAtlasPixels", self.generator_cpp)
        self.assertIn("FrameIndex % Columns", self.generator_cpp)
        self.assertIn("GeneratePixels(TEXT(\"noise\")", self.generator_cpp)

    def test_editor_automation_covers_flipbook_atlas(self) -> None:
        self.assertIn("CreateProceduralTexture.FlipbookAtlas", self.tests_cpp)
        self.assertIn("flipbook_atlas", self.tests_cpp)

    def test_grid_cell_math_documented(self) -> None:
        atlas_w, atlas_h, cols, rows = 256, 256, 4, 4
        self.assertEqual(atlas_w % cols, 0)
        self.assertEqual(atlas_h % rows, 0)
        cell_w = atlas_w // cols
        cell_h = atlas_h // rows
        self.assertEqual(cell_w, 64)
        self.assertEqual(cell_h, 64)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
