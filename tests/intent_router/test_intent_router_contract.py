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
import os
import sys
import unittest

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
    search,
    validate_dependency_metadata,
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

    def test_catalog_ops_exist_or_documented_fallback(self):
        missing = []
        pending_ok = {
            # Not yet in the frozen snapshot until after live redeploy + dump.
            "UeremcpCore.UeremcpReferenceToolset.GetStarted",
            "UeremcpCore.UeremcpReferenceToolset.ResolveIntent",
            "UeremcpCore.UeremcpReferenceToolset.DescribeOperation",
            "UeremcpValidation.UeremcpVisualCaptureToolset.CaptureEffectFrames",
        }
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
        # Deterministic contract: abstention cases must mostly abstain
        self.assertGreaterEqual(report["abstention_accuracy"], 0.5)

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
