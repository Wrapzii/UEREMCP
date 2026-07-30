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
 * Per ADR-0002 one UToolsetDefinition per domain. Primitives from MaterialTools.*
 * are internalised via execute_tool_script batching; agents see goal-level actions
 * such as create_vfx_material.
 */
UCLASS()
class UEREMCPMATERIAL_API UUeremcpMaterialToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:

	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0-wave2-scaffold"); }

	/**
	 * Protocol probe — mirrors UUeremcpReferenceToolset::Echo without touching assets.
	 *
	 * @param RequestJson  Request envelope (schemas/envelope/request.schema.json).
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString Echo(const FString& RequestJson);

	/**
	 * Create or update a VFX material instance from element templates.
	 *
	 * Wave 2 slice: elemental_projectile_core|trail (+ fireball aliases) under
	 * /Game/__UeremcpTests/. Ensures minimal master, creates MI, applies element
	 * defaults + modifiers, recompiles parent, re-reads parameters for validation.
	 *
	 * @param RequestJson  Request with action create_vfx_material and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString CreateVfxMaterial(const FString& RequestJson);
};
