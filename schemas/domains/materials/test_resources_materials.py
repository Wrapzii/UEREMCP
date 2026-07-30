#!/usr/bin/env python3
"""Offline guards for packaged Material resources (WS-08, bb04bb9).

Ensures Plugins/UEREMCP/Resources/Materials/element_presets.v1.json stays in sync
with schemas/domains/materials/element_presets.v1.json payload.

Usage::

    python schemas/domains/materials/test_resources_materials.py
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
SCHEMA_PRESETS = REPO_ROOT / "schemas/domains/materials/element_presets.v1.json"
BUNDLED_PRESETS = REPO_ROOT / "Plugins/UEREMCP/Resources/Materials/element_presets.v1.json"
RESOURCES_README = REPO_ROOT / "Plugins/UEREMCP/Resources/Materials/README.md"
LOADER_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialElementPresetsLoader.cpp"


def payload_without_comments(data: dict) -> dict:
    return {k: v for k, v in data.items() if not k.startswith("$")}


class ResourcesMaterialsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.schema = json.loads(SCHEMA_PRESETS.read_text(encoding="utf-8"))
        cls.bundled = json.loads(BUNDLED_PRESETS.read_text(encoding="utf-8"))
        cls.loader_cpp = LOADER_CPP.read_text(encoding="utf-8")

    def test_bundled_presets_file_exists(self) -> None:
        self.assertTrue(BUNDLED_PRESETS.is_file())

    def test_bundled_payload_matches_schema_authoritative(self) -> None:
        schema_payload = payload_without_comments(self.schema)
        bundled_payload = payload_without_comments(self.bundled)
        self.assertEqual(bundled_payload, schema_payload)

    def test_resources_readme_documents_loader_preference(self) -> None:
        readme = RESOURCES_README.read_text(encoding="utf-8")
        self.assertIn("Loader resolution order", readme)
        self.assertIn("schemas/domains/materials", readme)
        self.assertIn("bb04bb9", readme)

    def test_loader_documents_resolution_order(self) -> None:
        self.assertIn("bb04bb9", self.loader_cpp)
        self.assertIn("Resources/Materials/element_presets.v1.json", self.loader_cpp)
        self.assertIn("schemas/domains/materials/element_presets.v1.json", self.loader_cpp)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
