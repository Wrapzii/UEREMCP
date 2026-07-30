#!/usr/bin/env python
"""Fail the build when docs name a tool that does not exist.

An agent that trusts a tool name from our prose and gets "Unknown tool" burns a
call and then starts guessing. That is exactly the failure AGENTS.md rule 1
exists to prevent -- except the untrustworthy claim is ours, not the engine's.

Measured: I did this myself. Our docs referenced `inspect_system`; the registry
has `InspectSystem`. Near-miss detection below is aimed squarely at that class,
because the surface mixes two naming conventions -- UEREMCP uses PascalCase,
Epic's Python toolsets use snake_case -- so wrong guesses are structural, not
careless.

    python tools/dump_tool_registry.py    # refresh ground truth (needs editor)
    python tools/check_tool_names.py
"""
from __future__ import annotations

import difflib
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SNAPSHOT = os.path.join(HERE, "registry_snapshot.json")
SCAN_DIRS = ["docs"]

# Prose that legitimately mentions a name-like token which is not a tool.
IGNORE = {
    "create_niagara_effect", "capture_effect_frames", "execute_plan",
    "inspect_system", "create_vfx_material", "instantiate_template",
    "step_effect", "read_graph_dsl", "write_graph_dsl", "read_anim_bp",
    "inspect_montage", "create_material",
}


def load():
    if not os.path.exists(SNAPSHOT):
        print("no snapshot: run python tools/dump_tool_registry.py (needs a running editor)")
        sys.exit(2)
    with open(SNAPSHOT, encoding="utf-8") as fh:
        snap = json.load(fh)
    toolsets = snap.get("toolsets", {})
    qualified, short = set(), set()
    for ts_name, ts in toolsets.items():
        qualified.add(ts_name)
        for tool_name, tool in (ts.get("tools") or {}).items():
            short.add(tool_name)
            qualified.add("%s.%s" % (ts_name, tool_name))
            if tool.get("qualified_name"):
                qualified.add(tool["qualified_name"])
    return snap, qualified, short


def main() -> int:
    snap, qualified, short = load()
    toolset_prefixes = tuple(sorted(snap["toolsets"].keys()))

    # Backticked dotted identifiers, e.g. `UeremcpNiagara.UeremcpNiagaraToolset.Foo`
    dotted = re.compile(r"`([A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+){1,4})`")

    problems, checked = 0, 0
    for scan in SCAN_DIRS:
        base = os.path.join(ROOT, scan)
        for dirpath, _dirs, files in os.walk(base):
            for fn in files:
                if not fn.endswith(".md"):
                    continue
                path = os.path.join(dirpath, fn)
                rel = os.path.relpath(path, ROOT).replace("\\", "/")
                with open(path, encoding="utf-8", errors="replace") as fh:
                    for lineno, line in enumerate(fh, 1):
                        for token in dotted.findall(line):
                            # Only judge tokens that claim to be in a real toolset.
                            if not token.startswith(toolset_prefixes):
                                continue
                            checked += 1
                            if token in qualified:
                                continue
                            leaf = token.split(".")[-1]
                            if leaf in IGNORE:
                                continue
                            # Design docs legitimately name tools that do not
                            # exist yet. Say so on the line and it is a proposal,
                            # not a claim. Without this the checker punishes
                            # exactly the documents that should be written first.
                            # NB: no trailing \b -- "e.g." ends in a period, and
                            # a period followed by a space is not a word boundary.
                            if re.search(r"\b(proposed|propose|would be|future|not built|e\.g\.)",
                                         line, re.I):
                                continue
                            near = difflib.get_close_matches(leaf, short, n=2, cutoff=0.75)
                            hint = ("  did you mean: %s" % ", ".join(near)) if near else ""
                            print("%s:%d  UNKNOWN TOOL  %s%s" % (rel, lineno, token, hint))
                            problems += 1

    print("\nchecked %d qualified tool reference(s) against %d tools in %d toolsets"
          % (checked, snap["tool_count"], snap["toolset_count"]))
    print("%d problem(s)" % problems)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
