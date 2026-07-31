#!/usr/bin/env python
"""Validate operation_catalog.json against the committed registry snapshot.

The intent router enriches ResolveIntent from a hand-maintained catalog. When
catalog entries reference tools that are not in the live registry snapshot,
routing plans point agents at callables that do not exist.

    python tools/check_operation_catalog.py
"""
from __future__ import annotations

import json
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

    print("checked %d catalog operation(s) against registry snapshot" % len(operations))
    print("%d problem(s)" % problems)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
