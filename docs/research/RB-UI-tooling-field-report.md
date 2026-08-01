# RB-UI — MCP UI tooling field report (MMORPG overlays)

**Audience:** UEREMCP product owner / MCP author  
**Date:** 2026-07-31  
**Project:** `ueremcp_fieldtest`  
**MCP:** `user-unreal-mcp`  
**Branch / DLL context:** UEREMCP `ws-11-northridge-remaining-impl` @ `efd3675` (plugin junction)  
**Evidence:** Start-menu UI agent `12d01c80-55d5-4f1f-ba2a-dba715813ad2` (~171 lines); CaptureViewport PNGs under `Saved/UEREMCP/MMOStart/`; inventory quality-bar reference (4-class collage) in Cursor workspace assets; sibling inventory build targeting `/Game/UI/MMOInventory/`  
**Shorter catalog:** [`RB-UI-tooling-audit.md`](./RB-UI-tooling-audit.md)  
**Fieldtest mirror:** `ueremcp_fieldtest/docs/MCP_UI_Tooling_Field_Report.md`  
**Style peers:** [`RB-Northridge-validation-report.md`](./RB-Northridge-validation-report.md), fieldtest `MCP_Field_Report_Northridge.md` / `MCP_Backlog_API_Shapes.md`

---

## Executive summary

An agent built a branded MMORPG start menu + character-creation flow (`NORTHRIDGE ONLINE`) as a Widget Blueprint and got CaptureViewport proof by hosting the UI on a **world-space `WidgetComponent`** next to a live third-person preview stand. That path works — and the user likes it as **in-world / diegetic / “based in real life” UI**, not as an apology for missing screen-HUD tooling.

The user then raised the **quality bar**: a high-fidelity **inventory / character sheet** reference (4-class collage: Stormwarden, Ironcrown, Wildspeaker, Nightreaver) and asked to build that **in-world** next. That sheet — paper-doll equipment, icon grids, container weight, hotbar, live character in panel — is the stress test that will expose whether UI tooling can match production MMO fidelity. Sibling agent is building `/Game/UI/MMOInventory/` in parallel; start-menu evidence ships now, inventory findings stubbed until that build lands.

**Verdict (original field cycle):** Epic `UMGToolSet` + `ObjectTools` + `execute_tool_script` can author a designed 74-widget tree. We still **do not have goal-level UEREMCP UI tooling**. `ResolveIntent` abstains on start-menu intents. There is no theme/spec create API, no first-class Show-in-world/screen primitive, and `CaptureViewport` (even with `bShowUI:true`) does **not** composite screen-space UMG. Character preview had no mannequin helper — the agent used a cylinder (`MMO_BodyProxy`). Highest-value backlog: `UI-MCP-001…010` (start-menu gaps) + **`UI-MCP-011…017` (inventory / character-sheet gaps)**.

---

## Fixed (2026-07-31 — UeremcpUI domain ship)

**Branch:** `ws-11-northridge-remaining-impl`  
**Module:** `UeremcpUI` (`UeremcpUI.UeremcpUIToolset`)

| Gap | Fix |
|-----|-----|
| No UI domain / ResolveIntent abstain | `UeremcpUI` registered; GetStarted lists `ui_domain`; intent “create MMORPG start menu UMG overlay” → `CreateWidgetFromSpec` **high** confidence (not abstain) |
| No CreateWidgetFromSpec / theme | `create_widget_from_spec`, `apply_ui_theme` (`northridge_fantasy` / `northridge_diegetic`); `options.save` defaults **true** |
| No Show in world/screen | `show_widget_in_world` (preferred), `show_widget_on_screen` (honest CaptureViewport note) |
| CaptureViewport / bShowUI lie | `capture_ui_frame` — world WC path works; `screen_space_umg=true` → **rejected** `SCREEN_UMG_CAPTURE_UNSUPPORTED` + next_args |
| No mannequin helper | `spawn_character_preview` reports `mesh_source` (`project`\|`engine`\|`proxy`) |
| Unsaved WBP crash loss | Default save on create + `save_widget_asset` |
| Inventory grids / hotbar / paper-doll | `create_inventory_sheet_from_spec`, `set_slot_icon`, `set_container_weight`, `set_slot_layout` |
| Black MCPProbe proof | `SpawnUIHost` now applies transform **after** Root register; `capture_ui_frame` frames WC front explicitly, warms draws, reports `avg_luminance` / Simulate warnings |

### Live smoke (`/Game/UI/MCPProbe/`)

| Artifact | Path / result |
|----------|----------------|
| WBP | `/Game/UI/MCPProbe/WBP_Probe` (saved `.uasset` on disk) |
| World host | `UEREMCP_UIHost_Probe` |
| Preview | `UEREMCP_CharPreview_Probe` (`mesh_source=engine`, TutorialTPP) |
| Proof | `Saved/UEREMCP/MCPProbe/proof.png` (+ `proof_v2.png`) — non-black after framing + Simulate |
| Honest reject | `capture_ui_frame` + `screen_space_umg=true` → `SCREEN_UMG_CAPTURE_UNSUPPORTED` |

### Gotcha — black `capture_ui_frame` / CaptureViewport proofs

**Symptom:** `Saved/UEREMCP/MCPProbe/proof.png` was solid black (axis gizmo only).

**Root causes (stacked):**
1. **`SpawnUIHost` dropped the host transform** — `SetRootComponent` after `SpawnActor`/`SetActorTransform` reset the actor to identity at origin with scale 1, so the WC was oversized / mis-oriented vs the documented default `(200,-150,120) @ yaw 140 scale 0.25`.
2. **`FocusViewportOnBox` silently no-op’d** in this World Partition / far-camera session (camera stayed ~75km away looking at empty sky).
3. **Simulate was required** for WC paint; capture without Simulate + wrong camera = black void.

**Fix:** apply actor transform after Root register + `InitWidget`; replace box-focus with an explicit camera pose on the WC forward axis; multi-draw + `FlushRenderingCommands`; return `avg_luminance` / `simulate_running` / `camera_framed` warnings. Agents must still `StartPIE(bSimulate=true)` before proof.

### Remaining UI gaps

- Named icon **atlas** packing (`import_icon_atlas`) — `set_slot_icon` takes Texture2D only
- SceneCapture-to-widget panel preview (UI-MCP-016)
- CommonUI parent path when plugin present (honest reject only today)
- WidgetSwitcher navigation graph authoring / bind_widget_to_preview
- Class presets (UI-MCP-017), Enhanced Input hotbar binds
- SchemaPublishing still reports “domain schema file not found” for some UI specs at describe-time (files exist under `schemas/domains/ui/` — follow-up path wiring)

**Repeated calls are mostly not an instruction gap.** Approx. mix this session: ~110 `call_tool`, ~41 `execute_tool_script`, ~43 `CaptureViewport`, ~16 `describe_toolset`, ~8 `ResolveIntent`.

| Cause | Weight | Evidence |
|-------|--------|----------|
| Missing goal UI APIs (spec create, theme, show-in-viewport, preview stage) | **High** *(mitigated — see Fixed)* | Primitive UMG tree + ObjectTools blobs; no `UeremcpUI` domain |
| Capture / proof path broken for Screen UMG | **High** *(honest API now)* | ~43 CaptureViewport; `bShowUI:true` still missed overlay until World WC + Simulate |
| Discoverability / ResolveIntent abstain | **Medium** *(fixed)* | Intent “create MMORPG start menu UMG” → low confidence / no UI route |
| Reliability footguns (Restore Packages, BP create hang) | **Medium** | Autosaves clear; `BlueprintTools.create` → MCP `-32001` |
| Instruction / late batching | **Low–Medium** | Script batching found early and used well (~41 scripts) |

---

## 1. What shipped

| Item | Path / status |
|------|----------------|
| Widget BP | `/Game/UI/MMOStart/WBP_MMOStartMenu` — **74 widgets**, compiles |
| Brand | **NORTHRIDGE ONLINE** — main: Play / Create Character / Settings / Quit; create: name, Warrior/Mage/Ranger, body/hair/skin/armor, Back/Confirm |
| Preview material | `/Game/UI/MMOStart/Materials/M_CharPreview` |
| Live stage | Actor `MMO_UIHost` + **world-space** `WidgetComponent`; preview actors `MMO_CharPreview` / **`MMO_BodyProxy` (cylinder)** |
| Start menu proof | `Saved/UEREMCP/MMOStart/start_menu_overlay.png` (also intermediates `04_simulate_main_menu.png`, designer/PIE probes) |
| Char create + preview proof | `Saved/UEREMCP/MMOStart/character_creation_preview.png` (also `05_char_create_with_preview.png`) |

### World-space WidgetComponent approach

1. Create / compile `WBP_MMOStartMenu` via `UMGToolSet` + `ObjectTools.set_properties` (batched in `execute_tool_script`).
2. Spawn host actor; `ActorTools.add_component` → `UWidgetComponent`; set `WidgetClass`, `DrawSize`, **`Space = World`**, blend/draw flags.
3. Place preview mesh (intended mannequin; fell back to cylinder body proxy + MIC).
4. **Simulate** (or carefully framed editor view) so the WC draws into the 3D scene.
5. `EditorApp.CaptureViewport` — UI pixels appear because they are **scene geometry**, not Slate HUD.

### Cylinder preview caveat

`/Game` had **no usable Mannequin** skeletal mesh for a readable third-person stand-in. ControlRig / engine biped templates were nearly invisible in CaptureViewport. Agent shipped **`MMO_BodyProxy`** (cylinder + `M_CharPreview`) so character-create frames showed *something* next to the panel. Appearance buttons do **not** yet drive mesh/MIC swaps (no bind API; event stubs only). Treat cylinder as an honest tooling gap artifact, not final art direction.

**Open in editor:** Content Browser → `/Game/UI/MMOStart/WBP_MMOStartMenu`. Live overlay: select `MMO_UIHost`, World space on WC, Simulate.

---

## 2. User signal — product direction

User feedback (parent session, 2026-07-31): the start menu feels **“based in real life”** — UI living in the 3D world — and that is **desirable**. The same session asked to recreate a **diegetic inventory / character sheet** from a high-fidelity reference collage — confirming this is product direction for MMO overlays, not a one-off start-menu trick.

| Treat as | Do not treat as |
|----------|-----------------|
| First-class MMO overlay / character-select / **inventory sheet** / diegetic HUD pattern | A hack to apologize for because CaptureViewport cannot see Screen UMG |
| Reason to ship `ShowWidgetInWorld` + framed capture as **primary** agent path for this genre | Something to “fix” by forcing every menu into Screen + AddToViewport only |
| Design language that matches Northridge world composition (UI coexists with landscape/preview/character) | Temporary until “real” screen HUD tooling arrives |

Screen-space HUD remains required for many game systems (CommonUI input stacks, pause, settings). Both modes need tooling. For **MMO start / character create / inventory / in-world overlays**, world-space is the preferred product direction going forward.

---

## 2.1 Quality bar — inventory / character sheet reference

**Status:** Quality bar set by user; next stress test after start menu.  
**Reference (4-class collage):** Cursor workspace asset  
`(local Cursor workspace image — path redacted; not stored in this repo)`

The collage shows four class variants of the same dense dark-fantasy sheet (Stormwarden / Ironcrown / Wildspeaker / Nightreaver). Shared structure that tooling must eventually support:

| Region | What the reference demands |
|--------|----------------------------|
| Header | Serif name, level \| class, crest / subclass |
| Paper-doll | Vertical equipment slots (head → feet) framing a **live full-body character** in panel |
| Stats | Health / stamina / mana icons + bars; XP bar; total weight |
| Containers | Specialized rig / bandolier grid + pockets + main backpack grid; **per-container weight** (`kg / max`) |
| Icons | High-detail item icons (atlas / consistent lighting); stack counts on slots |
| Hotbar | Numbered 1–0 quick-use row under the sheet |
| Presets | Class-specific gear/icon sets without rebuilding the whole tree |

Start-menu `WBP_MMOStartMenu` proves agents can style panels and host them in world space. It does **not** prove item grids, icon atlases, paper-doll binding, or multi-container weight UI. **Inventory is the fidelity gate.**

---

## 2.2 Inventory UI stress test (in progress)

| | |
|--|--|
| **Target path** | `/Game/UI/MMOInventory/` (sibling agent, parent session 2026-07-31) |
| **Host approach** | In-world / world-space `WidgetComponent` (same product direction as start menu) |
| **Goal** | Match reference layout + aesthetic as closely as tooling allows; class presets preferred over four full systems |
| **Content as of this report** | **Not landed yet** — no `/Game/UI/MMOInventory` assets observed; findings below are backlog from the reference + start-menu gaps, not inventory-session call counts |

**Fold-in checklist when sibling finishes:** CaptureViewport paths; widget count; whether UniformGrid / WrapBox / icon brushes worked; paper-doll vs Image placeholders; live character mesh source; class-preset mechanism; any new pain (atlas import, drag-drop, stack text, weight TextBlocks). Update this section from “in progress” → scorecard + evidence; keep UI-MCP-011…017 IDs stable.

---

## 3. Tooling that worked

| Tool / path | Why it worked |
|-------------|---------------|
| `UMGToolSet.CreateWidgetBlueprint` | Creates WBP with `parentClass=/Script/UMG.UserWidget` |
| `AddWidget` / `RemoveWidget` / `RenameWidget` / `MoveWidget` | Tree surgery; returns widget + slot refs |
| `CompileWidgetBlueprint` | Required after structural/style edits |
| `ToggleWidgetAsVariable` + `BindToEventProperty` | Variable flag + `OnClicked` graph **stubs** (no navigation logic authored) |
| `ListWidgetClasses` | 96 classes discoverable; confirmed **no CommonUI** game widgets in this project |
| `ObjectTools.list/get/set_properties` | **Mandatory** — UMG props are camelCase (`brushColor`, `colorAndOpacity`, `widgetStyle`, canvas anchors) |
| `ProgrammaticToolset.execute_tool_script` | Only practical way to build 74 widgets + fantasy styling in one session |
| `SceneTools` + `ActorTools.add_component` | Host actor + `WidgetComponent` world-space path |
| `EditorApp.StartPIE` / `StopPIE` / Simulate | World WC draws; used for CaptureViewport proof |
| `EditorApp.CaptureViewport` | Works for **3D scene** including world-space WC once framed |
| `CaptureEditorImage` / Slate `Screenshot` | Designer / full-editor chrome when viewport misses UI |
| `SlateInspector` Escape / Click / Windows | Dismiss stuck menus so MCP can proceed |

---

## 4. Bugs / pain / missing tools (evidence-based)

### Scorecard

| Gap | Severity | Field effect | Evidence |
|-----|----------|--------------|----------|
| **No UEREMCP UI domain** | P0 | Agents fall to Epic primitives; no intent route | `ResolveIntent` abstains / low confidence on start-menu UMG |
| **`CaptureViewport` misses Screen UMG** | P0 | Proof loop burns dozens of captures; forces World WC or editor chrome | `bShowUI:true` still no overlay; ~43 CaptureViewport mentions |
| **No `ShowWidgetInWorld` / `ShowWidgetOnScreen`** | P0 | Manual host actor + WC + property soup | Session built `MMO_UIHost` by hand |
| **No `CreateWidgetFromSpec` / theme helpers** | P0 | Every TextBlock/Button is a huge `set_properties` blob | Full tree via script; fragile `widgetStyle` brushes |
| **No character preview / mannequin helper** | P1 | Cylinder stand-in; no appearance→mesh bind | No `/Game` Mannequin; `MMO_BodyProxy` |
| **No CommonUI helpers** | P1 | Cannot use CommonActivatable / input routing | `ListWidgetClasses(filter=Common)` empty for game |
| **No AddToViewport / PIE show-widget helper** | P1 | Screen HUD for PIE still custom BP BeginPlay or WC Screen mode | Tried Screen WC + PIE; Capture still unreliable |
| **`BindToEventProperty` stubs only** | P1 | No WidgetSwitcher navigation / Quit / Confirm logic | Stubs created; graph authoring not goal-level |
| **Slot property schema traps** | P1 | Hard fail on invalid keys (e.g. VB `size`) | `set_properties` throws instead of soft-skip + suggestions |
| **StrictDict in `execute_tool_script`** | P2 | `.get(key, default)` forbidden — noisy scripts | Documented in audit; hurts batch authors |
| **`BlueprintTools.create` hang** | P2 | Editor / MCP timeout mid-session | ~`-32001`; prefer existing asset + DSL |
| **Restore Packages modal** | P2 | Blocks game thread / MCP after restart | Cleared `Saved/Autosaves`; Win32 dismiss attempts |
| **No input-binding / focus helpers** | P2 | Gamepad/keyboard nav not authored | No CommonUI; no Enhanced Input ↔ UI bridge tool |
| **World WC draw flaky in some editor frames** | P2 | Agent restored cached CaptureViewport once when WC blank | Transcript: “World-space widget isn't drawing in this frame” |

### Blunt findings

1. **`bShowUI` on CaptureViewport is a lie for Screen-space game UMG.** Agents will retry forever unless docs/API state: *captures the 3D viewport; Screen HUD is excluded; use World WC, SceneCapture composite, or CaptureEditorImage.*
2. **There is no UI equivalent of `BuildEnvironment`.** Region agents got goal ops; UI agents got Lego bricks.
3. **World-space was discovered as a Capture workaround — then the user correctly promoted it to a feature.** Product should meet that signal with first-class APIs, not document “hack: set Space=World.”
4. **Appearance options without a preview director are dead chrome.** Buttons exist; mesh does not change.
5. **CommonUI is absent and unadvertised as a project capability.** Intent text asked for CommonUI; tooling silently degraded to raw UMG.

### Call-pattern snapshot (UI transcript)

| Pattern | Approx count |
|---------|-------------:|
| `call_tool` | ~110 |
| `CaptureViewport` | ~43 |
| `execute_tool_script` | ~41 |
| `Restore` / Autosaves friction | ~30 / ~16 |
| `WidgetComponent` | ~23 |
| `set_properties` | ~21 |
| `describe_toolset` | ~16 |
| `CompileWidget` | ~15 |
| `CommonUI` (mentions / probes) | ~14 |
| `StartPIE` / `StopPIE` | ~11 / ~12 |
| `ResolveIntent` | ~8 |
| `Mannequin` / cylinder / BodyProxy | ~7 / ~6 / ~11 |

---

## 5. Backlog — API shapes + acceptance tests

IDs are stable for tracking. Priorities: **P0** ship-blockers for MMO overlay agents; **P1** fidelity; **P2** polish. Prefer ADR-0003 envelopes on a new **`UeremcpUI`** (or `UeremcpPresentation`) toolset. Epic `UMGToolSet` stays internal where a goal tool exists.

### Why agents spam calls (UI session)

| Factor | Share | Action |
|--------|------:|--------|
| Missing goal UI APIs | ~45% | UI-MCP-001…004, 006 |
| Capture / Screen UMG blind spot | ~25% | UI-MCP-003, 005 |
| Discoverability / ResolveIntent abstain | ~15% | UI-MCP-001 routing + GetStarted |
| Reliability (modals, create hang, StrictDict) | ~10% | UI-MCP-008…010 |
| Instruction / late batching | ~5% | Script path already used; still need ExecutePlan examples |

---

### UI-MCP-001 — CreateWidgetFromSpec (+ theme)

| | |
|--|--|
| **Priority** | P0 |
| **Title** | CreateWidgetFromSpec / ApplyUiTheme |
| **Problem** | 74 widgets = dozens of AddWidget + camelCase property blobs. No intent-routed UI domain; ResolveIntent abstains. |

#### Proposed API

**Toolset:** `UeremcpUI.UeremcpUIToolset`  
**Actions:** `create_widget_from_spec`, `apply_ui_theme`

```json
{
  "protocol_version": "1.0",
  "action": "create_widget_from_spec",
  "request_id": "mmo-start-1",
  "options": { "dry_run": false, "save": true, "compile": true },
  "specification": {
    "asset_path": "/Game/UI/MMOStart/WBP_MMOStartMenu",
    "parent_class": "/Script/UMG.UserWidget",
    "theme": "northridge_fantasy",
    "screens": [
      {
        "id": "main",
        "layout": "overlay_left_panel",
        "brand": { "title": "NORTHRIDGE ONLINE", "tagline": "Forge your legend" },
        "actions": ["Play", "Create Character", "Settings", "Quit"]
      },
      {
        "id": "character_create",
        "layout": "overlay_left_panel",
        "fields": ["character_name", "archetype", "body", "hair", "skin", "armor"],
        "actions": ["Back", "Confirm"]
      }
    ],
    "navigation": {
      "Create Character": "character_create",
      "Back": "main"
    }
  }
}
```

**Theme helper:**

```json
{
  "action": "apply_ui_theme",
  "specification": {
    "widget_blueprint": "/Game/UI/MMOStart/WBP_MMOStartMenu",
    "theme": {
      "id": "northridge_fantasy",
      "panel": { "color": [0.06, 0.05, 0.04, 0.92], "padding": 24 },
      "text": { "title_size": 48, "body_size": 18, "color": [0.92, 0.86, 0.72, 1], "letter_spacing": 8 },
      "button": { "normal": [0.12, 0.1, 0.08, 1], "hovered": [0.35, 0.22, 0.1, 1], "outline": [0.72, 0.5, 0.22, 1] }
    },
    "targets": ["*"]
  }
}
```

Also expose thin helpers used internally: `ApplyTextStyle`, `ApplyButtonStyle`, `SetCanvasFill`, `SetBoxPadding`.

#### Acceptance tests

- **Given** empty `/Game/UI/Test`; **When** `create_widget_from_spec` with two screens + navigation; **Then** asset exists, compiles, WidgetSwitcher (or equivalent) has both screens, brand TextBlock non-default.
- **Given** existing WBP; **When** `apply_ui_theme`; **Then** sampled TextBlock/Button props match theme within tolerance — no raw `widgetStyle` required from agent.
- **Given** intent “create MMORPG start menu”; **When** `ResolveIntent`; **Then** recommends `UeremcpUI.create_widget_from_spec` (not abstain).
- Catalog CI: UI domain listed in GetStarted with worked example.

---

### UI-MCP-002 — ShowWidgetInWorld / ShowWidgetOnScreen

| | |
|--|--|
| **Priority** | P0 |
| **Title** | ShowWidgetInWorld vs ShowWidgetOnScreen |
| **Problem** | Manual `MMO_UIHost` + WC property soup. No AddToViewport helper. World path is the user-preferred MMO overlay mode. |

#### Proposed API

```json
{
  "protocol_version": "1.0",
  "action": "show_widget_in_world",
  "specification": {
    "widget_asset": "/Game/UI/MMOStart/WBP_MMOStartMenu",
    "host_label": "UEREMCP_UIHost_MMOStart",
    "transform": {
      "location": [200, -150, 120],
      "rotation": [0, 140, 0],
      "scale": [0.25, 0.25, 0.25]
    },
    "draw_size": [1920, 1080],
    "space": "World",
    "replace_owned": "label",
    "screen_id": "main"
  }
}
```

```json
{
  "action": "show_widget_on_screen",
  "specification": {
    "widget_asset": "/Game/UI/MMOStart/WBP_MMOStartMenu",
    "z_order": 100,
    "viewport": "editor_or_pie",
    "screen_id": "main",
    "replace_owned": true
  }
}
```

**Response sketch:** `{ "host_actor": "...", "widget_component": "...", "space": "World", "active_screen": "main" }`

#### Acceptance tests

- **Given** compiled WBP; **When** `show_widget_in_world`; **Then** labeled host exists, WC `Space=World`, widget class set, visible under Simulate.
- **Given** same asset; **When** `show_widget_on_screen` during PIE; **Then** widget in viewport hierarchy (Slate inspect or pixel probe).
- **Given** second show with `replace_owned=label`; **Then** single host, no duplicate overlays.

---

### UI-MCP-003 — CaptureViewport includes game UI (mode-aware)

| | |
|--|--|
| **Priority** | P0 |
| **Title** | Capture that includes UI |
| **Problem** | Screen UMG invisible to CaptureViewport; `bShowUI` ineffective for game HUD. Proof requires World WC or full editor chrome. |

#### Proposed API

Extend `EditorApp.CaptureViewport` **or** add `UeremcpUI.capture_ui_frame`:

```json
{
  "action": "capture_ui_frame",
  "specification": {
    "path": "Saved/UEREMCP/MMOStart/proof.png",
    "include": {
      "world": true,
      "world_space_widgets": true,
      "screen_space_umg": true,
      "editor_chrome": false
    },
    "camera": {
      "focus_actors": ["UEREMCP_UIHost_MMOStart", "UEREMCP_CharPreview"],
      "transform": null
    }
  }
}
```

Honesty rule: if `screen_space_umg=true` unsupported → `rejected` / `partially_completed` with `code=SCREEN_UMG_CAPTURE_UNSUPPORTED` + `next_args` suggesting World mode or editor capture — **never** silent success without UI pixels.

#### Acceptance tests

- **Given** Screen HUD visible in PIE; **When** capture with `screen_space_umg=true`; **Then** PNG contains UI pixels (histogram / template match) **or** honest unsupported.
- **Given** World WC host; **When** capture with defaults; **Then** panel + scene in frame.
- Contract test: `bShowUI` docs/schema match actual behavior (no false advertising).

---

### UI-MCP-004 — SpawnCharacterPreview / appearance director

| | |
|--|--|
| **Priority** | P0 (for MMO create flows) |
| **Title** | Character preview + mannequin helpers |
| **Problem** | No mannequin in `/Game`; cylinder proxy; appearance buttons do not drive mesh/MIC. |

#### Proposed API

```json
{
  "action": "spawn_character_preview",
  "specification": {
    "label": "UEREMCP_CharPreview",
    "mesh": "auto",
    "fallback": ["project_mannequin", "engine_mannequin", "proxy_capsule"],
    "location": [80, 40, 0],
    "rotation_yaw": -30,
    "material": "/Game/UI/MMOStart/Materials/M_CharPreview",
    "camera_framing": { "with_ui_host": "UEREMCP_UIHost_MMOStart" }
  }
}
```

```json
{
  "action": "bind_widget_to_preview",
  "specification": {
    "widget_blueprint": "/Game/UI/MMOStart/WBP_MMOStartMenu",
    "preview_actor": "UEREMCP_CharPreview",
    "bindings": [
      { "widget": "Btn_Skin_Fair", "material_param": "SkinTint", "value": [0.9, 0.8, 0.7, 1] },
      { "widget": "Btn_Archetype_Mage", "mesh_tag": "mage" }
    ]
  }
}
```

#### Acceptance tests

- **Given** empty level; **When** `spawn_character_preview` with `mesh=auto`; **Then** visible skeletal or documented proxy; response lists `mesh_source` (`project`|`engine`|`proxy`).
- **Given** bindings; **When** invoke bound button (or simulate click tool); **Then** MIC param or mesh tag changes.
- Never return success with zero-draw mesh when `require_visible=true`.

---

### UI-MCP-005 — Layout slot helpers

| | |
|--|--|
| **Priority** | P1 |
| **Title** | SetCanvasFill / SetBoxPadding / soft slot schema |
| **Problem** | Anchors/offsets hand-assembled; VB `size` rejected hard. |

#### Proposed API

```json
{
  "action": "set_slot_layout",
  "specification": {
    "slot": { "refPath": "..." },
    "canvas": { "anchors": "fill", "offsets": [0, 0, 0, 0] },
    "box": { "padding": [12, 8, 12, 8], "size_rule": "fill" }
  }
}
```

Invalid keys → `partially_completed` + `suggested_properties[]`, not throw.

#### Acceptance tests

- Fill canvas child → anchors (0,0)-(1,1), zero offsets.
- Unknown property → soft fail with suggestions including valid slot props.

---

### UI-MCP-006 — ResolveIntent + GetStarted UI routing

| | |
|--|--|
| **Priority** | P0 |
| **Title** | Intent route for UI / overlay / character create |
| **Problem** | ResolveIntent abstains; agents discover UMG via describe spam. |

#### Acceptance tests

- Intents containing “start menu”, “UMG overlay”, “character creation UI”, “inventory sheet”, “paper doll”, “world-space widget” → `UeremcpUI` ops with confidence ≥ threshold.
- GetStarted lists UI domain + world-space MMO overlay **and** inventory sheet examples.

---

### UI-MCP-007 — CommonUI optional path

| | |
|--|--|
| **Priority** | P1 |
| **Title** | CommonUI parent + style assets when plugin present |
| **Problem** | Fieldtest has no CommonUI widgets; intent requested CommonUI; silent degrade. |

#### Proposed API

`create_widget_from_spec` option `"ui_framework": "umg"|"common_ui"|"auto"`.  
If CommonUI missing and `common_ui` requested → `rejected` with `COMMON_UI_UNAVAILABLE` + next_args to `umg` — not silent UserWidget.

#### Acceptance tests

- Project without CommonUI + `ui_framework=common_ui` → honest reject.
- Project with CommonUI → parent is CommonActivatableWidget (or documented base).

---

### UI-MCP-008 — Editor session reliability for UI agents

| | |
|--|--|
| **Priority** | P1 |
| **Title** | Autosave restore dismiss + Blueprint create harden |
| **Problem** | Restore Packages blocks MCP; `BlueprintTools.create` hung (−32001). |

#### Acceptance tests

- Agent-session flag / auto-dismiss Restore Packages on editor start when MCP connected.
- Widget create path does not hang >N seconds; returns timeout envelope with recovery.

---

### UI-MCP-009 — StrictDict / script ergonomics

| | |
|--|--|
| **Priority** | P2 |
| **Title** | Allow dict `.get` defaults in execute_tool_script **or** document in `get_execution_environment` |
| **Problem** | Noisy failures mid UI batch scripts. |

#### Acceptance tests

- Script using `.get("x", 1)` succeeds **or** environment docs list forbidden patterns with rewrite example.

---

### UI-MCP-010 — Input binding / focus (PIE)

| | |
|--|--|
| **Priority** | P2 |
| **Title** | Bind UI actions / set input mode for PIE show |
| **Problem** | No helper to wire Enhanced Input / SetInputMode UIOnly / gamepad focus for start menu. |

#### Proposed API (sketch)

`set_ui_input_mode({ mode: "ui_only"|"game_and_ui", focus_widget })` + optional `bind_click_to_screen_nav`.

#### Acceptance tests

- PIE + show_widget_on_screen → focus on first button; Escape returns to game mode when requested.

---

### UI-MCP-011 — CreateInventorySheetFromSpec (item slot grids)

| | |
|--|--|
| **Priority** | P0 (inventory stress) |
| **Title** | Inventory / character sheet from spec — slot grids |
| **Problem** | Reference needs backpack N×M, pockets, specialized rig grids. Hand-building dozens of Border/Image/TextBlock cells via AddWidget does not scale; no UniformGrid fill helper. |

#### Proposed API

```json
{
  "protocol_version": "1.0",
  "action": "create_inventory_sheet_from_spec",
  "specification": {
    "asset_path": "/Game/UI/MMOInventory/WBP_MMOInventory",
    "theme": "northridge_diegetic",
    "layout": "character_sheet_v1",
    "grids": [
      { "id": "equipment", "kind": "paper_doll", "slots": ["head","neck","torso","arms","legs","feet"] },
      { "id": "rig", "kind": "uniform_grid", "rows": 2, "cols": 4, "cell_px": 72 },
      { "id": "pockets", "kind": "uniform_grid", "rows": 1, "cols": 4, "cell_px": 64 },
      { "id": "backpack", "kind": "uniform_grid", "rows": 4, "cols": 5, "cell_px": 72 },
      { "id": "hotbar", "kind": "hotbar", "slots": 10, "show_index": true }
    ],
    "stats": ["health", "stamina", "mana", "xp", "weight_total"],
    "host": { "space": "World", "label": "UEREMCP_UIHost_Inventory" }
  }
}
```

#### Acceptance tests

- **Given** spec with backpack 4×5; **When** create; **Then** 20 slot widgets named stably (`Slot_Backpack_r{c}_c{c}`), compiles, World host optional.
- **Given** `kind=hotbar` slots=10; **Then** indices 1–0 labels present.
- Cell count mismatch / invalid kind → `rejected` with `next_args`, not partial silent tree.

---

### UI-MCP-012 — Icon atlas / item brush helpers

| | |
|--|--|
| **Priority** | P0 (inventory stress) |
| **Title** | ImportIconAtlas + SetSlotIcon |
| **Problem** | Reference icons are dense, consistent, atlas-like. No MCP path to pack/assign item icons to slots; agents invent Image brushes one property blob at a time. |

#### Proposed API

```json
{
  "action": "import_icon_atlas",
  "specification": {
    "source_path": "D:/art/mmo_items_atlas.png",
    "dest_path": "/Game/UI/MMOInventory/Textures/T_ItemAtlas",
    "cell_px": 128,
    "margin_px": 2,
    "names": ["sword_storm", "potion_mana", "scroll_bolt"]
  }
}
```

```json
{
  "action": "set_slot_icon",
  "specification": {
    "widget_blueprint": "/Game/UI/MMOInventory/WBP_MMOInventory",
    "slot": "Slot_Backpack_r0_c1",
    "icon": { "atlas": "/Game/UI/MMOInventory/Textures/T_ItemAtlas", "name": "potion_mana" },
    "stack": { "count": 3, "max": 3, "show": true }
  }
}
```

#### Acceptance tests

- Atlas import yields addressable named icons; `set_slot_icon` updates brush + optional stack TextBlock.
- Missing icon name → `ICON_NOT_FOUND` + list of available names in `next_args`.

---

### UI-MCP-013 — Equipment paper-doll slots

| | |
|--|--|
| **Priority** | P0 (inventory stress) |
| **Title** | Paper-doll equipment slot layout + bind |
| **Problem** | Reference frames character with vertical gear slots. No helper for equip-slot chrome, empty-slot silhouettes, or binding slot → preview mesh socket/material. |

#### Proposed API

```json
{
  "action": "create_paper_doll",
  "specification": {
    "widget_blueprint": "/Game/UI/MMOInventory/WBP_MMOInventory",
    "preview_actor": "UEREMCP_CharPreview_Inventory",
    "slots": [
      { "id": "head", "socket": "head", "empty_icon": "silhouette_head" },
      { "id": "torso", "socket": "spine_02", "empty_icon": "silhouette_chest" }
    ],
    "layout": "vertical_flanking_preview"
  }
}
```

#### Acceptance tests

- Paper-doll creates N equip slots + preview framing region; equip icon set updates preview when bind API present (ties to UI-MCP-004).
- Unequip restores empty silhouette brush.

---

### UI-MCP-014 — Hotbar strip

| | |
|--|--|
| **Priority** | P1 |
| **Title** | CreateHotbar / BindHotbarSlot |
| **Problem** | Reference has numbered 1–0 quick-use row. Manual HorizontalBox of 10 slots + key labels is boilerplate; no input bind bridge. |

#### Proposed API

```json
{
  "action": "create_hotbar",
  "specification": {
    "widget_blueprint": "/Game/UI/MMOInventory/WBP_MMOInventory",
    "parent": "RootCanvas",
    "slots": 10,
    "index_labels": ["1","2","3","4","5","6","7","8","9","0"],
    "bind_enhanced_input": false
  }
}
```

#### Acceptance tests

- 10 slots + labels; optional Enhanced Input bind when project has IMC — else honest `INPUT_BIND_SKIPPED`.

---

### UI-MCP-015 — Container weight UI

| | |
|--|--|
| **Priority** | P1 |
| **Title** | SetContainerWeight / multi-container weight display |
| **Problem** | Reference shows per-container `current / max KG` (rig + backpack) plus total weight in stats. No typed helper; agents leave static TextBlocks. |

#### Proposed API

```json
{
  "action": "set_container_weight",
  "specification": {
    "widget_blueprint": "/Game/UI/MMOInventory/WBP_MMOInventory",
    "containers": [
      { "id": "rig", "label_widget": "Txt_RigWeight", "current_kg": 28.7, "max_kg": 45 },
      { "id": "backpack", "label_widget": "Txt_PackWeight", "current_kg": 18.3, "max_kg": 40 }
    ],
    "total": { "label_widget": "Txt_TotalWeight", "current_kg": 47.0, "max_kg": 85 }
  }
}
```

#### Acceptance tests

- Labels format `28.7 / 45 KG` (locale-stable); over-encumbered optional style flag when `current > max`.
- Unknown container id → soft fail with existing container ids listed.

---

### UI-MCP-016 — Live character in inventory panel

| | |
|--|--|
| **Priority** | P0 (inventory stress) |
| **Title** | Panel-embedded character preview (SceneCapture / world stage) |
| **Problem** | Reference centers a lit, class-accurate character **inside** the sheet. Start menu used a world actor beside the WC; inventory needs tighter framing (render target in Image, or staged mesh aligned to panel). Cylinder proxy fails this bar. |

#### Proposed API

Extend UI-MCP-004:

```json
{
  "action": "spawn_character_preview",
  "specification": {
    "label": "UEREMCP_CharPreview_Inventory",
    "presentation": "world_beside_panel" | "scene_capture_to_widget",
    "widget_image": "Img_CharacterPreview",
    "class_preset": "stormwarden",
    "require_visible": true
  }
}
```

#### Acceptance tests

- `scene_capture_to_widget` → Image brush is RenderTarget; CaptureViewport of World host shows character silhouette in panel region.
- `require_visible=true` rejects invisible/engine-default tiny meshes with `PREVIEW_NOT_VISIBLE`.

---

### UI-MCP-017 — Class presets (inventory variants)

| | |
|--|--|
| **Priority** | P1 |
| **Title** | ApplyInventoryClassPreset |
| **Problem** | Reference is four class skins of one layout. Rebuilding four WBPs is waste; need data-driven swap of icons, header text, preview mesh tags, accent colors. |

#### Proposed API

```json
{
  "action": "apply_inventory_class_preset",
  "specification": {
    "widget_blueprint": "/Game/UI/MMOInventory/WBP_MMOInventory",
    "preset": "stormwarden",
    "presets_path": "/Game/UI/MMOInventory/Data/InventoryClassPresets",
    "fields": {
      "display_name": "KAEL VEYRAN",
      "level": 24,
      "class_line": "STORMWARDEN | Level 24 | Mageblade",
      "accent": [0.25, 0.55, 0.95, 1],
      "preview_tag": "stormwarden",
      "slot_icons": { "head": "hood_storm", "torso": "robe_storm" }
    }
  }
}
```

#### Acceptance tests

- Switching Stormwarden → Nightreaver updates header + sample slot icons + preview tag without recreating WBP.
- Unknown preset → `PRESET_NOT_FOUND` + available preset ids.

---

## 6. Recommendations

1. **Keep world-space / diegetic UI as first-class** — User signal is explicit for start menu **and** inventory sheet. Ship `show_widget_in_world` + capture framing as the default agent path for MMO overlays.
2. **Build next (ordered):** UI-MCP-001 CreateWidgetFromSpec/theme → UI-MCP-002 Show InWorld/OnScreen → UI-MCP-003 Capture includes UI (honest) → UI-MCP-004/016 Character preview (panel-capable) → UI-MCP-011 Inventory sheet grids → UI-MCP-012 Icon atlas → UI-MCP-013 Paper-doll → UI-MCP-006 ResolveIntent routing.
3. **Treat inventory reference as the fidelity gate** — Start menu proved styling + World WC; inventory proves grids, atlases, weight, hotbar, class presets. Do not claim “UI tooling done” until a CaptureViewport of `/Game/UI/MMOInventory/` is recognizably on-bar vs the collage.
4. **Document CaptureViewport truth now** (docs-only, cheap): Screen UMG excluded; World WC + Simulate is supported proof path; do not advertise `bShowUI` as game-HUD composite until true.
5. **Do not invent PASS for CommonUI, mannequin, or inventory** until assets exist — cylinder + raw UMG were honest start-menu outcomes; inventory section stays “in progress” until sibling lands evidence.
6. **Mirror Northridge discipline:** every new UI op gets `error.code` + `next_args` and static acceptance tests before claiming field closure.
7. **Follow-up field cycles:** (a) start-menu after UI-MCP-001…004 with call budget &lt;25 `call_tool`; (b) inventory after UI-MCP-011…017 with collage side-by-side capture.

---

## 7. Design quality note (context, not tooling)

- Start-menu look is **not** default grey UMG: dark stone panels, bronze/ember accents, parchment text, letter-spaced titles.
- Gaps vs shippable MMO: Roboto-only, style-data buttons (not textured frames), no CommonUI input routing, appearance options do not recolor preview, cylinder stand-in.
- Inventory reference raises the bar further: skeuomorphic grimoire chrome, dense iconography, multi-container weight, hotbar, class-accurate live character **in** the panel — tooling, not taste, is the blocker.
- Tooling blocked fidelity more than taste: theme helpers + preview director + grid/atlas APIs would lift quality without new art direction.

---

## 8. Related docs

| Doc | Role |
|-----|------|
| [`RB-UI-tooling-audit.md`](./RB-UI-tooling-audit.md) | Short tooling catalog (points here) |
| Fieldtest `docs/MCP_UI_Tooling_Audit.md` | Mirror of short audit |
| Fieldtest `docs/MCP_UI_Tooling_Field_Report.md` | Mirror of this report |
| [`RB-Northridge-validation-report.md`](./RB-Northridge-validation-report.md) | Peer seriousness / scorecard style |
| [`RB-MCP-hard-gaps-fieldtest.md`](./RB-MCP-hard-gaps-fieldtest.md) | Expanded hard gaps (ArtKit bar, visual iterate, layout duty) |
| Fieldtest `MCP_Field_Report_Northridge.md` / `MCP_Backlog_API_Shapes.md` | Northridge evidence + MCP-001…018 shapes |
| Start-menu screenshots | `ueremcp_fieldtest/Saved/UEREMCP/MMOStart/start_menu_overlay.png`, `character_creation_preview.png` |
| Inventory reference | Workspace assets collage `B60B4682-…-d2a46b5a3de4.png` (Stormwarden / Ironcrown / Wildspeaker / Nightreaver) |
| Inventory build (landed) | `/Game/UI/MMOInventory/WBP_MMOInventory` — see §9; screenshots under `Saved/UEREMCP/MMOInventory/` |
| Transcript (start menu) | `12d01c80-55d5-4f1f-ba2a-dba715813ad2` |

---

## Appendix — Top 5 tooling bugs (operator-facing)

1. **`CaptureViewport` does not include Screen-space UMG** (`bShowUI` ineffective for game HUD) — proof requires World WC or editor chrome.
2. **No UEREMCP UI domain / `CreateWidgetFromSpec`** — ResolveIntent abstains; agents hand-build trees with ObjectTools.
3. **No `ShowWidgetInWorld` / `ShowWidgetOnScreen`** — host actor + WidgetComponent authored manually.
4. **No theme / text / button style helpers** — fragile full `widgetStyle` and font blobs via `set_properties`.
5. **No character preview / mannequin helper** — cylinder `MMO_BodyProxy`; appearance controls do not drive mesh. *(Inventory bar makes this worse: needs live character **in panel**, not only beside it — UI-MCP-016.)*

**Inventory-specific gaps (quality bar — see UI-MCP-011…017):** item slot grids, icon atlases, paper-doll equipment slots, hotbar, container weight UI, class presets.

---

## 9. Inventory UI attempt (2026-07-31 field cycle)

**Agent:** inventory build subagent (this section)  
**Goal:** In-world MMORPG inventory / character sheet matching dark-fantasy collage reference (Stormwarden / Ironcrown / Wildspeaker / Nightreaver layout).  
**Closeness vs reference:** **5 / 10** (layout ~7, aesthetic ~3–4, character preview ~2).

### What shipped

| Item | Path / status |
|------|----------------|
| Widget BP | `/Game/UI/MMOInventory/WBP_MMOInventory` — **saved to disk** (~282 widgets after rebuild); class preset buttons STORM/IRON/WILD/NIGHT |
| Icons | 30 Blender-generated PNGs → `/Game/UI/MMOInventory/Icons/T_Icon_*` |
| Preview material | `/Game/UI/MMOInventory/Materials/M_InvCharBody` |
| World host | Actor `MMO_InvUIHost` + world-space `WidgetComponent` (`DrawSize` 1600×1000, actor scale 0.22, WC scale 5 — same pattern as start menu) |
| Character stand-in | `MMO_InvBody` cylinder + `MMO_InvHead` sphere (no `/Game` Mannequin) |
| Proof captures | `Saved/UEREMCP/MMOInventory/inventory_ui_full.png`, `inventory_ui_closeup.png` (+ `inventory_designer.png`) |

### Honesty vs reference collage

**Matched:** Left header (name / level|class / class chips), paper-doll flanking slots (HEAD/NECK/TORSO/ARMS/LEGS/FEET), LIVE PREVIEW hole, vitals row + RGB bars, Tactical Rig / Pockets / Spirit / Backpack grids with weights, Quick Use 1–0, diegetic World WC proof path.

**Missing / weak:** Wrought-iron / parchment chrome (solid RoundedBox + Roboto only); icon fidelity is procedural silhouettes not painted fantasy art; no skeletal mannequin **inside** the preview pane (world primitives beside/near panel); class buttons do not swap mesh/MIC (OnClicked stubs only); serif display font unavailable via MCP; SceneCapture-into-Image not authored.

### Tooling bugs hit **while building this UI** (additive)

| Issue | Severity | Evidence |
|-------|----------|----------|
| **Unsaved WBP lost on editor crash** | P0 | First build lived only in memory; after WinError 10054 / crash, `ListWidgetBlueprints` showed only `WBP_MMOStartMenu`. Rebuild + `AssetTools.save_assets` required. Start menu survived because it was already on disk under `Content/UI/MMOStart/`. |
| **Restore Packages + Crash Reporter blocked relaunch** | P0 | Post-crash: hwnd titles `Restore Packages` / `ueremcp_fieldtest Crash Reporter`; MCP `:8000` down until kill + clear Autosaves + relaunch. |
| **No inventory/grid/atlas goal APIs** | P0 | Entire sheet = `execute_tool_script` + AddWidget/ObjectTools; confirms UI-MCP-011…017. |
| **World WC framing thrash** | P1 | Multiple CaptureViewport angles before full sheet readable; edge-on / too-close frames looked “empty” or partial. |
| **CreateVfxMaterial path jail** | P2 | Rejected `/Game/UI/MMOInventory/Materials/...`; only `__UeremcpTests` / `__UeremcpPoc`. Fell back to Epic `MaterialTools.create_material`. |
| **BindToEventProperty schema** | P2 | Requires `eventName` + `propertyName` + `propertyClass` (not widget ref) — first call failed. |
| **find_assets often returns `[]`** | P2 | `/Game` / Mannequin searches empty even when Content exists; `ListWidgetBlueprints` more reliable for WBPs. |
| **No mannequin helper** | P1 | Same as start menu; cylinder+sphere proxy again. |

### Operator takeaway

Inventory stress-tested the start-menu path and found a new hard requirement: **persist UI packages early** (`save_assets`) or a single editor crash wipes hours of UMG tree work. World-space WC remains the only reliable CaptureViewport proof path. Fidelity ceiling without UI-MCP-011…017 + theme/serif/preview APIs is roughly “readable dark layout with placeholder icons,” not collage-grade grimoire chrome.
