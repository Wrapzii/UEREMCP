"""Unit tests for provisional $ref resolution."""

from __future__ import annotations

import unittest

from ueremcp_protocol.ref_resolve import RefResolveError, resolve_refs


class RefResolveTests(unittest.TestCase):
    def test_resolve_primary_asset(self):
        spec = {
            "name": "Fireball",
            "core_material": {"$ref": "material.result.primary_asset"},
        }
        completed = {
            "material": {
                "result": {"primary_asset": "/Game/VFX/Materials/MI_Core"},
                "status": "created_and_validated",
            }
        }
        out = resolve_refs(spec, completed)
        self.assertEqual(out["core_material"], "/Game/VFX/Materials/MI_Core")
        self.assertEqual(out["name"], "Fireball")

    def test_nested_and_array(self):
        spec = {
            "effects": [
                {"$ref": "fx.result.primary_asset"},
                "literal",
            ],
            "nested": {"path": {"$ref": "fx.result.created_assets.0.asset_path"}},
        }
        completed = {
            "fx": {
                "result": {
                    "primary_asset": "/Game/NS_A",
                    "created_assets": [{"asset_path": "/Game/NS_A"}],
                }
            }
        }
        out = resolve_refs(spec, completed)
        self.assertEqual(out["effects"][0], "/Game/NS_A")
        self.assertEqual(out["effects"][1], "literal")
        self.assertEqual(out["nested"]["path"], "/Game/NS_A")

    def test_unresolved_fails_not_null(self):
        with self.assertRaises(RefResolveError) as ctx:
            resolve_refs(
                {"x": {"$ref": "missing.result.primary_asset"}},
                {},
            )
        self.assertNotIn("None", str(ctx.exception))

    def test_malformed_ref(self):
        with self.assertRaises(RefResolveError):
            resolve_refs({"x": {"$ref": "nopath"}}, {"nopath": {}})

    def test_extra_keys_not_treated_as_ref(self):
        # Strict: only {"$ref": "..."} is a substitution object.
        spec = {"x": {"$ref": "a.result.primary_asset", "note": "keep"}}
        completed = {"a": {"result": {"primary_asset": "/Game/X"}}}
        out = resolve_refs(spec, completed)
        self.assertEqual(out["x"]["$ref"], "a.result.primary_asset")
        self.assertEqual(out["x"]["note"], "keep")

    def test_dollar_string_primary_asset(self):
        spec = {"core_material": "$material", "name": "Fireball"}
        completed = {
            "material": {"result": {"primary_asset": "/Game/VFX/Materials/MI_Core"}}
        }
        out = resolve_refs(spec, completed)
        self.assertEqual(out["core_material"], "/Game/VFX/Materials/MI_Core")
        self.assertEqual(out["name"], "Fireball")

    def test_dollar_string_reagenttools_label(self):
        # [VERIFIED: batch_workflow_tools.py:37-48]
        spec = {"actor": "$spawn1"}
        completed = {"spawn1": {"label": "RE_Fireball", "path": "/Game/Map.Map:PersistentLevel.RE_Fireball"}}
        out = resolve_refs(spec, completed)
        self.assertEqual(out["actor"], "RE_Fireball")

    def test_dollar_string_path_fallback(self):
        spec = {"asset": "$a"}
        completed = {"a": {"path": "/Game/BP_X"}}
        out = resolve_refs(spec, completed)
        self.assertEqual(out["asset"], "/Game/BP_X")

    def test_dollar_string_unresolved_fails(self):
        with self.assertRaises(RefResolveError):
            resolve_refs({"x": "$missing"}, {})

    def test_both_forms_together(self):
        spec = {
            "a": {"$ref": "mat.result.primary_asset"},
            "b": "$mat",
        }
        completed = {"mat": {"result": {"primary_asset": "/Game/M"}}}
        out = resolve_refs(spec, completed)
        self.assertEqual(out["a"], "/Game/M")
        self.assertEqual(out["b"], "/Game/M")


if __name__ == "__main__":
    unittest.main()
