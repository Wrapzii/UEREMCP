#!/usr/bin/env python
"""Generate UEREMCP focus-mode config: hide the primitives UEREMCP supersedes.

WHY THIS EXISTS
---------------
Agents have Epic's raw toolset API in their training data. Given the choice they
fall back to it -- issuing dozens of primitive calls instead of one goal-level
call -- and then lose track of what they were doing. Documentation does not fix
this: the model has already read worse documentation than ours, and prose is not
binding.

Configuration is binding. Epic's own UToolsetRegistrySettings exposes
BlockedNames; a blocked toolset is "treated as non-existent". Fallback stops
being a discipline problem and becomes impossible.
[VERIFIED: $TR/Public/ToolsetRegistry/ToolsetRegistrySubsystem.h:22-39]
[VERIFIED: $TR/Public/ToolsetRegistry/ToolsetRegistry.h:39-46 AddBlockedName]

SAFETY
------
Blocking affects execution, not just listing. This is only safe because UEREMCP's
domain modules do not call Epic toolsets at runtime -- they use
UNiagaraExternalEditUtilities and friends directly. Verified by grepping
UeremcpNiagara/Private and UeremcpMaterial/Private for ExecuteTool /
execute_tool_script: no hits. **Re-check that before adding to SUPERSEDED.**

Only block where UEREMCP genuinely has a goal-level replacement. Blocking a
capability we do not cover removes it from the agent with no substitute --
strictly worse than the fallback problem it solves.

    python tools/gen_focus_config.py            # print the ini stanza
    python tools/gen_focus_config.py --write    # gated refusal: global hides require explicit design approval
    python tools/gen_focus_config.py --check    # verify against registry snapshot
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

from check_tool_names import (
    check_domains,
    check_snapshot_fresh,
    check_source_descriptions,
    discover_source_tools,
)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SNAPSHOT = os.path.join(HERE, "registry_snapshot.json")
# Filename is load-bearing. UToolsetRegistrySettings is
# config=EditorPerProjectUserSettings, and UE merges a plugin's
# Config/Default<Category>.ini into that category's hierarchy. Name it anything
# else and it is simply never read -- which fails silently and looks like the
# block list not working.
OUT_INI = os.path.join(ROOT, "Plugins", "UEREMCP", "Config",
                       "DefaultEditorPerProjectUserSettings.ini")

# Epic/RE toolset name patterns UEREMCP supersedes, and the tool that replaces
# them. Case-insensitive substring match unless wrapped in forward slashes.
#
# The replacement column is not decoration: if it is empty, the entry does not
# belong here.
# (pattern, replacement, exact toolsets this pattern is INTENDED to hide)
#
# The third column is enforced by --check. Bare substrings are greedy and
# over-block silently: "editor_toolset.toolsets.material" also swallows
# MaterialInstanceTools, and "NiagaraToolsets." swallows all five Niagara
# toolsets including the two nothing replaces. Removing a capability with no
# substitute is worse than the fallback problem this exists to solve, so a
# pattern that matches anything not listed here is a build failure.
SUPERSEDED = [
    # Authoring primitives only. NiagaraToolset_System is also the toolset whose
    # GetSystemSummary is the standing editor-crash suspect.
    ("/^NiagaraToolsets\\.NiagaraToolset_System$/",
     "UeremcpNiagara.UeremcpNiagaraToolset",
     ["NiagaraToolsets.NiagaraToolset_System"]),

    ("/^editor_toolset\\.toolsets\\.material\\./",
     "UeremcpMaterial.UeremcpMaterialToolset",
     ["editor_toolset.toolsets.material.MaterialTools"]),

    ("/^editor_toolset\\.toolsets\\.blueprint\\./",
     "UeremcpBlueprint.UeremcpBlueprintToolset",
     ["editor_toolset.toolsets.blueprint.BlueprintTools"]),

    ("re_agent_tools.toolsets.niagara_workflow_tools",
     "UeremcpNiagara.UeremcpNiagaraToolset",
     ["re_agent_tools.toolsets.niagara_workflow_tools.RENiagaraWorkflowTools"]),

    ("re_agent_tools.toolsets.material_workflow_tools",
     "UeremcpMaterial.UeremcpMaterialToolset",
     ["re_agent_tools.toolsets.material_workflow_tools.REMaterialWorkflowTools"]),

    ("re_agent_tools.toolsets.blueprint_workflow_tools",
     "UeremcpBlueprint.UeremcpBlueprintToolset",
     ["re_agent_tools.toolsets.blueprint_workflow_tools.REBlueprintWorkflowTools"]),

    ("re_agent_tools.toolsets.anim_workflow_tools",
     "UeremcpAnimation.UeremcpAnimationToolset",
     ["re_agent_tools.toolsets.anim_workflow_tools.REAnimWorkflowTools"]),
]

# Deliberately NOT blocked, with the reason. Keeping this explicit stops someone
# "tidying up" by blocking everything Epic ships.
KEEP = [
    ("NiagaraToolsets.NiagaraToolset_Component",
     "runtime component/User params -- UEREMCP authors assets, does not replace this"),
    ("NiagaraToolsets.NiagaraToolset_Assets",
     "Niagara script discovery -- no UEREMCP equivalent"),
    ("NiagaraToolsets.NiagaraToolset_Info", "enum/value lookups, harmless and useful"),
    ("NiagaraToolsets.NiagaraToolset_Blueprint",
     "Blueprint wrappers around systems -- not superseded"),
    ("editor_toolset.toolsets.material_instance",
     "MaterialInstanceConstant authoring -- UeremcpMaterial does not cover MIs"),
    ("EditorToolset.LogsToolset",          "no UEREMCP equivalent; agents need log tails"),
    ("EditorToolset.EditorAppToolset",     "viewport/PIE/CVar control, not superseded"),
    ("SlateInspectorToolset",              "editor UI automation, not superseded"),
    ("animation_toolset.toolsets.sequencer","Sequencer is not covered by UEREMCP"),
    ("editor_toolset.toolsets.asset",      "asset discovery, not superseded"),
    ("editor_toolset.toolsets.object",     "generic property access; UMG workflow depends on it"),
    ("editor_toolset.toolsets.scene",      "level/actor placement, not superseded"),
    ("SemanticSearchToolset",              "asset search, not superseded"),
    ("re_agent_tools.toolsets.capture_workflow_tools",
     "still the only working viewport capture until capture_effect_frames ships"),
    ("ProgrammaticToolset",                "batching escape hatch; removing it removes recovery"),
]


def match(pattern: str, names: list[str]) -> list[str]:
    """Replicate the registry's matching: /regex/ or case-insensitive substring.
    [VERIFIED: $TR/Public/ToolsetRegistry/ToolsetRegistry.h:39-46]"""
    if len(pattern) > 1 and pattern.startswith("/") and pattern.endswith("/"):
        rx = re.compile(pattern[1:-1], re.IGNORECASE)
        return [n for n in names if rx.search(n)]
    return [n for n in names if pattern.lower() in n.lower()]


def render_ini() -> str:
    lines = [
        "; UEREMCP focus mode -- GENERATED by tools/gen_focus_config.py. Do not hand-edit.",
        ";",
        "; Hides the primitive toolsets UEREMCP supersedes, so agents cannot fall back to",
        "; the raw Epic API they have memorised. A blocked toolset is treated as",
        "; non-existent by the registry.",
        ";",
        "; To disable: remove this file, or clear BlockedNames in",
        "; Project Settings > Plugins > Toolset Registry.",
        "",
        "[/Script/ToolsetRegistry.ToolsetRegistrySettings]",
    ]
    for pattern, replacement, _intended in SUPERSEDED:
        lines.append("; superseded by %s" % replacement)
        lines.append("+BlockedNames=%s" % pattern)
    lines.append("")
    lines.append("; Intentionally NOT blocked:")
    for name, reason in KEEP:
        lines.append(";   %-52s %s" % (name, reason))
    lines.append("")
    return "\n".join(lines)


def check() -> int:
    """Fail if a blocked pattern matches nothing, or matches something in KEEP."""
    if not os.path.exists(SNAPSHOT):
        print("no registry snapshot at %s" % SNAPSHOT)
        print("run: python tools/dump_tool_registry.py   (needs a running editor)")
        return 2

    with open(SNAPSHOT, encoding="utf-8") as fh:
        snap = json.load(fh)
    names = list(snap.get("toolsets", {}).keys())

    source_tools = discover_source_tools()
    problems = check_snapshot_fresh(snap, source_tools)
    problems += check_source_descriptions(source_tools)
    problems += check_domains(snap)
    for pattern, replacement, intended in SUPERSEDED:
        hits = sorted(match(pattern, names))

        if not hits:
            print("STALE    %-46s matches no registered toolset" % pattern)
            problems += 1
            continue

        # Over-block is the dangerous direction: it silently removes capability.
        unintended = [h for h in hits if h not in intended]
        if unintended:
            print("OVERBLOCK %-45s also hides %s" % (pattern, ", ".join(unintended)))
            problems += 1

        missing = [i for i in intended if i not in hits]
        if missing:
            print("UNDERBLOCK %-44s fails to hide %s" % (pattern, ", ".join(missing)))
            problems += 1

        for keep_name, reason in KEEP:
            for hit in hits:
                if keep_name.lower() in hit.lower():
                    print("CONFLICT %-46s blocks %s (kept: %s)" % (pattern, hit, reason))
                    problems += 1

        # Removing a capability with no substitute is worse than the fallback.
        if replacement not in names:
            print("MISSING  %-46s replacement %s is not registered"
                  % (pattern, replacement))
            problems += 1
            continue

        if not unintended and not missing:
            print("ok       %-46s -> %s" % (pattern, ", ".join(hits)))

    print("\n%d problem(s)" % problems)
    return 1 if problems else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true",
                    help="run gates, then refuse unsafe global capability hiding")
    ap.add_argument("--check", action="store_true", help="validate against the registry snapshot")
    args = ap.parse_args()

    if args.check:
        return check()

    text = render_ini()
    if args.write:
        gate = check()
        if gate:
            print("refusing to write focus config: discoverability gates failed", file=sys.stderr)
            return gate
        print(
            "refusing to write global BlockedNames by default: use ResolveIntent "
            "demotion so safe Epic discovery remains reachable",
            file=sys.stderr,
        )
        print(
            "Review the generated stanza explicitly if a per-session focus mode is required.",
            file=sys.stderr,
        )
        return 4

    print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
