#!/usr/bin/env python
"""Guards for the two defects a live field test exposed.

Both are the same shape: something was true in one registry and false in
another, and nothing checked that they agreed. Neither shows up in a code
review, and neither fails until an agent is halfway through a task.

  1. PLAN COVERAGE. Being AICallable and being usable inside execute_plan are
     separate registries. An agent built a correct texture -> material -> mesh
     -> scatter plan, got "no handler registered for 'submit_mesh_ops'", threw
     the plan away and issued the calls one at a time.

  2. PARTIAL APPLICATION. Asked to thin foliage, shorten it, and add snow above
     the treeline, an agent could do the first two and not the third, so it did
     NOTHING -- all-or-nothing was the only contract available. Honest, and
     still useless. options.on_unsupported=partial is the fix.

Run:  python tests/world_doc/test_plan_coverage.py
"""
from __future__ import annotations

import io
import json
import os
import re
import sys
import unittest

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(REPO, "tools"))

from check_operation_catalog import registered_plan_actions, PLAN_EXEMPT  # noqa: E402

SOURCE = os.path.join(REPO, "Plugins", "UEREMCP", "Source")
CATALOG = os.path.join(REPO, "tools", "intent_router", "operation_catalog.json")
ENVELOPE_H = os.path.join(SOURCE, "UeremcpProtocol", "Public", "UeremcpEnvelope.h")
ENVELOPE_CPP = os.path.join(SOURCE, "UeremcpProtocol", "Private", "UeremcpEnvelope.cpp")
REQUEST_SCHEMA = os.path.join(REPO, "schemas", "envelope", "request.schema.json")


def read(path):
    with io.open(path, encoding="utf-8", errors="replace") as fh:
        return fh.read()


def catalog():
    with io.open(CATALOG, encoding="utf-8") as fh:
        return json.load(fh)


class TestPlanHandlerCoverage(unittest.TestCase):
    """Every action the router can put in a batch must run inside that batch."""

    @classmethod
    def setUpClass(cls):
        cls.registered = registered_plan_actions()
        cls.catalog = catalog()

    def test_every_destructive_routable_action_is_plan_registered(self):
        missing = []
        for op in self.catalog["operations"]:
            action = op.get("action")
            if not action or action in PLAN_EXEMPT or "." in action:
                continue
            if not op.get("destructive"):
                continue
            if action not in self.registered:
                missing.append(action)
        self.assertEqual(missing, [],
                         "routable + destructive but not plan-registered: %s" % missing)

    def test_submit_graph_is_registered(self):
        """Named explicitly: the graph round-trip the document model is built
        on. If it cannot appear in a batch, any plan that authors an asset and
        then wires logic for it has to split in two."""
        self.assertIn("submit_graph", self.registered)

    def test_promote_to_template_is_registered(self):
        """What banks a result the agent just proved. Without a plan binding it
        can never be the final step of the plan that produced it."""
        self.assertIn("promote_to_template", self.registered)

    def test_primitive_floor_actions_are_registered(self):
        for action in ("create_procedural_texture", "create_master_material",
                       "submit_mesh_ops"):
            self.assertIn(action, self.registered,
                          "%s is a from-scratch floor op; a cascade that cannot "
                          "batch defeats the point of having one" % action)

    def test_every_dependency_edge_target_is_executable(self):
        """A depends_on chain is only useful if every link can run in the plan."""
        for edge in self.catalog["dependencies"]:
            action = edge.get("action")
            if action not in self.registered:
                continue
            for parent in edge.get("depends_on_actions") or []:
                if parent in PLAN_EXEMPT:
                    continue
                op = next((o for o in self.catalog["operations"]
                           if o.get("action") == parent), None)
                if op and op.get("destructive"):
                    self.assertIn(parent, self.registered,
                                  "%s depends on %s, which cannot run in a plan"
                                  % (action, parent))

    def test_no_handler_registered_twice(self):
        """Two modules binding the same action is a silent last-writer-wins."""
        seen, dupes = set(), []
        for dirpath, _dirs, files in os.walk(SOURCE):
            for name in files:
                if not name.endswith("PlanHandlers.cpp"):
                    continue
                body = read(os.path.join(dirpath, name))
                for m in re.finditer(r'Bind\(\s*TEXT\("([a-z0-9_]+)"\)', body):
                    action = m.group(1)
                    if action in seen:
                        dupes.append((action, name))
                    seen.add(action)
        self.assertEqual(dupes, [], "action bound in more than one module: %s" % dupes)

    def test_registered_actions_have_matching_unregister(self):
        """A Register without an Unregister leaks a dangling handler across a
        module reload, which then dispatches into freed code."""
        for dirpath, _dirs, files in os.walk(SOURCE):
            for name in files:
                if not name.endswith("PlanHandlers.cpp"):
                    continue
                body = read(os.path.join(dirpath, name))
                bound = set(re.findall(r'Bind\(\s*TEXT\("([a-z0-9_]+)"\)', body))
                if not bound:
                    continue
                # Two valid shapes: a literal UnregisterAction per action, or a
                # loop over a shared name list. Accept both; require one.
                unbound = set(re.findall(
                    r'UnregisterAction\(\s*TEXT\("([a-z0-9_]+)"\)', body))
                if re.search(r'UnregisterAction\(\s*(?!TEXT)\w+\s*\)', body):
                    unbound |= set(re.findall(r'TEXT\("([a-z0-9_]+)"\)', body))
                self.assertTrue(unbound, "%s never unregisters anything" % name)
                self.assertEqual(bound - unbound, set(),
                                 "%s registers without unregistering: %s"
                                 % (name, bound - unbound))


class TestOnUnsupportedOption(unittest.TestCase):
    """options.on_unsupported = fail | partial."""

    def test_declared_in_request_struct(self):
        self.assertIn("OnUnsupported", read(ENVELOPE_H))

    def test_defaults_to_fail(self):
        """A caller who asked for three things and silently got two is worse off
        than one who got none and was told why. Partial must be opt-in."""
        body = read(ENVELOPE_H)
        m = re.search(r'FString OnUnsupported\s*=\s*TEXT\("(\w+)"\)', body)
        self.assertIsNotNone(m, "OnUnsupported has no default")
        self.assertEqual(m.group(1), "fail")

    def test_parsed_and_allowlisted(self):
        body = read(ENVELOPE_CPP)
        self.assertIn('TEXT("on_unsupported")', body)
        # Present in the options allowlist, or an otherwise valid request is
        # rejected as "unknown options field".
        allow = re.search(r'OptAllowed\s*=\s*\{(.+?)\};', body, re.S)
        self.assertIsNotNone(allow)
        self.assertIn("on_unsupported", allow.group(1))

    def test_invalid_value_is_rejected_not_coerced(self):
        body = read(ENVELOPE_CPP)
        self.assertIn("invalid on_unsupported", body,
                      "an unrecognised value must be rejected, not silently "
                      "treated as fail -- the caller believes partial is active")

    def test_schema_declares_the_enum(self):
        with io.open(REQUEST_SCHEMA, encoding="utf-8") as fh:
            schema = json.load(fh)
        opt = schema["properties"]["options"]["properties"]["on_unsupported"]
        self.assertEqual(sorted(opt["enum"]), ["fail", "partial"])
        self.assertEqual(opt["default"], "fail")

    def test_schema_and_cpp_agree(self):
        """options has additionalProperties:false, so a field the C++ accepts
        and the schema omits is rejected before it ever reaches the C++."""
        with io.open(REQUEST_SCHEMA, encoding="utf-8") as fh:
            schema = json.load(fh)
        opts = schema["properties"]["options"]
        if opts.get("additionalProperties") is False:
            declared = set(opts.get("properties") or {})
            allow = re.search(r'OptAllowed\s*=\s*\{(.+?)\};', read(ENVELOPE_CPP), re.S)
            accepted = set(re.findall(r'TEXT\("(\w+)"\)', allow.group(1)))
            self.assertEqual(accepted - declared, set(),
                             "C++ accepts options the schema forbids: %s"
                             % (accepted - declared))

    def test_agents_are_told_it_exists(self):
        """An option nothing advertises is an option nothing uses. The router's
        envelope contract is on every routed response."""
        router = read(os.path.join(REPO, "tools", "intent_router", "router.py"))
        m = re.search(r'ENVELOPE_HINT = \((.+?)\n\)', router, re.S)
        self.assertIsNotNone(m)
        self.assertIn("on_unsupported", m.group(1))


class TestNextActionsChain(unittest.TestCase):
    """Serve the next step WITH the result.

    An agent finishing a call had two options: spend a whole round trip
    re-asking the router something the server already knew, or guess. Cost is
    superlinear in call count, so that round trip is among the most expensive
    things the protocol can spend one on.

    The catalog declares the forward chain; the envelope serves it with this
    response's primary_asset already substituted, so the suggested request is
    ready to send rather than something to fill in.
    """

    @classmethod
    def setUpClass(cls):
        cls.catalog = catalog()
        cls.by_action = {o.get("action"): o for o in cls.catalog["operations"]}

    def test_authoring_actions_declare_a_next_step(self):
        for action in ("create_procedural_texture", "create_master_material",
                       "submit_mesh_ops", "create_landscape", "create_niagara_effect"):
            op = self.by_action.get(action)
            self.assertIsNotNone(op, action)
            self.assertTrue(op.get("next_actions"),
                            "%s leaves the agent with nothing to do next" % action)

    def test_next_actions_name_real_catalog_actions(self):
        known = set(self.by_action)
        for action, op in self.by_action.items():
            for nxt in op.get("next_actions") or []:
                self.assertIn(nxt["action"], known,
                              "%s suggests %s, which is not a catalog action"
                              % (action, nxt["action"]))

    def test_each_suggestion_states_why(self):
        for op in self.by_action.values():
            for nxt in op.get("next_actions") or []:
                self.assertTrue((nxt.get("why") or "").strip(),
                                "%s -> %s has no reason" % (op["action"], nxt["action"]))

    def test_mesh_chain_carries_the_asset_forward(self):
        """The substitution is the whole value: a suggestion the agent must
        still fill in is a suggestion it has to think about."""
        op = self.by_action["submit_mesh_ops"]
        scatter = next(n for n in op["next_actions"] if n["action"] == "scatter_foliage")
        self.assertEqual(scatter["specification_hint"]["biome"]["mesh_path"],
                         "<primary_asset>")

    def test_chain_terminates(self):
        """A cycle would suggest work forever."""
        for start in self.by_action:
            seen, cur = set(), start
            while cur and cur not in seen:
                seen.add(cur)
                nxt = (self.by_action.get(cur) or {}).get("next_actions") or []
                cur = nxt[0]["action"] if nxt else None
            self.assertIsNone(cur, "next_actions cycle reachable from %s" % start)

    def test_authoring_chains_reach_a_verification_step(self):
        """Instance counts do not prove something reads as foliage. Every
        authoring chain should end at looking at the result."""
        for start in ("submit_mesh_ops", "create_landscape", "create_niagara_effect"):
            seen, cur, reached = set(), start, False
            while cur and cur not in seen:
                seen.add(cur)
                if cur.startswith(("capture_", "validate_")):
                    reached = True
                    break
                nxt = (self.by_action.get(cur) or {}).get("next_actions") or []
                cur = nxt[0]["action"] if nxt else None
            self.assertTrue(reached, "%s chain never reaches verification" % start)


class TestNextActionsWiring(unittest.TestCase):
    """The C++ half: declared in the schema, populated centrally, and suppressed
    on failure."""

    def test_schema_declares_next_actions(self):
        with io.open(os.path.join(REPO, "schemas", "envelope",
                                  "response.schema.json"), encoding="utf-8") as fh:
            schema = json.load(fh)
        item = schema["properties"]["next_actions"]["items"]
        self.assertEqual(sorted(item["required"]),
                         ["action", "relation", "request_json", "why"])
        self.assertEqual(sorted(item["properties"]["relation"]["enum"]),
                         ["consumed_by", "consumes", "implied"])

    def test_all_three_directions_are_emitted(self):
        """A single forward edge only helps an agent already going the right
        way. Up covers "I have a mesh, what wants a mesh?"; down covers "I have
        the material and still need its texture"."""
        body = read(os.path.join(SOURCE, "UeremcpCore", "Private",
                                 "UeremcpNextActions.cpp"))
        for relation in ("implied", "consumed_by", "consumes"):
            self.assertIn('TEXT("%s")' % relation, body)

    def test_up_and_down_are_derived_not_hand_written(self):
        """A separate "what comes after" table would drift from the dependency
        graph, and the drift would be invisible until an agent followed a stale
        edge."""
        body = read(os.path.join(SOURCE, "UeremcpCore", "Private",
                                 "UeremcpNextActions.cpp"))
        self.assertIn("depends_on_actions", body)
        self.assertIn("ConsumedBy", body)

    def test_suggestions_are_deduplicated_and_capped(self):
        """An action reachable two ways is offered once; a menu longer than a
        handful stops being guidance and becomes a listing."""
        body = read(os.path.join(SOURCE, "UeremcpCore", "Private",
                                 "UeremcpNextActions.cpp"))
        self.assertIn("Emitted", body)
        self.assertIn("SetNum(5)", body)

    def test_pairs_are_bidirectional_in_the_graph(self):
        """material offers texture, texture offers material -- for free, from
        one edge, because up and down are inverses."""
        edges = {d["action"]: set(d.get("depends_on_actions") or [])
                 for d in catalog()["dependencies"]}
        self.assertIn("create_procedural_texture", edges.get("create_master_material", set()))
        inverted = set()
        for action, parents in edges.items():
            if "create_procedural_texture" in parents:
                inverted.add(action)
        self.assertIn("create_master_material", inverted)

    def test_envelope_populates_centrally(self):
        """Asking every domain to know the whole graph is how the graph goes
        stale in nine places at once."""
        body = read(ENVELOPE_CPP)
        self.assertIn("next_actions", body)
        self.assertIn("NextActionsProvider", body)

    def test_suppressed_on_terminal_failure(self):
        """'You succeeded, now do X' is actively misleading when nothing was
        produced."""
        body = read(os.path.join(SOURCE, "UeremcpCore", "Private",
                                 "UeremcpNextActions.cpp"))
        self.assertIn("TerminalFailures", body)
        for status in ("rejected", "failed_validation", "rolled_back", "error"):
            self.assertIn('TEXT("%s")' % status, body)

    def test_provider_is_unregistered_on_shutdown(self):
        """A stale static delegate dispatches into freed code after a reload."""
        body = read(os.path.join(SOURCE, "UeremcpCore", "Private",
                                 "UeremcpCoreModule.cpp"))
        self.assertIn("SetNextActionsProvider", body)
        self.assertIn("ClearNextActionsProvider", body)

    def test_reads_the_shipped_catalog_not_the_tools_copy(self):
        body = read(os.path.join(SOURCE, "UeremcpCore", "Private",
                                 "UeremcpNextActions.cpp"))
        self.assertIn("Content/IntentRouter/operation_catalog.json", body)

if __name__ == "__main__":
    unittest.main(verbosity=2)
