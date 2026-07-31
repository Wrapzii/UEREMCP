#!/usr/bin/env python
"""Prototype: the world document -- creation as read/modify/apply, not verbs.

WHY THIS EXISTS
---------------
Blueprint editing works well because the JSON *is* the artifact: ReadGraph ->
edit -> SubmitGraph (mode=replace with a whole `graph`, or mode=patch with `ops`
against a base_revision). [VERIFIED: schemas/domains/blueprints/submit_graph.schema.json]

World creation does not work that way. It is verbs -- CreateLandscape,
CreateWaterBody, PlaceStructures, CreateNiagaraEffect. Verbs do not round-trip:
there is no document to read back, so there is nothing to diff, nothing to
batch, and every follow-up ("make the mountains taller") re-derives the scene
from prose.

This prototype applies the Blueprint model to the world:

    ReadWorld(level)  -> world document
    <LLM edits the document>
    ApplyWorld(doc)   -> reconcile desired vs actual -> ONE execute_plan

Creation is applying a document to an empty world. Editing is applying a
document to a populated one. Same schema both directions.

WHAT IS DETERMINISTIC HERE
--------------------------
The reconciler. Given (actual, desired) the emitted plan is a pure function --
no ranking, no scoring, no LLM judgement. The fuzzy part (which words mean
"lake") stays in the router; once a document exists, everything downstream is
mechanical. That is the point: ranking a tool wrong is recoverable, applying a
world wrong is not.

The capability map below IS declared rather than generated, and that is its
weakest property -- same caveat as DOMAIN_ORDER in route_prototype.py. It is
cross-checked against registry_snapshot.json where it can be, and every entry
carries a status so the reconciler refuses rather than silently downgrades.

    python tools/world_doc_prototype.py --demo
    python tools/world_doc_prototype.py --read > world.json
    python tools/world_doc_prototype.py --apply world.json
    python tools/world_doc_prototype.py --advise CreateWaterBody
    python tools/world_doc_prototype.py --gaps
"""
from __future__ import annotations

import copy
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SNAPSHOT = os.path.join(HERE, "registry_snapshot.json")

DOC_VERSION = "1.0"

# ---------------------------------------------------------------------------
# Capability map: document shape -> the action that realises it.
#
# status is the honesty gate and the whole reason this table exists:
#   supported  implementation proven and reachable as an action
#   unexposed  the C++ capability EXISTS and is verified, but no AICallable
#              action reaches it -- it is welded inside a higher-level verb.
#              Cheapest class of gap by far: a toolset wrapper, not new code.
#   stub       ADVERTISED but not implemented -- reconciler REFUSES, never
#              silently downgrades (the CreateWaterBody lake->river defect)
#   missing    no implementation at all; reconciler reports it as a gap
#
# Sources are tagged. Verified against UeremcpEnvironment on main (2026-07-31);
# entries that remain UNVERIFIED are agent-reported and still owe a source.
# ---------------------------------------------------------------------------
CAPABILITIES = {
    ("terrain", "*"): dict(
        action="create_landscape", domain="environment", status="supported",
        note="seeded heightmap import; mountain profiles exist",
        source="[VERIFIED: UeremcpEnvironment module present on main]"),
    ("terrain", "shore"): dict(
        action="create_landscape", domain="environment", status="missing",
        note="no beach/shore heightmap recipe; author a profile or extend the tool",
        source="[UNVERIFIED: agent-reported]"),

    ("water", "river"): dict(
        action="create_water_body", domain="environment", status="supported",
        note="AWaterBodyRiver", source="[VERIFIED: UeremcpEnvironmentService.cpp:1438]"),
    ("water", "lake"): dict(
        action="create_water_body", domain="environment", status="stub",
        note="header advertises river/lake/ocean; zero AWaterBodyLake symbols exist",
        source="[VERIFIED: UeremcpEnvironmentToolset.h:56 advertises; "
               "no AWaterBodyLake in UeremcpEnvironment; "
               "build_environment.schema.json:73 says 'Phase 2']"),
    ("water", "ocean"): dict(
        action="create_water_body", domain="environment", status="stub",
        note="header advertises river/lake/ocean; zero AWaterBodyOcean symbols exist",
        source="[VERIFIED: UeremcpEnvironmentToolset.h:56 advertises; "
               "build_environment.schema.json:74 says 'Phase 2']"),

    ("structures", "barrier_wall"): dict(
        action="place_structures", domain="environment", status="supported",
        note="GeometryScript AppendBox",
        source="[VERIFIED: UeremcpEnvironmentService.cpp:1794]"),
    ("structures", "castle_keep"): dict(
        action="place_structures", domain="environment", status="missing",
        note="only ice_wall_ring|barrier_wall|box_along_river; unsupported kinds "
             "are REJECTED not substituted -- correct behaviour, keep it",
        source="[VERIFIED: UeremcpEnvironmentService.cpp:795]"),

    ("foliage", "*"): dict(
        action="scatter_foliage", domain="environment", status="supported",
        note="HISM scatter; falls back to /Engine/BasicShapes/Cube when "
             "biome.mesh_path is absent -- the cubes-for-trees defect, still live",
        source="[VERIFIED: UeremcpEnvironmentService.cpp:1495-1497; "
               "also UeremcpWeatherFollower.cpp:33]"),

    ("weather", "attach"): dict(
        action="attach_weather", domain="environment", status="supported",
        note="player-follow rain/fog", source="[UNVERIFIED: agent-reported]"),
    ("weather", "cycle"): dict(
        action="attach_weather", domain="environment", status="missing",
        note="no day/night or state-cycle contract; single attached state only",
        source="[VERIFIED: no day_night/cycle symbol in UeremcpEnvironment]"),
    ("weather", "altitude_gated"): dict(
        action="create_niagara_effect", domain="niagara", status="missing",
        note="right domain, but no altitude-gate contract exists",
        source="[UNVERIFIED: agent-reported]"),

    ("lighting", "*"): dict(
        action="configure_lighting", domain="environment", status="missing",
        note="no goal-level lighting surface; see COVERAGE_PLAN Part V",
        source="[VERIFIED: docs/COVERAGE_PLAN.md]"),
}

# ---------------------------------------------------------------------------
# ASSET RECIPES -- how a declared need bottoms out into things that exist.
#
# Reporting "you need a conifer mesh" is only half an answer: from an empty
# project there is no mesh, no material for it, and no texture for that. A
# dependency must expand RECURSIVELY until every leaf is a primitive the engine
# can actually make from parameters, and those leaves must be emitted FIRST.
#
# The floor is create_procedural_texture: 7 parametric generators, no library
# lookup, real from an empty project
# [VERIFIED: schemas/domains/materials/create_procedural_texture.schema.json].
#
# The two rungs above it do not exist yet. They are declared here anyway, with
# status=missing, so the plan shows the CORRECT shape and names exactly which
# rung cannot execute -- rather than stopping at "unresolved" and making the
# agent rediscover the cascade.
# ---------------------------------------------------------------------------
ASSET_RECIPES = {
    "texture": dict(
        action="create_procedural_texture", domain="material", prefix="tex",
        status="supported", needs=[], floor=True,
        source="[VERIFIED: schemas/domains/materials/create_procedural_texture.schema.json]"),
    "material": dict(
        action="create_master_material", domain="material", prefix="mat",
        status="unexposed", floor=False,
        needs=[("texture", "base_color"), ("texture", "normal")],
        source="[VERIFIED: UeremcpMaterialMasterBuilder::EnsureMasterMaterial builds a "
               "real master from Features[] via MaterialEditingLibrary, called at "
               "UeremcpMaterialService.cpp:1007 -- but only from CreateVfxMaterial. "
               "No AICallable action takes Features[] directly.]"),
    "static_mesh": dict(
        action="submit_mesh_ops", domain="mesh", prefix="mesh",
        status="unexposed", floor=False, needs=[("material", "surface")],
        source="[VERIFIED: GeometryScriptingCore is a dependency at "
               "UeremcpEnvironment.Build.cs:37 and AppendBox is used at "
               "UeremcpEnvironmentService.cpp:1244 -- welded inside place_structures. "
               "No mesh-authoring action exists.]"),
    "niagara_system": dict(
        action="create_niagara_effect", domain="niagara", prefix="fx",
        status="supported", floor=False, needs=[("material", "sprite")],
        source="[VERIFIED: tools/registry_snapshot.json]"),
}

# Generator parameters per slot. Deliberately parametric, never a preset name:
# a name would reintroduce exactly the library dependency this cascade removes.
SLOT_SPECS = {
    "base_color": {"generate": "noise", "dimensions": [1024, 1024]},
    "normal":     {"generate": "noise", "dimensions": [1024, 1024], "as_normal": True},
    "surface":    {"shading_model": "default_lit"},
    "sprite":     {"shading_model": "unlit", "blend_mode": "additive"},
}


def slug(role: str) -> str:
    return role.replace(".", "_").replace("-", "_")


def expand_to_floor(role, asset_type, hint, ops, emitted, blocked, chain=()):
    """Depth-first expansion. Emits dependencies BEFORE the thing needing them.

    Returns the operation id satisfying `role`, or None if the asset type has no
    recipe (the caller reports that as unmapped rather than guessing).
    """
    recipe = ASSET_RECIPES.get(asset_type)
    if recipe is None:
        return None
    oid = "%s_%s" % (recipe["prefix"], slug(role))
    if oid in emitted:
        return oid
    if oid in chain:                      # recipes are a DAG; guard anyway
        return oid

    dep_ids = []
    for sub_type, slot in recipe["needs"]:
        sub = expand_to_floor("%s.%s" % (role, slot), sub_type,
                              dict(SLOT_SPECS.get(slot, {}), derived_from=role),
                              ops, emitted, blocked, chain + (oid,))
        if sub:
            dep_ids.append(sub)

    emitted.add(oid)
    if recipe["status"] != "supported":
        blocked[recipe["action"]] = {
            "action": recipe["action"], "domain": recipe["domain"],
            "status": recipe["status"], "source": recipe["source"],
            "fix": ("expose the existing capability as an action -- a toolset "
                    "wrapper over verified C++, not new implementation"
                    if recipe["status"] == "unexposed" else
                    "implement; no capability exists"),
            "blocks_roles": sorted(set(
                blocked.get(recipe["action"], {}).get("blocks_roles", []) + [role])),
        }
    ops.append({
        "id": oid,
        "action": recipe["action"],
        "depends_on": dep_ids,
        "specification": dict(hint or {}, role=role),
        "_floor": bool(recipe.get("floor")),
        "_executable": recipe["status"] == "supported",
        "_why": ("primitive floor -- parametric, needs nothing"
                 if recipe.get("floor") else
                 "base asset for %s" % role),
    })
    return oid


# Apply order. Unlike a tool ranking, this is a real dependency order: you
# cannot place water or structures before the terrain they sit on.
DOC_ORDER = [
    ("assets",     0, "generated meshes/materials must exist before anything binds them"),
    ("terrain",    1, "everything else is placed on the landscape"),
    ("water",      2, "carves into terrain"),
    ("structures", 3, "sits on terrain"),
    ("foliage",    4, "scattered over terrain, avoids water and structures"),
    ("weather",    5, "attaches to the finished world"),
    ("lighting",   5, "attaches to the finished world"),
]
RANK = {s: r for s, r, _ in DOC_ORDER}
WHY = {s: w for s, _, w in DOC_ORDER}

# Structural prerequisites between world sections -- what must physically
# exist before a section can be placed, independent of asset dependencies.
SECTION_PREREQS = {
    "water":      ("terrain",),
    "structures": ("terrain",),
    "foliage":    ("terrain",),
    "weather":    (),
    "lighting":   (),
}

SINGLETON = {"terrain", "weather", "lighting"}
LISTY = {"assets", "water", "structures", "foliage"}


# ---------------------------------------------------------------------------
# Document plumbing
# ---------------------------------------------------------------------------
def content_hash(doc) -> str:
    """ADR-0006 revision. Excludes the revision field itself."""
    body = {k: v for k, v in doc.items() if k != "revision"}
    blob = json.dumps(body, sort_keys=True, separators=(",", ":"))
    return "sha256:" + hashlib.sha256(blob.encode("utf-8")).hexdigest()[:16]


def empty_world(level: str) -> dict:
    doc = {
        "doc_version": DOC_VERSION,
        "kind": "world",
        "level": level,
        "assets": [],
        "terrain": None,
        "water": [],
        "structures": [],
        "foliage": [],
        "weather": None,
        "lighting": None,
    }
    doc["revision"] = content_hash(doc)
    return doc


def kind_of(section: str, entity) -> str:
    if entity is None:
        return "*"
    if section == "water":
        return entity.get("type", "*")
    if section == "structures":
        return entity.get("kind", "*")
    if section == "weather":
        return "cycle" if entity.get("cycle") else "attach"
    if section == "terrain":
        return "shore" if entity.get("shore") else "*"
    return "*"


def capability(section: str, entity):
    cap = CAPABILITIES.get((section, kind_of(section, entity)))
    if cap is None:
        cap = CAPABILITIES.get((section, "*"))
    return cap


# ---------------------------------------------------------------------------
# The reconciler -- a pure function of (actual, desired)
# ---------------------------------------------------------------------------
def diff(actual: dict, desired: dict) -> list[dict]:
    changes = []
    for section in [s for s, _, _ in DOC_ORDER]:
        a, d = actual.get(section), desired.get(section)
        if section in SINGLETON:
            if d and not a:
                changes.append(dict(op="create", section=section, id=section, value=d))
            elif d and a and d != a:
                changes.append(dict(op="update", section=section, id=section, value=d))
            elif a and not d:
                changes.append(dict(op="delete", section=section, id=section, value=a))
            continue

        ai = {e["id"]: e for e in (a or [])}
        di = {e["id"]: e for e in (d or [])}
        for eid, ent in di.items():
            if eid not in ai:
                changes.append(dict(op="create", section=section, id=eid, value=ent))
            elif ai[eid] != ent:
                changes.append(dict(op="update", section=section, id=eid, value=ent))
        for eid, ent in ai.items():
            if eid not in di:
                changes.append(dict(op="delete", section=section, id=eid, value=ent))
    changes.sort(key=lambda c: RANK[c["section"]])
    return changes


def collect_dependencies(desired: dict) -> list[dict]:
    """Roles a section consumes but does not own, with nothing satisfying them.

    This is the Part VI/VII contract expressed as a property of the document:
    a `role` and a `hint`, never a bare asset path.
    """
    have = {a.get("role") for a in desired.get("assets") or []}
    unresolved = []
    for section in ("foliage", "structures"):
        for ent in desired.get(section) or []:
            for dep in ent.get("needs") or []:
                role = dep["role"]
                if role in have:
                    continue
                unresolved.append({
                    "role": role,
                    "asset_type": dep.get("asset_type", "static_mesh"),
                    "required_by": "%s[%s]" % (section, ent["id"]),
                    "reason": "no asset in doc.assets declares role '%s'" % role,
                    "satisfied_by": {
                        "domain": dep.get("domain", "mesh"),
                        "action": dep.get("action", "generate_mesh"),
                    },
                    "suggested_spec": dep.get("hint", {}),
                })
    return unresolved


def reconcile(desired: dict, actual: dict | None = None,
              generate_if_missing: bool = True) -> dict:
    """Desired-state apply. Emits ONE plan conforming to batch/plan.schema.json."""
    actual = actual or empty_world(desired.get("level", "/Game/Maps/Untitled"))
    changes = diff(actual, desired)
    unresolved = collect_dependencies(desired)

    operations, refused, gaps = [], [], []
    section_ops: dict[str, list[str]] = {}

    # generate_if_missing: expand every declared need down to the primitive
    # floor and emit base assets FIRST.
    #
    # Deduplicated BY ROLE across the whole cascade, not by consumer: two
    # structures needing the same stone material produce ONE material op and one
    # shared pair of textures. Emitting twice violates the unique-id rule in
    # batch/plan.schema.json and builds the asset twice.
    blocked_rungs: dict[str, dict] = {}
    role_to_op: dict[str, str] = {}
    if generate_if_missing and unresolved:
        emitted: set[str] = set()
        cascade: list[dict] = []
        for dep in unresolved:
            if dep["role"] in role_to_op:
                continue
            oid = expand_to_floor(dep["role"], dep["asset_type"],
                                  dep.get("suggested_spec"),
                                  cascade, emitted, blocked_rungs)
            if oid:
                role_to_op[dep["role"]] = oid
        operations.extend(cascade)

    for ch in changes:
        section, ent = ch["section"], ch["value"]
        if section == "assets":
            continue
        cap = capability(section, ent)
        if cap is None:
            gaps.append({"path": "%s[%s]" % (section, ch["id"]),
                         "reason": "no capability mapped"})
            continue
        if cap["status"] in ("stub", "missing"):
            # The lying-catalog gate. Refuse; never quietly become a river.
            refused.append({
                "path": "%s[%s]" % (section, ch["id"]),
                "requested": kind_of(section, ent),
                "action": cap["action"],
                "status": cap["status"],
                "reason": cap["note"],
                "source": cap["source"],
            })
            continue

        # Precise dependencies, not "everything of a lower rank". An op depends
        # on the assets ITS OWN roles required, plus the structural prerequisite
        # for its section. Over-broad depends_on is not merely untidy: it
        # serialises work the executor could run in parallel and misreports the
        # graph to anyone reading the plan.
        rank = RANK[section]
        depends = [role_to_op[d["role"]] for d in (ent.get("needs") or [])
                   if d.get("role") in role_to_op]
        for prereq in SECTION_PREREQS.get(section, ()):
            depends.extend(oid for oid in section_ops.get(prereq, []))
        depends = sorted(set(depends))
        oid = "%s_%s" % (section, ch["id"])
        operations.append({
            "id": oid,
            "action": cap["action"],
            "depends_on": depends,
            "specification": ent,
            "_why": WHY[section],
        })
        section_ops.setdefault(section, []).append(oid)

    status = "completed"
    if refused or gaps:
        status = "blocked" if not operations else "partially_completed"
    elif unresolved and not generate_if_missing:
        status = "partially_completed"

    out = {
        "status": status,
        "base_revision": actual["revision"],
        "target_revision": content_hash(desired),
        "round_trips": 1 if status != "blocked" else 0,
        "plan": {
            "transaction": {"atomic": True, "rollback_on_failure": True,
                            "compile_policy": "at_boundaries"},
            "operations": operations,
            "on_failure": "rollback_all",
        },
    }
    if unresolved:
        out["unresolved_dependencies"] = unresolved
        out["dependency_policy"] = ("expanded to primitive floor (generate_if_missing=true)"
                                    if generate_if_missing else "declared only")
    if blocked_rungs:
        # The cascade is correct but cannot fully execute. Say which rung, and
        # say it once -- not once per consumer.
        out["blocked_rungs"] = sorted(blocked_rungs.values(), key=lambda r: r["action"])
        out["blocked_rungs_contract"] = (
            "The plan shows the correct build order down to the primitive floor. "
            "Operations marked _executable=false name a domain action that does "
            "not exist yet; nothing above them can run until it does.")
        if out["status"] == "completed":
            out["status"] = "partially_completed"
    if refused:
        out["refused"] = refused
        out["refusal_contract"] = (
            "An advertised value the implementation cannot honour is REFUSED, "
            "never substituted. Silent downgrade is the cubes-for-trees defect.")
    if gaps:
        out["unmapped"] = gaps
    return out


# ---------------------------------------------------------------------------
# The nudge: tell an agent it is doing it the expensive way, without blocking it
# ---------------------------------------------------------------------------
DOC_BACKED = {
    "create_landscape", "create_water_body", "place_structures",
    "scatter_foliage", "attach_weather", "build_environment",
    "configure_lighting",
}


class Advisor:
    """Attaches a routing_advisory to direct domain calls. Never fails the call.

    Rationale for advisory-not-error: the agent may be right. A hard block on a
    legitimate single call teaches the agent the tool is broken, which is worse
    than the round trips it saves. Severity escalates with evidence instead.
    """

    def __init__(self):
        self.direct_calls: list[str] = []

    def observe(self, action: str) -> dict | None:
        if action not in DOC_BACKED:
            self.direct_calls.clear()
            return None
        self.direct_calls.append(action)
        n = len(set(self.direct_calls))
        if n == 1:
            return {
                "severity": "info",
                "message": ("'%s' edits a region of the world document. "
                            "ReadWorld/ApplyWorld does this as one document you "
                            "can also read back and revise." % action),
                "alternative": {"action": "read_world",
                                "then": "apply_world",
                                "cost": "2 round trips for any number of edits"},
            }
        return {
            "severity": "warn" if n < 4 else "strong",
            "message": ("%d document-backed domains called separately (%s). "
                        "One apply_world would have covered all of them in a "
                        "single transaction with rollback."
                        % (n, ", ".join(sorted(set(self.direct_calls))))),
            "observed_round_trips": len(self.direct_calls),
            "would_have_been": 2,
            "alternative": {"action": "read_world", "then": "apply_world"},
        }


# ---------------------------------------------------------------------------
# Demo: the castle intent, as one document
# ---------------------------------------------------------------------------
def castle_doc() -> dict:
    doc = empty_world("/Game/Maps/Castle")
    doc["terrain"] = {
        "size_km": 4.0, "max_altitude_m": 1800, "snowline_m": 1150,
        "features": [{"kind": "mountain_range", "coverage": 0.45, "ridge_seed": 7},
                     {"kind": "valley", "coverage": 0.3}],
    }
    doc["water"] = [{"id": "mountain_lake", "type": "lake",
                     "centre": [1200, 800], "radius_m": 220}]
    doc["structures"] = [
        {"id": "keep", "kind": "castle_keep", "transform": {"loc": [0, 0, 0]},
         "needs": [{"role": "structure.stone_wall", "asset_type": "material",
                    "domain": "material", "action": "create_material",
                    "hint": {"kind": "cut_stone", "weathering": 0.6}}]},
        {"id": "curtain", "kind": "barrier_wall", "transform": {"loc": [0, 0, 0]},
         "ring_radius_m": 90,
         "needs": [{"role": "structure.stone_wall", "asset_type": "material",
                    "domain": "material", "action": "create_material",
                    "hint": {"kind": "cut_stone", "weathering": 0.6}}]},
    ]
    doc["foliage"] = [{
        "id": "conifers", "density_per_100m2": 12, "altitude_band_m": [0, 1400],
        "needs": [{"role": "foliage.conifer", "asset_type": "static_mesh",
                   "domain": "mesh", "action": "generate_mesh",
                   "hint": {"kind": "conifer"}}],
        # The altitude-keyed material swap the user asked for, as data.
        "variants": [{"above_m": 1150, "material_role": "foliage.snow_frosted"}],
    }]
    doc["weather"] = {"cycle": [{"state": "clear", "weight": 0.4},
                                {"state": "rain", "weight": 0.2},
                                {"state": "sleet", "weight": 0.15},
                                {"state": "hail", "weight": 0.1},
                                {"state": "snow", "weight": 0.15,
                                 "gate": {"above_m": 1150}}],
                      "day_night": {"cycle_minutes": 20},
                      "wind": {"base_speed": 4.0}}
    doc["revision"] = content_hash(doc)
    return doc


def gaps_report() -> int:
    live = set()
    if os.path.exists(SNAPSHOT):
        with open(SNAPSHOT, encoding="utf-8") as fh:
            snap = json.load(fh)
        for ts, v in snap["toolsets"].items():
            for t in (v.get("tools") or {}):
                live.add(t.lower().replace("_", ""))
    print("capability map vs registry_snapshot.json\n")
    print("%-11s %-18s %-9s %-8s %s" % ("SECTION", "KIND", "STATUS", "IN SNAP", "NOTE"))
    rc = 0
    for (section, kind), cap in sorted(CAPABILITIES.items()):
        present = cap["action"].replace("_", "") in live
        if cap["status"] != "supported":
            rc = 1
        print("%-11s %-18s %-9s %-8s %s" % (
            section, kind, cap["status"], "yes" if present else "no", cap["note"][:52]))
    print("\nsnapshot has no environment toolset at all -- it predates the module.")
    print("Nothing here is a live-verified gap; it is a declared map to be checked")
    print("against a fresh dump once branches are merged to main.")
    return rc


def main() -> int:
    args = sys.argv[1:]
    cmd = args[0] if args else "--demo"

    if cmd == "--gaps":
        return gaps_report()

    if cmd == "--read":
        level = args[1] if len(args) > 1 else "/Game/Maps/Untitled"
        print(json.dumps(empty_world(level), indent=1))
        return 0

    if cmd == "--apply":
        with open(args[1], encoding="utf-8") as fh:
            desired = json.load(fh)
        print(json.dumps(reconcile(desired), indent=1))
        return 0

    if cmd == "--advise":
        adv = Advisor()
        for action in (args[1:] or ["create_water_body"]):
            print("call: %s" % action)
            r = adv.observe(action)
            print(json.dumps(r, indent=1) if r else "  (no advisory)")
            print()
        return 0

    # --demo
    desired = castle_doc()
    print("=" * 72)
    print("STEP 1  read_world -> empty document (2 sections shown)")
    print("=" * 72)
    base = empty_world("/Game/Maps/Castle")
    print(json.dumps({k: base[k] for k in ("kind", "level", "revision", "terrain")}, indent=1))

    print("\n" + "=" * 72)
    print("STEP 2  the LLM writes the whole ask into ONE document")
    print("=" * 72)
    print(json.dumps(desired, indent=1))

    print("\n" + "=" * 72)
    print("STEP 3  apply_world -> reconcile -> ONE execute_plan")
    print("=" * 72)
    result = reconcile(desired)
    print(json.dumps(result, indent=1))

    print("\n" + "=" * 72)
    print("STEP 4  the same goal via direct calls -- what the advisor says")
    print("=" * 72)
    adv = Advisor()
    for action in ("create_landscape", "create_water_body",
                   "place_structures", "scatter_foliage", "attach_weather"):
        r = adv.observe(action)
        if r:
            print("%-20s %-8s %s" % (action, r["severity"], r["message"][:88]))

    ops = len(result["plan"]["operations"])
    print("\nsummary: %d operations, 1 apply call, %d refusals, %d delegated deps"
          % (ops, len(result.get("refused", [])),
             len(result.get("unresolved_dependencies", []))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
