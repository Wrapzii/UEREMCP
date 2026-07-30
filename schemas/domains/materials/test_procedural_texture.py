#!/usr/bin/env python3
"""Unit tests for WS-08 create_procedural_texture schema.

Usage::

    python schemas/domains/materials/test_procedural_texture.py
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

SUPPORTED_GENERATE_KINDS = frozenset({
    "noise",
    "gradient",
    "voronoi",
    "ring_mask",
    "flow_map",
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


class ProceduralTextureSchemaTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.registry = load_registry()
        cls.schema = json.loads(
            (MATERIALS_DIR / "create_procedural_texture.schema.json").read_text(encoding="utf-8")
        )
        cls.validator = Draft202012Validator(cls.schema, registry=cls.registry)

    def test_examples_validate(self) -> None:
        examples = self.schema.get("examples", [])
        self.assertGreater(len(examples), 0)
        for example in examples:
            with self.subTest(example=example):
                self.validator.validate(example)

    def test_generate_enum_matches_cpp_surface(self) -> None:
        enum_values = frozenset(self.schema["properties"]["generate"]["enum"])
        self.assertEqual(enum_values, SUPPORTED_GENERATE_KINDS)

    def test_create_vfx_material_flow_map_slot_validates(self) -> None:
        vfx_schema = json.loads(
            (MATERIALS_DIR / "create_vfx_material.schema.json").read_text(encoding="utf-8")
        )
        validator = Draft202012Validator(vfx_schema, registry=self.registry)
        validator.validate({
            "purpose": "elemental_projectile_trail",
            "element": "ice",
            "textures": {
                "FlowMap": {"generate": "flow_map", "dimensions": [512, 512]},
            },
        })


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
