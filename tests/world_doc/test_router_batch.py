#!/usr/bin/env python
"""Conformance for intent_router batch assembly.

The router already knew the dependency graph -- action_order_rank reads
depends_on_actions out of operation_catalog.json. It just never used it to
BUILD anything: it emitted N independently-callable steps, each with its own
request_json, and never mentioned execute_plan.

That is why agents issued one call per domain. Nothing told them otherwise.

Two defects these tests pin down, both found by running the router rather than
reading it:

  1. Candidates were deduped by TOOLSET. Every environment operation lives in
     UeremcpEnvironment.UeremcpEnvironmentToolset, so a five-operation build
     collapsed to a single step -- the "1-step plan" behaviour.
  2. A composite (build_environment) was emitted alongside the primitives it
     performs, which would do the work twice.

Run:  python tests/world_doc/test_router_batch.py
"""
from __future__ import annotations

import json
import os
import sys
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tools", "intent_router"))

import router as R  # noqa: E402

MULTI = "create a landscape, add a river, scatter trees on the banks and attach rain"
SINGLE = "read the editor log for errors"


def _plan(query):
    snap = json.load(open(os.path.join(REPO, "tools", "registry_snapshot.json"),
                          encoding="utf-8"))
    catalog = R.load_catalog()
    docs, meta = R.build_index(snap, catalog)
    return R.plan(query, docs, meta, catalog, snap_hash=R.registry_hash(snap))


class TestMultiDomainNoLongerCollapses(unittest.TestCase):
    """Defect 1: dedupe by toolset swallowed a whole build sequence."""

    @classmethod
    def setUpClass(cls):
        cls.out = _plan(MULTI)

    def test_emits_more_than_one_step(self):
        self.assertGreater(len(self.out["plan"]), 1,
                           "a four-domain intent collapsed to a single step")

    def test_distinct_actions_from_one_toolset(self):
        actions = {s["request_json"]["action"] for s in self.out["plan"]
                   if "request_json" in s}
        for expected in ("create_landscape", "create_water_body", "scatter_foliage"):
            self.assertIn(expected, actions)


class TestBatchAssembly(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.out = _plan(MULTI)
        cls.batch = cls.out.get("batch")

    def test_batch_is_emitted(self):
        self.assertIsNotNone(self.batch, "multi-domain plan produced no batch")

    def test_batch_is_an_execute_plan_envelope(self):
        req = self.batch["request_json"]
        self.assertEqual(req["action"], "execute_plan")
        self.assertEqual(req["protocol_version"], "1.0")
        self.assertIn("operations", req["specification"])

    def test_conforms_to_plan_schema_shape(self):
        spec = self.batch["request_json"]["specification"]
        self.assertIn(spec["on_failure"],
                      ("stop", "continue_independent", "rollback_all"))
        self.assertIn(spec["transaction"]["compile_policy"],
                      ("per_operation", "at_boundaries", "at_end", "never"))
        ids = [o["id"] for o in spec["operations"]]
        self.assertEqual(len(ids), len(set(ids)), "operation ids must be unique")
        for op in spec["operations"]:
            self.assertRegex(op["id"], r"^[a-zA-Z0-9_-]+$")
            self.assertRegex(op["action"], r"^[a-z][a-z0-9_]*$")

    def test_depends_on_only_names_present_operations(self):
        """A depends_on naming an absent operation fails the whole plan."""
        spec = self.batch["request_json"]["specification"]
        ids = {o["id"] for o in spec["operations"]}
        for op in spec["operations"]:
            for dep in op.get("depends_on") or []:
                self.assertIn(dep, ids)

    def test_operations_emitted_in_dependency_order(self):
        spec = self.batch["request_json"]["specification"]
        seen = set()
        for op in spec["operations"]:
            for dep in op.get("depends_on") or []:
                self.assertIn(dep, seen,
                              "%s depends on %s, emitted later" % (op["id"], dep))
            seen.add(op["id"])

    def test_reports_the_saving(self):
        rt = self.batch["round_trips"]
        self.assertEqual(rt["as_one_plan"], 1)
        self.assertGreater(rt["as_separate_calls"], 1)

    def test_states_how_to_chain_results(self):
        self.assertIn("$ref", self.batch["chaining"])

    def test_destructive_batch_defaults_to_dry_run(self):
        req = self.batch["request_json"]
        if any(op["action"].startswith(("create_", "place_", "attach_"))
               for op in req["specification"]["operations"]):
            self.assertTrue(req["options"].get("dry_run"),
                            "a destructive batch must default to dry_run")


class TestCompositeNotDoubleCounted(unittest.TestCase):
    """Defect 2: build_environment performs the four primitives. Emitting both
    does the work twice; silently substituting the composite changes the
    failure modes and placeholder behaviour the caller asked for."""

    @classmethod
    def setUpClass(cls):
        cls.batch = _plan(MULTI).get("batch")

    def test_composite_excluded_from_batch(self):
        actions = [o["action"] for o in
                   self.batch["request_json"]["specification"]["operations"]]
        self.assertNotIn("build_environment", actions)

    def test_omission_is_explained_not_silent(self):
        self.assertTrue(self.batch.get("omitted"))
        self.assertIn("build_environment", " ".join(self.batch["omitted"]))

    def test_catalog_records_the_coverage_as_data(self):
        """The relationship belongs in the catalog, not in router code."""
        catalog = R.load_catalog()
        entry = next(e for e in catalog["dependencies"]
                     if e.get("action") == "build_environment")
        self.assertTrue(set(entry["covers"]) >= {"create_landscape",
                                                 "create_water_body"})


class TestVerificationRunsLast(unittest.TestCase):

    def test_verify_actions_depend_on_the_build(self):
        batch = _plan(MULTI).get("batch")
        ops = {o["id"]: o for o in
               batch["request_json"]["specification"]["operations"]}
        verify = [o for o in ops.values()
                  if o["action"].startswith(("validate_", "capture_", "inspect_"))]
        for op in verify:
            self.assertTrue(op.get("depends_on"),
                            "%s must observe something first" % op["action"])


class TestSingleDomainUnchanged(unittest.TestCase):
    """A batch for one operation is pure overhead, and widening the step cap
    must not turn a focused intent into five speculative calls."""

    def test_independent_steps_do_not_become_a_batch(self):
        """A batch claims the operations belong together. Three unrelated reads
        wrapped in one rollback_all transaction is worse than three calls: a
        failure in any one discards the others.

        This intent used to abstain entirely; after the catalog merge restored
        GetLogEntries it routes confidently, which exposed the real bug --
        assembly fired on "two or more operations" rather than on an actual
        dependency."""
        out = _plan(SINGLE)
        self.assertFalse(out["abstained"])
        self.assertTrue(any("LogsToolset" in s["qualified"] for s in out["plan"]))
        self.assertIsNone(out.get("batch"),
                          "unrelated steps were wrapped in a transaction")

    def test_one_step_plan_gets_a_note_not_a_batch(self):
        out = _plan("inspect the niagara system at /Game/__UeremcpTests/Fireball")
        if not out["abstained"] and len(out["plan"]) == 1:
            self.assertIsNone(out.get("batch"))
            self.assertIn("batch_note", out)
        else:
            self.skipTest("intent did not yield a single confident step")


class TestNoRegression(unittest.TestCase):

    def test_baseline_routing_accuracy_held(self):
        """Widening the cap and re-keying the dedupe must not cost accuracy.
        Measured 3/7 top-1 and 5/7 top-3 both before and after."""
        snap = json.load(open(os.path.join(REPO, "tools", "registry_snapshot.json"),
                              encoding="utf-8"))
        docs, meta = R.build_index(snap, R.load_catalog())
        res = R.evaluate_baseline(docs, meta)
        self.assertGreaterEqual(res["top1"], 3)
        self.assertGreaterEqual(res["top3"], 5)

    def test_no_hallucinated_tool_names(self):
        """The structural guarantee: candidates come only from the snapshot."""
        snap = json.load(open(os.path.join(REPO, "tools", "registry_snapshot.json"),
                              encoding="utf-8"))
        known = set()
        for ts, v in snap["toolsets"].items():
            for t in (v.get("tools") or {}):
                known.add("%s.%s" % (ts, t))
        for step in _plan(MULTI)["plan"]:
            self.assertIn(step["qualified"], known)

class TestQualityIntentRoutesToImport(unittest.TestCase):
    """The happy path must not teach that a primitive is finished art.

    Measured: an agent asked for a lived-in landscape shipped cylinder-and-cone
    trees, and said so plainly -- "cone trees were acceptable to me because the
    tooling's happy path made them the intended empty-project answer." It had
    read the note saying import_file is the only path to reference-faithful
    geometry, and shipped cones anyway, because the catalog example for
    SubmitMeshOps was a cylinder plus a cone named SM_Conifer.

    An example IS a recommendation. These tests hold that line.
    """

    QUALITY = ("I want realistic high quality trees that look like the reference photos",
               "photoreal foliage, not blockout",
               "hero assets for the forest, final art quality")

    def test_quality_vocabulary_reaches_import(self):
        for intent in self.QUALITY:
            out = _plan(intent)
            tools = [s["qualified"] for s in out["plan"]]
            self.assertTrue(any("import_file" in t for t in tools),
                            "%r never surfaced an import path; got %s" % (intent, tools))

    def test_quality_vocabulary_does_not_lead_with_primitives(self):
        for intent in self.QUALITY:
            out = _plan(intent)
            if not out["plan"]:
                continue
            self.assertNotIn("SubmitMeshOps", out["plan"][0]["qualified"],
                             "%r led with primitive authoring" % intent)

    def test_submit_mesh_ops_example_is_not_a_tree(self):
        """The example is the quality ceiling an agent infers."""
        catalog = R.load_catalog()
        op = next((o for o in catalog["operations"]
                   if o.get("action") == "submit_mesh_ops"), None)
        self.assertIsNotNone(op)
        blob = json.dumps(op["example_request"]).lower()
        for word in ("conifer", "pine", "tree", "cone"):
            self.assertNotIn(word, blob,
                             "submit_mesh_ops example still teaches %r as the shape "
                             "to author" % word)

    def test_submit_mesh_ops_warns_against_hero_use(self):
        catalog = R.load_catalog()
        op = next((o for o in catalog["operations"]
                   if o.get("action") == "submit_mesh_ops"), None)
        blob = (" ".join(op.get("do_not_use_for") or []) + " " + op.get("recovery", "")).lower()
        self.assertIn("import_file", blob,
                      "nothing points a quality ask at the import path")

    def test_scatter_depends_on_a_real_mesh(self):
        """Placing geometry before its surface exists is how blockout gets
        mistaken for finished work."""
        catalog = R.load_catalog()
        edge = next((d for d in catalog["dependencies"]
                     if d.get("action") == "scatter_foliage"), None)
        self.assertIsNotNone(edge)
        self.assertIn("submit_mesh_ops", edge["depends_on_actions"])

if __name__ == "__main__":
    unittest.main(verbosity=2)
