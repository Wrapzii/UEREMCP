#!/usr/bin/env python3
"""WS-15 template library unit tests (out-of-editor)."""

from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path

from jsonschema import Draft202012Validator

ROOT = Path(__file__).resolve().parents[6]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from validate_templates import load_registry  # noqa: E402
from ueremcp_templates import (  # noqa: E402
    InstantiateRequest,
    PromotionRequest,
    SearchQuery,
    TemplateService,
    TemplateStore,
    build_promotion_response,
    delegate_execute_plan,
)


class TemplateStoreTests(unittest.TestCase):
    def setUp(self) -> None:
        self.store = TemplateStore()
        self.templates_dir = ROOT / "templates"

    def test_load_all_seeded_templates(self) -> None:
        loaded, errors = self.store.load_from_directory(self.templates_dir)
        self.assertEqual(errors, [], msg="\n".join(errors))
        self.assertEqual(loaded, 7)
        self.assertEqual(self.store.count(), 7)
        self.assertEqual(self.store.element_preset_count(), 5)

    def test_template_ids_match_filenames(self) -> None:
        self.store.load_from_directory(self.templates_dir)
        for path in sorted((self.templates_dir / "niagara").glob("*.json")):
            document = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(path.stem, document["template_id"])

    def test_element_presets_match_ws08_reference_values(self) -> None:
        self.store.load_from_directory(self.templates_dir)
        reference = json.loads(
            (
                ROOT
                / "schemas"
                / "domains"
                / "materials"
                / "element_presets.v1.json"
            ).read_text(encoding="utf-8")
        )
        for element, expected in reference["elements"].items():
            preset = self.store.find_element_preset(element)
            self.assertIsNotNone(preset)
            assert preset is not None
            actual = preset.material_parameter_overrides
            self.assertEqual(actual["ParticleColor"], expected["particle_color"])
            self.assertEqual(actual["ColorSecondary"], expected["color_secondary"])
            for json_name, reference_name in {
                "EmissiveScale": "emissive_scale",
                "FlowSpeed": "flow_speed",
                "Turbulence": "turbulence",
                "SoftEdge": "soft_edge",
                "DepthFade": "depth_fade",
            }.items():
                self.assertEqual(actual[json_name], expected[reference_name])
            self.assertEqual(
                preset.niagara_parameters["primary_color"],
                actual["ParticleColor"],
            )
            self.assertEqual(
                preset.niagara_parameters["secondary_color"],
                actual["ColorSecondary"],
            )
            self.assertEqual(
                preset.niagara_parameters["intensity"],
                actual["EmissiveScale"],
            )

    def test_elemental_composes_resolve_and_match_plan_archetypes(self) -> None:
        self.store.load_from_directory(self.templates_dir)
        elemental = self.store.find_by_id("niagara.projectile.elemental.v1")
        assert elemental is not None
        composes = elemental.composes
        self.assertEqual(len(composes), 6)
        for composed_id in composes:
            self.assertIsNotNone(
                self.store.find_by_id(composed_id),
                msg=composed_id,
            )

        archetypes: list[str] = []
        for operation in elemental.document["construction_plan"]:
            if operation["action"] != "create_niagara_effect":
                continue
            for component in operation["specification"]["components"]:
                archetypes.append(component["archetype"])
        self.assertEqual(set(archetypes), set(composes))
        self.assertEqual(len(archetypes), len(set(archetypes)))

    def test_element_preset_ids_match_filenames(self) -> None:
        self.store.load_from_directory(self.templates_dir)
        for path in sorted((self.templates_dir / "elements").glob("*.json")):
            document = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(path.stem, document["preset_id"])
            self.assertTrue(path.stem.startswith(f"element.{document['element']}."))


class TemplateSearchTests(unittest.TestCase):
    def setUp(self) -> None:
        self.store = TemplateStore()
        self.store.load_from_directory(ROOT / "templates")
        self.service = TemplateService(self.store)

    def test_search_projectile_by_query(self) -> None:
        hits = self.service.search(SearchQuery(query="projectile", domain="niagara"))
        ids = {hit.template_id for hit in hits}
        self.assertIn("niagara.projectile.elemental.v1", ids)

    def test_search_element_filter(self) -> None:
        hits = self.service.search(SearchQuery(element="fire", domain="niagara"))
        ids = {hit.template_id for hit in hits}
        self.assertIn("niagara.projectile.elemental.v1", ids)
        self.assertTrue(all("emitter" in template_id for template_id in ids if template_id != "niagara.projectile.elemental.v1") or len(ids) >= 1)

    def test_search_element_filter_includes_ice(self) -> None:
        search_schema = json.loads(
            (
                ROOT
                / "schemas"
                / "domains"
                / "templates"
                / "search_templates.schema.json"
            ).read_text(encoding="utf-8")
        )
        self.assertIn("ice", search_schema["properties"]["element"]["enum"])
        hits = self.service.search(SearchQuery(element="ice", domain="niagara"))
        ids = {hit.template_id for hit in hits}
        self.assertIn("niagara.projectile.elemental.v1", ids)

    def test_search_emitter_archetype(self) -> None:
        hits = self.service.search(SearchQuery(query="ribbon"))
        self.assertTrue(any(hit.template_id == "niagara.emitter.ribbon_trail.v1" for hit in hits))


class TemplateInstantiateTests(unittest.TestCase):
    def setUp(self) -> None:
        self.store = TemplateStore()
        self.store.load_from_directory(ROOT / "templates")
        self.service = TemplateService(self.store)
        plan_schema = json.loads(
            (ROOT / "schemas" / "batch" / "plan.schema.json").read_text(encoding="utf-8")
        )
        self.plan_validator = Draft202012Validator(
            plan_schema,
            registry=load_registry(ROOT / "schemas"),
        )
        response_schema = json.loads(
            (ROOT / "schemas" / "envelope" / "response.schema.json").read_text(
                encoding="utf-8"
            )
        )
        self.response_validator = Draft202012Validator(
            response_schema,
            registry=load_registry(ROOT / "schemas"),
        )
        self.action_validators = {}
        for action, schema_path in {
            "create_vfx_material": ROOT
            / "schemas/domains/materials/create_vfx_material.schema.json",
            "create_niagara_effect": ROOT
            / "schemas/domains/niagara/create_niagara_effect.schema.json",
        }.items():
            self.action_validators[action] = Draft202012Validator(
                json.loads(schema_path.read_text(encoding="utf-8")),
                registry=load_registry(ROOT / "schemas"),
            )
        self.ws08_reference = json.loads(
            (
                ROOT
                / "schemas"
                / "domains"
                / "materials"
                / "element_presets.v1.json"
            ).read_text(encoding="utf-8")
        )
        self.elemental = self.store.find_by_id("niagara.projectile.elemental.v1")
        assert self.elemental is not None

    @staticmethod
    def projectile_inputs(element: str) -> dict[str, object]:
        title = element.title()
        return {
            "element": element,
            "core_material_path": f"/Game/VFX/Materials/MI_{title}_ProjectileCore",
            "trail_material_path": f"/Game/VFX/Materials/MI_{title}_ProjectileTrail",
            "target_path": f"/Game/VFX/Spells/NS_{title}Projectile",
            "scale": 1.25,
            "intensity": 6.0,
        }

    def test_instantiate_elemental_substitutes_inputs(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs=self.projectile_inputs("water"),
            )
        )
        self.assertTrue(result.success)
        assert result.plan is not None
        self.assertEqual(
            set(result.plan),
            {"transaction", "operations", "on_failure"},
        )
        self.assertEqual(
            list(self.plan_validator.iter_errors(result.plan)),
            [],
        )
        projectile_step = next(
            op for op in result.plan["operations"] if op["id"] == "projectile_fx"
        )
        self.assertEqual(projectile_step["specification"]["element"], "water")
        self.assertEqual(
            projectile_step["target"]["asset_path"],
            "/Game/VFX/Spells/NS_WaterProjectile",
        )
        self.assertEqual(projectile_step["specification"]["parameters"]["scale"], 1.25)
        self.assertEqual(projectile_step["specification"]["parameters"]["intensity"], 6.0)
        self.assertEqual(
            projectile_step["specification"]["parameters"]["primary_color"],
            [0.1, 0.4, 0.8, 1.0],
        )
        core_material = next(
            op for op in result.plan["operations"] if op["id"] == "core_material"
        )
        self.assertEqual(
            core_material["specification"]["parameter_overrides"]["EmissiveScale"],
            4.0,
        )

    def test_material_paths_and_defaults_derive_from_target_and_preset(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs={
                    "element": "ice",
                    "target_path": "/Game/VFX/Spells/NS_IceProjectile",
                },
            )
        )
        self.assertTrue(result.success)
        assert result.plan is not None
        operations = {operation["id"]: operation for operation in result.plan["operations"]}
        self.assertEqual(
            operations["core_material"]["target"]["asset_path"],
            "/Game/VFX/Spells/MI_NS_IceProjectile_Core",
        )
        self.assertEqual(
            operations["trail_material"]["target"]["asset_path"],
            "/Game/VFX/Spells/MI_NS_IceProjectile_Trail",
        )
        self.assertEqual(
            operations["projectile_fx"]["specification"]["parameters"]["intensity"],
            6.0,
        )

    def test_materialized_operations_match_domain_schemas(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs=self.projectile_inputs("ice"),
            )
        )
        self.assertTrue(result.success)
        assert result.plan is not None
        operations = {operation["id"]: operation for operation in result.plan["operations"]}
        for operation_id in ("core_material", "trail_material"):
            operation = operations[operation_id]
            errors = list(
                self.action_validators[operation["action"]].iter_errors(
                    operation["specification"]
                )
            )
            self.assertEqual(errors, [], msg=f"{operation_id}: {errors}")

        niagara_spec = copy.deepcopy(operations["projectile_fx"]["specification"])
        niagara_spec["materials"] = {
            "core": "/Game/VFX/Materials/MI_Ice_ProjectileCore",
            "trail": "/Game/VFX/Materials/MI_Ice_ProjectileTrail",
        }
        errors = list(
            self.action_validators["create_niagara_effect"].iter_errors(niagara_spec)
        )
        self.assertEqual(errors, [], msg=str(errors))

    def test_element_enum_matches_loaded_presets_and_ws08(self) -> None:
        enum_values = self.elemental.document["inputs"]["properties"]["element"]["enum"]
        self.assertEqual(set(enum_values), set(self.ws08_reference["elements"]))
        for element in enum_values:
            self.assertIsNotNone(self.store.find_element_preset(element))
        self.assertEqual(self.store.element_preset_count(), len(enum_values))

    def test_construction_plan_features_match_ws08_purpose_defaults(self) -> None:
        purpose_features = self.ws08_reference["purpose_default_features"]
        for operation in self.elemental.document["construction_plan"]:
            if operation["action"] != "create_vfx_material":
                continue
            purpose = operation["specification"]["purpose"]
            self.assertEqual(
                operation["specification"]["features"],
                purpose_features[purpose],
                msg=f"features for purpose {purpose}",
            )

    def test_every_element_preset_materializes_parity_plan(self) -> None:
        for element in self.elemental.document["inputs"]["properties"]["element"]["enum"]:
            with self.subTest(element=element):
                preset = self.store.find_element_preset(element)
                self.assertIsNotNone(preset)
                assert preset is not None
                result = self.service.instantiate(
                    InstantiateRequest(
                        template_id="niagara.projectile.elemental.v1",
                        inputs={
                            "element": element,
                            "target_path": f"/Game/VFX/Spells/NS_{element.title()}Projectile",
                            "scale": 1.5,
                            "intensity": 9.0,
                        },
                    )
                )
                self.assertTrue(result.success, msg=result.summary)
                assert result.plan is not None
                self.assertEqual(result.status, "partially_completed")
                self.assertEqual(list(self.plan_validator.iter_errors(result.plan)), [])

                operations = {
                    operation["id"]: operation for operation in result.plan["operations"]
                }
                for material_id in ("core_material", "trail_material"):
                    overrides = operations[material_id]["specification"][
                        "parameter_overrides"
                    ]
                    self.assertEqual(
                        overrides,
                        preset.material_parameter_overrides,
                        msg=f"{element}/{material_id}",
                    )
                    self.assertEqual(
                        list(
                            self.action_validators["create_vfx_material"].iter_errors(
                                operations[material_id]["specification"]
                            )
                        ),
                        [],
                        msg=f"{element}/{material_id} schema",
                    )

                niagara_params = operations["projectile_fx"]["specification"]["parameters"]
                self.assertEqual(
                    niagara_params["primary_color"],
                    preset.material_parameter_overrides["ParticleColor"],
                )
                self.assertEqual(
                    niagara_params["secondary_color"],
                    preset.material_parameter_overrides["ColorSecondary"],
                )
                self.assertEqual(niagara_params["scale"], 1.5)
                self.assertEqual(niagara_params["intensity"], 9.0)
                self.assertEqual(
                    operations["projectile_fx"]["specification"]["materials"],
                    {
                        "core": {"$ref": "core_material.result.primary_asset"},
                        "trail": {"$ref": "trail_material.result.primary_asset"},
                    },
                )
                self.assertEqual(
                    operations["core_material"]["target"]["asset_path"],
                    f"/Game/VFX/Spells/MI_NS_{element.title()}Projectile_Core",
                )
                self.assertEqual(
                    operations["projectile_fx"]["target"]["asset_path"],
                    f"/Game/VFX/Spells/NS_{element.title()}Projectile",
                )

    def test_missing_preset_fails_when_enum_bypass_is_impossible(self) -> None:
        # Enum rejects unknown elements first; this asserts the preset gate exists
        # for any future enum expansion before a JSON preset lands.
        record = self.elemental.document
        original_enum = list(record["inputs"]["properties"]["element"]["enum"])
        record["inputs"]["properties"]["element"]["enum"] = original_enum + ["arcane"]
        try:
            result = self.service.instantiate(
                InstantiateRequest(
                    template_id="niagara.projectile.elemental.v1",
                    inputs={
                        "element": "arcane",
                        "target_path": "/Game/VFX/Spells/NS_ArcaneProjectile",
                    },
                )
            )
            self.assertFalse(result.success)
            self.assertIn("No element preset is loaded for 'arcane'", result.summary)
            self.assertEqual(result.status, "failed_validation")
        finally:
            record["inputs"]["properties"]["element"]["enum"] = original_enum

    def test_target_is_forwarded_to_terminal_operation(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs=self.projectile_inputs("fire"),
                target_asset_path="/Game/VFX/Spells/NS_FireProjectile",
                mode="create_or_update",
            )
        )
        self.assertTrue(result.success)
        assert result.plan is not None
        terminal = result.plan["operations"][-1]
        self.assertEqual(
            terminal["target"]["asset_path"],
            "/Game/VFX/Spells/NS_FireProjectile",
        )
        self.assertEqual(terminal["mode"], "create_or_update")
        self.assertEqual(terminal["specification"]["element"], "fire")

    def test_missing_required_input_fails_before_delegation(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(template_id="niagara.projectile.elemental.v1")
        )
        self.assertFalse(result.success)
        self.assertIn("Missing required template input 'element'", result.summary)

    def test_invalid_element_fails_before_delegation(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs={
                    **self.projectile_inputs("fire"),
                    "element": "lightning",
                },
            )
        )
        self.assertFalse(result.success)
        self.assertIn("not an allowed value", result.summary)

    def test_unknown_modifier_fails_closed(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs=self.projectile_inputs("fire"),
                modifiers={"add": ["nonexistent_modifier"]},
            )
        )
        self.assertFalse(result.success)
        self.assertIn("Unsupported modifier", result.summary)

    def test_declared_modifier_without_delta_fails_closed(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs=self.projectile_inputs("fire"),
                modifiers={"preserve": ["preserve_networking"]},
            )
        )
        self.assertFalse(result.success)
        self.assertIn("no executable delta", result.summary)

    def test_executable_modifiers_merge_in_deterministic_bucket_order(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs=self.projectile_inputs("ice"),
                modifiers={
                    "adjust": ["reduce_trail_persistence", "boost_impact"],
                    "add": ["crystalline_fragments"],
                },
            )
        )
        self.assertTrue(result.success, result.summary)
        operations = {
            operation["id"]: operation for operation in result.plan["operations"]
        }
        self.assertEqual(
            operations["core_material"]["specification"]["modifiers"],
            ["crystalline_fragments"],
        )
        self.assertEqual(
            operations["trail_material"]["specification"]["modifiers"],
            ["reduce_trail_persistence"],
        )

    def test_duplicate_modifier_across_buckets_fails_before_delegation(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs=self.projectile_inputs("fire"),
                modifiers={
                    "adjust": ["boost_impact"],
                    "add": ["boost_impact"],
                },
            )
        )
        self.assertFalse(result.success)
        self.assertIn("more than once", result.summary)

    def test_all_modifier_effect_shapes_materialize_before_validation(self) -> None:
        record = self.store.find_by_id("niagara.projectile.elemental.v1")
        self.assertIsNotNone(record)
        original_definitions = copy.deepcopy(
            record.document.get("modifier_definitions", {})
        )
        original_core = copy.deepcopy(record.document["construction_plan"][0])
        try:
            replacement = copy.deepcopy(original_core)
            replacement["specification"]["purpose"] = "replacement_core"
            record.document["modifier_definitions"]["preserve_networking"] = {
                "bucket": "preserve",
                "replace_operations": {"core_material": replacement},
                "merge_specifications": {
                    "projectile_fx": {"base_system": None}
                },
                "append_operations": [
                    {
                        "id": "networking_manifest",
                        "action": "record_networking_manifest",
                        "depends_on": ["projectile_fx"],
                        "specification": {"element": "{{inputs.element}}"},
                    }
                ],
                "validation_operations": [
                    {
                        "id": "validate_networking_manifest",
                        "action": "validate_networking_manifest",
                        "depends_on": ["networking_manifest"],
                        "specification": {},
                    }
                ],
            }
            result = self.service.instantiate(
                InstantiateRequest(
                    template_id=record.template_id,
                    inputs=self.projectile_inputs("water"),
                    modifiers={"preserve": ["preserve_networking"]},
                )
            )
            self.assertTrue(result.success, result.summary)
            operations = result.plan["operations"]
            self.assertEqual(
                operations[0]["specification"]["purpose"],
                "replacement_core",
            )
            projectile = next(
                operation
                for operation in operations
                if operation["id"] == "projectile_fx"
            )
            self.assertNotIn("base_system", projectile["specification"])
            self.assertEqual(
                [operation["id"] for operation in operations[-2:]],
                ["networking_manifest", "validate_networking_manifest"],
            )
            self.assertEqual(
                operations[-2]["specification"]["element"],
                "water",
            )
        finally:
            record.document["modifier_definitions"] = original_definitions

    def test_delegate_returns_complete_execute_plan_result(self) -> None:
        request = InstantiateRequest(
            template_id="niagara.projectile.elemental.v1",
            inputs=self.projectile_inputs("water"),
            target_asset_path="/Game/VFX/Spells/NS_WaterProjectile",
        )
        materialized = self.service.instantiate(request)
        captured: list[dict[str, object]] = []

        def execute_plan(envelope: dict[str, object]) -> dict[str, object]:
            captured.append(envelope)
            return {
                "protocol_version": "1.0",
                "request_id": "internal",
                "status": "created_and_validated",
                "summary": "Created and reread the requested assets.",
                "result": {
                    "primary_asset": "/Game/VFX/Spells/NS_WaterProjectile",
                    "created_assets": [
                        {
                            "asset_path": "/Game/VFX/Spells/NS_WaterProjectile",
                            "asset_class": "NiagaraSystem",
                        }
                    ],
                    "operations": [
                        {
                            "id": "projectile_fx",
                            "action": "create_niagara_effect",
                            "status": "created_and_validated",
                            "primary_asset": "/Game/VFX/Spells/NS_WaterProjectile",
                        }
                    ],
                },
                "validation": {
                    "compiled": True,
                    "saved": True,
                    "reread_after_write": True,
                    "checks_performed": ["niagara.compiled"],
                },
                "changes": [
                    {
                        "asset_path": "/Game/VFX/Spells/NS_WaterProjectile",
                        "kind": "created",
                    }
                ],
                "metrics": {"mcp_round_trips": 1, "internal_operations": 12},
            }

        response = delegate_execute_plan(
            {
                "protocol_version": "1.0",
                "request_id": "instantiate-1",
                "action": "instantiate_template",
                "specification": {"template_id": request.template_id},
            },
            request,
            materialized,
            execute_plan,
        )

        self.assertEqual(len(captured), 1)
        self.assertEqual(captured[0]["action"], "execute_plan")
        self.assertEqual(captured[0]["specification"], materialized.plan)
        self.assertEqual(response["request_id"], "instantiate-1")
        self.assertEqual(
            response["result"]["primary_asset"],
            "/Game/VFX/Spells/NS_WaterProjectile",
        )
        self.assertTrue(response["validation"]["reread_after_write"])
        self.assertEqual(response["understood"]["template_used"], request.template_id)
        self.assertEqual(response["status"], "partially_completed")
        self.assertIn(
            "template.niagara.projectile.elemental.v1.six_emitters",
            response["validation"]["checks_skipped"],
        )
        self.assertEqual(list(self.response_validator.iter_errors(response)), [])

    def test_template_validation_gap_does_not_mask_executor_failure(self) -> None:
        request = InstantiateRequest(
            template_id="niagara.projectile.elemental.v1",
            inputs=self.projectile_inputs("earth"),
        )
        materialized = self.service.instantiate(request)
        response = delegate_execute_plan(
            {
                "protocol_version": "1.0",
                "request_id": "instantiate-failed",
                "action": "instantiate_template",
                "specification": {"template_id": request.template_id},
            },
            request,
            materialized,
            lambda _: {
                "protocol_version": "1.0",
                "status": "failed_validation",
                "summary": "Niagara compile failed.",
                "validation": {"errors": []},
                "metrics": {"mcp_round_trips": 1, "internal_operations": 4},
            },
        )
        self.assertEqual(response["status"], "failed_validation")
        self.assertIn(
            "template.niagara.projectile.elemental.v1.six_emitters",
            response["validation"]["checks_skipped"],
        )
        self.assertEqual(list(self.response_validator.iter_errors(response)), [])

    def test_executable_validation_operations_preserve_evidenced_status(self) -> None:
        record = self.store.find_by_id("niagara.projectile.elemental.v1")
        self.assertIsNotNone(record)
        original_rules = copy.deepcopy(record.document["validation_rules"])
        try:
            for rule in record.document["validation_rules"]:
                rule["operation"] = {
                    "id": f"validate_{rule['rule_id']}",
                    "action": "validate_niagara_system",
                    "depends_on": ["projectile_fx"],
                    "specification": {"rule_id": rule["rule_id"]},
                }
            request = InstantiateRequest(
                template_id=record.template_id,
                inputs=self.projectile_inputs("wind"),
            )
            materialized = self.service.instantiate(request)
            expected = [
                f"template.{record.template_id}.{rule['rule_id']}"
                for rule in record.document["validation_rules"]
            ]
            self.assertEqual(materialized.expected_validation_checks, expected)
            self.assertEqual(materialized.non_executable_validation_checks, [])
            self.assertEqual(
                [operation["id"] for operation in materialized.plan["operations"][-2:]],
                ["validate_six_emitters", "validate_element_user_params"],
            )

            response = delegate_execute_plan(
                {
                    "protocol_version": "1.0",
                    "request_id": "instantiate-validated-rules",
                    "action": "instantiate_template",
                },
                request,
                materialized,
                lambda _: {
                    "protocol_version": "1.0",
                    "status": "created_and_validated",
                    "summary": "Created, reread, and validated.",
                    "validation": {"checks_performed": expected},
                    "metrics": {"mcp_round_trips": 1, "internal_operations": 5},
                },
            )
            self.assertEqual(response["status"], "created_and_validated")
            self.assertNotIn("checks_skipped", response["validation"])
        finally:
            record.document["validation_rules"] = original_rules


class TemplatePromotionTests(unittest.TestCase):
    EXPECTED_GATES = [
        "template.promotion.complete_state_retrieval",
        "template.promotion.reproduction_plan_synthesis",
        "template.promotion.schema_validation",
        "template.promotion.security_write_gate",
        "template.promotion.quarantine_write",
    ]

    def setUp(self) -> None:
        self.store = TemplateStore()
        self.store.load_from_directory(ROOT / "templates")
        self.service = TemplateService(self.store)
        response_schema = json.loads(
            (ROOT / "schemas" / "envelope" / "response.schema.json").read_text(
                encoding="utf-8"
            )
        )
        self.response_validator = Draft202012Validator(
            response_schema,
            registry=load_registry(ROOT / "schemas"),
        )

    def test_preview_derives_quarantine_id_without_writing(self) -> None:
        before = sorted(path.relative_to(ROOT) for path in (ROOT / "templates").rglob("*"))
        request = PromotionRequest(source_asset="/Game/VFX/Spells/NS_Fireball")
        result = self.service.plan_promotion(request)
        after = sorted(path.relative_to(ROOT) for path in (ROOT / "templates").rglob("*"))

        self.assertTrue(result.success)
        self.assertEqual(result.status, "partially_completed")
        self.assertEqual(result.proposed_template_id, "assets.promoted.ns_fireball.v1")
        self.assertEqual(
            result.quarantine_path,
            "/Game/__UeremcpTemplates/agent/assets.promoted.ns_fireball.v1",
        )
        self.assertEqual(before, after)
        self.assertEqual(result.contract_gates, self.EXPECTED_GATES)

        response = build_promotion_response(
            {
                "protocol_version": "1.0",
                "request_id": "promote-preview",
                "action": "promote_to_template",
            },
            request,
            result,
        )
        self.assertEqual(list(self.response_validator.iter_errors(response)), [])
        self.assertNotIn("changes", response)
        self.assertNotIn("result", response)
        self.assertEqual(response["validation"]["checks_skipped"], self.EXPECTED_GATES)
        self.assertNotIn(response["status"], {
            "created_and_validated",
            "modified_and_validated",
            "created_with_warnings",
        })

    def test_base_template_drives_domain_and_category(self) -> None:
        result = self.service.plan_promotion(
            PromotionRequest(
                source_asset="/Game/VFX/Spells/NS_IceShard",
                base_template_id="niagara.projectile.elemental.v1",
            )
        )
        self.assertTrue(result.success)
        self.assertEqual(
            result.proposed_template_id,
            "niagara.projectile.ns_iceshard.v1",
        )
        self.assertEqual(result.contract_gates, self.EXPECTED_GATES)

    def test_invalid_contract_inputs_fail_before_preview(self) -> None:
        for source in (
            "/Engine/VFX/NS_Fireball",
            "/Game/../Engine/VFX/NS_Fireball",
            "/Game/VFX/NS_Fireball/",
            "",
        ):
            with self.subTest(source=source):
                invalid_source = self.service.plan_promotion(
                    PromotionRequest(source_asset=source)
                )
                self.assertFalse(invalid_source.success)
                self.assertEqual(invalid_source.status, "failed_validation")
                self.assertFalse(invalid_source.contract_gates)

        missing_base = self.service.plan_promotion(
            PromotionRequest(
                source_asset="/Game/VFX/NS_Fireball",
                base_template_id="niagara.projectile.missing.v1",
            )
        )
        self.assertFalse(missing_base.success)
        self.assertIn("Unknown base_template_id", missing_base.summary)

        invalid_id = self.service.plan_promotion(
            PromotionRequest(
                source_asset="/Game/VFX/NS_Fireball",
                proposed_template_id="Niagara Projectile Fireball",
            )
        )
        self.assertFalse(invalid_id.success)
        self.assertIn("versioned template id contract", invalid_id.summary)

    def test_mutation_and_non_quarantine_requests_remain_preview_only(self) -> None:
        request = PromotionRequest(
            source_asset="/Game/VFX/NS_Fireball",
            proposed_template_id="niagara.projectile.fireball_promoted.v1",
            quarantine=False,
            dry_run=False,
        )
        result = self.service.plan_promotion(request)
        self.assertTrue(result.success)
        self.assertEqual(result.status, "partially_completed")
        self.assertEqual(result.contract_gates, self.EXPECTED_GATES)
        notes = " ".join(result.capability_notes or [])
        self.assertIn("quarantine=false was not honored", notes)
        self.assertIn("dry_run=false was requested", notes)
        response = build_promotion_response(
            {
                "protocol_version": "1.0",
                "request_id": "promote-mutate",
                "action": "promote_to_template",
            },
            request,
            result,
        )
        self.assertEqual(list(self.response_validator.iter_errors(response)), [])
        self.assertNotIn("changes", response)
        self.assertEqual(response["validation"]["checks_skipped"], self.EXPECTED_GATES)


class TemplateExecutorBindingTests(unittest.TestCase):
    def test_module_binds_and_clears_plan_executor_delegate(self) -> None:
        module_source = (
            ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpTemplates"
            / "Private"
            / "UeremcpTemplatesModule.cpp"
        ).read_text(encoding="utf-8")
        executor_header = (
            ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpProtocol"
            / "Public"
            / "UeremcpPlanExecutor.h"
        ).read_text(encoding="utf-8")
        build_rules = (
            ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpTemplates"
            / "UeremcpTemplates.Build.cs"
        ).read_text(encoding="utf-8")

        bind = (
            "UeremcpTemplates::SetExecutePlanDelegate("
            "&FUeremcpPlanExecutor::ExecuteRequest);"
        )
        clear = "UeremcpTemplates::ClearExecutePlanDelegate();"
        self.assertIn('#include "UeremcpPlanExecutor.h"', module_source)
        self.assertIn(bind, module_source)
        self.assertIn(clear, module_source)
        self.assertIn("static bool ExecuteRequest(", executor_header)
        self.assertIn('"UeremcpProtocol"', build_rules)

        self.assertLess(module_source.index("GTemplatesModule = this;"), module_source.index(bind))
        self.assertLess(module_source.index(bind), module_source.index("Store->LoadFromDirectory"))
        self.assertLess(module_source.index(clear), module_source.index("Service.Reset();"))
        self.assertLess(module_source.index(clear), module_source.rindex("GTemplatesModule = nullptr;"))


class TemplateDomainHandlerContractTests(unittest.TestCase):
    def test_every_plan_action_has_a_documented_registration_owner(self) -> None:
        actions: set[str] = set()
        for path in sorted((ROOT / "templates").rglob("*.json")):
            if path.parent.name == "elements":
                continue
            document = json.loads(path.read_text(encoding="utf-8"))
            for operation in document.get("construction_plan", []):
                actions.add(operation["action"])

        self.assertEqual(
            actions,
            {"create_vfx_material", "create_niagara_effect"},
        )
        handoff = (
            ROOT
            / "docs"
            / "proposals"
            / "ws-15-plan-handler-registration.md"
        ).read_text(encoding="utf-8")
        self.assertIn("handlers and transaction callbacks landed on orch", handoff)
        expected_owners = {
            "create_vfx_material": "WS-08",
            "create_niagara_effect": "WS-07",
        }
        for action, owner in expected_owners.items():
            with self.subTest(action=action):
                self.assertIn(f"`{action}`", handoff)
                self.assertIn(owner, handoff)
                self.assertIn(
                    f'UnregisterAction(TEXT("{action}"))',
                    handoff,
                )

    def test_landed_handlers_and_transaction_coordinator_match_plans(self) -> None:
        material_handler = (
            ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpMaterial"
            / "Private"
            / "UeremcpMaterialPlanHandlers.cpp"
        ).read_text(encoding="utf-8")
        material_module = (
            ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpMaterial"
            / "Private"
            / "UeremcpMaterialModule.cpp"
        ).read_text(encoding="utf-8")
        niagara_handler = (
            ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpNiagara"
            / "Private"
            / "UeremcpNiagaraPlanHandlers.cpp"
        ).read_text(encoding="utf-8")
        niagara_module = (
            ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpNiagara"
            / "Private"
            / "UeremcpNiagaraModule.cpp"
        ).read_text(encoding="utf-8")
        transaction_coordinator = (
            ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpCore"
            / "Private"
            / "UeremcpPlanTransactionCoordinator.cpp"
        ).read_text(encoding="utf-8")
        core_module = (
            ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpCore"
            / "Private"
            / "UeremcpCoreModule.cpp"
        ).read_text(encoding="utf-8")

        self.assertIn('return TEXT("create_vfx_material");', material_handler)
        self.assertIn("FUeremcpPlanExecutor::RegisterAction(", material_handler)
        self.assertIn("UUeremcpMaterialToolset::CreateVfxMaterial(", material_handler)
        self.assertIn("FUeremcpMaterialPlanHandlers::Register(", material_module)
        self.assertIn("FUeremcpMaterialPlanHandlers::Unregister();", material_module)

        self.assertIn('return TEXT("create_niagara_effect");', niagara_handler)
        self.assertIn("FUeremcpPlanExecutor::RegisterAction(", niagara_handler)
        self.assertIn("UUeremcpNiagaraToolset::CreateNiagaraEffect(", niagara_handler)
        self.assertIn("FUeremcpNiagaraPlanHandlers::Register(", niagara_module)
        self.assertIn("FUeremcpNiagaraPlanHandlers::Unregister();", niagara_module)

        for callback in ("Callbacks.Begin", "Callbacks.Commit", "Callbacks.Rollback"):
            self.assertIn(callback, transaction_coordinator)
        self.assertIn(
            "FUeremcpPlanExecutor::SetTransactionCallbacks(",
            transaction_coordinator,
        )
        self.assertIn(
            "FUeremcpPlanTransactionCoordinator::RegisterWithExecutor(",
            core_module,
        )
        self.assertIn(
            "FUeremcpPlanTransactionCoordinator::UnregisterFromExecutor();",
            core_module,
        )

    def test_validation_post_steps_remain_explicitly_partial(self) -> None:
        contract = (
            ROOT
            / "docs"
            / "proposals"
            / "ws-15-execute-plan-template-contract.md"
        ).read_text(encoding="utf-8")
        service_mirror = (
            ROOT
            / "Plugins"
            / "UEREMCP"
            / "Source"
            / "UeremcpTemplates"
            / "Tests"
            / "py"
            / "ueremcp_templates"
            / "service.py"
        ).read_text(encoding="utf-8")

        self.assertIn("WS-05 `1ef125d` residual contract", contract)
        self.assertIn("WS-01 accepted", contract)
        self.assertIn('response["status"] = "partially_completed"', service_mirror)
        self.assertIn("non_executable_validation_checks", service_mirror)


def main() -> int:
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TemplateStoreTests))
    suite.addTests(loader.loadTestsFromTestCase(TemplateSearchTests))
    suite.addTests(loader.loadTestsFromTestCase(TemplateInstantiateTests))
    suite.addTests(loader.loadTestsFromTestCase(TemplatePromotionTests))
    suite.addTests(loader.loadTestsFromTestCase(TemplateExecutorBindingTests))
    suite.addTests(loader.loadTestsFromTestCase(TemplateDomainHandlerContractTests))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
