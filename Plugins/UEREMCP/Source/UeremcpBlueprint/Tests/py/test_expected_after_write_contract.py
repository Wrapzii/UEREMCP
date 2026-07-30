"""Regression contracts for A6 post-write node/link assertions."""

from __future__ import annotations

import unittest

from schema_registry import repo_root


WRITER_CPP = (
    repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintGraphWriter.cpp"
)
EDITOR_TEST_CPP = (
    repo_root()
    / "Plugins/UEREMCP/Source/UeremcpBlueprint/Private/Tests/UeremcpBlueprintToolsetTests.cpp"
)


class ExpectedAfterWriteContractTests(unittest.TestCase):
    def test_selector_keeps_all_matching_node_ids(self) -> None:
        body = WRITER_CPP.read_text(encoding="utf-8")
        self.assertIn("TMap<FString, TArray<FString>> NodeIdsByKey", body)
        self.assertIn("FromNodeIds->Contains(FromNode)", body)
        self.assertIn("ToNodeIds->Contains(ToNode)", body)
        self.assertNotIn("Matches.Num() != 1", body)

    def test_editor_assertion_resolves_link_endpoints_by_node_id(self) -> None:
        body = EDITOR_TEST_CPP.read_text(encoding="utf-8")
        self.assertIn("TMap<FString, FString> NodeClassById", body)
        self.assertIn("NodeClassById.FindRef(FromNode)", body)
        self.assertIn("NodeClassById.FindRef(ToNode)", body)


if __name__ == "__main__":
    unittest.main()
