# RB — Ice wall VFX plugin gaps (fieldtest)

**Audience:** UEREMCP product owner / MCP author  
**Date:** 2026-07-31  
**Trigger:** Ice wall VFX agent run on `ueremcp_fieldtest` (plus related hard gaps)  
**Peers:** [`RB-MCP-hard-gaps-fieldtest.md`](./RB-MCP-hard-gaps-fieldtest.md), [`RB-unreal-watch-miss-report.md`](./RB-unreal-watch-miss-report.md), [`RB-07-niagara.md`](./RB-07-niagara.md), [`RB-08-materials-and-textures.md`](./RB-08-materials-and-textures.md)

**User bar:** Agents must source or create reasonable VFX without ArtKit excuses. Tools must not force the wrong material purpose (projectile-core additive whiteout for ice barriers). Prefer silent, dialog-free import paths over “watch will dismiss Import Content.”

**Out of scope here:** UnrealWatch / REAgentTools (separate product). Note only — do not re-fix watch in this change set.

---

## Gap index

| ID | Priority | Status (this change set) | Title |
|----|----------|--------------------------|-------|
| IW-001 | **P0** | **Fixed** | Silent FBX / StaticMesh import (no Interchange UI) |
| IW-002 | **P0** | **Fixed** | Ice / translucent Fresnel VFX material purposes |
| IW-003 | **P0** | **Fixed** | Mutator `request_id` → structured `next_args` |
| IW-004 | **P1** | **Fixed** | `CaptureWorldFrames` framing / near-black |
| IW-005 | **P1** | **Partial** | Niagara ground mist density / color / radius |
| IW-006 | **P2** | **Documented** | Visual iterate-against-screenshot (stub / skip) |
| IW-007 | — | **Note only** | Watch is separate (REAgentTools) |

---

## IW-001 — Silent FBX / StaticMesh import

| | |
|--|--|
| **Symptom** | Interchange **Import Content** modal blocks the game thread; MCP hangs (`WinError 10054/10061`). Agents that drag/drop or use non-automated import paths stall the whole session. |
| **Root cause** | MCP import composed Epic `StaticMeshTools.import_file`, which can still surface Interchange UI depending on project Interchange settings. There was no guaranteed `bAutomated` / unattended AssetTools path inside `import_mesh_for_world`. |
| **Proposed API** | `import_mesh_for_world` (and any MCP mesh import) must use `UAssetImportTask` with `bAutomated=true` (+ unattended guard) as the primary path. If a dialog still cannot be suppressed, reject with a clear code **before** hanging — never leave the agent waiting on a modal. Prefer this over Watch dismiss as the primary fix. |
| **Acceptance** | Given an FBX on disk; When `import_mesh_for_world`; Then StaticMesh appears under `target.asset_path` with **no** Import Content modal; MCP stays responsive. Bounds gate (`MESH_BOUNDS_MISMATCH`) still applies. |
| **Priority** | P0 |

---

## IW-002 — Ice / translucent Fresnel VFX materials

| | |
|--|--|
| **Symptom** | Agents needing an ice wall / crystal barrier were forced onto `elemental_projectile_core` → Unlit **Additive** masters with high emissive → white blowout. Cyan Fresnel detail from reference was unrecoverable. Generic purposes like `ice` / `projectile_core` were rejected as “not yet implemented.” |
| **Root cause** | `CreateVfxMaterial` purpose gate only wired `elemental_projectile_core\|trail` and `fireball_*`. Feature graph always set `BLEND_Additive`. Ice element defaults used emissive_scale **6.0** tuned for projectiles, not barriers. |
| **Proposed API** | Accept purposes `ice_crystal` and `elemental_ice_barrier` (aliases ok). Build **translucent** Unlit Fresnel masters (`M_Ueremcp_IceCrystal_*`) with lower default emissive. Alias common misspellings (`projectile_core` → elemental core). Stop telling agents “use projectile core for ice walls.” |
| **Acceptance** | `purpose=elemental_ice_barrier` + `element=ice` creates a translucent master; Capture of a mesh with that MI is not full-white blowout; Fresnel/opacity path present. |
| **Priority** | P0 |

---

## IW-003 — Mutator `request_id` footgun

| | |
|--|--|
| **Symptom** | Mutating tools fail or queue forever when `request_id` is missing/empty or agents rotate ids on every `MUTATOR_BUSY` retry. Recovery prose said “retry with SAME request_id” but did not hand back a usable id in `error.next_args`. |
| **Root cause** | Envelope allows omitting `request_id`. Mutator acquire then fails with a plain string reason, or queues without a mergeable patch. |
| **Proposed API** | Missing/empty `request_id` on mutating dispatch → `rejected` + `error.code=REQUEST_ID_REQUIRED` + `error.next_args.request_id=<usable guid>`. `MUTATOR_BUSY` responses include `next_args.request_id` set to the id that owns/queues the slot (retry patch). |
| **Acceptance** | Omit `request_id` on a mutating call → structured reject with mergeable `next_args`; merge + retry acquires. Busy response carries the same id to keep retrying. |
| **Priority** | P0 |

---

## IW-004 — `CaptureWorldFrames` unreliable framing

| | |
|--|--|
| **Symptom** | `CaptureWorldFrames` often returns near-black / wrong framing for hand-placed VFX actors in Untitled levels. `EditorAppToolset.CaptureViewport` with an explicit transform was the reliable proof path in field. |
| **Root cause** | World capture hard-aims a disposable **StageOrigin** (`49300,0,0`) used by effect/material stages — not the actors the agent just placed near the level origin. |
| **Proposed API** | Frame around world content bounds (or `specification.focus_*` when present). If mean luminance stays near-black, mark `partially_completed` / capability note with **CaptureViewport** fallback — do not claim a good proof. |
| **Acceptance** | Place a lit mesh near origin; `CaptureWorldFrames` mean luminance is not near-zero sky; or response honestly points to CaptureViewport. |
| **Priority** | P1 |

---

## IW-005 — Niagara ground mist weak vs reference

| | |
|--|--|
| **Symptom** | Precipitation / mist roles (`HangingParticulates`) look thin vs ice-wall ground fog reference. Agents rediscover modules instead of dialing density/color/radius. |
| **Root cause** | `CreateNiagaraEffect` exposes `intensity` / `scale` / colors but not mist-oriented knobs (`density`, `radius`, mist color aliases). Template choice is fixed; no goal-level “thicker fog” adapt. |
| **Proposed API** | Map `parameters.density` / `mist_density` / `radius` / mist color aliases onto `User.*` params for precipitation/mist creates. Longer-term: mist-specific template or `adapt_niagara_effect` (see HG-003). |
| **Acceptance** | Create precipitation/mist with `density` + color → User params present on system; second create with higher density does not require stack surgery. |
| **Priority** | P1 (params shipped; template swap deferred) |

---

## IW-006 — Visual iterate-against-screenshot (document)

| | |
|--|--|
| **Symptom** | User pastes a reference (“more like this ice wall”); agent captures endlessly with no scored delta. Same class as **HG-001**. |
| **Root cause** | Capture tools return PNGs + coarse luminance; no `compare_frames_to_reference`. |
| **Proposed API** | `compare_frames_to_reference({ capture_path, reference_path, metrics:[...] })` → deltas + `next_args` hints. Stub returning `COMPARE_UNSUPPORTED` is acceptable until shipped. |
| **Acceptance** | Documented; full implement deferred (large). Do not block ice-wall P0s on this. |
| **Priority** | P2 — **deferred** |

---

## IW-007 — Watch is separate (note only)

Import Content **detection/dismiss** belongs to REAgentTools UnrealWatchMCP (v0.6+). UEREMCP’s duty is **not opening the dialog** (IW-001). Do not re-fix watch here. See [`RB-unreal-watch-miss-report.md`](./RB-unreal-watch-miss-report.md) §10.

---

## Implementation checklist (this change set)

1. `import_mesh_for_world` → silent `UAssetImportTask` primary path  
2. `ice_crystal` / `elemental_ice_barrier` + translucent feature graph + ice emissive defaults  
3. `REQUEST_ID_REQUIRED` + `MUTATOR_BUSY` `next_args.request_id`  
4. `CaptureWorldFrames` content-aware framing + near-black honesty  
5. Niagara mist `density` / `radius` / color aliases on create  
6. Cross-link from hard-gaps doc  

---

## Verify

- `python tools/check_operation_catalog.py`  
- `python tests/run_unit_tests.py` (world_doc / materials / schema slices)  
- UE `RunUAT BuildPlugin` when editor mutex free  
- Live: import FBX via MCP (no Import Content); `CreateVfxMaterial` ice barrier; omit `request_id` → merge `next_args`; place actor near origin → `CaptureWorldFrames` not near-black  
