# MCP UI Tooling Audit — MMORPG Start Menu Fieldtest

**Date:** 2026-07-31  
**Project:** `ueremcp_fieldtest`  
**MCP:** `user-unreal-mcp`  
**Brand / asset:** `NORTHRIDGE ONLINE` → `/Game/UI/MMOStart/WBP_MMOStartMenu`  
**Branch / DLL context:** UEREMCP `ws-11-northridge-remaining-impl` @ `efd3675` (plugin junction)

> **Full bugs / backlog / API shapes:** [`RB-UI-tooling-field-report.md`](./RB-UI-tooling-field-report.md)  
> Fieldtest mirror: `ueremcp_fieldtest/docs/MCP_UI_Tooling_Field_Report.md`  
> User signal: **world-space / diegetic UI is desirable** (“based in real life”) — product direction.  
> **Next quality bar:** inventory / character sheet reference (4-class collage) → backlog `UI-MCP-011…017`; sibling build `/Game/UI/MMOInventory/` (in progress at report time).

---

## Deliverable summary

| Item | Path / status |
|------|----------------|
| Widget BP | `/Game/UI/MMOStart/WBP_MMOStartMenu` (74 widgets, compiles) |
| Preview material | `/Game/UI/MMOStart/Materials/M_CharPreview` |
| Live preview stage | `MMO_UIHost` (World-space `WidgetComponent`) + `MMO_CharPreview` / `MMO_BodyProxy` (cylinder) in current level |
| Start menu CaptureViewport | `Saved/UEREMCP/MMOStart/start_menu_overlay.png` |
| Char create + preview CaptureViewport | `Saved/UEREMCP/MMOStart/character_creation_preview.png` |

**Open in editor:** Content Browser → `/Game/UI/MMOStart/WBP_MMOStartMenu` (Designer). For live overlay in viewport: place/select `MMO_UIHost`, set `WidgetComponent` Space=World (or Screen in PIE), Simulate.

---

## Tooling catalog (tried)

### Exists and useful

| Capability | Toolset / tool | Notes |
|------------|----------------|-------|
| Create Widget Blueprint | `UMGToolSet.CreateWidgetBlueprint` | Needs `parentClass` (`/Script/UMG.UserWidget`) |
| Add / remove / rename / move widgets | `AddWidget`, `RemoveWidget`, `RenameWidget`, `MoveWidget` | Returns widget + slot refs |
| Compile | `CompileWidgetBlueprint` | Required after tree/style edits |
| Variable flag | `ToggleWidgetAsVariable` | Needed before event binds |
| Event stub | `BindToEventProperty` | Creates handler graph stub (`OnClicked`) — **does not author navigation logic** |
| Class discovery | `ListWidgetClasses` | 96 classes; **no CommonUI** in this project |
| Property inspect/set | `ObjectTools.list/get/set_properties` | **Mandatory** — UMG property names are camelCase (`brushColor`, `colorAndOpacity`, `widgetStyle`) |
| Batch orchestration | `ProgrammaticToolset.execute_tool_script` | Essential; 74-widget tree impractical as single-call MCP |
| Viewport capture | `EditorApp.CaptureViewport` | Captures **3D scene**; Screen-space UMG **not** included |
| Editor chrome capture | `CaptureEditorImage`, Slate `Screenshot` | Good for Designer / full editor |
| Slate dismiss / click | `SlateInspector.*` | Escape, Click, Windows list — needed for stuck menus |
| Actor + WidgetComponent | `SceneTools` + `ActorTools.add_component` | World-space WC is the reliable CaptureViewport path for UI-in-frame **and** preferred MMO overlay style |
| PIE / Simulate | `StartPIE` / `StopPIE` | World-space WC draws under Simulate |

### Missing / painful (summary — full backlog in field report)

| Gap | Why it hurts | Backlog ID |
|-----|--------------|------------|
| **No UEREMCP UI domain toolset** | `ResolveIntent` abstains on “create start menu UMG” | UI-MCP-001, 006 |
| **No CommonUI helpers** | `ListWidgetClasses(filter=Common)` empty for game widgets | UI-MCP-007 |
| **No text-style / theme API** | Every `TextBlock` needs full `font` + `colorAndOpacity` blob | UI-MCP-001 |
| **No button style helper** | Full `widgetStyle` brush tree is huge and fragile | UI-MCP-001 |
| **No layout helper** | Canvas anchors/offsets hand-assembled; VB `size` rejected | UI-MCP-005 |
| **No AddToViewport / ShowWidget** | Screen HUD requires WC or custom BP BeginPlay | UI-MCP-002 |
| **CaptureViewport ignores Screen UMG** | World-space WC + Simulate for UI pixels (also user-preferred diegetic path) | UI-MCP-003 |
| **No character-preview helper** | Cylinder `MMO_BodyProxy`; no mannequin in `/Game` | UI-MCP-004 |
| **No appearance bind API** | Class/hair/skin/armor buttons don’t drive mesh/MIC | UI-MCP-004 |
| **Blueprint create timeout** | `BlueprintTools.create` hung editor (~MCP -32001) | UI-MCP-008 |
| **Restore Packages modal** | Editor restart blocked MCP until Autosaves cleared | UI-MCP-008 |
| **Slot property schema traps** | Invalid keys throw instead of soft-skip | UI-MCP-005 |
| **StrictDict in execute_tool_script** | `.get(key, default)` forbidden | UI-MCP-009 |

### execute_tool_script / field techniques used

- Built entire 74-widget tree + fantasy styling in one sandboxed script (UMG + ObjectTools).
- World-space `WidgetComponent` + Simulate to put UI into `CaptureViewport` frames (now a **product direction**, not only a workaround).
- Cylinder `MMO_BodyProxy` as visible third-person stand-in when skeletal templates failed visually.
- Cleared `Saved/Autosaves` to bypass Restore Packages deadlock after forced editor restart.

---

## Design quality assessment (honest)

- **Not default grey UMG:** dark stone panels, bronze/ember accents, parchment text, letter-spaced titles, rounded bronze-outlined buttons.
- **Hierarchy present:** brand → tagline → primary actions; create flow has name, archetype, body/hair/skin/armor, Back/Confirm.
- **Gaps vs shippable MMO UI:** Roboto-only (no display font asset), button chrome is style-data not textured frames, no CommonUI input routing, appearance options do not yet recolor/swap the preview mesh (tooling gap), preview mesh quality limited by available assets (cylinder caveat).

---

## Tooling verdict

**We do not yet have proper goal-level tooling for MMO UI.** Epic `UMGToolSet` + `ObjectTools` can author a designed Widget Blueprint if the agent batches via `execute_tool_script`, but there is no intent-routed UI domain, no theme/style helpers, no Show-in-viewport primitive, and `CaptureViewport` cannot see Screen-space HUD. World-space WidgetComponent + Simulate is both a workable proof path **and** the user-preferred in-world overlay style — promote it with first-class APIs (`ShowWidgetInWorld`, capture that includes UI, preview director). Highest-value backlog: `CreateWidgetFromSpec`, style helpers, `ShowWidgetInWorld` / `ShowWidgetOnScreen`, CaptureViewport game-UI compositing (honest), character preview helpers — see field report UI-MCP-001…010.
