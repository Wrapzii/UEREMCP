"""Offline policy: read_graph summary/complete share the same revision hash."""

from __future__ import annotations

import unittest
from pathlib import Path

from schema_registry import repo_root


READER_CPP = (
    repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintGraphReader.cpp"
)
TOOLSET_TESTS = (
    repo_root()
    / "Plugins/UEREMCP/Source/UeremcpBlueprint/Private/Tests/UeremcpBlueprintToolsetTests.cpp"
)


class ReadGraphRevisionPolicyTests(unittest.TestCase):
    def test_reader_hashes_before_summary_truncation(self) -> None:
        body = READER_CPP.read_text(encoding="utf-8")
        self.assertIn("bIncludeFullNodesInResponse", body)
        self.assertIn("EdOptions.bEmitNodesAndLinks = true", body)
        hash_idx = body.index("ComputeContentHash(GraphObj")
        strip_idx = body.index("RemoveField(TEXT(\"nodes\"))")
        self.assertLess(hash_idx, strip_idx, msg="hash must run before summary node strip")

    def test_editor_test_expects_matching_summary_revision(self) -> None:
        body = TOOLSET_TESTS.read_text(encoding="utf-8")
        self.assertIn("summary and complete revisions match", body)


if __name__ == "__main__":
    unittest.main()
