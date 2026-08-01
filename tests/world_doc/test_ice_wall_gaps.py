"""Static acceptance checks for RB-ice-wall-plugin-gaps.md (IW-001..005)."""
from __future__ import annotations

import json
import os
import sys
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

MATERIAL_SERVICE = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpMaterial", "Private",
    "UeremcpMaterialService.cpp")
MATERIAL_FEATURES = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpMaterial", "Private",
    "UeremcpMaterialFeatures.cpp")
FEATURE_GRAPH = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpMaterial", "Private",
    "UeremcpMaterialFeatureGraph.cpp")
IMPORT_MESH = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpEnvironment", "Private",
    "UeremcpImportMesh.cpp")
CAPTURE = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpValidation", "Private",
    "UeremcpVisualCaptureToolset.cpp")
CAPTURE_COMMON = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpValidation", "Private",
    "UeremcpVisualCaptureCommon.cpp")
NIAGARA_CREATE = os.path.join(
    REPO, "Plugins", "UEREMCP", "Source", "UeremcpNiagara", "Private",
    "UeremcpNiagaraCreate.cpp")
ELEMENT_PRESETS = os.path.join(
    REPO, "Plugins", "UEREMCP", "Resources", "Materials", "element_presets.v1.json")
GAPS_DOC = os.path.join(REPO, "docs", "research", "RB-ice-wall-plugin-gaps.md")
HARD_GAPS = os.path.join(REPO, "docs", "research", "RB-MCP-hard-gaps-fieldtest.md")


def read(path: str) -> str:
    with open(path, encoding="utf-8") as fh:
        return fh.read()


class TestIceWallDocs(unittest.TestCase):
    def test_gaps_doc_exists(self):
        body = read(GAPS_DOC)
        self.assertIn("IW-001", body)
        self.assertIn("IW-002", body)
        self.assertIn("IW-003", body)
        self.assertNotIn("C:\\Users\\WhiteWidow", body)

    def test_hard_gaps_cross_link(self):
        body = read(HARD_GAPS)
        self.assertIn("RB-ice-wall-plugin-gaps.md", body)


class TestSilentImport(unittest.TestCase):
    def test_asset_import_task_primary(self):
        body = read(IMPORT_MESH)
        self.assertIn("UAssetImportTask", body)
        self.assertIn("bAutomated", body)
        self.assertIn("ImportAssetTasks", body)
        self.assertIn("GIsAutomationTesting", body)


class TestIceMaterials(unittest.TestCase):
    def test_purposes_wired(self):
        service = read(MATERIAL_SERVICE)
        features = read(MATERIAL_FEATURES)
        graph = read(FEATURE_GRAPH)
        self.assertIn("elemental_ice_barrier", service)
        self.assertIn("ice_crystal", service)
        self.assertIn("NormalizePurpose", service)
        self.assertIn("IsIceBarrierPurpose", features)
        self.assertIn("M_Ueremcp_IceCrystal", features)
        self.assertIn("BLEND_Translucent", graph)
        self.assertIn("bTranslucentBlend", graph)

    def test_presets_json(self):
        with open(ELEMENT_PRESETS, encoding="utf-8") as fh:
            data = json.load(fh)
        self.assertIn("ice_crystal", data["purpose_default_features"])
        self.assertIn("elemental_ice_barrier", data["purpose_default_features"])
        self.assertEqual(
            data["purpose_master_base"]["elemental_ice_barrier"],
            "M_Ueremcp_IceCrystal")


class TestCaptureFraming(unittest.TestCase):
    def test_world_content_bounds(self):
        common = read(CAPTURE_COMMON)
        toolset = read(CAPTURE)
        self.assertIn("ComputeWorldContentBounds", common)
        self.assertIn("ScaledCameraOffset", common)
        self.assertIn("world_content_bounds", toolset)
        self.assertIn("NEAR_BLACK", toolset)
        self.assertIn("focus_location", toolset)
        self.assertIn("CaptureViewport", toolset)


class TestNiagaraMistParams(unittest.TestCase):
    def test_density_radius_aliases(self):
        body = read(NIAGARA_CREATE)
        self.assertIn("User.Density", body)
        self.assertIn("User.MistDensity", body)
        self.assertIn("User.Radius", body)
        self.assertIn("mist_density", body)
        self.assertIn("niagara.default_mist_density", body)


if __name__ == "__main__":
    unittest.main()
