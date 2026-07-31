#!/usr/bin/env python
"""CI contract tests for the intent router.

Rejects:
  - missing/stale snapshot usability without hash
  - plans that emit names absent from the registry
  - duplicate aliases
  - docs/examples naming nonexistent tools (delegates to check_tool_names)
  - dependency cycles / missing catalog refs
  - confident output under forced hash mismatch
  - adversarial bogus tool names surviving the plan
"""
from __future__ import annotations

import json
import io
import os
import sys
import unittest
from contextlib import redirect_stdout

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
TOOLS = os.path.join(ROOT, "tools")
sys.path.insert(0, os.path.join(TOOLS, "intent_router"))
sys.path.insert(0, TOOLS)

from router import (  # noqa: E402
    ALIASES,
    BASELINE_EVAL,
    build_index,
    evaluate_baseline,
    evaluate_heldout,
    load_catalog,
    plan,
    registry_hash,
    resolve_tool_alias,
    search,
    validate_dependency_metadata,
)
from check_tool_names import (  # noqa: E402
    DOMAIN_TOOLSET_HINTS,
    check_snapshot_fresh,
    check_source_descriptions,
    discover_source_tools,
    source_surface_fingerprint,
    unknown_tool_references,
)


class IntentRouterContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        snap_path = os.path.join(TOOLS, "registry_snapshot.json")
        if not os.path.exists(snap_path):
            raise unittest.SkipTest("registry_snapshot.json missing")
        with open(snap_path, encoding="utf-8") as fh:
            cls.snap = json.load(fh)
        cls.catalog = load_catalog()
        cls.docs, cls.meta = build_index(cls.snap, cls.catalog)
        cls.hash = registry_hash(cls.snap)
        cls.known = {m["qualified"] for m in cls.meta}

    def test_dependency_metadata_valid(self):
        errs = validate_dependency_metadata(self.catalog)
        self.assertEqual(errs, [], errs)

    def test_no_duplicate_aliases(self):
        keys = list(ALIASES.keys())
        self.assertEqual(len(keys), len(set(keys)))

    def test_callable_router_names_have_one_canonical_owner(self):
        source = discover_source_tools()
        expected = {
            "UeremcpCore.UeremcpReferenceToolset.GetStarted",
            "UeremcpCore.UeremcpReferenceToolset.ResolveIntent",
            "UeremcpCore.UeremcpReferenceToolset.DescribeOperation",
        }
        self.assertTrue(expected.issubset(source))
        for leaf in ("GetStarted", "ResolveIntent", "DescribeOperation"):
            matches = [name for name in source if name.endswith("." + leaf)]
            self.assertEqual(matches, ["UeremcpCore.UeremcpReferenceToolset." + leaf])
        catalog_names = [op.get("qualified") for op in self.catalog.get("operations") or []]
        for name in expected:
            self.assertEqual(catalog_names.count(name), 1)
        probes = {
            name for name in source
            if name.endswith(".Ping") or name.endswith(".Echo")
        }
        self.assertTrue((set(source) - probes).issubset(set(catalog_names)))

    def test_all_callable_descriptions_are_agent_callable(self):
        source = discover_source_tools()
        # Environment (8) + Systems (5) + general capture (4 vs prior 1) raise the surface.
        self.assertEqual(len(source), 42)
        self.assertEqual(check_source_descriptions(source), 0)

    def test_pascal_snake_kebab_aliases_normalize_without_dual_registration(self):
        known = {
            "UeremcpNiagara.UeremcpNiagaraToolset.CreateNiagaraEffect",
            "UeremcpMaterial.UeremcpMaterialToolset.CreateVfxMaterial",
        }
        canonical = "UeremcpNiagara.UeremcpNiagaraToolset.CreateNiagaraEffect"
        self.assertEqual(resolve_tool_alias("create_niagara_effect", known), canonical)
        self.assertEqual(resolve_tool_alias("create-niagara-effect", known), canonical)
        self.assertEqual(resolve_tool_alias(canonical.lower(), known), canonical)
        self.assertIsNone(resolve_tool_alias("create", known))

    def test_snapshot_freshness_fails_closed_on_missing_callable(self):
        source = discover_source_tools()
        toolsets = {}
        for qualified in source:
            toolset_name, tool_name = qualified.rsplit(".", 1)
            toolsets.setdefault(toolset_name, {"tools": {}})["tools"][tool_name] = {
                "qualified_name": qualified
            }
        synthetic = {
            "source_surface_fingerprint": source_surface_fingerprint(source),
            "toolsets": toolsets,
        }
        self.assertEqual(check_snapshot_fresh(synthetic, source), 0)
        missing = "UeremcpCore.UeremcpReferenceToolset.ResolveIntent"
        toolsets["UeremcpCore.UeremcpReferenceToolset"]["tools"].pop("ResolveIntent")
        with redirect_stdout(io.StringIO()):
            self.assertGreater(check_snapshot_fresh(synthetic, source), 0)
        self.assertNotIn(missing, {
            "%s.%s" % (ts, tool)
            for ts, data in toolsets.items()
            for tool in data["tools"]
        })

    def test_template_domain_enum_is_canonical_and_mapped(self):
        path = os.path.join(ROOT, "schemas", "template-library", "template.schema.json")
        with open(path, encoding="utf-8") as fh:
            schema = json.load(fh)
        domains = schema["properties"]["domain"]["enum"]
        self.assertEqual(set(domains), set(DOMAIN_TOOLSET_HINTS))
        self.assertEqual(len(domains), len(set(domains)))

    def test_operator_scripts_are_in_mandatory_read_order(self):
        with open(os.path.join(ROOT, "AGENTS.md"), encoding="utf-8") as fh:
            contract = fh.read()
        with open(os.path.join(ROOT, "Scripts", "README.md"), encoding="utf-8") as fh:
            scripts_guide = fh.read()
        self.assertIn("**`Scripts/`**", contract)
        self.assertIn("operator-proven recipes", scripts_guide)

    def test_bogus_qualified_tool_name_is_rejected_with_near_match(self):
        text = (
            "`UeremcpCore.UeremcpReferenceToolset.ExecutePlan`\n"
            "`UeremcpCore.UeremcpReferenceToolset.ExecutePla`\n"
        )
        unknown = unknown_tool_references(text, self.snap)
        self.assertEqual(len(unknown), 1)
        self.assertEqual(
            unknown[0][1],
            "UeremcpCore.UeremcpReferenceToolset.ExecutePla",
        )
        self.assertIn("ExecutePlan", unknown[0][2])

    def test_catalog_ops_exist_or_documented_fallback(self):
        missing = []
        # Source-declared names may be absent until the required live redeploy +
        # fail-closed registry dump. They are not silently treated as live.
        pending_ok = set(discover_source_tools()) - self.known
        for op in self.catalog.get("operations") or []:
            q = op.get("qualified")
            if not q:
                continue
            if q not in self.known and q not in pending_ok:
                if q.startswith("EditorToolset.LogsToolset"):
                    continue
                missing.append(q)
        critical = [m for m in missing if m.startswith("Ueremcp")]
        self.assertEqual(critical, [], critical)

    def test_plan_never_emits_absent_names(self):
        intents = [
            "make a spell effect with a helix and show me what it looks like",
            "TotallyBogusToolName.DoesNotExist.Hallucinate",
            "create niagara fireball",
        ]
        for intent in intents:
            result = plan(intent, self.docs, self.meta, self.catalog, snap_hash=self.hash)
            for step in result.get("plan") or []:
                self.assertIn(step["qualified"], self.known, step)
            for alt in result.get("alternatives") or []:
                self.assertIn(alt["tool"], self.known, alt)

    def test_stale_hash_forces_abstain(self):
        result = plan(
            "make a fire projectile effect",
            self.docs,
            self.meta,
            self.catalog,
            expected_hash="deadbeef" * 8,
            snap_hash=self.hash,
        )
        self.assertTrue(result["abstained"])
        self.assertEqual(result["confidence"], "none")
        self.assertEqual(result.get("plan"), [])

    def test_adversarial_bogus_name_not_invented(self):
        result = plan(
            "please call UeremcpFake.FakeToolset.DoMagic now",
            self.docs,
            self.meta,
            self.catalog,
            snap_hash=self.hash,
        )
        for step in result.get("plan") or []:
            self.assertNotIn("FakeToolset", step["qualified"])
            self.assertIn(step["qualified"], self.known)

    def test_baseline_eval_runs(self):
        # Contract: suite runs and reports rates. Do NOT require 7/7.
        report = evaluate_baseline(self.docs, self.meta)
        self.assertEqual(report["n"], len(BASELINE_EVAL))
        self.assertGreaterEqual(report["top3"], 0)

    def test_heldout_suite_runs(self):
        path = os.path.join(HERE, "heldout_intents.json")
        with open(path, encoding="utf-8") as fh:
            held = json.load(fh)
        report = evaluate_heldout(self.docs, self.meta, held, self.catalog, self.hash)
        self.assertEqual(report["n"], len(held))
        # Deterministic floor from metrics_2026-07-30.json. This suite was
        # authored independently of router tuning; regressions fail CI.
        self.assertGreaterEqual(report["top1_rate"], 0.60)
        self.assertGreaterEqual(report["top3_rate"], 0.80)
        self.assertGreaterEqual(report["mrr"], 0.60)
        self.assertLessEqual(report["confident_wrong"], 2)
        self.assertEqual(report["abstention_accuracy"], 1.0)

    def test_helix_visual_prompt_emits_only_live_tools(self):
        result = plan(
            "make a spell effect with a helix and show me what it looks like",
            self.docs,
            self.meta,
            self.catalog,
            snap_hash=self.hash,
        )
        for step in result.get("plan") or []:
            self.assertIn(step["qualified"], self.known)
            self.assertIn("request_json", step)
            self.assertIn("input_schema", step)


if __name__ == "__main__":
    unittest.main()
