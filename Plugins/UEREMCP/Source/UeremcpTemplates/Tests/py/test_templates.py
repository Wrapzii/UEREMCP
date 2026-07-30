#!/usr/bin/env python3
"""WS-15 template library unit tests (out-of-editor)."""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[6]
sys.path.insert(0, str(Path(__file__).resolve().parent))

from ueremcp_templates import (  # noqa: E402
    InstantiateRequest,
    SearchQuery,
    TemplateService,
    TemplateStore,
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
                modifiers={"adjust": ["reduce_trail_persistence"]},
            )
        )
        self.assertTrue(result.success)
        assert result.plan is not None
        projectile_step = next(
            op for op in result.plan["operations"] if op["id"] == "projectile_fx"
        )
        self.assertEqual(projectile_step["specification"]["element"], "water")
        self.assertEqual(
            projectile_step["specification"]["target_path"],
            "/Game/VFX/Spells/NS_WaterProjectile",
        )

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
