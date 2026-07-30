"""Offline validation of Blueprint domain specification schemas (WS-06)."""

from __future__ import annotations

import json
import unittest
from pathlib import Path

from schema_registry import blueprint_domain_validator, repo_root


FIXTURES_DIR = Path(__file__).resolve().parent.parent / "fixtures"


def load_fixture(name: str) -> dict:
    data = json.loads((FIXTURES_DIR / name).read_text(encoding="utf-8"))
    if isinstance(data, dict):
        data.pop("$comment", None)
    return data


def validation_errors(validator, instance: dict) -> list[str]:
    errors = sorted(validator.iter_errors(instance), key=lambda e: list(e.path))
    return [f"{'/'.join(str(p) for p in err.path) or '<root>'}: {err.message}" for err in errors]


class BlueprintDomainSchemaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.read_validator = blueprint_domain_validator("read_graph")
        cls.submit_validator = blueprint_domain_validator("submit_graph")

    def test_read_graph_spec_fixture_validates(self) -> None:
        spec = load_fixture("read_graph_spec.fixture.json")
        errors = validation_errors(self.read_validator, spec)
        self.assertEqual(errors, [], msg="\n".join(errors))

    def test_read_graph_schema_example_validates(self) -> None:
        schema_path = repo_root() / "schemas" / "domains" / "blueprints" / "read_graph.schema.json"
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        for example in schema.get("examples", []):
            with self.subTest(example=example):
                errors = validation_errors(self.read_validator, example)
                self.assertEqual(errors, [], msg="\n".join(errors))

    def test_submit_graph_replace_spec_validates(self) -> None:
        graph = load_fixture("submit_graph_replace.fixture.json")
        spec = {
            "graph_id": "EventGraph",
            "graph": graph,
        }
        errors = validation_errors(self.submit_validator, spec)
        self.assertEqual(errors, [], msg="\n".join(errors))

    def test_submit_graph_expected_after_write_validates(self) -> None:
        graph = load_fixture("translate_branch_if.fixture.json")
        spec = {
            "graph_id": "EventGraph",
            "graph": graph,
            "expected_after_write": {
                "nodes": [
                    {"key": "event", "node_class": "K2Node_Event"},
                    {"key": "branch", "node_class": "K2Node_IfThenElse"},
                    {"key": "print", "node_class": "K2Node_CallFunction"},
                ],
                "links": [
                    {"from": "event", "from_pin": "then", "to": "branch", "to_pin": "execute"},
                    {"from": "branch", "from_pin": "then", "to": "print", "to_pin": "execute"},
                ],
            },
        }
        errors = validation_errors(self.submit_validator, spec)
        self.assertEqual(errors, [], msg="\n".join(errors))

    def test_submit_graph_schema_examples_validate(self) -> None:
        schema_path = repo_root() / "schemas" / "domains" / "blueprints" / "submit_graph.schema.json"
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        for example in schema.get("examples", []):
            with self.subTest(example_keys=sorted(example.keys())):
                errors = validation_errors(self.submit_validator, example)
                self.assertEqual(errors, [], msg="\n".join(errors))

    def test_read_graph_rejects_unknown_fields(self) -> None:
        spec = load_fixture("read_graph_spec.fixture.json")
        spec["unexpected"] = True
        errors = validation_errors(self.read_validator, spec)
        self.assertTrue(any("unexpected" in err for err in errors))

    def test_submit_graph_rejects_unknown_fields(self) -> None:
        spec = {
            "graph_id": "EventGraph",
            "graph": load_fixture("translate_begin_play_print.fixture.json"),
            "agent_note": "should not be here",
        }
        errors = validation_errors(self.submit_validator, spec)
        self.assertTrue(any("agent_note" in err for err in errors))

    def test_submit_graph_patch_example_shape(self) -> None:
        spec = {
            "graph_id": "EventGraph",
            "patch": {
                "base_revision": "sha256:placeholder",
                "ops": [{"op": "ensure_entry", "entry_kind": "event", "name": "EventBeginPlay"}],
            },
        }
        errors = validation_errors(self.submit_validator, spec)
        self.assertEqual(errors, [], msg="\n".join(errors))


if __name__ == "__main__":
    unittest.main()
