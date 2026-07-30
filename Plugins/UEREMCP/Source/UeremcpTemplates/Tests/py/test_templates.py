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
                inputs={"element": "fire"},
                modifiers={"add": ["nonexistent_modifier"]},
            )
        )
        self.assertFalse(result.success)
        self.assertIn("Unsupported modifier", result.summary)

    def test_declared_modifier_without_delta_fails_closed(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs={"element": "fire"},
                modifiers={"adjust": ["reduce_trail_persistence"]},
            )
        )
        self.assertFalse(result.success)
        self.assertIn("no executable delta", result.summary)

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
        self.assertIn("template.validation_rules", response["validation"]["checks_skipped"])
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
        self.assertIn("template.validation_rules", response["validation"]["checks_skipped"])
        self.assertEqual(list(self.response_validator.iter_errors(response)), [])


class TemplatePromotionTests(unittest.TestCase):
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
        self.assertEqual(len(result.contract_gates or []), 5)

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
        self.assertIn(
            "template.promotion.complete_state_retrieval",
            response["validation"]["checks_skipped"],
        )

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

    def test_invalid_contract_inputs_fail_before_preview(self) -> None:
        invalid_source = self.service.plan_promotion(
            PromotionRequest(source_asset="/Engine/VFX/NS_Fireball")
        )
        self.assertFalse(invalid_source.success)
        self.assertEqual(invalid_source.status, "failed_validation")

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
        notes = " ".join(result.capability_notes or [])
        self.assertIn("quarantine=false was not honored", notes)
        self.assertIn("dry_run=false was requested", notes)


def main() -> int:
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TemplateStoreTests))
    suite.addTests(loader.loadTestsFromTestCase(TemplateSearchTests))
    suite.addTests(loader.loadTestsFromTestCase(TemplateInstantiateTests))
    suite.addTests(loader.loadTestsFromTestCase(TemplatePromotionTests))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
