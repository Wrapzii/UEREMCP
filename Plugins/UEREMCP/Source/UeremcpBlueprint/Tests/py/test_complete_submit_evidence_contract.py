"""Regression contracts for complete Blueprint read/submit evidence."""

from __future__ import annotations

import unittest

from schema_registry import repo_root


READER_CPP = (
    repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintGraphReader.cpp"
)
TOOLSET_CPP = (
    repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintToolset.cpp"
)
EPIC_BRIDGE_CPP = (
    repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintEpicBridge.cpp"
)


class CompleteSubmitEvidenceContractTests(unittest.TestCase):
    def test_complete_read_always_emits_context_arrays(self) -> None:
        body = READER_CPP.read_text(encoding="utf-8")
        self.assertIn('SetArrayField(TEXT("variables"), Variables)', body)
        self.assertIn('SetArrayField(TEXT("dependencies"), DepJson)', body)
        self.assertNotIn("if (Variables.Num() > 0)", body)
        self.assertNotIn("if (EdResult.DependencyPaths.Num() > 0)", body)

    def test_submit_attaches_compile_save_and_complete_reread(self) -> None:
        body = TOOLSET_CPP.read_text(encoding="utf-8")
        self.assertIn("AttachSubmitWriteEvidence", body)
        self.assertIn('SetBoolField(TEXT("compiled"), WriteResult.bCompiled)', body)
        self.assertIn('SetBoolField(TEXT("saved"), WriteResult.bSaved)', body)
        self.assertIn("AttachGraphDiagnostics(Response, WriteResult.RereadGraph)", body)

    def test_changed_replace_bootstraps_official_epic_toolset(self) -> None:
        body = EPIC_BRIDGE_CPP.read_text(encoding="utf-8")
        self.assertIn(
            'TEXT("editor_toolset.toolsets.blueprint.BlueprintTools")',
            body,
        )
        self.assertIn(
            'FModuleManager::Get().LoadModule(TEXT("PythonScriptPlugin"))',
            body,
        )
        self.assertIn(
            "UToolsetRegistry::IsToolsetRegistered(EpicBlueprintToolsetName)",
            body,
        )


if __name__ == "__main__":
    unittest.main()
