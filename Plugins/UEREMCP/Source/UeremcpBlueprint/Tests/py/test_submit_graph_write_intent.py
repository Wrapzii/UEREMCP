"""Offline policy: submit_graph no_change requires matching write intent."""

from __future__ import annotations

import copy
import unittest

from graph_json_to_dsl import resolve_write_dsl
from schema_registry import repo_root


WRITER_CPP = (
    repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintGraphWriter.cpp"
)
TOOLSET_CPP = (
    repo_root() / "Plugins/UEREMCP/Source/UeremcpBlueprint/Private/UeremcpBlueprintToolset.cpp"
)
FIXTURES = (
    repo_root()
    / "Plugins/UEREMCP/Source/UeremcpBlueprint/Tests/fixtures/submit_graph_replace.fixture.json"
)


def load_fixture() -> dict:
    import json

    data = json.loads(FIXTURES.read_text(encoding="utf-8"))
    data.pop("$comment", None)
    return data


class SubmitGraphWriteIntentTests(unittest.TestCase):
    def test_writer_exports_write_intent_differs(self) -> None:
        body = WRITER_CPP.read_text(encoding="utf-8")
        self.assertIn("WriteIntentDiffers", body)

    def test_toolset_checks_write_intent_before_no_change(self) -> None:
        body = TOOLSET_CPP.read_text(encoding="utf-8")
        self.assertIn("WriteIntentDiffers", body)
        anchor = "Submitted Blueprint graph already matches"
        self.assertIn(anchor, body)
        self.assertLess(body.index("WriteIntentDiffers"), body.index(anchor))

    def test_extensions_dsl_only_change_is_write_intent_delta(self) -> None:
        base = load_fixture()
        current_dsl, _ = resolve_write_dsl(base)
        changed = copy.deepcopy(base)
        changed["extensions"]["blueprint"]["dsl"] = (
            "(event EventBeginPlay\n  (Development|PrintString :InString \"dry_run changed\"))"
        )
        changed_dsl, _ = resolve_write_dsl(changed)
        self.assertNotEqual(current_dsl.strip(), changed_dsl.strip())


if __name__ == "__main__":
    unittest.main()
