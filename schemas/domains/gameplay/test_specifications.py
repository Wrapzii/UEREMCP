#!/usr/bin/env python3
"""Unit tests for WS-09 gameplay specification schemas."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

try:
    import jsonschema
    from jsonschema import Draft202012Validator
    from referencing import Registry, Resource
except ImportError:
    sys.stderr.write("error: pip install jsonschema\n")
    raise SystemExit(1)

REPO_ROOT = Path(__file__).resolve().parents[3]
SCHEMAS_DIR = REPO_ROOT / "schemas"
GAMEPLAY_DIR = SCHEMAS_DIR / "domains" / "gameplay"


def load_registry() -> Registry:
    resources = []
    for path in sorted(SCHEMAS_DIR.rglob("*.schema.json")):
        schema = json.loads(path.read_text(encoding="utf-8"))
        resources.append((schema["$id"], Resource.from_contents(schema)))
    return Registry().with_resources(resources)


class GameplaySpecificationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.schema = json.loads(
            (GAMEPLAY_DIR / "create_spell.schema.json").read_text(encoding="utf-8")
        )
        cls.validator = Draft202012Validator(cls.schema, registry=load_registry())
        cls.example = cls.schema["examples"][0]

    def test_embedded_examples_validate(self) -> None:
        self.assertGreater(len(self.schema["examples"]), 0)
        for example in self.schema["examples"]:
            self.validator.validate(example)

    def test_pattern_b_is_not_advisory(self) -> None:
        bad = json.loads(json.dumps(self.example))
        bad["networking"]["authority"] = "client"
        with self.assertRaises(jsonschema.ValidationError):
            self.validator.validate(bad)

    def test_row_name_is_stable_identifier(self) -> None:
        bad = json.loads(json.dumps(self.example))
        bad["row_name"] = "Fireball 1"
        with self.assertRaises(jsonschema.ValidationError):
            self.validator.validate(bad)

    def test_status_uses_re_enum_name(self) -> None:
        bad = json.loads(json.dumps(self.example))
        bad["impact"]["status"] = "Burning"
        with self.assertRaises(jsonschema.ValidationError):
            self.validator.validate(bad)

    def test_unknown_primitive_is_rejected(self) -> None:
        bad = json.loads(json.dumps(self.example))
        bad["create_gameplay_effect"] = {}
        with self.assertRaises(jsonschema.ValidationError):
            self.validator.validate(bad)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
