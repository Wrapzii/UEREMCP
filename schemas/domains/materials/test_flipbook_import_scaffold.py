#!/usr/bin/env python3
"""Offline guards for flipbook_import Phase A scaffold (WS-08).

Usage::

    python schemas/domains/materials/test_flipbook_import_scaffold.py
"""

from __future__ import annotations

import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]
PROCEDURAL_CPP = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpMaterial/Private/UeremcpProceduralTextureService.cpp"
PROPOSAL = REPO_ROOT / "docs/proposals/ws-08-flipbook-external-sheet-import.md"


class FlipbookImportScaffoldTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.procedural_cpp = PROCEDURAL_CPP.read_text(encoding="utf-8")

    def test_flipbook_import_recognized_not_implemented(self) -> None:
        self.assertIn("flipbook_import", self.procedural_cpp)
        self.assertIn("IsImplementedGenerateKind", self.procedural_cpp)
        self.assertIn('Kind != TEXT("flipbook_import")', self.procedural_cpp)

    def test_scaffold_returns_partially_completed(self) -> None:
        self.assertIn("flipbook_import Phase A scaffold", self.procedural_cpp)
        self.assertIn("ImportBufferAsTexture2D not invoked", self.procedural_cpp)
        self.assertNotIn("ImportBufferAsTexture2D(", self.procedural_cpp)

    def test_parse_accepts_source_file_path(self) -> None:
        self.assertIn("OutSourceFilePath", self.procedural_cpp)
        self.assertIn('TryGetObjectField(TEXT("source")', self.procedural_cpp)

    def test_proposal_documents_phase_a(self) -> None:
        proposal = PROPOSAL.read_text(encoding="utf-8")
        self.assertIn("Phase A", proposal)
        self.assertIn("flipbook_import", proposal)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
