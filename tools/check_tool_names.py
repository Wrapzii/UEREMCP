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
import hashlib
import json
import os
import re
import sys
from pathlib import Path

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SNAPSHOT = os.path.join(HERE, "registry_snapshot.json")
SCAN_DIRS = ["docs"]
SOURCE_ROOT = os.path.join(ROOT, "Plugins", "UEREMCP", "Source")

# Prose that legitimately mentions a name-like token which is not a tool.
IGNORE = {
    "create_niagara_effect", "capture_effect_frames", "execute_plan",
    "inspect_system", "create_vfx_material", "instantiate_template",
    "step_effect", "read_graph_dsl", "write_graph_dsl", "read_anim_bp",
    "inspect_montage", "create_material",
}


def discover_source_tools() -> dict[str, dict[str, str]]:
    """Return qualified AICallable names and their source descriptions.

    This is not a replacement for the live registry. It is the expected surface
    used to fail closed when a committed snapshot predates source declarations.
    """
    tools: dict[str, dict[str, str]] = {}
    for header in Path(SOURCE_ROOT).glob("*/Public/*Toolset.h"):
        text = header.read_text(encoding="utf-8", errors="replace")
        module = header.parents[1].name
        toolset = header.stem
        pattern = re.compile(
            r"/\*\*(?P<doc>.*?)\*/\s*"
            r"UFUNCTION\s*\(\s*meta\s*=\s*\(\s*AICallable\s*\).*?\)\s*"
            r"static\s+FString\s+(?P<name>[A-Za-z][A-Za-z0-9_]*)\s*\(",
            re.S,
        )
        for match in pattern.finditer(text):
            qualified = "%s.%s.%s" % (module, toolset, match.group("name"))
            tools[qualified] = {
                "description": match.group("doc"),
                "header": str(header.relative_to(ROOT)).replace("\\", "/"),
            }
    return tools


def source_surface_fingerprint(source_tools: dict[str, dict[str, str]] | None = None) -> str:
    source_tools = source_tools or discover_source_tools()
    blob = "\n".join(sorted(source_tools)).encode("utf-8")
    return hashlib.sha256(blob).hexdigest()


def snapshot_names(snap: dict) -> tuple[set[str], set[str]]:
    qualified, short = set(), set()
    for ts_name, ts in (snap.get("toolsets") or {}).items():
        qualified.add(ts_name)
        for tool_name, tool in (ts.get("tools") or {}).items():
            short.add(tool_name)
            qualified.add("%s.%s" % (ts_name, tool_name))
            if tool.get("qualified_name"):
                qualified.add(tool["qualified_name"])
    return qualified, short


def load():
    if not os.path.exists(SNAPSHOT):
        print("no snapshot: run python tools/dump_tool_registry.py (needs a running editor)")
        sys.exit(2)
    with open(SNAPSHOT, encoding="utf-8") as fh:
        snap = json.load(fh)
    qualified, short = snapshot_names(snap)
    return snap, qualified, short


def check_snapshot_fresh(snap: dict, source_tools: dict[str, dict[str, str]] | None = None) -> int:
    """Fail when source declares UEREMCP callables absent from live ground truth."""
    source_tools = source_tools or discover_source_tools()
    qualified, _ = snapshot_names(snap)
    live_tools = {name for name in qualified if name.count(".") >= 2}
    missing = sorted(set(source_tools) - live_tools)
    problems = 0
    for name in missing:
        print("STALE SNAPSHOT missing source callable: %s" % name)
        problems += 1

    ueremcp_snapshot = sum(
        len(ts.get("tools") or {})
        for ts_name, ts in (snap.get("toolsets") or {}).items()
        if ts_name.startswith("Ueremcp")
    )
    source_count = len(source_tools)
    if ueremcp_snapshot < source_count:
        print("STALE SNAPSHOT ueremcp tool count %d < source callable count %d"
              % (ueremcp_snapshot, source_count))
        problems += 1

    recorded = snap.get("source_surface_fingerprint")
    current = source_surface_fingerprint(source_tools)
    if not recorded:
        print("STALE SNAPSHOT missing source_surface_fingerprint; refresh with dump_tool_registry.py")
        problems += 1
    elif recorded != current:
        print("STALE SNAPSHOT source fingerprint mismatch: recorded=%s current=%s"
              % (recorded[:12], current[:12]))
        problems += 1
    return problems


def check_source_descriptions(source_tools: dict[str, dict[str, str]] | None = None) -> int:
    """Enforce useful describe_toolset text for every UEREMCP callable."""
    source_tools = source_tools or discover_source_tools()
    problems = 0
    for qualified, info in sorted(source_tools.items()):
        doc = re.sub(r"^\s*\*\s?", "", info["description"], flags=re.M).strip()
        leaf = qualified.rsplit(".", 1)[-1]
        requirements = {
            "task vocabulary ('Use when:')": "use when:" in doc.lower(),
            "input contract ('Inputs:')": "inputs:" in doc.lower(),
            "worked request ('Example:')": "example:" in doc.lower(),
        }
        if leaf == "Ping":
            requirements["input contract ('Inputs:')"] = (
                "inputs:" in doc.lower() or "no arguments" in doc.lower()
            )
        for label, ok in requirements.items():
            if not ok:
                print("DESCRIPTION GAP %s: missing %s (%s)"
                      % (qualified, label, info["header"]))
                problems += 1

        if leaf != "Ping":
            example_match = re.search(r"Example:\s*(\{[^\r\n]*\})", doc)
            if example_match:
                try:
                    example = json.loads(example_match.group(1))
                except json.JSONDecodeError as exc:
                    print("DESCRIPTION GAP %s: example is not valid JSON: %s (%s)"
                          % (qualified, exc, info["header"]))
                    problems += 1
                else:
                    if not all(key in example for key in ("protocol_version", "action", "specification")):
                        print("DESCRIPTION GAP %s: example must be a complete envelope with "
                              "protocol_version/action/specification (%s)"
                              % (qualified, info["header"]))
                        problems += 1

        if leaf != "Ping":
            has_spec_contract = (
                "specification." in doc
                or "specification required keys:" in doc.lower()
                or "specification has no required keys" in doc.lower()
                or "specification optional" in doc.lower()
            )
            if not has_spec_contract:
                print("DESCRIPTION GAP %s: required specification keys are not explicit (%s)"
                      % (qualified, info["header"]))
                problems += 1
    return problems


def unknown_tool_references(
    text: str,
    snap: dict,
    additional_known: set[str] | None = None,
) -> list[tuple[int, str, list[str]]]:
    """Return unqualified claims as (line, token, near matches)."""
    qualified, short = snapshot_names(snap)
    qualified.update(additional_known or set())
    toolset_prefixes = tuple(sorted((snap.get("toolsets") or {}).keys()))
    dotted = re.compile(r"`([A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+){1,4})`")
    found: list[tuple[int, str, list[str]]] = []
    for lineno, line in enumerate(text.splitlines(), 1):
        for token in dotted.findall(line):
            if not token.startswith(toolset_prefixes) or token in qualified:
                continue
            leaf = token.split(".")[-1]
            if leaf in IGNORE:
                continue
            if re.search(r"\b(proposed|propose|would be|future|not built|e\.g\.)",
                         line, re.I):
                continue
            near = difflib.get_close_matches(leaf, short, n=2, cutoff=0.75)
            found.append((lineno, token, near))
    return found


def main() -> int:
    snap, qualified, short = load()
    toolset_prefixes = tuple(sorted(snap["toolsets"].keys()))
    source_tools = discover_source_tools()
    problems = check_snapshot_fresh(snap, source_tools)
    description_problems = check_source_descriptions(source_tools)
    problems += description_problems
    print("%d description problem(s)" % description_problems)

    checked = 0
    for scan in SCAN_DIRS:
        base = os.path.join(ROOT, scan)
        for dirpath, _dirs, files in os.walk(base):
            for fn in files:
                if not fn.endswith(".md"):
                    continue
                path = os.path.join(dirpath, fn)
                rel = os.path.relpath(path, ROOT).replace("\\", "/")
                with open(path, encoding="utf-8", errors="replace") as fh:
                    text = fh.read()
                checked += sum(
                    1 for token in re.findall(
                        r"`([A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z0-9_]+){1,4})`", text
                    )
                    if token.startswith(toolset_prefixes)
                )
                for lineno, token, near in unknown_tool_references(
                    text, snap, set(source_tools)
                ):
                    hint = ("  did you mean: %s" % ", ".join(near)) if near else ""
                    print("%s:%d  UNKNOWN TOOL  %s%s" % (rel, lineno, token, hint))
                    problems += 1

    print("\nchecked %d qualified tool reference(s) against %d tools in %d toolsets"
          % (checked, snap["tool_count"], snap["toolset_count"]))
    print("%d problem(s)" % problems)

    # Domain fiction check (BACKLOG 2.5 / 0.3): template schema domains must not
    # advertise capabilities that have neither a live toolset prefix nor an
    # explicit provisional allowlist.
    domain_problems = check_domains(snap)
    problems += domain_problems
    print("%d domain problem(s)" % domain_problems)
    return 1 if problems else 0


# Map schema domain → expected UEREMCP toolset name substring.
DOMAIN_TOOLSET_HINTS = {
    "niagara": "UeremcpNiagara",
    "materials": "UeremcpMaterial",
    "blueprints": "UeremcpBlueprint",
    "gameplay": "UeremcpGameplay",
    "animation": "UeremcpAnimation",
    "validation": "UeremcpValidation",
    "environment": "UeremcpEnvironment",
    "templates": "UeremcpTemplates",
}

FORBIDDEN_FICTION = {"world", "level_design", "pcg", "behavior", "ai", "ui",
                     "data_assets", "import_export", "project", "source_control",
                     "sequencer", "audio", "networking", "world_partition",
                     "testing", "assets", "gameplay_abilities", "control_rig"}


def check_domains(snap) -> int:
    import pathlib
    schema_path = pathlib.Path(ROOT) / "schemas" / "template-library" / "template.schema.json"
    if not schema_path.exists():
        print("domain check: template.schema.json missing")
        return 1
    schema = json.loads(schema_path.read_text(encoding="utf-8"))
    try:
        domains = schema["properties"]["domain"]["enum"]
    except KeyError:
        print("domain check: cannot find properties.domain.enum")
        return 1

    toolsets = set(snap.get("toolsets", {}).keys())
    problems = 0
    if len(domains) != len(set(domains)):
        print("DOMAIN ENUM contains duplicates")
        problems += 1
    for d in domains:
        if d in FORBIDDEN_FICTION:
            print("FICTIONAL DOMAIN still advertised: %s" % d)
            problems += 1
            continue
        hint = DOMAIN_TOOLSET_HINTS.get(d)
        if hint and any(hint in ts for ts in toolsets):
            continue
        if hint:
            print("DOMAIN WITHOUT LIVE TOOLSET: %s (expected ~%s); refresh snapshot after deploy"
                  % (d, hint))
            problems += 1
            continue
        print("DOMAIN UNMAPPED: %s — add a registered toolset mapping or remove it" % d)
        problems += 1
    return problems


if __name__ == "__main__":
    sys.exit(main())
