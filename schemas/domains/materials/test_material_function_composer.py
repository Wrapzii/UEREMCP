#!/usr/bin/env python3
"""Offline guards for Phase C MaterialFunction composer stub (WS-08).

Usage::

    python schemas/domains/materials/test_material_function_composer.py
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
COMPOSER_H = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Public/UeremcpMaterialFunctionComposer.h"
COMPOSER_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFunctionComposer.cpp"
GRAPH_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialFeatureGraph.cpp"
COMPOSITION_JSON = REPO_ROOT / "schemas/domains/materials/feature_composition.v1.json"
MASTER_BUILDER_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialMasterBuilder.cpp"
MATERIAL_SERVICE = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpMaterialService.cpp"
FLIPBOOK_PROPOSAL = REPO_ROOT / "docs/proposals/ws-08-flipbook-external-sheet-import.md"


class MaterialFunctionComposerStubTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.composer_h = COMPOSER_H.read_text(encoding="utf-8")
        cls.composer_cpp = COMPOSER_CPP.read_text(encoding="utf-8")
        cls.graph_cpp = GRAPH_CPP.read_text(encoding="utf-8")
        cls.service_cpp = MATERIAL_SERVICE.read_text(encoding="utf-8")
        cls.master_builder_cpp = MASTER_BUILDER_CPP.read_text(encoding="utf-8")
        cls.material_service = MATERIAL_SERVICE.read_text(encoding="utf-8")
        cls.composition = json.loads(COMPOSITION_JSON.read_text(encoding="utf-8"))

    def test_composer_header_exports_probe_and_try(self) -> None:
        self.assertIn("ProbeComposition", self.composer_h)
        self.assertIn("TryComposeFeature", self.composer_h)
        self.assertIn("[VERIFIED: MaterialExpressionMaterialFunctionCall.h:157-158]", self.composer_h)

    def test_stub_returns_partially_completed_for_deferred_candidates(self) -> None:
        self.assertIn('TEXT("partially_completed")', self.composer_cpp)
        self.assertIn("bUsedExpressionFallback", self.composer_cpp)
        self.assertIn("DeferredFeatures", self.composer_cpp)
        for token in ("fresnel", "depth_fade"):
            self.assertIn(f'TEXT("{token}")', self.composer_cpp)

    def test_feature_graph_invokes_composer_probe(self) -> None:
        self.assertIn("UeremcpMaterialFunctionComposer::ProbeComposition", self.graph_cpp)
        self.assertIn("CompositionStatus", self.graph_cpp)

    def test_service_propagates_master_composition_notes(self) -> None:
        self.assertIn("MasterResult.InterpretationNotes", self.service_cpp)
        self.assertIn("MasterResult.CapabilityNotes", self.service_cpp)

    def test_composition_json_marks_engine_mf_candidates(self) -> None:
        for token in ("fresnel", "depth_fade"):
            feature = self.composition["features"][token]
            self.assertEqual(feature["strategy"], "engine_material_function")
            self.assertIsNone(feature["engine_material_function_path"])
            self.assertEqual(feature["fallback"], "expression_fallback")

    def test_stub_does_not_claim_set_material_function(self) -> None:
        self.assertIn("not invoked", self.composer_cpp.lower())
        self.assertNotIn("SetMaterialFunction(", self.composer_cpp)

    def test_master_builder_propagates_graph_composition_notes(self) -> None:
        self.assertIn("GraphResult.InterpretationNotes", self.master_builder_cpp)
        self.assertIn("GraphResult.CapabilityNotes", self.master_builder_cpp)

    def test_composition_partially_completed_does_not_override_envelope_validated(self) -> None:
        self.assertIn("ResolveMaterialSuccessStatus", self.material_service)
        self.assertNotIn("CompositionStatus", self.material_service)
        self.assertRegex(
            self.material_service,
            r"ResolveMaterialSuccessStatus\([\s\S]*bValidate",
        )

    def test_flipbook_external_import_proposal_exists(self) -> None:
        self.assertTrue(FLIPBOOK_PROPOSAL.is_file())
        proposal = FLIPBOOK_PROPOSAL.read_text(encoding="utf-8")
        self.assertIn("ImportBufferAsTexture2D", proposal)
        self.assertIn("not implemented", proposal.lower())


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
