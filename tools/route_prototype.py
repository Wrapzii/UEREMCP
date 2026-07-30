#!/usr/bin/env python
"""Prototype: intent text in, ranked real tools + how to call them out.

Answers "which tool do I use?" in ONE round trip instead of
list_toolsets (4.3k tokens) + describe_toolset (0.6-29k) + N rejection round
trips. Measured cost of the current path to one successful call: 5 MCP round
trips plus reading three schema files out of the repo -- which an agent without
repo access cannot do at all.

THE DETERMINISTIC PART
----------------------
Ranking is fuzzy; the *output* is not. Candidates come only from
registry_snapshot.json, so the router structurally cannot invent a tool name --
the single most damaging thing a router could do. Scoring is lexical (BM25-ish)
rather than embedding-based on purpose: deterministic, debuggable, zero
dependency, and explainable ("matched: niagara, effect"). A wrong ranking is
visible and fixable; a wrong embedding is neither.

WHAT IT MUST RETURN
-------------------
Names alone do not help. Measured: knowing `CreateNiagaraEffect` existed still
cost 3 rejections, because the blocker was the envelope shape, not the name. So
every hit carries its call contract.

    python tools/route_prototype.py "make a spell effect with a helix"
    python tools/route_prototype.py --eval        # scored intent suite
"""
from __future__ import annotations

import json
import math
import os
import re
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
SNAPSHOT = os.path.join(HERE, "registry_snapshot.json")

# Domain vocabulary the raw text does not contain. Kept small and evidence-led:
# add a term only when a measured intent misses because of it.
ALIASES = {
    "spell": "niagara effect vfx particle",
    "vfx": "niagara effect particle",
    "particle": "niagara effect emitter",
    "helix": "niagara effect ribbon spiral",
    "beam": "niagara effect emitter",
    "explosion": "niagara effect burst",
    "shader": "material",
    "texture": "material texture",
    "screenshot": "capture viewport image render",
    "picture": "capture viewport image render",
    "look": "capture viewport image render",
    "animation": "montage sequence anim skeletal",
    "logic": "blueprint graph node",
    "ability": "gameplay ability gas",
}

# Tools whose intent is better served by a UEREMCP goal-level tool. Generated
# from the same supersession map the focus-mode config uses -- one source of
# truth, so the router and the block list can never disagree.
try:
    sys.path.insert(0, HERE)
    from gen_focus_config import SUPERSEDED, match as _match
except Exception:
    SUPERSEDED, _match = [], None


# Ranking on "a"/"and"/"with" is how the first version scored a material tool
# top for a Niagara intent. IDF alone does not save you: dividing by document
# length rewards short descriptions that happen to contain a stopword.
STOP = {
    "a","an","the","and","or","of","to","for","in","on","with","from","by","at",
    "is","are","be","it","its","this","that","these","those","as","if","then",
    "me","my","i","we","you","your","new","make","get","set","use","using","want",
    "need","some","kind","like","looks","look","what","which","how","do","does",
    "can","should","would","one","all","any","into","out","up","down","when",
}


def tokenize(text: str) -> list[str]:
    words = re.findall(r"[a-z0-9]+", text.lower())
    out = []
    for w in words:
        out.append(w)
        # split snake_case/PascalCase compounds so "create_niagara_effect" and
        # "CreateNiagaraEffect" both index as create/niagara/effect. The surface
        # mixes both conventions, which is itself a measured source of misses.
        out.extend(re.findall(r"[a-z0-9]+", re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", w)))
    return [t for t in out if t not in STOP and len(t) > 1]


def expand(query: str) -> list[str]:
    toks = tokenize(query)
    for t in list(toks):
        if t in ALIASES:
            toks.extend(tokenize(ALIASES[t]))
    return toks


def build_index(snap):
    docs, meta = [], []
    superseded_map = {}
    if _match:
        names = list(snap["toolsets"].keys())
        for pat, replacement, _intended in SUPERSEDED:
            for hit in _match(pat, names):
                superseded_map[hit] = replacement

    for ts_name, ts in snap["toolsets"].items():
        for tool_name, tool in (ts.get("tools") or {}).items():
            text = " ".join([
                ts_name, tool_name,
                tool.get("description") or "",
                " ".join(tool.get("properties") or []),
            ])
            docs.append(Counter(tokenize(text)))
            meta.append({
                "toolset": ts_name,
                "tool": tool_name,
                "qualified": "%s.%s" % (ts_name, tool_name),
                "description": (tool.get("description") or "").strip(),
                "required": tool.get("required") or [],
                "properties": tool.get("properties") or [],
                "superseded_by": superseded_map.get(ts_name),
                "is_ueremcp": ts_name.startswith("Ueremcp"),
            })
    return docs, meta


def search(query, docs, meta, top=5):
    toks = expand(query)
    n = len(docs)
    df = Counter()
    for d in docs:
        for t in set(toks):
            if t in d:
                df[t] += 1

    avgdl = sum(sum(d.values()) for d in docs) / max(n, 1)
    k1, b = 1.5, 0.75

    scored = []
    for i, d in enumerate(docs):
        dl = sum(d.values()) or 1
        score = 0.0
        matched = []
        for t in toks:
            if t not in d:
                continue
            idf = math.log(1 + (n - df[t] + 0.5) / (df[t] + 0.5))
            tf = d[t]
            score += idf * (tf * (k1 + 1)) / (tf + k1 * (1 - b + b * dl / avgdl))
            matched.append(t)
        if score <= 0:
            continue
        m = meta[i]
        # Prefer the goal-level surface: one call beats orchestrating primitives,
        # and it is the whole reason UEREMCP exists.
        if m["is_ueremcp"]:
            score *= 1.6
        # Actively demote what we already decided is superseded, rather than
        # relying on the agent to know.
        if m["superseded_by"]:
            score *= 0.35
        scored.append((score, sorted(set(matched)), m))
    scored.sort(key=lambda s: -s[0])
    return scored[:top]


ENVELOPE_HINT = (
    'UEREMCP tools take ONE string arg `requestJson`. Envelope top-level fields: '
    'protocol_version(req), action(req), request_id, target, specification, '
    'options, mode, project, expected_revision, idempotency_key. '
    'dry_run is options.dry_run, NOT top-level.'
)


def render(query, hits):
    print("intent: %s\n" % query)
    if not hits:
        print("  no confident match -- fall back to list_toolsets")
        return
    for score, matched, m in hits:
        flag = "  [GOAL-LEVEL]" if m["is_ueremcp"] else ""
        print("  %6.1f  %s%s" % (score, m["qualified"], flag))
        if m["superseded_by"]:
            print("          SUPERSEDED -> prefer %s" % m["superseded_by"])
        desc = m["description"].replace("\n", " ")
        if desc:
            print("          %s" % desc[:120])
        print("          args: %s%s" % (
            ", ".join(m["properties"]) or "(none)",
            "   required: " + ", ".join(m["required"]) if m["required"] else ""))
        print("          matched: %s" % ", ".join(matched[:8]))
        if m["is_ueremcp"]:
            print("          %s" % ENVELOPE_HINT)
        print()


# Intent -> the toolset that should win. Scored, not vibes.
EVAL = [
    ("make a new fire projectile effect",            "UeremcpNiagara.UeremcpNiagaraToolset"),
    ("spell being cast with a helix around a circle","UeremcpNiagara.UeremcpNiagaraToolset"),
    ("make the fireball material more orange",       "UeremcpMaterial.UeremcpMaterialToolset"),
    ("show me what the effect looks like",           "re_agent_tools.toolsets.capture_workflow_tools.RECaptureWorkflowTools"),
    ("add logic to a blueprint when the spell hits", "UeremcpBlueprint.UeremcpBlueprintToolset"),
    ("wire up a montage for the attack animation",   "UeremcpAnimation.UeremcpAnimationToolset"),
    ("read the editor log for errors",               "EditorToolset.LogsToolset"),
]


def evaluate(docs, meta):
    hit1 = hit3 = 0
    for query, want in EVAL:
        hits = search(query, docs, meta, top=3)
        got = [h[2]["toolset"] for h in hits]
        top1 = bool(got) and got[0] == want
        top3 = want in got
        hit1 += top1
        hit3 += top3
        print("%-3s %-48s -> %s" % ("ok" if top1 else ("~" if top3 else "MISS"),
                                    query, got[0] if got else "(none)"))
        if not top3:
            print("      wanted %s" % want)
    print("\ntop-1 %d/%d   top-3 %d/%d" % (hit1, len(EVAL), hit3, len(EVAL)))
    return 0 if hit3 == len(EVAL) else 1


# Domain ordering. THE WEAKEST PART OF THIS DESIGN, flagged deliberately: the
# registry knows what tools exist, not what depends on what. This is declared,
# which means it can go stale -- unlike everything else here, which is generated.
#
# A stale ordering is much less harmful than a wrong tool name (the agent
# reorders and continues), but it should eventually be derived from
# batch/plan.schema.json dependencies and template construction_plans rather
# than asserted here.
DOMAIN_ORDER = [
    ("material",  1, "textures and materials exist before an effect can bind them"),
    ("texture",   1, "textures and materials exist before an effect can bind them"),
    ("niagara",   2, "the effect consumes the materials above"),
    ("blueprint", 3, "logic references the assets it spawns"),
    ("animation", 3, "montages reference the effects they trigger"),
    ("gameplay",  4, "abilities wire together the assets above"),
    ("capture",   9, "verify last, once there is something to look at"),
    ("validation",9, "verify last, once there is something to look at"),
]


def domain_of(meta) -> tuple[int, str]:
    blob = (meta["toolset"] + " " + meta["tool"]).lower()
    for key, rank, why in DOMAIN_ORDER:
        if key in blob:
            return rank, why
    return 5, "no ordering constraint known"


def plan(query, docs, meta_all, per_domain=1):
    """Intent in -> ordered call plan with schemas. One round trip."""
    hits = search(query, docs, meta_all, top=25)
    best = {}
    for score, matched, m in hits:
        rank, why = domain_of(m)
        key = (rank, m["toolset"])
        if key not in best or score > best[key][0]:
            best[key] = (score, matched, m, why)

    chosen = sorted(best.values(), key=lambda x: (domain_of(x[2])[0], -x[0]))
    # Weak signal is worse than no signal: a confidently wrong plan costs more
    # than admitting uncertainty, because the agent stops questioning it.
    top_score = max((c[0] for c in chosen), default=0.0)
    confidence = "high" if top_score >= 6.0 else ("low" if top_score > 0 else "none")

    out = {
        "intent": query,
        "confidence": confidence,
        "envelope_contract": ENVELOPE_HINT,
        "plan": [],
    }
    if confidence == "none":
        out["fallback"] = "no match; use list_toolsets/describe_toolset"
        return out

    for i, (score, matched, m, why) in enumerate(chosen[:6], 1):
        step = {
            "step": i,
            "tool": m["qualified"],
            "score": round(score, 1),
            "why_here": why,
            "matched_terms": matched[:6],
            "purpose": m["description"].replace("\n", " ")[:160],
            "input_schema": {
                "properties": m["properties"],
                "required": m["required"],
            },
        }
        if m["superseded_by"]:
            step["warning"] = "superseded; prefer %s" % m["superseded_by"]
        if m["is_ueremcp"]:
            step["example"] = {
                "protocol_version": "1.0",
                "action": "<see purpose>",
                "target": {"asset_path": "/Game/__UeremcpTests/Foo"},
                "options": {"dry_run": True},
                "specification": {},
            }
        out["plan"].append(step)
    return out


def main() -> int:
    if not os.path.exists(SNAPSHOT):
        print("no snapshot: run python tools/dump_tool_registry.py")
        return 2
    with open(SNAPSHOT, encoding="utf-8") as fh:
        snap = json.load(fh)
    docs, meta = build_index(snap)

    args = sys.argv[1:]
    if args and args[0] == "--plan":
        print(json.dumps(plan(" ".join(args[1:]), docs, meta), indent=1))
        return 0
    if not args or args[0] == "--eval":
        print("indexed %d tools from %d toolsets\n" % (len(docs), snap["toolset_count"]))
        return evaluate(docs, meta)
    render(" ".join(args), search(" ".join(args), docs, meta))
    return 0


if __name__ == "__main__":
    sys.exit(main())
