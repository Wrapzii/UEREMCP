// UEREMCP — Material domain toolset (WS-08).
//
// Wave 2 scaffold: envelope echo + create_vfx_material stub with honest
// capability_notes until real Epic MaterialTools composition lands.

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
	 * Create or update a VFX material instance from element templates (stub).
	 *
	 * Wave 2 first slice: validates the envelope, echoes understood specification
	 * fields, and returns honest capability_notes. Does not yet call Epic
	 * MaterialTools / MaterialEditingLibrary.
	 *
	 * @param RequestJson  Request with action create_vfx_material and target.asset_path set.
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Material")
	static FString CreateVfxMaterial(const FString& RequestJson);
};
