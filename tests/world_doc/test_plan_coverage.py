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


if __name__ == "__main__":
    unittest.main(verbosity=2)
