#!/usr/bin/env python
"""Production intent router (offline + CI twin of UeremcpCore ResolveIntent).

Deterministic floor:
  - Candidates ONLY from registry_snapshot.json (or an injected live snapshot).
  - Impossible to emit a qualified tool name absent from that registry.
  - SUPERSEDED redirects via gen_focus_config.SUPERSEDED (demote, do not hide).
  - Abstain on low confidence or registry hash mismatch.

Do NOT tune ALIASES against the baseline EVAL set. Add aliases only from measured misses.
"""
from __future__ import annotations

import hashlib
import json
import math
import os
import re
import sys
from collections import Counter, defaultdict
from typing import Any

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
ROOT = os.path.dirname(TOOLS)
SNAPSHOT = os.path.join(TOOLS, "registry_snapshot.json")
CATALOG = os.path.join(HERE, "operation_catalog.json")

sys.path.insert(0, TOOLS)
try:
    from gen_focus_config import SUPERSEDED, match as _match
except Exception:  # pragma: no cover
    SUPERSEDED, _match = [], None

# Evidence-led only. Do not add terms to force baseline EVAL to 7/7.
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

STOP = {
    "a", "an", "the", "and", "or", "of", "to", "for", "in", "on", "with", "from", "by", "at",
    "is", "are", "be", "it", "its", "this", "that", "these", "those", "as", "if", "then",
    "me", "my", "i", "we", "you", "your", "new", "make", "get", "set", "use", "using", "want",
    "need", "some", "kind", "like", "looks", "look", "what", "which", "how", "do", "does",
    "can", "should", "would", "one", "all", "any", "into", "out", "up", "down", "when",
    # Weak generic tokens that mass-match Epic tools and cause confident-wrong routes.
    "open", "world", "game", "feel", "more", "across", "lay", "build", "system", "screen",
    "let", "lets", "players", "join", "display", "temporary", "import", "layers", "layer",
    "ping", "sessions", "server", "music", "lobby",
}

# Epic/RE tools allowed in a UEREMCP-preferring plan (discovery / capture fallbacks).
PLAN_ALLOW_PREFIXES = (
    "Ueremcp",
    "re_agent_tools.toolsets.capture_workflow_tools",
    "EditorToolset.LogsToolset",
)

# Never recommend these as goal routes (reachability/protocol probes only).
ROUTE_EXCLUDE_TOOLS = {"Ping", "Echo", "GetStarted", "ResolveIntent", "DescribeOperation"}

DOMAIN_ANCHORS = {
    "niagara", "material", "blueprint", "montage", "anim", "spell", "helix", "capture",
    "template", "texture", "projectile", "emitter", "vfx", "graph", "ability", "gas",
    "screenshot", "viewport", "job", "plan", "ribbon", "particle", "shader",
}

ENVELOPE_HINT = (
    "UEREMCP tools take ONE string arg `requestJson`. Envelope top-level fields: "
    "protocol_version(req), action(req), request_id, target, specification, "
    "options, mode, project, expected_revision, idempotency_key. "
    "dry_run is options.dry_run, NOT top-level. "
    "options.on_unsupported = fail (default) | partial: use partial for incremental "
    "edits so supported parts are applied and the rest reported, instead of the whole "
    "request being rejected because one piece is unsupported."
)

# Baseline EVAL — Opus original 7. Do not retarget aliases to force 7/7.
BASELINE_EVAL = [
    ("make a new fire projectile effect", "UeremcpNiagara.UeremcpNiagaraToolset"),
    ("spell being cast with a helix around a circle", "UeremcpNiagara.UeremcpNiagaraToolset"),
    ("make the fireball material more orange", "UeremcpMaterial.UeremcpMaterialToolset"),
    ("show me what the effect looks like", "re_agent_tools.toolsets.capture_workflow_tools.RECaptureWorkflowTools"),
    ("add logic to a blueprint when the spell hits", "UeremcpBlueprint.UeremcpBlueprintToolset"),
    ("wire up a montage for the attack animation", "UeremcpAnimation.UeremcpAnimationToolset"),
    ("read the editor log for errors", "EditorToolset.LogsToolset"),
]


def tokenize(text: str) -> list[str]:
    words = re.findall(r"[a-z0-9]+", text.lower())
    out: list[str] = []
    for w in words:
        out.append(w)
        out.extend(re.findall(r"[a-z0-9]+", re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", w)))
    return [t for t in out if t not in STOP and len(t) > 1]


def expand(query: str) -> list[str]:
    toks = tokenize(query)
    for t in list(toks):
        if t in ALIASES:
            toks.extend(tokenize(ALIASES[t]))
    return toks


def normalize_tool_name(name: str) -> str:
    """Normalize lookup aliases without changing live canonical registry names."""
    return re.sub(r"[^a-z0-9]", "", name.lower())


def resolve_tool_alias(query: str, known_names: set[str]) -> str | None:
    """Resolve Pascal/snake/kebab and case variants; fail closed on ambiguity."""
    if query in known_names:
        return query
    exact_ci = [name for name in known_names if name.lower() == query.lower()]
    if len(exact_ci) == 1:
        return exact_ci[0]
    needle = normalize_tool_name(query)
    matches = [
        name for name in known_names
        if normalize_tool_name(name) == needle
        or normalize_tool_name(name.rsplit(".", 1)[-1]) == needle
    ]
    return matches[0] if len(matches) == 1 else None


def registry_hash(snap: dict) -> str:
    names: list[str] = []
    for ts_name, ts in sorted((snap.get("toolsets") or {}).items()):
        for tool_name in sorted((ts.get("tools") or {}).keys()):
            names.append("%s.%s" % (ts_name, tool_name))
    blob = "\n".join(names).encode("utf-8")
    # SHA-1 to match FSHA1 live digest in UeremcpIntentRouter.cpp
    return hashlib.sha1(blob).hexdigest()


def load_catalog(path: str = CATALOG) -> dict:
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def validate_dependency_metadata(catalog: dict) -> list[str]:
    """Return error strings; empty means OK. Detects missing action refs and cycles."""
    ops = {o.get("action"): o for o in catalog.get("operations", []) if o.get("action")}
    deps = catalog.get("dependencies") or []
    errors: list[str] = []
    graph: dict[str, list[str]] = defaultdict(list)
    for edge in deps:
        action = edge.get("action")
        if not action:
            errors.append("dependency missing action")
            continue
        for dep in edge.get("depends_on_actions") or []:
            if dep not in ops and dep not in {e.get("action") for e in deps}:
                # soft: dep may be an action without a catalog op row
                pass
            graph[action].append(dep)

    # cycle detect
    visiting, visited = set(), set()

    def dfs(node: str, stack: list[str]) -> None:
        if node in visiting:
            errors.append("dependency cycle: " + " -> ".join(stack + [node]))
            return
        if node in visited:
            return
        visiting.add(node)
        for nxt in graph.get(node, []):
            dfs(nxt, stack + [node])
        visiting.remove(node)
        visited.add(node)

    for n in list(graph):
        dfs(n, [])
    return errors


def build_index(snap: dict, catalog: dict | None = None):
    catalog = catalog or {}
    enrich: dict[str, dict] = {}
    for op in catalog.get("operations") or []:
        q = op.get("qualified")
        if q:
            enrich[q] = op

    superseded_map: dict[str, str] = {}
    if _match:
        names = list(snap["toolsets"].keys())
        for pat, replacement, _intended in SUPERSEDED:
            for hit in _match(pat, names):
                superseded_map[hit] = replacement

    docs, meta = [], []
    for ts_name, ts in snap["toolsets"].items():
        for tool_name, tool in (ts.get("tools") or {}).items():
            qualified = "%s.%s" % (ts_name, tool_name)
            op = enrich.get(qualified, {})
            extra = " ".join(op.get("use_when") or [])
            text = " ".join([
                ts_name, tool_name,
                tool.get("description") or "",
                " ".join(tool.get("properties") or []),
                extra,
                op.get("action") or "",
            ])
            docs.append(Counter(tokenize(text)))
            meta.append({
                "toolset": ts_name,
                "tool": tool_name,
                "qualified": qualified,
                "description": (tool.get("description") or "").strip(),
                "required": tool.get("required") or [],
                "properties": tool.get("properties") or [],
                "superseded_by": superseded_map.get(ts_name),
                "is_ueremcp": ts_name.startswith("Ueremcp"),
                "catalog": op,
            })
    return docs, meta


def search(query: str, docs, meta, top: int = 5):
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
        if m["tool"] in ROUTE_EXCLUDE_TOOLS:
            qset = set(toks)
            if not (qset & {"ping", "echo", "liveness", "reachability", "alive", "probe"}):
                continue
        if m["is_ueremcp"]:
            score *= 1.6
        if m["superseded_by"]:
            score *= 0.35
        scored.append((score, sorted(set(matched)), m))
    scored.sort(key=lambda s: -s[0])
    return scored[:top]


def action_order_rank(action: str | None, catalog: dict) -> tuple[int, str]:
    if not action:
        return 50, "no action metadata"
    # topological tier from depends_on: producers before consumers
    preds = {edge["action"]: set(edge.get("depends_on_actions") or [])
             for edge in (catalog.get("dependencies") or []) if edge.get("action")}
    # rank = 1 + max(pred ranks); memoize
    memo: dict[str, int] = {}

    def rank(a: str) -> int:
        if a in memo:
            return memo[a]
        memo[a] = -1  # cycle guard
        ps = preds.get(a) or set()
        if not ps:
            memo[a] = 10
        else:
            memo[a] = 10 + max(rank(p) for p in ps)
        # invert so prerequisites sort first: lower number earlier
        return memo[a]

    r = rank(action)
    why = next((e.get("why") for e in (catalog.get("dependencies") or []) if e.get("action") == action),
               "catalog dependency order")
    return r, why



def assemble_batch(steps: list[dict], catalog: dict) -> dict | None:
    """Turn recommended steps into ONE execute_plan body.

    The router already knows the dependency graph -- action_order_rank reads
    depends_on_actions out of the catalog to sort steps. It just never used it
    to BUILD anything: it emitted N independently-callable steps, each with its
    own request_json, and never mentioned that execute_plan exists.

    That is why agents issue one call per domain. Nothing told them otherwise.
    Cost is superlinear in call count (docs/WHY.md), so this is not a tidiness
    fix -- for a five-domain goal it is the difference between 5 round trips and
    2 (dry-run, then execute).

    Returns None for a single-step plan, where a batch would be pure overhead.
    """
    actionable = [s for s in steps if (s.get("request_json") or {}).get("action")
                  and s["request_json"]["action"] != "<see purpose>"]

    # A composite performs its constituents. Including both does the work twice.
    # When the intent enumerated the steps, keep the enumeration -- substituting
    # a composite silently changes the failure modes and the placeholder
    # behaviour the caller gets.
    present = {s["request_json"]["action"] for s in actionable}
    superseded, notes = set(), []
    for e in (catalog.get("dependencies") or []):
        covers = set(e.get("covers") or [])
        if e.get("action") in present and len(covers & present) >= 2:
            superseded.add(e["action"])
            notes.append("%s omitted from the batch: it performs %s, which the "
                         "intent named explicitly. Call it alone instead if you "
                         "want the one-call composite."
                         % (e["action"], ", ".join(sorted(covers & present))))
    actionable = [s for s in actionable
                  if s["request_json"]["action"] not in superseded]
    if len(actionable) < 2:
        return None

    edges = {e["action"]: set(e.get("depends_on_actions") or [])
             for e in (catalog.get("dependencies") or []) if e.get("action")}
    why_of = {e["action"]: e.get("why") for e in (catalog.get("dependencies") or [])
              if e.get("action")}

    by_action = {}
    for s in actionable:
        by_action.setdefault(s["request_json"]["action"], "op%d_%s" % (
            s["step"], s["request_json"]["action"]))

    operations = []
    for s in actionable:
        req = s["request_json"]
        action = req["action"]
        # Only depend on prerequisites actually present in THIS plan. A
        # depends_on naming an absent operation fails the whole plan.
        deps = sorted(by_action[p] for p in edges.get(action, set()) if p in by_action)
        op = {
            "id": by_action[action],
            "action": action,
            "specification": req.get("specification") or {},
        }
        if req.get("target"):
            op["target"] = req["target"]
        if deps:
            op["depends_on"] = deps
        if why_of.get(action):
            op["_why"] = why_of[action]
        operations.append(op)

    # Verification observes; it cannot precede what it observes. The catalog
    # records no edges for it because it depends on everything.
    VERIFY = ("validate_", "capture_", "inspect_")
    build_ids = [o["id"] for o in operations if not o["action"].startswith(VERIFY)]
    for o in operations:
        if o["action"].startswith(VERIFY) and build_ids:
            o["depends_on"] = build_ids
            o["_why"] = "verification runs last, once there is something to observe"

    # Emit in dependency order so a reader sees the build order directly.
    resolved, remaining = [], list(operations)
    while remaining:
        ready = [o for o in remaining
                 if all(d in {r["id"] for r in resolved} for d in o.get("depends_on") or [])]
        if not ready:                      # cycle guard; emit rest as-is
            resolved.extend(remaining)
            break
        ready.sort(key=lambda o: o["id"])
        resolved.extend(ready)
        remaining = [o for o in remaining if o not in ready]
    operations = resolved

    any_destructive = any(s.get("safety", {}).get("destructive") for s in actionable)
    return {
        "why": ("These operations are dependent. execute_plan runs them in one "
                "transaction with rollback instead of %d separate calls."
                % len(actionable)),
        "round_trips": {"as_separate_calls": len(actionable), "as_one_plan": 1},
        "request_json": {
            "protocol_version": "1.0",
            "action": "execute_plan",
            "request_id": "batch-1",
            "options": {"dry_run": True} if any_destructive else {},
            "specification": {
                "transaction": {"atomic": True, "rollback_on_failure": True,
                                "compile_policy": "at_boundaries"},
                "operations": operations,
                "on_failure": "rollback_all",
            },
        },
        "next": ("Dry-run this plan, read the per-operation statuses, then resend "
                 "with options.dry_run=false." if any_destructive else
                 "Send this plan as-is."),
        "chaining": ('Reference an earlier result with {"$ref": "<op_id>.result.primary_asset"} '
                     'or the short form "$<op_id>".'),
        **({"omitted": notes} if notes else {}),
    }


def plan(
    query: str,
    docs,
    meta_all,
    catalog: dict,
    *,
    expected_hash: str | None = None,
    snap_hash: str | None = None,
    mode: str = "recommend",
    known_names: set[str] | None = None,
) -> dict[str, Any]:
    known = known_names or {m["qualified"] for m in meta_all}

    out: dict[str, Any] = {
        "intent": query,
        "mode": mode,
        "envelope_contract": ENVELOPE_HINT,
        "registry_hash": snap_hash,
        "plan": [],
        "alternatives": [],
        "clarification_questions": [],
        "abstained": False,
    }

    if expected_hash and snap_hash and expected_hash != snap_hash:
        out["confidence"] = "none"
        out["confidence_reason"] = (
            "registry hash mismatch; refusing confident routing on a stale snapshot"
        )
        out["abstained"] = True
        out["fallback"] = "refresh registry_snapshot.json via dump_tool_registry.py; then ResolveIntent again"
        return out

    if mode == "execute_if_complete":
        out["execution"] = {
            "attempted": False,
            "reason": (
                "execute_if_complete is not enabled in this build: safe verifiable "
                "auto-execution requires complete fields + ADR-0008 plan reuse which "
                "cannot be guaranteed from plain text alone. Use mode=recommend."
            ),
        }

    hits = search(query, docs, meta_all, top=25)
    best: dict[str, tuple] = {}
    for score, matched, m in hits:
        action = (m.get("catalog") or {}).get("action")
        rank, why = action_order_rank(action, catalog)
        # Dedupe by ACTION when the catalog knows one, else by toolset.
        #
        # Keying on toolset alone is why multi-domain goals collapsed to a
        # single step: every environment operation -- CreateLandscape,
        # CreateWaterBody, ScatterFoliage, AttachWeather -- lives in
        # UeremcpEnvironment.UeremcpEnvironmentToolset, so a five-operation
        # build kept exactly one candidate. The toolset fallback still
        # suppresses five near-identical uncatalogued primitives.
        key = action or m["toolset"]
        if key not in best or score > best[key][0]:
            best[key] = (score, matched, m, why, rank)

    # Score-primary ordering (BACKLOG 1.3d). dependsOn ranks remain on each hit
    # for near-tie diagnostics but do not force weak domains into the plan.
    chosen = sorted(best.values(), key=lambda x: (-x[0], x[4]))

    def allowed_in_plan(m) -> bool:
        q = m["qualified"]
        if m["is_ueremcp"] or m.get("catalog"):
            return True
        return any(q.startswith(p) or m["toolset"].startswith(p) for p in PLAN_ALLOW_PREFIXES)

    best_hit_score = max((h[0] for h in hits), default=0.0)
    plan_score_floor = best_hit_score * 0.35
    chosen_gated = [c for c in chosen if c[0] + 1e-9 >= plan_score_floor]

    ueremcp_top = max((c[0] for c in chosen_gated if c[2]["is_ueremcp"]), default=0.0)
    catalog_top = max((c[0] for c in chosen_gated if c[2].get("catalog")), default=0.0)
    top_score = best_hit_score
    top_matched = set()
    for c in chosen_gated[:3]:
        top_matched.update(c[1])
    has_anchor = bool(top_matched & DOMAIN_ANCHORS)

    # Prefer abstaining over confident Epic misroutes on underspecified / unsupported goals.
    strong_signal = max(ueremcp_top, catalog_top) >= 25.0 and (has_anchor or len(top_matched) >= 2)
    if top_score >= 25.0 and strong_signal:
        confidence, reason = "high", "strong UEREMCP/catalog lexical match against registry-backed candidates"
    elif max(ueremcp_top, catalog_top) > 0 or top_score > 0:
        confidence, reason = "low", "weak or non-UEREMCP lexical signal; prefer clarification"
    else:
        confidence, reason = "none", "no registry candidate matched tokens"

    out["confidence"] = confidence
    out["confidence_reason"] = reason
    out["plan_score_floor"] = round(plan_score_floor, 1)

    if not strong_signal:
        out["abstained"] = True
        out["fallback"] = "no confident UEREMCP match; use list_toolsets/describe_toolset or answer clarification_questions"
        out["clarification_questions"] = [
            "What asset type are you creating or inspecting (Niagara, Material, Blueprint, Ability, Template)?",
            "Is this create, inspect, modify, or visual-verify?",
            "Do you already have an asset path under /Game/?",
        ]
        for score, matched, m, why, rank in chosen_gated[:3]:
            if m["qualified"] in known:
                out["alternatives"].append({
                    "tool": m["qualified"],
                    "score": round(score, 1),
                    "matched_terms": matched[:6],
                })
        return out

    if confidence == "low":
        out["clarification_questions"] = [
            "Confirm the primary domain (Niagara / Material / Blueprint / other).",
        ]

    # Default cap 3 — single-domain intents must not emit five speculative
    # steps. But when the chosen actions are CONNECTED by catalog dependency
    # edges, they are a build sequence rather than competing guesses, and
    # truncating one costs a whole round trip. Widen only on that evidence.
    plan_pool = [c for c in chosen_gated if allowed_in_plan(c[2])]
    _dep_edges = {e["action"]: set(e.get("depends_on_actions") or [])
                  for e in (catalog.get("dependencies") or []) if e.get("action")}
    _pool_actions = {(c[2].get("catalog") or {}).get("action") for c in plan_pool}
    _pool_actions.discard(None)
    _linked = {a for a in _pool_actions if _dep_edges.get(a, set()) & _pool_actions}
    plan_cap = 6 if len(_linked) >= 2 else 3

    for i, (score, matched, m, why, rank) in enumerate(plan_pool[:plan_cap], 1):
        if m["qualified"] not in known:
            continue  # structural impossibility guard
        cat = m.get("catalog") or {}
        step = {
            "step": i,
            "toolset": m["toolset"],
            "tool": m["tool"],
            "qualified": m["qualified"],
            "score": round(score, 1),
            "confidence": confidence,
            "confidence_reason": reason,
            "why_here": why,
            "matched_terms": matched[:6],
            "purpose": (cat.get("use_when") and "; ".join(cat["use_when"][:3]))
                       or m["description"].replace("\n", " ")[:160],
            "do_not_use_for": cat.get("do_not_use_for") or [],
            "input_schema": {
                "properties": m["properties"],
                "required": m["required"],
            },
            "missing_fields": [],
            "safety": {
                "destructive": bool(cat.get("destructive")),
                "idempotent": bool(cat.get("idempotent", True)),
                "requires_revision": bool(cat.get("requires_revision")),
                "prefer_dry_run": bool(cat.get("destructive")) or m["is_ueremcp"],
            },
            "expected_statuses": cat.get("expected_statuses") or [],
            "recovery": cat.get("recovery") or "Re-read the tool response; never claim *_validated without evidence.",
            "next_tool": None,
        }
        if m["superseded_by"]:
            step["warning"] = "superseded; prefer %s" % m["superseded_by"]
        example = cat.get("example_request")
        if example:
            step["request_json"] = example
            missing = []
            if isinstance(example.get("expected_revision"), str) and example["expected_revision"].startswith("<"):
                missing.append("expected_revision")
            if cat.get("requires_revision"):
                missing.append("expected_revision from prior read_graph")
            step["missing_fields"] = missing
        elif m["is_ueremcp"]:
            step["request_json"] = {
                "protocol_version": "1.0",
                "action": cat.get("action") or "<see purpose>",
                "target": {"asset_path": "/Game/__UeremcpTests/Foo"},
                "options": {"dry_run": True},
                "specification": {},
            }
            step["missing_fields"] = ["action-specific specification fields"]
        if m["superseded_by"]:
            step["next_tool"] = m["superseded_by"]
        elif cat.get("recovery"):
            step["next_tool"] = cat.get("recovery")
        out["plan"].append(step)

    # alternatives = next-best hits not already in plan
    planned = {s["qualified"] for s in out["plan"]}
    for score, matched, m in hits:
        if m["qualified"] in planned or m["qualified"] not in known:
            continue
        out["alternatives"].append({
            "tool": m["qualified"],
            "score": round(score, 1),
            "matched_terms": matched[:6],
            "superseded_by": m.get("superseded_by"),
        })
        if len(out["alternatives"]) >= 5:
            break

    # Assemble the batch. The single most consequential line in this file:
    # without it the agent never learns that execute_plan applies to its goal.
    batch = assemble_batch(out["plan"], catalog)
    if batch:
        out["batch"] = batch
    elif len(out["plan"]) == 1:
        out["batch_note"] = ("single-operation goal; a batch would add overhead. "
                             "Multi-domain goals return a `batch` field.")

    # structural assert: no absent names
    for s in out["plan"]:
        assert s["qualified"] in known, s["qualified"]
    return out


def evaluate_baseline(docs, meta) -> dict:
    hit1 = hit3 = 0
    rows = []
    for query, want in BASELINE_EVAL:
        hits = search(query, docs, meta, top=3)
        got = [h[2]["toolset"] for h in hits]
        top1 = bool(got) and got[0] == want
        top3 = want in got
        hit1 += int(top1)
        hit3 += int(top3)
        rows.append({"intent": query, "want": want, "got": got, "top1": top1, "top3": top3})
    return {
        "suite": "baseline_opus_7",
        "top1": hit1,
        "top3": hit3,
        "n": len(BASELINE_EVAL),
        "rows": rows,
    }


def mrr_at(ranks: list[int | None]) -> float:
    s = 0.0
    for r in ranks:
        if r is None:
            continue
        s += 1.0 / r
    return s / max(len(ranks), 1)


def evaluate_heldout(docs, meta, heldout: list[dict], catalog: dict, snap_hash: str) -> dict:
    top1 = top3 = abstain_ok = confident_wrong = 0
    ranks: list[int | None] = []
    rows = []
    for item in heldout:
        query = item["intent"]
        result = plan(query, docs, meta, catalog, snap_hash=snap_hash)
        hits = search(query, docs, meta, top=5)
        got_sets = [h[2]["toolset"] for h in hits]
        want = item.get("expected_top1_toolset")
        any_of = item.get("expected_any_of_toolsets") or ([] if want is None else [want])
        should_abstain = bool(item.get("should_abstain"))

        if should_abstain:
            ok = result.get("abstained") or result.get("confidence") in ("none", "low")
            abstain_ok += int(ok)
            ranks.append(None)
            rows.append({"id": item["id"], "abstain": ok, "confidence": result.get("confidence")})
            continue

        t1 = bool(got_sets) and got_sets[0] == want
        t3 = any(g in any_of for g in got_sets[:3])
        top1 += int(t1)
        top3 += int(t3)
        rank = None
        for i, g in enumerate(got_sets, 1):
            if g in any_of:
                rank = i
                break
        ranks.append(rank)
        if result.get("confidence") == "high" and not t3:
            confident_wrong += 1
        rows.append({
            "id": item["id"],
            "top1": t1,
            "top3": t3,
            "rank": rank,
            "got": got_sets[:3],
            "confidence": result.get("confidence"),
        })

    n_route = sum(1 for i in heldout if not i.get("should_abstain"))
    n_abs = sum(1 for i in heldout if i.get("should_abstain"))
    return {
        "suite": "heldout",
        "n": len(heldout),
        "n_routable": n_route,
        "n_abstain": n_abs,
        "top1": top1,
        "top3": top3,
        "top1_rate": top1 / max(n_route, 1),
        "top3_rate": top3 / max(n_route, 1),
        "mrr": mrr_at(ranks),
        "confident_wrong": confident_wrong,
        "abstention_accuracy": abstain_ok / max(n_abs, 1),
        "rows": rows,
        "note": (
            "Routing accuracy is not end-to-end completion. Report E2E separately."
        ),
    }


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if not os.path.exists(SNAPSHOT):
        print("no snapshot: run python tools/dump_tool_registry.py", file=sys.stderr)
        return 2
    with open(SNAPSHOT, encoding="utf-8") as fh:
        snap = json.load(fh)
    catalog = load_catalog()
    dep_errs = validate_dependency_metadata(catalog)
    if dep_errs and "--skip-dep-check" not in argv:
        print("dependency metadata errors:", file=sys.stderr)
        for e in dep_errs:
            print(" ", e, file=sys.stderr)
        return 3

    docs, meta = build_index(snap, catalog)
    h = registry_hash(snap)

    if argv and argv[0] == "--hash":
        print(h)
        return 0
    if argv and argv[0] == "--plan":
        print(json.dumps(plan(" ".join(argv[1:]), docs, meta, catalog, snap_hash=h), indent=2))
        return 0
    if argv and argv[0] == "--eval-baseline":
        print(json.dumps(evaluate_baseline(docs, meta), indent=2))
        return 0
    if argv and argv[0] == "--eval-heldout":
        path = argv[1] if len(argv) > 1 else os.path.join(
            ROOT, "tests", "intent_router", "heldout_intents.json")
        with open(path, encoding="utf-8") as fh:
            held = json.load(fh)
        print(json.dumps(evaluate_heldout(docs, meta, held, catalog, h), indent=2))
        return 0
    if not argv:
        print("indexed %d tools; hash=%s" % (len(docs), h[:12]))
        print(json.dumps(evaluate_baseline(docs, meta), indent=2))
        return 0

    # default: search render
    q = " ".join(argv)
    hits = search(q, docs, meta)
    print("intent:", q)
    for score, matched, m in hits:
        print("  %6.1f  %s%s" % (score, m["qualified"], " [GOAL]" if m["is_ueremcp"] else ""))
        if m["superseded_by"]:
            print("          SUPERSEDED ->", m["superseded_by"])
        print("          matched:", ", ".join(matched[:8]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
