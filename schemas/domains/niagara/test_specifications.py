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
PROTOCOL_TESTS = REPO_ROOT / "Plugins/UEREMCP/Source/UeremcpProtocol/Tests/py"

EXPECTED_LOSSY_AREAS = frozenset({
    "event_handler_stacks",
    "module_reorder_without_readd",
    "script_graph_internals",
})

EXPECTED_CREATE_CAPABILITY_SNIPPETS = (
    "material_bindings",
    "partially_completed",
    "POC B",
    "post-create inspect",
    "mode 'replace'",
)


def load_fixture(name: str) -> dict:
    return json.loads((NIAGARA_DIR / "fixtures" / name).read_text(encoding="utf-8"))


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
        self.assertIn("renderer_material_bindings", header)
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

    def test_inspect_probe_fixture_shape(self) -> None:
        fixture = load_fixture("inspect_probe_minimal.json")
        system = fixture["system_graph"]
        self.assertEqual(system["graph_type"], "NiagaraSystemGraph")
        self.assertFalse(system["fidelity"]["round_trip_supported"])
        self.assertEqual(set(system["fidelity"]["lossy_areas"]), EXPECTED_LOSSY_AREAS)

        niagara = system["extensions"]["niagara"]
        self.assertIn("event_handlers", niagara)
        self.assertIsInstance(niagara["event_handlers"], list)
        self.assertIn("user_parameters", niagara)

        placeholder = fixture["event_handler_placeholder_shape"]
        self.assertEqual(placeholder["script_usage"], "ParticleEventScript")
        self.assertEqual(placeholder["modules"], [])
        self.assertIn("fidelity_note", placeholder)

    def test_emitter_graph_renderer_fixture(self) -> None:
        fixture = load_fixture("inspect_probe_minimal.json")
        emitter = fixture["emitter_graph_example"]
        self.assertEqual(emitter["graph_type"], "NiagaraEmitterGraph")
        self.assertIn("renderer_material_bindings", emitter["fidelity"]["lossy_areas"])

        nodes = emitter["nodes"]
        self.assertEqual(nodes[0]["semantic_type"], "niagara_renderer")
        renderer = emitter["extensions"]["niagara"]["renderers"][0]
        self.assertEqual(
            renderer["material_path_fidelity"],
            "extracted_from_property_values_not_validated",
        )

    def test_round_trip_fixture_expectations(self) -> None:
        fixture = load_fixture("inspect_probe_minimal.json")
        expectations = fixture["round_trip_offline_expectations"]
        self.assertGreater(len(expectations["create_emitters"]), 0)
        self.assertIn("NiagaraSystemGraph", expectations["inspect_must_include_graph_types"])
        self.assertEqual(
            set(expectations["validation_never_claims"]),
            {"created_and_validated", "modified_and_validated"},
        )

    def test_create_replace_probe_fixture(self) -> None:
        fixture = load_fixture("create_replace_probe.json")
        request = fixture["request"]
        self.assertEqual(request["mode"], "replace")
        self.assertTrue(
            request["target"]["asset_path"].startswith(
                fixture["expectations"]["allowed_probe_root"]
            )
        )
        expectations = fixture["expectations"]
        for forbidden in expectations["forbidden_delete_paths"]:
            self.assertFalse(
                forbidden.startswith(expectations["allowed_probe_root"]),
                "fixture must document paths outside probe root as forbidden",
            )
        self.assertIn("niagara.replace_delete_probe_asset", expectations["checks_when_replacing"])

    def test_hash_round_trip_scaffold_fixture(self) -> None:
        import sys

        sys.path.insert(0, str(PROTOCOL_TESTS))
        from ueremcp_protocol.content_hash import content_hash

        fixture = load_fixture("hash_round_trip_scaffold.json")
        graph = fixture["module_stack_graph"]
        expectations = fixture["hash_scaffold_expectations"]

        self.assertFalse(expectations["round_trip_supported"])
        self.assertFalse(graph["fidelity"]["round_trip_supported"])

        digest = content_hash(graph)
        self.assertTrue(digest.startswith(expectations["content_hash_prefix"]))

        digest_again = content_hash(graph)
        self.assertEqual(digest, digest_again, "retrieve→retrieve hash must be stable offline")

        for check in expectations["checks_skipped"]:
            self.assertIn("round_trip", check)


if __name__ == "__main__":
    raise SystemExit(unittest.main(verbosity=2))
