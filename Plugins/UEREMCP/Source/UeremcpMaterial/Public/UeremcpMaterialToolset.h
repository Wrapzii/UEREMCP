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
 * Prefer CreateVfxMaterial / CreateProceduralTexture over Epic MaterialTools
 * expression graphs for VFX materials. Use ResolveIntent if unsure.
 */
UCLASS()
class UEREMCPMATERIAL_API UUeremcpMaterialToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	virtual FString GetToolsetVersion() const override { return TEXT("0.2.2-intent-vocab"); }

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
	 * Create or update a VFX material (elemental projectile core/trail, fresnel, color).
	 *
	 * Use when: make a VFX/shader material, change fireball color/orange tint, translucent FX mats.
	 * Inputs: action=create_vfx_material, target.asset_path, specification.purpose required;
	 * prefer options.dry_run + validate=true; idempotency_key recommended.
	 * Outputs: created/modified statuses with parameter re-read when validate=true.
	 * Do not use for: MaterialTools expression graphs; MaterialInstanceConstant authoring gaps.
	 * Next tool: CreateNiagaraEffect to bind the material into an effect.
	 * Example: {"protocol_version":"1.0","action":"create_vfx_material","target":{"asset_path":"/Game/__UeremcpTests/M_FireCore"},"options":{"dry_run":true,"validate":true},"specification":{"purpose":"projectile_core","element":"fire"}}
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
	 * Author a master material directly from a feature list. No template library.
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
};
