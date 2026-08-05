// UEREMCP — Material domain toolset (WS-08).
//
// Wave 2: create_vfx_material composes MaterialEditingLibrary (Epic MaterialTools
// substrate) for elemental projectile core/trail; envelope Echo for protocol checks.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "UeremcpMaterialToolset.generated.h"

/**
 * Agent-facing material operations for UEREMCP.
 *
 * Prefer InspectMaterial → SubmitMaterialGraph for existing masters/MIs (Free_Spells
 * included). Prefer CreateVfxMaterial / CreateMasterMaterial for new VFX masters.
 * Use ResolveIntent if unsure. CaptureMaterialFrames for visual proof.
 */
UCLASS()
class UEREMCPMATERIAL_API UUeremcpMaterialToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	virtual FString GetToolsetVersion() const override { return TEXT("0.3.0-material-graph"); }

	/**
	 * Protocol probe — mirrors UUeremcpReferenceToolset::Echo without touching assets.
	 *
	 * Use when: validating the Material module envelope path.
	 * Do not use for: creating materials or textures.
	 * Inputs: requestJson envelope; specification has no required keys.
	 * Example: {"protocol_version":"1.0","action":"echo","specification":{}}
	 *
	 * @param RequestJson  Request envelope (schemas/envelope/request.schema.json).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString Echo(const FString& RequestJson);

	/**
	 * Create or update a VFX material (elemental projectile, ice barrier/crystal, fresnel).
	 *
	 * Use when: VFX/shader materials including ice walls — prefer purpose ice_crystal /
	 *   elemental_ice_barrier (translucent Fresnel) over projectile_core for barriers.
	 * Inputs: action=create_vfx_material, target.asset_path, specification.purpose required;
	 * prefer options.dry_run + validate=true; idempotency_key recommended.
	 * Outputs: created/modified statuses with parameter re-read when validate=true.
	 * Do not use for: MaterialTools expression graphs; MaterialInstanceConstant authoring gaps.
	 * Next tool: CreateNiagaraEffect to bind the material into an effect.
	 * Example: {"protocol_version":"1.0","action":"create_vfx_material","target":{"asset_path":"/Game/__UeremcpTests/M_IceBarrier"},"options":{"dry_run":true,"validate":true},"specification":{"purpose":"elemental_ice_barrier","element":"ice"}}
	 *
	 * @param RequestJson  Request with action create_vfx_material and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString CreateVfxMaterial(const FString& RequestJson);

	/**
	 * Generate a procedural Texture2D (noise, dissolve masks) under /Game/__UeremcpTests/.
	 *
	 * Use when: tileable noise/mask textures for VFX materials.
	 * Inputs: action=create_procedural_texture, target.asset_path, specification.generate required.
	 * Do not use for: full material graphs — use CreateVfxMaterial.
	 * Next tool: CreateVfxMaterial referencing the texture path.
	 * Example: {"protocol_version":"1.0","action":"create_procedural_texture","target":{"asset_path":"/Game/__UeremcpTests/T_DissolveNoise"},"options":{"dry_run":true},"specification":{"generate":"noise","dimensions":[256,256],"seed":42}}
	 *
	 * @param RequestJson  Request with action create_procedural_texture and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString CreateProceduralTexture(const FString& RequestJson);

	/**
	 * Author a VFX master material from a feature list. No template library.
	 *
	 * VFX ONLY. The feature vocabulary is radial_falloff, animated_noise, fresnel,
	 *   erosion, depth_fade, distortion, panning_textures, flow_maps, flipbook_subuv,
	 *   dynamic_color, dynamic_intensity. There is NO base_color, roughness, metallic,
	 *   normal or tiling, so this CANNOT express a surface material. Asked for snow,
	 *   rock or grass it produces a named shell with nothing in it -- measured.
	 *   For terrain and prop surfaces use editor_toolset MaterialTools
	 *   (create_material + add_expression + connect_to_output), or import a real
	 *   material. A goal-level surface action does not exist yet.
	 *
	 * Use when: you need a material from scratch in an empty project, or a master
	 *   whose feature set no preset covers. This is the material PRIMITIVE FLOOR:
	 *   it composes MaterialEditingLibrary expressions and needs no existing asset.
	 * Inputs: action=create_master_material, target.asset_path (UMaterial under
	 *   /Game/__UeremcpTests/ or /Game/__UeremcpPoc/); specification.features is a
	 *   REQUIRED non-empty array of feature tokens; specification.trail optional bool.
	 *   Known tokens: radial_falloff, animated_noise, fresnel, erosion, depth_fade,
	 *   distortion, panning_textures, flow_maps, flipbook_subuv, dynamic_color,
	 *   dynamic_intensity. Unknown tokens are reported in skipped_features, not faked.
	 * Outputs: primary_asset (the master), wired_features, skipped_features.
	 * Do not use for: material INSTANCES with preset defaults — use CreateVfxMaterial.
	 * Next tool: CreateVfxMaterial with master_template set to this asset path.
	 * Example: {"protocol_version":"1.0","action":"create_master_material","target":{"asset_path":"/Game/__UeremcpTests/Materials/Masters/M_Stone"},"options":{"dry_run":true},"specification":{"features":["fresnel","erosion","dynamic_color"]}}
	 *
	 * @param RequestJson  Request with action create_master_material and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString CreateMasterMaterial(const FString& RequestJson);

	/**
	 * Author a layered LANDSCAPE material from height and slope bands.
	 *
	 * Use when: terrain surfaces -- snow above a treeline, rock on steep faces,
	 *   grass on flats, sand at a coast. This is the SURFACE material floor and
	 *   the only goal-level path to one.
	 * Inputs: action=create_landscape_material, target.asset_path (UMaterial);
	 *   specification.layers is a REQUIRED non-empty array. Each layer:
	 *   {"name":"grass","base_color":[r,g,b],"roughness":0.9,"min_height_m":0,
	 *    "max_height_m":800,"max_slope_deg":30,"base_color_texture":"/Game/..."}.
	 *   Layer name becomes the landscape paint layer.
	 * Outputs: primary_asset. Sets bUsedWithLandscape, without which a landscape
	 *   silently refuses the material -- a measured failure.
	 * Do not use for: VFX or particle materials -- use CreateVfxMaterial or
	 *   CreateMasterMaterial, whose feature vocabulary is VFX-only and cannot
	 *   express a surface at all.
	 * Next tool: assign it to the landscape, then CaptureWorldFrames to look.
	 * Example: {"protocol_version":"1.0","action":"create_landscape_material","target":{"asset_path":"/Game/__UeremcpPoc/Materials/M_Terrain"},"options":{"dry_run":true},"specification":{"layers":[{"name":"grass","base_color":[0.15,0.35,0.1],"roughness":0.9,"max_height_m":900,"max_slope_deg":30},{"name":"rock","base_color":[0.35,0.33,0.3],"roughness":0.8,"max_slope_deg":90},{"name":"snow","base_color":[0.9,0.92,0.95],"roughness":0.6,"min_height_m":1100}]}}
	 *
	 * @param RequestJson  Request with action create_landscape_material and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString CreateLandscapeMaterial(const FString& RequestJson);

	/**
	 * Set scalar/vector/texture parameter overrides on an existing MaterialInstanceConstant.
	 *
	 * Use when: tuning surface MI params (voxel cave anti-tile, roughness breakup, tiling)
	 *   on assets that already exist — e.g. MI_CaveVoxel_DryRock WorldTiling / AntiTileBlend /
	 *   WallStripeBreak without Epic MaterialInstanceTools.
	 * Inputs: action=update_material_instance_parameters, target.asset_path (MIC package path);
	 *   specification.scalar_overrides {string:float} required unless vector/texture overrides present;
	 *   optional specification.vector_overrides {string:[r,g,b,a]};
	 *   optional specification.texture_overrides {string:texture_asset_path};
	 *   options.dry_run (default false), options.save (default true).
	 * Outputs: parameter_changes with before/after per key, errors for refused keys.
	 * Do not use for: authoring new VFX materials — use CreateVfxMaterial; graph edits — use
	 *   CreateMasterMaterial.
	 * Next tool: CaptureWorldFrames to verify wall anti-tile in viewport.
	 * Example: {"protocol_version":"1.0","action":"update_material_instance_parameters","target":{"asset_path":"/Game/RE/Caves/Materials/MI_CaveVoxel_DryRock"},"options":{"dry_run":false,"save":true},"specification":{"scalar_overrides":{"AntiTileBlend":0.58,"WallStripeBreak":0.18,"WorldTiling":840.0}}}
	 *
	 * @param RequestJson  Request with action update_material_instance_parameters and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString UpdateMaterialInstanceParameters(const FString& RequestJson);

	/**
	 * Read a UMaterial or MaterialInstanceConstant into ADR-0004 MaterialGraph JSON.
	 *
	 * Use when: inspecting Free_Spells / production / scratch materials by path or name —
	 *   one call returns result.graphs[] (masters) and/or full parameter inventory with values.
	 * Inputs: action=inspect_material; target.asset_path OR specification.query/asset_name
	 *   under search_root (default /Game). Defaults to response_detail=complete.
	 * Outputs: result.asset_path, asset_class, graphs[], parameters, fidelity.round_trip_supported=false.
	 * Do not use for: Niagara systems — use InspectSystem; mutating edits — use SubmitMaterialGraph.
	 * Next tool: edit result.graphs[] / parameters then SubmitMaterialGraph; CaptureMaterialFrames for proof.
	 * Example: {"protocol_version":"1.0","action":"inspect_material","specification":{"query":"M_Free_Spells_Flash","search_root":"/Game"}}
	 *
	 * @param RequestJson  Request with action inspect_material.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString InspectMaterial(const FString& RequestJson);

	/**
	 * Apply edited MaterialGraph JSON and/or parameter maps to an existing material.
	 *
	 * Use when: writing back InspectMaterial edits (MIC params or master link/property rewires).
	 * Inputs: action=submit_material_graph, target.asset_path; specification.graphs and/or parameters;
	 *   options.dry_run supported. Production: in-place only; never silent-deletes masters.
	 * Outputs: planned/applied changes; fidelity.round_trip_supported=false; status partially_completed
	 *   or no_change_required (never *_validated until hash proof).
	 * Do not use for: authoring brand-new masters from empty graphs — use CreateMasterMaterial /
	 *   CreateVfxMaterial; Niagara — use SubmitNiagaraGraph.
	 * Next tool: InspectMaterial to re-read; CaptureMaterialFrames for visual proof.
	 * Example: {"protocol_version":"1.0","action":"submit_material_graph","target":{"asset_path":"/Game/.../MI_Free_Spells_Flash2"},"options":{"dry_run":true},"specification":{"parameters":{"scalar":{"EmissiveScale":4.0}}}}
	 *
	 * @param RequestJson  Request with action submit_material_graph.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString SubmitMaterialGraph(const FString& RequestJson);
};
