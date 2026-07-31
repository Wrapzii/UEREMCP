#!/usr/bin/env python
"""WS-11 conformance suite for the world-document reconciler (COVERAGE_PLAN Part VIII).

This is the VII.6 acceptance test ("is it a subsystem?") mechanised. It exists
because every claim in Part VIII is falsifiable, and the two failure modes this
project keeps hitting -- silent substitution and round-trip inflation -- are
exactly the kind that pass a human read-through.

The four VII.6 criteria map to test classes below:

  1. Novel-request test    -> TestNovelRequest
  2. No hardcoded paths    -> TestNoBareAssetPaths
  3. Peer dependencies     -> TestDelegation
  4. Round-trip count       -> TestRoundTripCount

Run:  python tests/world_doc/test_reconcile.py
      python -m unittest discover -s tests/world_doc
"""
from __future__ import annotations

import copy
import json
import os
import sys
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tools"))

from world_doc_prototype import (  # noqa: E402
    Advisor, capability, castle_doc, content_hash, diff, empty_world, reconcile,
)


class TestReconcilerIsPure(unittest.TestCase):
    """Part VIII.2: the reconciler is a pure function of (actual, desired).

    If it is not, nothing downstream can be trusted -- a plan that varies run to
    run cannot be reviewed, cached, or reproduced from a bug report.
    """

    def test_deterministic(self):
        desired = castle_doc()
        a = reconcile(desired, empty_world(desired["level"]))
        b = reconcile(desired, empty_world(desired["level"]))
        self.assertEqual(json.dumps(a, sort_keys=True), json.dumps(b, sort_keys=True))

    def test_does_not_mutate_inputs(self):
        desired = castle_doc()
        actual = empty_world(desired["level"])
        before = (copy.deepcopy(desired), copy.deepcopy(actual))
        reconcile(desired, actual)
        self.assertEqual(desired, before[0], "reconcile mutated the desired doc")
        self.assertEqual(actual, before[1], "reconcile mutated the actual doc")

    def test_revision_is_content_addressed(self):
        d1, d2 = castle_doc(), castle_doc()
        self.assertEqual(content_hash(d1), content_hash(d2))
        d2["terrain"]["max_altitude_m"] = 2400
        self.assertNotEqual(content_hash(d1), content_hash(d2),
                            "a real content change must move the revision")

    def test_revision_excludes_itself(self):
        d = castle_doc()
        h = content_hash(d)
        d["revision"] = "sha256:deadbeef"
        self.assertEqual(content_hash(d), h, "revision must not hash itself")


class TestRoundTripCount(unittest.TestCase):
    """VII.6.4 / Part VIII.3: N domains must cost ONE apply, not N calls.

    This is the whole economic claim. Cost is superlinear in call count
    (docs/WHY.md), so an implementation that quietly fans out defeats the model
    even when its output is correct.
    """

    def test_multi_domain_goal_is_one_plan(self):
        r = reconcile(castle_doc())
        self.assertEqual(r["round_trips"], 1)
        self.assertIn("operations", r["plan"])
        domains = {op["action"] for op in r["plan"]["operations"]}
        self.assertGreaterEqual(len(domains), 3,
                                "castle intent should span >=3 actions in one plan")

    def test_plan_conforms_to_batch_schema_shape(self):
        plan = reconcile(castle_doc())["plan"]
        self.assertEqual(set(plan) - {"transaction", "operations", "on_failure"}, set())
        self.assertIn(plan["on_failure"], ("stop", "continue_independent", "rollback_all"))
        self.assertIn(plan["transaction"]["compile_policy"],
                      ("per_operation", "at_boundaries", "at_end", "never"))
        for op in plan["operations"]:
            self.assertIn("id", op)
            self.assertIn("action", op)
            self.assertRegex(op["action"], r"^[a-z][a-z0-9_]*$")

    def test_depends_on_is_acyclic_and_backward_only(self):
        """A forward reference means the executor's topological sort has to fix
        our ordering. It should never have to."""
        ops = reconcile(castle_doc())["plan"]["operations"]
        seen = set()
        for op in ops:
            for dep in op.get("depends_on") or []:
                self.assertIn(dep, seen,
                              "%s depends on %s which is not emitted earlier"
                              % (op["id"], dep))
            seen.add(op["id"])

    def test_operation_ids_unique(self):
        ops = reconcile(castle_doc())["plan"]["operations"]
        ids = [o["id"] for o in ops]
        self.assertEqual(len(ids), len(set(ids)))


class TestIdempotency(unittest.TestCase):
    """ADR-0006: applying a document to a world that already matches is a no-op.

    Without this, "apply" is not desired-state -- it is a command replay, and
    re-running it duplicates the world.
    """

    def _terrain_only(self):
        d = empty_world("/Game/Maps/Idem")
        d["terrain"] = {"size_km": 2.0, "max_altitude_m": 900}
        d["revision"] = content_hash(d)
        return d

    def test_applying_to_matching_world_emits_nothing(self):
        d = self._terrain_only()
        r = reconcile(d, actual=d)
        self.assertEqual(r["plan"]["operations"], [])
        self.assertEqual(r["status"], "completed")

    def test_change_emits_update_not_recreate(self):
        before = self._terrain_only()
        after = copy.deepcopy(before)
        after["terrain"]["max_altitude_m"] = 1800
        after["revision"] = content_hash(after)
        changes = diff(before, after)
        self.assertEqual(len(changes), 1)
        self.assertEqual(changes[0]["op"], "update")

    def test_removal_emits_delete(self):
        before = empty_world("/Game/Maps/Del")
        before["water"] = [{"id": "river_1", "type": "river"}]
        after = copy.deepcopy(before)
        after["water"] = []
        changes = diff(before, after)
        self.assertEqual([c["op"] for c in changes], ["delete"])


class TestRefusalNotDowngrade(unittest.TestCase):
    """Part VIII.4 -- the defect that produced cubes-for-trees and lake-as-river.

    The rule under test: an advertised value the implementation cannot honour is
    REFUSED. A response that succeeds while producing something else is the
    single most expensive failure mode in this project, because nothing blocks
    and the shape is right.
    """

    def test_stub_capability_is_refused(self):
        d = empty_world("/Game/Maps/Water")
        d["water"] = [{"id": "l", "type": "lake"}]
        r = reconcile(d)
        paths = {x["path"] for x in r.get("refused", [])}
        self.assertIn("water[l]", paths)

    def test_refused_entity_produces_no_operation(self):
        """The actual anti-substitution assertion: refusing must not also
        quietly emit the downgraded operation."""
        d = empty_world("/Game/Maps/Water")
        d["water"] = [{"id": "l", "type": "lake"}]
        r = reconcile(d)
        self.assertEqual([op for op in r["plan"]["operations"]
                          if op["id"].startswith("water_")], [])

    def test_supported_capability_is_not_refused(self):
        d = empty_world("/Game/Maps/Water")
        d["terrain"] = {"size_km": 1.0}
        d["water"] = [{"id": "r", "type": "river"}]
        r = reconcile(d)
        self.assertEqual(r.get("refused", []), [])
        self.assertTrue(any(op["id"] == "water_r" for op in r["plan"]["operations"]))

    def test_refusal_carries_a_reason_and_a_source(self):
        r = reconcile(castle_doc())
        self.assertTrue(r["refused"])
        for item in r["refused"]:
            self.assertTrue(item["reason"].strip(), "refusal must say why")
            self.assertTrue(item["source"].strip(), "refusal must cite evidence")
            self.assertIn(item["status"], ("stub", "missing"))

    def test_status_degrades_when_anything_is_refused(self):
        r = reconcile(castle_doc())
        self.assertIn(r["status"], ("partially_completed", "blocked"))
        self.assertNotEqual(r["status"], "completed",
                            "refusals must never report as completed")


class TestNovelRequest(unittest.TestCase):
    """VII.6.1: an unanticipated request must produce the thing, or name what is
    missing. Silent success is a failure."""

    def test_unanticipated_kind_is_named_not_ignored(self):
        d = empty_world("/Game/Maps/Novel")
        d["terrain"] = {"size_km": 1.0}
        d["structures"] = [{"id": "zig", "kind": "ziggurat"}]
        r = reconcile(d)
        reported = ({x["path"] for x in r.get("refused", [])}
                    | {x["path"] for x in r.get("unmapped", [])})
        emitted = {op["id"] for op in r["plan"]["operations"]}
        self.assertTrue("structures[zig]" in reported or "structures_zig" in emitted,
                        "an unknown kind was neither built nor reported")

    def test_never_silently_completed_with_unmet_needs(self):
        d = empty_world("/Game/Maps/Novel")
        d["terrain"] = {"size_km": 1.0}
        d["foliage"] = [{"id": "f", "density_per_100m2": 5,
                         "needs": [{"role": "foliage.alien", "asset_type": "static_mesh"}]}]
        r = reconcile(d, generate_if_missing=False)
        self.assertNotEqual(r["status"], "completed")
        self.assertTrue(r["unresolved_dependencies"])


class TestDelegation(unittest.TestCase):
    """VI.5 / VII.3: generate_if_missing delegates; otherwise it declares."""

    ASSET_PREFIXES = ("tex_", "mat_", "mesh_", "fx_")

    def _asset_ids(self, ops):
        return [o["id"] for o in ops if o["id"].startswith(self.ASSET_PREFIXES)]

    def test_delegation_emits_generator_ops_first(self):
        r = reconcile(castle_doc(), generate_if_missing=True)
        ops = r["plan"]["operations"]
        asset_ops = [i for i, o in enumerate(ops)
                     if o["id"].startswith(self.ASSET_PREFIXES)]
        other_ops = [i for i, o in enumerate(ops)
                     if not o["id"].startswith(self.ASSET_PREFIXES)]
        self.assertTrue(asset_ops, "nothing was delegated")
        self.assertLess(max(asset_ops), min(other_ops),
                        "generated assets must be emitted before consumers")

    def test_declare_only_emits_no_generator_ops(self):
        r = reconcile(castle_doc(), generate_if_missing=False)
        self.assertFalse(self._asset_ids(r["plan"]["operations"]))
        self.assertTrue(r["unresolved_dependencies"])

    def test_consumers_depend_on_generated_assets(self):
        r = reconcile(castle_doc(), generate_if_missing=True)
        ops = {o["id"]: o for o in r["plan"]["operations"]}
        foliage = ops.get("foliage_conifers")
        self.assertIsNotNone(foliage)
        self.assertIn("mesh_foliage_conifer", foliage["depends_on"],
                      "foliage must depend on the mesh generated for it")


class TestCascadeToPrimitiveFloor(unittest.TestCase):
    """The user's correction: reporting "you need a mesh" is half an answer.

    From an empty project there is no mesh, no material for it, and no texture
    for that. A declared need must expand RECURSIVELY until every leaf is
    something the engine can make from parameters alone, and those leaves must
    be emitted FIRST. Otherwise the agent discovers the next missing rung one
    round trip at a time -- which is the cost model this project exists to fix.
    """

    def setUp(self):
        self.r = reconcile(castle_doc(), generate_if_missing=True)
        self.ops = {o["id"]: o for o in self.r["plan"]["operations"]}

    def test_cascade_reaches_the_texture_floor(self):
        floor = [o for o in self.ops.values() if o.get("_floor")]
        self.assertTrue(floor, "cascade never bottomed out")
        for op in floor:
            self.assertEqual(op["action"], "create_procedural_texture")
            self.assertEqual(op["depends_on"], [],
                             "a floor primitive must need nothing")

    def test_floor_ops_are_parametric_not_named(self):
        """The whole point: a floor spec must be parameters, never a preset
        name. A name would reintroduce the template-library dependency."""
        for op in self.ops.values():
            if not op.get("_floor"):
                continue
            self.assertIn("generate", op["specification"],
                          "floor op %s has no generator parameter" % op["id"])

    def test_full_chain_mesh_to_material_to_texture(self):
        mesh = self.ops["mesh_foliage_conifer"]
        self.assertEqual(mesh["action"], "submit_mesh_ops")
        mat_id = mesh["depends_on"][0]
        mat = self.ops[mat_id]
        self.assertEqual(mat["action"], "create_master_material")
        self.assertTrue(mat["depends_on"], "material must depend on textures")
        for tex_id in mat["depends_on"]:
            self.assertTrue(self.ops[tex_id].get("_floor"))

    def test_shared_role_generates_one_asset_not_two(self):
        """Both keep and curtain need structure.stone_wall."""
        mats = [o for o in self.ops.values() if o["id"] == "mat_structure_stone_wall"]
        self.assertEqual(len(mats), 1)

    def test_blocked_rungs_named_once_with_source(self):
        rungs = {r["action"]: r for r in self.r["blocked_rungs"]}
        self.assertIn("create_master_material", rungs)
        self.assertIn("submit_mesh_ops", rungs)
        for rung in rungs.values():
            self.assertTrue(rung["blocks_roles"])
            self.assertRegex(rung["source"], r"^\[(VERIFIED|UNVERIFIED|DOCS)")

    def test_unexposed_is_distinguished_from_missing(self):
        """A capability that exists but has no action is a toolset wrapper, not
        an implementation project. Collapsing the two misprices the work by an
        order of magnitude, so the reconciler must report them differently."""
        for rung in self.r["blocked_rungs"]:
            self.assertIn(rung["status"], ("unexposed", "missing", "stub"))
            self.assertTrue(rung["fix"].strip())
            if rung["status"] == "unexposed":
                self.assertIn("expose", rung["fix"])
                self.assertIn("VERIFIED", rung["source"],
                              "claiming a capability exists requires evidence")

    def test_blocked_rung_degrades_status(self):
        self.assertNotEqual(self.r["status"], "completed")

    def test_executable_flag_matches_blocked_rungs(self):
        blocked_actions = {r["action"] for r in self.r["blocked_rungs"]}
        for op in self.ops.values():
            if op["action"] in blocked_actions:
                self.assertFalse(op.get("_executable", True), op["id"])

    def test_dependencies_are_precise_not_rank_wide(self):
        """Terrain needs no generated asset. Depending on one would serialise
        work the executor could run in parallel and misreport the graph."""
        self.assertEqual(self.ops["terrain_terrain"]["depends_on"], [])
        foliage = self.ops["foliage_conifers"]["depends_on"]
        self.assertNotIn("mat_structure_stone_wall", foliage,
                         "foliage must not depend on the wall material")

    def test_structural_prerequisite_still_enforced(self):
        for section_op in ("foliage_conifers", "structures_curtain"):
            self.assertIn("terrain_terrain", self.ops[section_op]["depends_on"])

    def test_declared_asset_suppresses_delegation(self):
        d = castle_doc()
        d["assets"] = [{"id": "a1", "role": "foliage.conifer"},
                       {"id": "a2", "role": "structure.stone_wall"}]
        r = reconcile(d)
        self.assertNotIn("unresolved_dependencies", r)


class TestNoBareAssetPaths(unittest.TestCase):
    """VI.3 -- my own error, mechanised so it cannot come back.

    A dependency must be expressed as role + hint. The moment a bare
    /Game/... path is the only way to state a need, the caller is back to
    owning composition and cubes-for-trees returns.
    """

    def test_dependencies_carry_role_and_satisfied_by(self):
        for dep in reconcile(castle_doc())["unresolved_dependencies"]:
            self.assertIn("role", dep)
            self.assertIn("satisfied_by", dep)
            self.assertIn("domain", dep["satisfied_by"])
            self.assertIn("action", dep["satisfied_by"])
            self.assertIn("suggested_spec", dep)

    def test_no_engine_content_paths_anywhere_in_output(self):
        blob = json.dumps(reconcile(castle_doc()))
        for bad in ("/Engine/BasicShapes", "BasicShapes/Cube"):
            self.assertNotIn(bad, blob,
                             "engine placeholder leaked into the plan: %s" % bad)

    def test_dependency_states_why_it_is_unresolved(self):
        for dep in reconcile(castle_doc())["unresolved_dependencies"]:
            self.assertTrue(dep["reason"].strip())
            self.assertTrue(dep["required_by"].strip())


class TestAdvisory(unittest.TestCase):
    """Part VIII.6: escalating, never blocking."""

    def test_non_document_action_gets_nothing(self):
        self.assertIsNone(Advisor().observe("capture_effect_frames"))

    def test_first_direct_call_is_info(self):
        self.assertEqual(Advisor().observe("create_landscape")["severity"], "info")

    def test_severity_escalates_with_distinct_domains(self):
        adv = Advisor()
        sev = [adv.observe(a)["severity"] for a in
               ("create_landscape", "create_water_body", "place_structures",
                "scatter_foliage", "attach_weather")]
        self.assertEqual(sev[0], "info")
        self.assertEqual(sev[-1], "strong")
        rank = {"info": 0, "warn": 1, "strong": 2}
        self.assertEqual([rank[s] for s in sev], sorted(rank[s] for s in sev),
                         "severity must be monotonic")

    def test_advisory_states_the_saving(self):
        adv = Advisor()
        for a in ("create_landscape", "create_water_body", "place_structures"):
            r = adv.observe(a)
        self.assertEqual(r["would_have_been"], 2)
        self.assertGreater(r["observed_round_trips"], r["would_have_been"])

    def test_advisory_is_never_an_error(self):
        adv = Advisor()
        for a in ("create_landscape", "create_water_body", "place_structures",
                  "scatter_foliage", "attach_weather", "build_environment"):
            r = adv.observe(a)
            self.assertNotIn(r["severity"], ("error", "fatal"))

    def test_unrelated_action_resets_the_streak(self):
        adv = Advisor()
        adv.observe("create_landscape")
        adv.observe("capture_effect_frames")
        self.assertEqual(adv.observe("create_water_body")["severity"], "info")


class TestCapabilityMapHonesty(unittest.TestCase):
    """Part VIII.7. The map is DECLARED, not generated -- its weakest property.
    These tests bound the damage: every entry must be self-describing and
    sourced, so a stale entry is visible rather than silently authoritative."""

    def test_every_capability_is_sourced_and_statused(self):
        from world_doc_prototype import CAPABILITIES
        for key, cap in CAPABILITIES.items():
            self.assertIn(cap["status"], ("supported", "stub", "missing"), key)
            self.assertTrue(cap["note"].strip(), key)
            self.assertTrue(cap["source"].startswith("["), key)

    def test_unverified_claims_are_tagged_as_such(self):
        """AGENTS.md rule 1: an untagged API claim is an unverified one."""
        from world_doc_prototype import CAPABILITIES
        for key, cap in CAPABILITIES.items():
            self.assertRegex(cap["source"], r"^\[(VERIFIED|VERIFIED-RUNTIME|DOCS|UNVERIFIED)",
                             "capability %s carries an untagged source" % (key,))

    def test_unknown_section_falls_back_not_crashes(self):
        self.assertIsNone(capability("teleporters", {"kind": "x"}))


if __name__ == "__main__":
    unittest.main(verbosity=2)
