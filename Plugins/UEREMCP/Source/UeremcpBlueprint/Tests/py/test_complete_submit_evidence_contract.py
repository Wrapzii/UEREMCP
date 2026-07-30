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


if __name__ == "__main__":
    unittest.main()
