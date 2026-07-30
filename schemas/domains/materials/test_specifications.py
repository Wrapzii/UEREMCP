#!/usr/bin/env python3
"""Unit tests for WS-08 materials specification schemas.

Runs outside the editor. Validates embedded examples, WS-15 elemental projectile
alignment, and fidelity lossy-area keys stay aligned with UeremcpMaterialCapabilityNotes.h.

Usage::

    python schemas/domains/materials/test_specifications.py
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
MATERIALS_DIR = SCHEMAS_DIR / "domains" / "materials"
ELEMENTAL_TEMPLATE = REPO_ROOT / "templates" / "niagara" / "niagara.projectile.elemental.v1.json"

EXPECTED_LOSSY_AREAS = frozenset({
    "expression_subclass_properties",
    "material_function_internals",
    "editor_chrome",
})

EXPECTED_CAPABILITY_SNIPPETS = frozenset({
    "Minimal master graph",
    "textures.generate",
})

WS15_ELEMENTAL_PURPOSES = frozenset({
    "elemental_projectile_core",
    "elemental_projectile_trail",
})

WS15_SUPPORTED_MODIFIERS = frozenset({
    "crystalline_fragments",
    "reduce_trail_persistence",
    "boost_impact",
    "preserve_networking",
})


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
    return json.loads((MATERIALS_DIR / filename).read_text(encoding="utf-8"))


class MaterialsSpecificationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.registry = load_registry()

    def _validate_examples(self, schema_file: str) -> None:
        schema = load_schema(schema_file)
        validator = Draft202012Validator(schema, registry=self.registry)
        examples = schema.get("examples", [])
        self.assertGreater(len(examples), 0, f"{schema_file} must ship at least one example")
        for example in examples:
            validator.validate(example)

    def test_create_vfx_material_examples(self) -> None:
        self._validate_examples("create_vfx_material.schema.json")

    def test_create_vfx_material_requires_purpose(self) -> None:
        schema = load_schema("create_vfx_material.schema.json")
        validator = Draft202012Validator(schema, registry=self.registry)
        with self.assertRaises(jsonschema.ValidationError):
            validator.validate({"element": "fire"})

    def test_readme_lossy_areas_documented(self) -> None:
        readme = (MATERIALS_DIR / "README.md").read_text(encoding="utf-8")
        for area in EXPECTED_LOSSY_AREAS:
            self.assertIn(area, readme, f"README must document lossy area {area}")

    def test_capability_header_documents_limitations(self) -> None:
        header = (
            REPO_ROOT
            / "Plugins/UEREMCP/Source/UeremcpMaterial/Public/UeremcpMaterialCapabilityNotes.h"
        ).read_text(encoding="utf-8")
        for area in EXPECTED_LOSSY_AREAS:
            self.assertIn(area, header, f"Capability header must define {area}")
        for snippet in EXPECTED_CAPABILITY_SNIPPETS:
            self.assertIn(snippet, header, f"Capability header must mention {snippet}")

    def test_ws15_elemental_template_specs_validate(self) -> None:
        if not ELEMENTAL_TEMPLATE.is_file():
            self.skipTest("niagara.projectile.elemental.v1.json not present")
        schema = load_schema("create_vfx_material.schema.json")
        validator = Draft202012Validator(schema, registry=self.registry)
        template = json.loads(ELEMENTAL_TEMPLATE.read_text(encoding="utf-8"))
        for step in template.get("construction_plan", []):
            if step.get("action") != "create_vfx_material":
                continue
            spec = step.get("specification", {})
            purpose = spec.get("purpose")
            self.assertIn(
                purpose,
                WS15_ELEMENTAL_PURPOSES,
                f"Unexpected purpose in elemental template: {purpose}",
            )
            # Template uses {{inputs.element}} placeholders — validate shape without element value.
            spec_without_placeholders = {
                k: v for k, v in spec.items() if not (isinstance(v, str) and v.startswith("{{"))
            }
            validator.validate(spec_without_placeholders)

    def test_ws15_modifiers_subset_of_schema_documentation(self) -> None:
        readme = (MATERIALS_DIR / "README.md").read_text(encoding="utf-8")
        for modifier in WS15_SUPPORTED_MODIFIERS:
            self.assertIn(modifier, readme, f"README must document modifier {modifier}")


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
