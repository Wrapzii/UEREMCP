"""Offline contract for FUeremcpBlueprintMutatingGate (orch dispatch adapter)."""

from __future__ import annotations

import unittest
from pathlib import Path

from schema_registry import repo_root


GATE_HEADER = repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/Public/UeremcpBlueprintMutatingGate.h"
GATE_CPP = repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintMutatingGate.cpp"
BUILD_CS = repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/UeremcpBlueprint.Build.cs"
TOOLSET = repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintToolset.cpp"
PROPOSAL = repo_root() / "docs/proposals/ws-06-mutating-dispatch-adoption.md"


class BlueprintMutatingGateContractTests(unittest.TestCase):
    def test_gate_header_exports_adapter(self) -> None:
        text = GATE_HEADER.read_text(encoding="utf-8")
        for symbol in (
            "FUeremcpBlueprintMutatingGate",
            "TryBeginRead",
            "TryBeginMutating",
            "Complete",
        ):
            with self.subTest(symbol=symbol):
                self.assertIn(symbol, text)

    def test_compile_gate_enabled_with_core_deps(self) -> None:
        build = BUILD_CS.read_text(encoding="utf-8")
        self.assertIn("UEREMCP_BLUEPRINT_MUTATING_DISPATCH=1", build)
        self.assertIn("UeremcpCore", build)
        self.assertIn("UeremcpSecurity", build)

    def test_toolset_wires_read_and_submit(self) -> None:
        body = TOOLSET.read_text(encoding="utf-8")
        self.assertIn("TryBeginRead", body)
        self.assertIn("TryBeginMutating", body)
        self.assertIn("FinishSubmitResponse", body)

    def test_cpp_guards_core_include(self) -> None:
        body = GATE_CPP.read_text(encoding="utf-8")
        self.assertIn("#if UEREMCP_BLUEPRINT_MUTATING_DISPATCH", body)
        self.assertIn("UeremcpMutatingDispatch.h", body)

    def test_proposal_documents_orch_try_begin_api(self) -> None:
        proposal = PROPOSAL.read_text(encoding="utf-8")
        self.assertIn("TryBegin", proposal)
        self.assertIn("not RunOnGameThread", proposal)
        self.assertIn("UEREMCP_BLUEPRINT_MUTATING_DISPATCH=1", proposal)


if __name__ == "__main__":
    unittest.main()
