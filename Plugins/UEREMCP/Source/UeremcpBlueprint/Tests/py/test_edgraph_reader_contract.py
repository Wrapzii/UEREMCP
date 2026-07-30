"""Offline contract test for shared FUeremcpEdGraphReader API documentation."""

from __future__ import annotations

import unittest
from pathlib import Path

from schema_registry import repo_root


HEADER = repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/Public/UeremcpEdGraphReader.h"
RESPONSE = repo_root() / "docs/proposals/ws-06-response-edgraph-share-ws10.md"


class EdGraphReaderContractTests(unittest.TestCase):
    def test_public_header_exists(self) -> None:
        self.assertTrue(HEADER.is_file(), msg=str(HEADER))

    def test_header_exports_core_symbols(self) -> None:
        text = HEADER.read_text(encoding="utf-8")
        for symbol in (
            "FUeremcpEdGraphReader",
            "FUeremcpEdGraphReadOptions",
            "FUeremcpEdGraphSemanticHooks",
            "ReadGraph",
            "MakeNodeId",
            "MakePinId",
            "PinTypeToJson",
        ):
            with self.subTest(symbol=symbol):
                self.assertIn(symbol, text)

    def test_ws10_response_documents_option_a(self) -> None:
        body = RESPONSE.read_text(encoding="utf-8")
        self.assertIn("Option A accepted", body)
        self.assertIn("UeremcpAnimation.Build.cs", body)


if __name__ == "__main__":
    unittest.main()
