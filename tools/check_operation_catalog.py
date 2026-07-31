#!/usr/bin/env python
"""Validate operation_catalog.json against the committed registry snapshot.

The intent router enriches ResolveIntent from a hand-maintained catalog. When
catalog entries reference tools that are not in the live registry snapshot,
routing plans point agents at callables that do not exist.

    python tools/check_operation_catalog.py
"""
from __future__ import annotations

import json
import re
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CATALOG = os.path.join(
    ROOT, "Plugins", "UEREMCP", "Content", "IntentRouter", "operation_catalog.json"
)
SNAPSHOT = os.path.join(HERE, "registry_snapshot.json")


def load_snapshot_names() -> set[str]:
    with open(SNAPSHOT, encoding="utf-8") as fh:
        snap = json.load(fh)
    names: set[str] = set()
    for ts_name, ts in (snap.get("toolsets") or {}).items():
        names.add(ts_name)
        for tool_name, tool in (ts.get("tools") or {}).items():
            names.add("%s.%s" % (ts_name, tool_name))
            if tool.get("qualified_name"):
                names.add(tool["qualified_name"])
    return names


# Actions that legitimately have no execute_plan handler: they orchestrate or
# describe plans rather than being steps inside one.
PLAN_EXEMPT = {
    "get_started", "resolve_intent", "describe_operation",
    "execute_plan", "get_job_result", "cancel_job",
}


def registered_plan_actions() -> set[str]:
    """Actions bound via FUeremcpPlanExecutor::RegisterAction, read from source.

    Being AICallable and being usable inside execute_plan are TWO registries.
    A tool present in one and absent from the other fails only at plan time --
    "no handler registered for '<action>'" -- after the agent has already
    committed to a batch and has to abandon it. Measured in a live run: a
    correct texture -> material -> mesh -> scatter plan was thrown away and
    re-issued one call at a time, because submit_mesh_ops and
    create_procedural_texture were never plan-registered.
    """
    found: set[str] = set()
    src = os.path.join(ROOT, "Plugins", "UEREMCP", "Source")
    for dirpath, _dirs, files in os.walk(src):
        for name in files:
            if not name.endswith("PlanHandlers.cpp"):
                continue
            with open(os.path.join(dirpath, name), encoding="utf-8", errors="replace") as fh:
                body = fh.read()
            for m in re.finditer(r'Bind\(\s*TEXT\("([a-z0-9_]+)"\)', body):
                found.add(m.group(1))
            for m in re.finditer(r'RegisterAction\(\s*TEXT\("([a-z0-9_]+)"\)', body):
                found.add(m.group(1))
            for m in re.finditer(r'return\s+TEXT\("([a-z0-9_]+)"\);', body):
                found.add(m.group(1))
    return found


def main() -> int:
    if not os.path.exists(CATALOG):
        print("missing catalog: %s" % CATALOG)
        return 2
    if not os.path.exists(SNAPSHOT):
        print("missing snapshot: run python tools/dump_tool_registry.py")
        return 2

    with open(CATALOG, encoding="utf-8") as fh:
        catalog = json.load(fh)
    registry = load_snapshot_names()
    problems = 0

    operations = catalog.get("operations") or []
    seen_actions: set[str] = set()
    for entry in operations:
        action = entry.get("action")
        qualified = entry.get("qualified")
        if not qualified:
            print("CATALOG GAP: operation missing qualified: %r" % entry)
            problems += 1
            continue
        if action is None:
            if qualified not in registry:
                print("CATALOG UNKNOWN demoted tool: %s" % qualified)
                problems += 1
            continue
        if not action:
            print("CATALOG GAP: operation missing action for %s" % qualified)
            problems += 1
            continue
        if action in seen_actions:
            print("CATALOG DUPLICATE action: %s" % action)
            problems += 1
        seen_actions.add(action)
        if qualified not in registry:
            print("CATALOG UNKNOWN TOOL: %s (action=%s)" % (qualified, action))
            problems += 1

    for dep in catalog.get("dependencies") or []:
        action = dep.get("action")
        if action and action not in seen_actions:
            print("CATALOG ORPHAN dependency action: %s" % action)
            problems += 1
        for parent in dep.get("depends_on_actions") or []:
            if parent not in seen_actions:
                print("CATALOG UNKNOWN depends_on_actions: %s (from %s)" % (parent, action))
                problems += 1

    # Every mutating action the router can put in a batch must be executable
    # inside that batch.
    plan_actions = registered_plan_actions()
    for op in operations:
        action = op.get("action")
        if not action or action in PLAN_EXEMPT or "." in action:
            continue
        if not op.get("destructive"):
            continue
        if action not in plan_actions:
            print("NO PLAN HANDLER %s: routable and destructive, but no "
                  "FUeremcpPlanExecutor::RegisterAction binding found in any "
                  "*PlanHandlers.cpp. execute_plan will reject a batch "
                  "containing it." % action)
            problems += 1

    print("checked %d catalog operation(s) against registry snapshot; "
          "%d plan-registered action(s)" % (len(operations), len(plan_actions)))
    print("%d problem(s)" % problems)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
