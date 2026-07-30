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
	 *
	 * @param RequestJson  Request envelope (schemas/envelope/request.schema.json).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString Echo(const FString& RequestJson);

	/**
	 * Create or update a VFX material (elemental projectile core/trail, fresnel, color).
	 *
	 * Use when: make a VFX/shader material, change fireball color/orange tint, translucent FX mats.
	 * Inputs: action=create_vfx_material, target.asset_path, specification element/role;
	 * prefer options.dry_run + validate=true; idempotency_key recommended.
	 * Outputs: created/modified statuses with parameter re-read when validate=true.
	 * Do not use for: MaterialTools expression graphs; MaterialInstanceConstant authoring gaps.
	 * Next tool: CreateNiagaraEffect to bind the material into an effect.
	 *
	 * @param RequestJson  Request with action create_vfx_material and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString CreateVfxMaterial(const FString& RequestJson);

	/**
	 * Generate a procedural Texture2D (noise, dissolve masks) under /Game/__UeremcpTests/.
	 *
	 * Use when: tileable noise/mask textures for VFX materials.
	 * Do not use for: full material graphs — use CreateVfxMaterial.
	 * Next tool: CreateVfxMaterial referencing the texture path.
	 *
	 * @param RequestJson  Request with action create_procedural_texture and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString CreateProceduralTexture(const FString& RequestJson);
};
