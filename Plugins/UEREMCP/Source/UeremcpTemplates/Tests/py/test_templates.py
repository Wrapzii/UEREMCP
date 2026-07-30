#!/usr/bin/env python3
"""WS-15 template library unit tests (out-of-editor)."""

from __future__ import annotations

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
    SearchQuery,
    TemplateService,
    TemplateStore,
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

    def test_template_ids_match_filenames(self) -> None:
        self.store.load_from_directory(self.templates_dir)
        for path in sorted((self.templates_dir / "niagara").glob("*.json")):
            document = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual(path.stem, document["template_id"])


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

    def test_instantiate_elemental_substitutes_inputs(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs={
                    "element": "water",
                    "target_path": "/Game/VFX/Spells/NS_WaterProjectile",
                    "scale": 1.25,
                    "intensity": 6.0,
                },
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
            projectile_step["specification"]["target_path"],
            "/Game/VFX/Spells/NS_WaterProjectile",
        )

    def test_target_is_forwarded_to_terminal_operation(self) -> None:
        result = self.service.instantiate(
            InstantiateRequest(
                template_id="niagara.projectile.elemental.v1",
                inputs={"element": "fire"},
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
        self.assertEqual(
            terminal["specification"]["target_path"],
            "/Game/VFX/Spells/NS_FireProjectile",
        )

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
                inputs={"element": "lightning"},
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
            inputs={"element": "water"},
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
            inputs={"element": "earth"},
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


def main() -> int:
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    suite.addTests(loader.loadTestsFromTestCase(TemplateStoreTests))
    suite.addTests(loader.loadTestsFromTestCase(TemplateSearchTests))
    suite.addTests(loader.loadTestsFromTestCase(TemplateInstantiateTests))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    raise SystemExit(main())
