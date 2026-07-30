#!/usr/bin/env python3
"""Offline verification for feature-graph wiring contracts (WS-08).

Exercises IsImplementedFeature ↔ VerifyFeatureGraph parity and distortion /
flipbook_subuv graph hooks without requiring a live editor.

Usage::

    python schemas/domains/materials/test_feature_graph_verification.py
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
FEATURES_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatures.cpp"
GRAPH_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatureGraph.cpp"
SERVICE_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialService.cpp"
README = REPO_ROOT / "schemas/domains/materials/README.md"


def extract_implemented_features(cpp: str) -> set[str]:
    match = re.search(
        r"static const TSet<FString> Implemented = \{(.*?)\};",
        cpp,
        re.DOTALL,
    )
    if not match:
        raise AssertionError("IsImplementedFeature set not found")
    return set(re.findall(r'TEXT\("([^"]+)"\)', match.group(1)))


def extract_verify_feature_wired_keys(cpp: str) -> set[str]:
    return set(re.findall(r'FeatureWired\.Add\(TEXT\("([^"]+)"\)', cpp))


class FeatureGraphVerificationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.features_cpp = FEATURES_CPP.read_text(encoding="utf-8")
        cls.graph_cpp = GRAPH_CPP.read_text(encoding="utf-8")
        cls.service_cpp = SERVICE_CPP.read_text(encoding="utf-8")
        cls.readme = README.read_text(encoding="utf-8")
        cls.implemented = extract_implemented_features(cls.features_cpp)
        cls.verify_keys = extract_verify_feature_wired_keys(cls.features_cpp)

    def test_implemented_features_have_verify_feature_wired_entry(self) -> None:
        missing = self.implemented - self.verify_keys
        self.assertFalse(missing, f"VerifyFeatureGraph missing keys: {sorted(missing)}")

    def test_verify_feature_graph_called_after_master_build(self) -> None:
        self.assertIn("UeremcpMaterialFeatures::VerifyFeatureGraph", self.graph_cpp)

    def test_distortion_verify_uses_bump_offset(self) -> None:
        self.assertIn("MaterialExpressionBumpOffset", self.features_cpp)
        self.assertIn('TEXT("distortion")', self.features_cpp)
        self.assertIn("BumpOffsets.Num() > 0", self.features_cpp)

    def test_flipbook_verify_uses_subuv_sample(self) -> None:
        self.assertIn("MaterialExpressionTextureSampleParameterSubUV", self.features_cpp)
        self.assertIn('TEXT("flipbook_subuv")', self.features_cpp)
        self.assertIn("SubUvSamples.Num() > 0", self.features_cpp)

    def test_distortion_graph_wires_bump_offset_and_strength(self) -> None:
        self.assertIn('Has(TEXT("distortion"))', self.graph_cpp)
        self.assertIn("UMaterialExpressionBumpOffset", self.graph_cpp)
        self.assertIn("HeightRatioInput", self.graph_cpp)
        self.assertIn('ParameterName = FName(TEXT("DistortionStrength"))', self.graph_cpp)

    def test_flipbook_graph_uses_subuv_main_texture(self) -> None:
        self.assertIn('Has(TEXT("flipbook_subuv"))', self.graph_cpp)
        self.assertIn("UMaterialExpressionTextureSampleParameterSubUV", self.graph_cpp)
        self.assertIn('ParameterName = FName(TEXT("MainTexture"))', self.graph_cpp)

    def test_distortion_strength_applied_on_mi(self) -> None:
        self.assertIn('FName(TEXT("DistortionStrength"))', self.service_cpp)

    def test_readme_documents_distortion_and_flipbook(self) -> None:
        for token in ("distortion", "flipbook_subuv"):
            self.assertIn(token, self.readme)
            self.assertIn("VERIFIED", self.readme)

    def test_editor_automation_covers_distortion_and_flipbook(self) -> None:
        tests_cpp = (
            REPO_ROOT
            / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/Tests/UeremcpMaterialToolsetTests.cpp"
        )
        text = tests_cpp.read_text(encoding="utf-8")
        self.assertIn("CreateVfxMaterial.Distortion", text)
        self.assertIn("CreateVfxMaterial.FlipbookSubuv", text)
        self.assertIn("VerifyFeatureGraph", text)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
