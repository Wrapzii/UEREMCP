// UEREMCP — goal-level UI / presentation toolset (UI-MCP-001…017).
#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpUIToolset.generated.h"

/**
 * Goal-level UMG authoring for MMO overlays (start menu, inventory, diegetic HUD).
 *
 * Prefer CreateWidgetFromSpec + ShowWidgetInWorld over Epic UMGToolSet primitives.
 * World-space WidgetComponent is the preferred CaptureViewport proof path for
 * diegetic UI. CaptureUiFrame is honest about screen-space UMG limits.
 * Use ResolveIntent for start menu / inventory / paper-doll intents.
 */
UCLASS()
class UEREMCPUI_API UUeremcpUIToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0-ui-v1"); }

	/**
	 * Create a Widget Blueprint from a declarative screen/spec tree.
	 *
	 * Use when: MMORPG start menu, character create overlay, multi-screen UMG from one call.
	 * Inputs: action=create_widget_from_spec; specification.asset_path REQUIRED;
	 *   screens[] with brand/actions/fields; theme (northridge_fantasy|northridge_diegetic|custom);
	 *   options.save defaults true (crash-safe); options.compile defaults true.
	 * Outputs: asset_path, widget_count, screens, theme_id.
	 * Do not use for: hand-editing one property — use ObjectTools after create.
	 * Next: show_widget_in_world, apply_ui_theme, save_widget_asset.
	 * Example: {"protocol_version":"1.0","action":"create_widget_from_spec","options":{"save":true},"specification":{"asset_path":"/Game/UI/MCPProbe/WBP_Probe","theme":"northridge_fantasy","screens":[{"id":"main","layout":"overlay_left_panel","brand":{"title":"NORTHRIDGE ONLINE","tagline":"Forge your legend"},"actions":["Play","Quit"]}]}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|UI")
	static FString CreateWidgetFromSpec(const FString& RequestJson);

	/**
	 * Apply a fantasy MMO theme (or custom colors) to an existing Widget Blueprint.
	 *
	 * Use when: restyle panels/text/buttons without raw widgetStyle blobs.
	 * Inputs: action=apply_ui_theme; specification.widget_blueprint; theme id or inline theme object.
	 * Example: {"protocol_version":"1.0","action":"apply_ui_theme","specification":{"widget_blueprint":"/Game/UI/MCPProbe/WBP_Probe","theme":"northridge_fantasy"}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|UI")
	static FString ApplyUiTheme(const FString& RequestJson);

	/**
	 * Host a Widget Blueprint on a world-space WidgetComponent (preferred MMO overlay path).
	 *
	 * Use when: diegetic / in-world UI that CaptureViewport can see under Simulate.
	 * Inputs: action=show_widget_in_world; specification.widget_asset; host_label; transform; draw_size.
	 *   replace_owned=label destroys prior host with the same label.
	 * Outputs: host_actor, widget_component, space=World.
	 * Example: {"protocol_version":"1.0","action":"show_widget_in_world","specification":{"widget_asset":"/Game/UI/MCPProbe/WBP_Probe","host_label":"UEREMCP_UIHost_Probe","transform":{"location":[200,-150,120],"rotation":[0,140,0],"scale":[0.25,0.25,0.25]},"draw_size":[1280,720]}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|UI")
	static FString ShowWidgetInWorld(const FString& RequestJson);

	/**
	 * Host a Widget Blueprint as screen-space UI (WidgetComponent Screen or AddToViewport when PIE).
	 *
	 * Use when: HUD / pause / settings that must be screen-space.
	 * Honesty: CaptureViewport does NOT composite screen UMG — use capture_ui_frame or world host for proof.
	 * Example: {"protocol_version":"1.0","action":"show_widget_on_screen","specification":{"widget_asset":"/Game/UI/MCPProbe/WBP_Probe","z_order":100,"replace_owned":true}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|UI")
	static FString ShowWidgetOnScreen(const FString& RequestJson);

	/**
	 * Capture a proof frame that understands UI modes.
	 *
	 * Use when: CaptureViewport proof of world-space widgets, or honest reject for screen UMG.
	 * Inputs: action=capture_ui_frame; specification.path; include.world_space_widgets / screen_space_umg.
	 * If screen_space_umg=true and unsupported → rejected SCREEN_UMG_CAPTURE_UNSUPPORTED + next_args for World path.
	 * Example: {"protocol_version":"1.0","action":"capture_ui_frame","specification":{"path":"Saved/UEREMCP/MCPProbe/proof.png","include":{"world":true,"world_space_widgets":true,"screen_space_umg":false},"camera":{"focus_actors":["UEREMCP_UIHost_Probe"]}}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|UI")
	static FString CaptureUiFrame(const FString& RequestJson);

	/**
	 * Spawn a character preview stand (mannequin search with capsule/cylinder fallback).
	 *
	 * Use when: character create / inventory live preview next to a world UI host.
	 * Inputs: action=spawn_character_preview; specification.label; mesh=auto; fallback[]; location.
	 * Outputs: mesh_source = project|engine|proxy; actor_label.
	 * Example: {"protocol_version":"1.0","action":"spawn_character_preview","specification":{"label":"UEREMCP_CharPreview_Probe","mesh":"auto","location":[80,40,0],"require_visible":true}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|UI")
	static FString SpawnCharacterPreview(const FString& RequestJson);

	/**
	 * Persist a Widget Blueprint package to disk (crash-safe).
	 *
	 * Use when: after create/edit — unsaved WBPs die on editor crash (UI-MCP field finding).
	 * Inputs: action=save_widget_asset; specification.widget_blueprint OR asset_path.
	 * Example: {"protocol_version":"1.0","action":"save_widget_asset","specification":{"widget_blueprint":"/Game/UI/MCPProbe/WBP_Probe"}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|UI")
	static FString SaveWidgetAsset(const FString& RequestJson);

	/**
	 * Create an inventory / character-sheet Widget Blueprint from grid/hotbar/paper-doll specs.
	 *
	 * Use when: MMO inventory sheet with backpack grids, equipment slots, hotbar.
	 * Inputs: action=create_inventory_sheet_from_spec; asset_path; grids[]; stats[]; theme.
	 * Example: {"protocol_version":"1.0","action":"create_inventory_sheet_from_spec","options":{"save":true},"specification":{"asset_path":"/Game/UI/MCPProbe/WBP_InvProbe","theme":"northridge_diegetic","grids":[{"id":"backpack","kind":"uniform_grid","rows":2,"cols":4,"cell_px":64},{"id":"hotbar","kind":"hotbar","slots":10,"show_index":true},{"id":"equipment","kind":"paper_doll","slots":["head","torso","feet"]}]}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|UI")
	static FString CreateInventorySheetFromSpec(const FString& RequestJson);

	/**
	 * Assign an icon brush (texture) and optional stack count to a named inventory slot widget.
	 *
	 * Use when: populate backpack/hotbar/equipment cells after create_inventory_sheet_from_spec.
	 * Inputs: action=set_slot_icon; widget_blueprint; slot name; icon.texture or icon.atlas+name.
	 * Example: {"protocol_version":"1.0","action":"set_slot_icon","specification":{"widget_blueprint":"/Game/UI/MCPProbe/WBP_InvProbe","slot":"Slot_Backpack_r0_c0","icon":{"texture":"/Engine/EngineResources/DefaultTexture"},"stack":{"count":3,"show":true}}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|UI")
	static FString SetSlotIcon(const FString& RequestJson);

	/**
	 * Update container weight TextBlocks (current / max KG).
	 *
	 * Use when: rig/backpack/total weight labels on an inventory sheet.
	 * Example: {"protocol_version":"1.0","action":"set_container_weight","specification":{"widget_blueprint":"/Game/UI/MCPProbe/WBP_InvProbe","containers":[{"id":"backpack","label_widget":"Txt_BackpackWeight","current_kg":18.3,"max_kg":40}]}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|UI")
	static FString SetContainerWeight(const FString& RequestJson);

	/**
	 * Apply fill-canvas / box-padding layout helpers without raw slot property soup.
	 *
	 * Use when: anchors fill or padding on a named widget/slot.
	 * Example: {"protocol_version":"1.0","action":"set_slot_layout","specification":{"widget_blueprint":"/Game/UI/MCPProbe/WBP_Probe","widget":"RootCanvas","canvas":{"anchors":"fill","offsets":[0,0,0,0]}}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|UI")
	static FString SetSlotLayout(const FString& RequestJson);
};
