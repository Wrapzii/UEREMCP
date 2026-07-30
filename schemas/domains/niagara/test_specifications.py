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
    "orphaned_inline_creates",
    "POC B",
    "six emitter",
    "post-create inspect",
    "mode 'replace'",
    "execute_plan",
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

    def test_create_material_bindings_fixture(self) -> None:
        fixture = load_fixture("create_material_bindings.json")
        spec = fixture["request"]["specification"]
        self.assertIn("materials", spec)
        self.assertTrue(
            spec["materials"]["sparks"].startswith(fixture["expectations"]["allowed_material_root"])
        )
        expectations = fixture["expectations"]
        self.assertIn("create_spec", spec["materials"]["ribbon_trail"])
        self.assertEqual(
            set(expectations["validation_never_claims"]),
            {"created_and_validated", "modified_and_validated"},
        )
        self.assertTrue(expectations["continues_on_orphaned_inline_bind_failure"])
        self.assertEqual(expectations["response_status"], "partially_completed")
        self.assertEqual(
            expectations["checks_skipped_when_orphaned"],
            [
                "niagara.material_bindings",
                "niagara.material_bindings_orphaned_inline_creates",
            ],
        )

    def test_create_material_bindings_orphan_partial_fixture(self) -> None:
        fixture = load_fixture("create_material_bindings_orphan_partial.json")
        diagnostics = fixture["material_bindings_diagnostics_example"]
        expectations = fixture["expectations"]

        self.assertIn("orphaned_inline_creates", diagnostics)
        self.assertEqual(diagnostics["orphaned_inline_creates"], ["ribbon_trail"])

        inline_entries = diagnostics["inline_material_creates"]
        self.assertEqual(len(inline_entries), 1)
        inline = inline_entries[0]
        self.assertEqual(inline["role"], "ribbon_trail")
        self.assertTrue(inline["success"])
        self.assertEqual(inline["status"], "partially_completed")
        self.assertTrue(inline["primary_asset"].startswith(expectations["allowed_material_root"]))

        for path in diagnostics["resolved_paths"].values():
            self.assertTrue(path.startswith("/Game/__UeremcpTests/"))

        self.assertEqual(expectations["response_status"], "partially_completed")
        self.assertEqual(
            set(expectations["validation_never_claims"]),
            {"created_and_validated", "modified_and_validated"},
        )
        self.assertEqual(
            expectations["checks_skipped_when_orphaned"],
            [
                "niagara.material_bindings",
                "niagara.material_bindings_orphaned_inline_creates",
            ],
        )

        unresolved = diagnostics["unresolved"]
        self.assertTrue(any(entry.startswith("ribbon_trail:") for entry in unresolved))
        self.assertIn("sparks/renderer_0", diagnostics["renderer_bindings_verified"])
        self.assertNotIn("ribbon_trail/renderer_0", diagnostics["renderer_bindings_verified"])

    def test_create_poc_b_six_emitter_plan_fixture(self) -> None:
        fixture = load_fixture("create_poc_b_six_emitter_plan.json")
        spec = fixture["request"]["specification"]
        expectations = fixture["expectations"]
        plan = fixture["poc_b_emitter_plan"]

        validator = Draft202012Validator(
            load_schema("create_niagara_effect.schema.json"),
            registry=self.registry,
        )
        validator.validate(spec)

        self.assertEqual(len(spec["components"]), expectations["emitter_count"])
        self.assertEqual(len(plan["roles"]), expectations["emitter_count"])
        self.assertTrue(
            fixture["request"]["target"]["asset_path"].startswith(
                expectations["allowed_probe_root"]
            )
        )
        self.assertEqual(expectations["response_status"], "partially_completed")
        self.assertEqual(
            set(expectations["validation_never_claims"]),
            {"created_and_validated", "modified_and_validated"},
        )

        plan_roles = {entry["role"]: entry for entry in plan["roles"]}
        for role in spec["components"]:
            self.assertIn(role, plan_roles)
            entry = plan_roles[role]
            self.assertEqual(entry["emitter_name"], "".join(part.capitalize() for part in role.split("_")))

        self.assertIn("B3_six_emitters", expectations["poc_b_criteria_status"])
        self.assertEqual(
            expectations["poc_b_criteria_status"]["B3_six_emitters"],
            "implemented_probe",
        )
        self.assertIn("niagara.runtime_smoke_test", expectations["checks_skipped_honest_gaps"])
        self.assertIn("poc_b_gates", expectations)
        self.assertFalse(expectations["poc_b_gates"]["round_trip_supported"])

    def test_poc_b_inspect_gate_signals_fixture(self) -> None:
        fixture = load_fixture("poc_b_inspect_gate_signals.json")
        expectations = fixture["expectations"]

        expected_emitters = fixture["expected_emitters"]
        graphs = fixture["inspect_graphs"]
        self.assertEqual(len(expected_emitters), 2)

        emitter_graphs = [g for g in graphs if g["graph_type"] == "NiagaraEmitterGraph"]
        self.assertEqual(len(emitter_graphs), 2)
        self.assertEqual(expectations["emitters_with_renderer_refs"], 2)
        self.assertEqual(expectations["total_renderer_refs"], 2)
        self.assertEqual(expectations["renderers_with_extracted_material_path"], 1)
        self.assertEqual(
            expectations["used_data_interfaces"],
            graphs[0]["extensions"]["niagara"]["dependencies"]["used_data_interfaces"],
        )
        self.assertTrue(expectations["B7_renderers_present"])
        self.assertFalse(expectations["B7_data_interfaces_complete"])
        self.assertTrue(expectations["B7_renderers_bound_requires_material_verify"])
        self.assertEqual(
            set(expectations["validation_never_claims"]),
            {"created_and_validated", "modified_and_validated"},
        )

    def test_execute_plan_create_niagara_effect_fixture(self) -> None:
        fixture = load_fixture("execute_plan_create_niagara_effect_dry.json")
        expectations = fixture["expectations"]
        operation = fixture["plan_request"]["specification"]["operations"][0]

        self.assertEqual(fixture["registered_action"], "create_niagara_effect")
        self.assertEqual(fixture["owner"], "WS-07")
        self.assertEqual(operation["action"], "create_niagara_effect")
        self.assertTrue(operation["target"]["asset_path"].startswith("/Game/__UeremcpTests/"))
        self.assertTrue(expectations["requires_registered_handler"])
        self.assertEqual(expectations["honest_domain_status"], "partially_completed")
        self.assertTrue(expectations["atomic_plan_still_blocked_without_ws03_callbacks"])

    def test_hash_round_trip_poc_b_scaffold_fixture(self) -> None:
        import sys

        sys.path.insert(0, str(PROTOCOL_TESTS))
        from ueremcp_protocol.content_hash import content_hash

        fixture = load_fixture("hash_round_trip_poc_b_scaffold.json")
        graphs = fixture["inspect_graphs"]
        hash_expectations = fixture["hash_scaffold_expectations"]
        gate_expectations = fixture["poc_b_gate_expectations"]

        self.assertEqual(len(graphs), hash_expectations["graph_count"])
        self.assertEqual(
            sum(1 for graph in graphs if graph["graph_type"] == "NiagaraEmitterGraph"),
            hash_expectations["emitter_graph_count"],
        )
        self.assertFalse(hash_expectations["round_trip_supported"])

        for graph in graphs:
            self.assertFalse(graph["fidelity"]["round_trip_supported"])
            digest = content_hash(graph)
            self.assertTrue(digest.startswith(hash_expectations["content_hash_prefix"]))
            digest_again = content_hash(graph)
            self.assertEqual(digest, digest_again)

        for check in hash_expectations["checks_skipped"]:
            self.assertIn("round_trip", check)

        self.assertFalse(gate_expectations["round_trip_supported"])
        self.assertTrue(gate_expectations["B7_renderers_bound_null_without_inspect"])
        for check in gate_expectations["checks_skipped_always"]:
            self.assertIn("round_trip", check)

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
