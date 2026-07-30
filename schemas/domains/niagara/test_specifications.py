#!/usr/bin/env python3
"""Unit tests for WS-07 Niagara specification schemas.

Runs outside the editor. Validates embedded examples and fidelity lossy-area keys
stay aligned with UeremcpNiagaraCapabilityNotes.h.

Usage::

    python schemas/domains/niagara/test_specifications.py
"""

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
NIAGARA_DIR = SCHEMAS_DIR / "domains" / "niagara"

EXPECTED_LOSSY_AREAS = frozenset({
    "event_handler_stacks",
    "module_reorder_without_readd",
    "script_graph_internals",
})

EXPECTED_CREATE_CAPABILITY_SNIPPETS = (
    "material_bindings",
    "partially_completed",
    "POC B",
)


def load_registry() -> Registry:
    resources = []
    for path in sorted(SCHEMAS_DIR.rglob("*.schema.json")):
        schema = json.loads(path.read_text(encoding="utf-8"))
        schema_id = schema.get("$id")
        if not schema_id:
            raise RuntimeError(f"{path}: missing $id")
        resources.append((schema_id, Resource.from_contents(schema)))
    return Registry().with_resources(resources)


def load_schema(filename: str) -> dict:
    return json.loads((NIAGARA_DIR / filename).read_text(encoding="utf-8"))


class NiagaraSpecificationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.registry = load_registry()

    def _validate_examples(self, schema_file: str) -> None:
        schema = load_schema(schema_file)
        validator = Draft202012Validator(schema, registry=self.registry)
        examples = schema.get("examples", [])
        self.assertGreater(len(examples), 0, f"{schema_file} must ship at least one example")
        for i, example in enumerate(examples):
            validator.validate(example)

    def test_inspect_system_examples(self) -> None:
        self._validate_examples("inspect_system.schema.json")

    def test_create_niagara_effect_examples(self) -> None:
        self._validate_examples("create_niagara_effect.schema.json")

    def test_graph_ext_examples(self) -> None:
        self._validate_examples("graph-ext.schema.json")

    def test_create_niagara_effect_requires_effect_type(self) -> None:
        schema = load_schema("create_niagara_effect.schema.json")
        validator = Draft202012Validator(schema, registry=self.registry)
        with self.assertRaises(jsonschema.ValidationError):
            validator.validate({"name": "NoType"})

    def test_readme_lossy_areas_documented(self) -> None:
        readme = (NIAGARA_DIR / "README.md").read_text(encoding="utf-8")
        for area in EXPECTED_LOSSY_AREAS:
            self.assertIn(area, readme, f"README must document lossy area {area}")

    def test_capability_header_lossy_areas(self) -> None:
        header = (
            REPO_ROOT
            / "Plugins/UEREMCP/Source/UeremcpNiagara/Public/UeremcpNiagaraCapabilityNotes.h"
        ).read_text(encoding="utf-8")
        for area in EXPECTED_LOSSY_AREAS:
            self.assertIn(area, header, f"Capability header must define {area}")

    def test_capability_header_create_notes(self) -> None:
        header = (
            REPO_ROOT
            / "Plugins/UEREMCP/Source/UeremcpNiagara/Public/UeremcpNiagaraCapabilityNotes.h"
        ).read_text(encoding="utf-8")
        self.assertIn("DefaultCreateCapabilityNotes", header)
        for snippet in EXPECTED_CREATE_CAPABILITY_SNIPPETS:
            self.assertIn(snippet, header, f"Create capability notes must mention {snippet}")

    def test_probe_path_convention_documented(self) -> None:
        readme = (NIAGARA_DIR / "README.md").read_text(encoding="utf-8")
        self.assertIn("/Game/__UeremcpTests/", readme)
        paths_header = (
            REPO_ROOT
            / "Plugins/UEREMCP/Source/UeremcpNiagara/Public/UeremcpNiagaraPaths.h"
        ).read_text(encoding="utf-8")
        self.assertIn("TestsContentRoot", paths_header)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
