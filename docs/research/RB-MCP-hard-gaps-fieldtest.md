# RB — MCP hard gaps (ueremcp_fieldtest stress test)

**Audience:** UEREMCP product owner / MCP author  
**Date:** 2026-07-31  
**Project:** `ueremcp_fieldtest` (MCP capability stress test — **not** the main game)  
**Branch context:** `ws-11-northridge-remaining-impl` @ `1a9c05b` / `efd3675` + UeremcpUI ship; REAgentTools UnrealWatch miss docs  
**Fieldtest mirror:** `ueremcp_fieldtest/docs/MCP_Hard_Gaps_Fieldtest.md`  
**Peers:** [`RB-UI-tooling-field-report.md`](./RB-UI-tooling-field-report.md), [`RB-Northridge-validation-report.md`](./RB-Northridge-validation-report.md), [`RB-Northridge-efficiency-verify.md`](./RB-Northridge-efficiency-verify.md), [`RB-unreal-watch-miss-report.md`](./RB-unreal-watch-miss-report.md), fieldtest `MCP_Field_Report_Northridge.md` / `MCP_Backlog_API_Shapes.md`

**Live probe (this write-up):** `user-unreal-mcp` ready (Niagara / Material / Systems / VisualCapture / Animation / UI / Environment present). `user-unreal-watch` still `serverStatus=error`. `user-blender` ready.

---

## 1. User bar vs excuses

This project exists to **stress MCP agents**, not to ship Northridge as a product. The user pushback-corrected a narrative that treated missing ArtKits as blockers. Correct bar:

| Intent | Correct agent duty | Invalid excuse |
|--------|--------------------|----------------|
| **"Make rain"** | Source or create realistic droplets (Niagara / `AttachWeather` / `CreateNiagaraEffect` precipitation), place in level, **iterate** on feedback (harder / softer / lens hits) | "Need an ArtKit / weather pack first" |
| **"Put trees"** | Place **reasonable non-block** trees now (`find_project_assets` → PCG / Northridge meshes, or Blender import); swap species later | "Need a foliage pack before any trees" |
| **UI from collage** | Match **reference LAYOUT** (regions, grids, paper-doll structure, hotbar row). Pixel-perfect wrought-iron chrome is optional. Icons may later be mesh renders | "Can't proceed without icon atlas pack / CommonUI / serif font asset" |
| **Landscape look** | Source or synthesize textures, assign + paint, **visually compare** to user reference, adapt (too bright / dark / more stone) | "White landscape is fine until someone drops Quixel" |
| **ArtKit** | Optional upgrade path when quality bar rises | **Not** a prerequisite for a first reasonable result |

### Process failure vs tooling gap (be honest)

| Failure | Class | Why |
|---------|-------|-----|
| Inventory UI ~**5/10**, wrong layout vs collage | **Mostly agent process** + **partial tooling** | Layout regions *were* approximable with UMG primitives / later `create_inventory_sheet_from_spec`. Agent did not treat collage as a **layout contract** (measure regions, mirror structure). Tooling still lacks `layout_from_reference` scoring and SceneCapture-in-panel. |
| Demanding ArtKit before rain/trees | **Agent laziness / wrong narrative** | Precipitation + AttachWeather + find_project_assets + Blender already exist. Excusing first-pass quality with "no pack" is process failure. |
| Cylinder character preview | **Tooling** (mitigated by `spawn_character_preview`) | No `/Game` mannequin; agent shipped proxy. Helper now reports `mesh_source`. |
| White / unpainted landscape after material | **Was tooling** (LayerInfo + paint) | Fixed on branch; remaining gap is **texture fidelity + visual adapt loop**, not paint existence. |
| Floating castle / trees in walls | **Was tooling** (snap / clear / place) | Mostly closed by goal Env APIs; leftover is species quality + visual iterate. |
| ~43 CaptureViewport thrash on Screen UMG | **Tooling honesty** (fixed) | `bShowUI` lie; now `capture_ui_frame` rejects screen UMG honestly. |
| describe_toolset spam (~61 Env) | **Tooling** (mitigated) | Slim/ETag exists; agents that ignore it still burn tokens — partly instruction. |
| Watch never catching closed editor | **External tooling** | Cursor discovery dead + pull-only + offline `agent_instruction` lie. |

**Rule of thumb:** If a default-reasonable result can be authored from engine samples, procedural synth, Blender, PolyHaven, or project AssetRegistry hits, **"need a pack"** is an agent failure. If the agent cannot *see* whether the result matches a reference (layout/visual), that is a **tooling** gap — even when create APIs exist.

---

## 2. Already fixed (brief — do not re-litigate)

| Area | What closed | Evidence |
|------|-------------|----------|
| Invented `/Game/Meshes/SM_Pine` | `find_project_assets` | Efficiency verify A: 1 RTT |
| Silent bad FBX scale | `import_mesh_for_world` + bounds gate (+ delete) | Efficiency C |
| Floating prefab / canopy Z | `place_prefab_on_landscape` + LandscapeZAt | Efficiency D / `efd3675` |
| White landscape LayerInfo | `paint_landscape_layers` auto LayerInfo | Efficiency E |
| Stage wipe / additive env | Stage-presence gate + scoped replace | Efficiency F |
| Scale recovery prose-only | `error.next_args` (NEEDLE both axes) | Efficiency G: 2 RTT |
| Multi-water / flatten_pad | `body_type` lake/ocean + SetHeightData pad | `1a9c05b` (live re-proof as noted in validation) |
| Mutator FIFO forever-poll | Stale clear + `MUTATOR_BUSY` | Code on branch |
| UI domain / ResolveIntent abstain | `UeremcpUI` + CreateWidgetFromSpec / inventory sheet / show in world | UI field report Fixed |
| Screen UMG capture lie | `capture_ui_frame` honest reject | UI Fixed |
| Character preview helper | `spawn_character_preview` mesh_source | UI Fixed |
| GetStarted watch advertising | Points at check_unreal / status (when watch works) | Validation |

Core Northridge discovery / import / place / paint / additive / next_args path can land in **low-teens RTTs** when agents follow slim describe + matching terrain blocks. That is **not** the remaining product risk.

---

## 3. Still hard / finicky / high-RTT / missing

Prefer **new** IDs (`HG-*`). Known leftovers kept only if still true after WS-11 / UI ship.

### HG-001 — No visual-iterate-against-reference loop

| | |
|--|--|
| **Symptom** | User pastes screenshots ("too bright", "needs stone", "castle too small", inventory collage). Agent captures endlessly (~36–43 CaptureViewport) but has **no scored compare** to the reference. Adapt is guesswork. |
| **Why hard** | `CaptureWorldFrames` / `CaptureViewport` / `CaptureEffectFrames` return PNGs + coarse pixel stats. No `compare_to_reference` (histogram / region layout / template match / "brighter than ref by X"). Beauty gates are explicitly out of scope — but **directional adapt** is still missing. |
| **RTT / cost** | Quality-fix pass: dozens of captures + human read; inventory ~43 CaptureViewport mentions; Northridge ~36 CaptureViewport. |
| **Goal API** | `compare_frames_to_reference({ capture_path, reference_path, metrics:["luminance","region_layout","dominant_hue"], regions? })` → `{ deltas, next_args hints }` (e.g. darken albedo 20%, enlarge scale). |
| **Acceptance** | Given landscape capture + stone reference; When compare; Then returns luminance delta + suggested material/paint patch that, after one apply, reduces delta below threshold **or** honest `COMPARE_UNSUPPORTED` with next_args. |

**Class:** tooling (blocks intent-matching).

---

### HG-002 — UI layout-from-reference (mandatory duty, weak tooling)

| | |
|--|--|
| **Symptom** | Inventory sheet scored **~5/10** vs collage — layout ~7 claimed but structure still drifted (wrong proportions / missing chrome hierarchy). User bar: **layout must match**; chrome fidelity optional. |
| **Why hard** | `create_inventory_sheet_from_spec` helps grids but does not ingest a collage. No region extractor / overlay proof. Agents can (and should) manually map regions — many didn't treat collage as a contract. |
| **RTT / cost** | Full inventory rebuild after crash + multi-angle framing thrash; hours of UMG tree work. |
| **Goal API** | `create_widget_layout_from_reference({ reference_image, layout_hints })` **or** `score_widget_layout_vs_reference` after create; plus agent rule: refuse "done" until layout score ≥ bar. |
| **Acceptance** | Side-by-side CaptureViewport of World WC vs collage; structural regions (header / paper-doll / grids / hotbar) align within tolerance; aesthetic chrome may remain flat. |

**Class:** **process + tooling**. Layout miss without trying region mapping = agent failure. Missing score/ingest = tooling.

---

### HG-003 — Rain / VFX adapt loop (harder / softer / lens hits)

| | |
|--|--|
| **Symptom** | "Make rain" then "harder" / "lens hits" requires rediscovering modules, user params, or rebuilding systems. Agents may stall asking for weather ArtKits. |
| **Why hard** | `CreateNiagaraEffect` + `AttachWeather` / BuildEnvironment precipitation path exist (`effect_type=precipitation`, roles rain+mist — RB-16). **Goal-level adapt** (`set_effect_params` / intensity / camera collision / splash) is thin. Epic Niagara stack edits = high describe + module RTT. Event-handler stacks still lossy (RB-07). Schema files often "not found" at describe-time. |
| **RTT / cost** | First rain via AttachWeather can be 1–3 RTT; iterate-to-look is unbounded without param contract. |
| **Goal API** | `adapt_niagara_effect({ asset, feedback: "harder rain|softer|lens_hits", patch: { intensity, spawn_rate, size, camera_collision }})` composing user-param writes + CaptureEffectFrames. ResolveIntent: "make rain" → weather/Niagara, **never** "import ArtKit". |
| **Acceptance** | Empty project: create rain → capture proves particles → adapt intensity up → second capture delta rises; "lens hits" enables splash/collision path or honest unsupported. |

**Class:** tooling for iterate; ArtKit demand = process failure.

---

### HG-004 — Default-reasonable foliage without a pack

| | |
|--|--|
| **Symptom** | Agents invent cubes/cones or refuse trees until a marketplace pack exists. User wants **non-block trees now**, species swap later. |
| **Why hard** | `find_project_assets` + ScatterFoliage work when meshes exist (PCG / Northridge). Empty-ish projects still need **synthesize/import** (Blender / PolyHaven) as first-class next_args — not a hard stop. Scatter often returns `failed_validation` **after** creating actors (confusing). |
| **RTT / cost** | Old: invent path loops; New happy path 1 RTT find + 1 scatter; species upgrade = Blender multi-step. |
| **Goal API** | `ensure_foliage_meshes({ roles:["tree"], quality:"reasonable", sources:["project","engine","blender_proc","polyhaven"] })` → resolved paths + optional import; then scatter. |
| **Acceptance** | Project with only Engine basics: ensure_foliage → non-cube tree mesh path → scatter instances > 0; no ArtKit required. |

**Class:** tooling (ensure/synth); refusing without pack = process.

---

### HG-005 — Surface / landscape material from image + adapt

| | |
|--|--|
| **Symptom** | Terrain flat-color or wrong look vs user reference ("needs stone", "too bright"). Agents hit VFX-only masters or expression-graph footguns. |
| **Why hard** | `CreateLandscapeMaterial` + paint exist. `CreateMasterMaterial` is **VFX-only** — snow/rock/grass tokens produce empty shells (**measured** honesty in tool text). `CreateProceduralTexture` is noise/mask oriented under test paths. No `create_surface_material_from_image` / albedo import+tile+roughness defaults. No brightness adapt tied to HG-001. Path jail: CreateVfxMaterial rejects `/Game/UI/...` (inventory hit). |
| **RTT / cost** | Northridge materials: multi-call invent + white landscape era; now mat+paint ~2 RTT structurally, fidelity still open. |
| **Goal API** | `create_or_adapt_surface_material({ purpose:"landscape_rock\|prop", reference_image?, albedo_path?, adjust:{exposure,saturation} })` + landscape layer bind. |
| **Acceptance** | Reference stone photo → material on plane/landscape → capture luminance within band of reference; "darker" feedback → one adapt call. |

**Class:** tooling.

---

### HG-006 — SceneCapture → widget / icon-from-mesh

| | |
|--|--|
| **Symptom** | Inventory needs live character **in panel** and later icons that are renders of pickup meshes. Agents put cylinder beside WC; Blender silhouette icons; no RT→Image brush pipeline. |
| **Why hard** | UI-MCP-016 still open. No `spawn_character_preview(presentation=scene_capture_to_widget)`. No `render_mesh_icon({ mesh, dest_texture, lighting })`. Capture toolsets stage for proof, not for binding into UMG. |
| **RTT / cost** | Manual WC framing thrash; icon gen via Blender (~30 PNGs) outside Unreal. |
| **Goal API** | `bind_scene_capture_to_image_widget` + `render_static_mesh_icon`. |
| **Acceptance** | Inventory CaptureViewport shows skeletal/proxy **inside** preview hole; slot icon is RT/texture of mesh under consistent light. |

**Class:** tooling (blocks inventory fidelity gate).

---

### HG-007 — Crash / modal / MCP death recovery (still P0 in field)

| | |
|--|--|
| **Symptom** | Unsaved WBP wiped after crash; Restore Packages + Crash Reporter block `:8000`; agents retry Unreal MCP into WinError 10054/10061; watch MCP unusable so no fail-fast. |
| **Why hard** | Editor session reliability is half host (UnrealWatch) half UEREMCP (default save helped UI create; BlueprintTools.create hang −32001 remains). Watch: Cursor discovery error **still live this session**; pull-only; offline `agent_instruction` lied ("clear") historically. |
| **RTT / cost** | Full inventory rebuild; hours lost; multitask agents thrash dead ports. |
| **Goal API** | Persist-by-default everywhere; `BlueprintTools.create` timeout envelope; UnrealWatch v0.4+ discoverable + `status=editor_offline` STOP; optional auto-dismiss Restore Packages when MCP connected. |
| **Acceptance** | Kill editor mid-UI-edit → relaunch → asset on disk OR honest recovery plan; closed editor → agents stop MCP within 1 watch call. |

**Class:** tooling (host + editor).

---

### HG-008 — Niagara graph / weather finicky depth

| | |
|--|--|
| **Symptom** | Beyond template create: reorder modules, event handlers, camera lens FX, custom renderers → Epic primitive soup + lossy inspect. |
| **Why hard** | RB-07: event handlers incomplete on topology; reorder lossy; script graphs out of POC; domain schema publishing often missing → describe friction. Mutator queue can still busy long Env ops (`MUTATOR_BUSY` better than hang, still finicky). |
| **RTT / cost** | High; often abandon to "good enough" rain. |
| **Goal API** | Keep templates for create; ship `adapt_*` + document lossy_areas; never require ArtKit for precipitation. |
| **Acceptance** | InspectSystem lists known lossy; adapt API covers intensity/size without stack surgery for rain/projectile. |

**Class:** tooling (deep VFX).

---

### HG-009 — Domains still thin: animation write, input, audio synth, sequencer/camera, lighting/PP

| | |
|--|--|
| **Symptom** | Live schemas: Animation = **read-only** (`ReadAnimBp`, `InspectMontage`). Systems audio = cue from **existing** waves (no synth / MetaSound blocked). No UEREMCP goal layer for Enhanced Input, post-process volumes, cinematic cameras, lighting scenarios — only Epic Sequencer/ControlRig primitives + EditorApp. |
| **Why hard** | Coverage toolset is honest but shallow; stress test barely entered these domains. |
| **RTT / cost** | Unknown in fieldtest (under-tested) — expect high when asked. |
| **Goal API** | Prioritize only when field intents hit them: `set_post_process`, `place_cine_camera`, `bind_enhanced_input_action`, `create_audio_from_file` import helper. |
| **Acceptance** | ResolveIntent routes; one-shot create for each; honest reject if MetaSound. |

**Class:** missing tools (under-tested).

---

### HG-010 — Discovery tax leftovers

| | |
|--|--|
| **Symptom** | Full `describe_toolset` Environment still ~55 KB; verbose `next_actions` on success; SchemaPublishing "domain schema file not found" for UI/Niagara/Material specs; `find_assets` often `[]` while `ListWidgetBlueprints` works; StrictDict forbids `.get` in `execute_tool_script`. |
| **Why hard** | Slim/ETag fixed the *availability* of cheap describe; agents + publishing path still wasteful. |
| **RTT / cost** | Old ~61 describe; new agents still burn 16+ describe on UI session. |
| **Goal API** | Default GetStarted → ExecutePlan examples; fix schema publish paths; soft StrictDict or document; unify asset find. |
| **Acceptance** | Happy-path rain/trees/UI without full toolset dump; describe index &lt; 4 KB. |

**Class:** tooling polish (still high token cost).

---

### HG-011 — Landscape sculpt / foundation beyond flatten_pad

| | |
|--|--|
| **Symptom** | Roads, beaches, castle pads, cliff integration need sculpt. `flatten_pad` shipped; general sculpt / brush / road carve still absent → Blender mesh or leave heightmap as-is. |
| **Why hard** | Heightmap edit surface exists for pad; no goal `sculpt_landscape` / `carve_road`. |
| **RTT / cost** | Manual / Blender workarounds in Northridge HF upgrade. |
| **Goal API** | `sculpt_landscape({ ops:[{type:flatten\|ramp\|noise, ...}] })`. |
| **Acceptance** | Road corridor flatter than surrounds; measured height stats. |

**Class:** tooling.

---

### HG-012 — Scatter / foliage messaging & species swap

| | |
|--|--|
| **Symptom** | Scatter creates forest then `failed_validation` on bank gates; agents think it failed. Species swap = re-scatter with new mesh, not a typed upgrade. |
| **Why hard** | Validation conflated with create status; no `replace_foliage_meshes({ from, to })`. |
| **Goal API** | Split `created` vs `validation`; `swap_foliage_species`. |
| **Acceptance** | Status `created_with_warnings` when instances exist; swap pine→oak preserves count band. |

**Class:** tooling (finicky UX).

---

### Known leftovers still true (short)

| ID | Note |
|----|------|
| UI-MCP-012 atlas | `import_icon_atlas` still missing; `set_slot_icon` Texture2D-only |
| UI-MCP-016 | SceneCapture in panel — folded into HG-006 |
| UI-MCP-008 | Restore Packages / create hang — folded into HG-007 |
| Watch Cursor connect | Still error live — HG-007 |
| Ocean/lake live re-proof | Code claimed; treat as verify debt if not re-shot |

---

## 4. Domains under-tested

Stress these next — highest learning per hour for the MCP product:

| Domain | Current floor | Field gap to probe |
|--------|---------------|-------------------|
| **Niagara adapt-from-sample** | CreateNiagaraEffect + Inspect + CaptureEffectFrames | Clone engine/project rain → adapt intensity/lens; measure RTT vs ArtKit excuse |
| **Material from image** | Landscape mat + procedural noise | Import photo → tileable albedo/roughness → assign → adapt brightness vs ref |
| **Visual diff loop** | Capture* pixel stats | Pair every user screenshot feedback with automated delta + next_args |
| **SceneCapture preview** | World WC beside panel | RT into inventory Image; mannequin require_visible |
| **Icon-from-mesh render** | Blender PNGs | Unreal mesh → icon texture → set_slot_icon |
| **Layout-from-reference** | Spec grids | Collage → region map → score ≥ bar before "done" |
| **Animation / Sequencer** | Read-only Anim + Epic Sequencer primitives | Idle pose / camera cut — expect missing goal APIs |
| **Audio** | CreateAudioCue from waves | Rain bed + footstep without MetaSound |
| **Input** | None goal-level | Hotbar 1–0 Enhanced Input bind |
| **Lighting / PP** | Manual Actor/CVar | Moody inventory stage / wet rain PP |
| **Landscape sculpt** | flatten_pad | Beach shelf + road carve |
| **Physics / RT** | PhysicsAsset toolset Epic | Interactable pickup + icon RT |

---

## 5. Top 15 ranked by "blocks matching user intent"

| Rank | ID | Blocks | Process vs tooling |
|-----:|----|--------|--------------------|
| 1 | **HG-001** | Visual iterate vs user screenshots (bright/dark/stone/scale) | Tooling |
| 2 | **HG-002** | UI collage **layout** match | Process + tooling |
| 3 | **HG-003** | Rain then harder/softer/lens without pack | Tooling (ArtKit excuse = process) |
| 4 | **HG-005** | Landscape/prop look from ref textures | Tooling |
| 5 | **HG-006** | Character-in-panel + icon-from-mesh | Tooling |
| 6 | **HG-007** | Crash/modal/watch — session death | Tooling |
| 7 | **HG-004** | Reasonable trees without marketplace pack | Tooling + process |
| 8 | **HG-008** | Deep Niagara iterate beyond templates | Tooling |
| 9 | **HG-011** | Pads/roads/beach sculpt | Tooling |
| 10 | **HG-010** | Describe/schema/find tax burns budget | Tooling (+ instruction) |
| 11 | **HG-012** | Foliage success/validation confusion + species swap | Tooling |
| 12 | **HG-009** | Input / PP / camera / anim write when asked | Missing |
| 13 | UI-MCP-012 | Icon atlas packing | Tooling |
| 14 | Scatter bank gates status | Agents re-scatter thinking failure | Tooling UX |
| 15 | Path jail / schema publish | CreateVfxMaterial / describe friction | Tooling |

---

## 6. Recommendations (product)

1. **Kill the ArtKit prerequisite narrative** in GetStarted / ResolveIntent / agent skills: rain → AttachWeather / CreateNiagaraEffect precipitation; trees → find → ensure → scatter; materials → landscape mat + import/synth.
2. **Ship HG-001 compare + next_args** before more create APIs — this fieldtest fails on *adapt*, not only *create*.
3. **Treat collage layout as a gate** for UI agents (score or checklist); chrome is secondary.
4. **SceneCapture-to-widget + mesh icons** unlock inventory fidelity without painted atlases.
5. **Fix UnrealWatch Cursor discovery** so closed-editor / modal is fail-fast — otherwise every other gap is multiplied by dead-port thrash.
6. Keep world-space diegetic UI as preferred proof path; do not reopen Screen UMG capture as silent success.

---

## Related

| Doc | Role |
|-----|------|
| `RB-UI-tooling-field-report.md` | UI Fixed + inventory 5/10 evidence |
| `RB-Northridge-efficiency-verify.md` | RTT scorecard for closed Env gaps |
| `RB-unreal-watch-miss-report.md` | Watch discovery / offline lie |
| Fieldtest `MCP_Hard_Gaps_Fieldtest.md` | Mirror of this file |
