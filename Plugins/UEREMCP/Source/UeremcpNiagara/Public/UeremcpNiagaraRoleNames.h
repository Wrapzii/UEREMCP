// UEREMCP — role name conventions shared by create + material bind (WS-07).

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace UeremcpNiagaraRoles
{
	/** Map specification role token to emitter name (e.g. ribbon_trail → RibbonTrail). */
	FString RoleToEmitterName(const FString& Role);

	/** POC B default emitter template soft path for a component role. */
	FString ResolveEmitterTemplatePath(const FString& Role);

	/** Canonical six-emitter POC B projectile plan (docs/POC_ACCEPTANCE.md B3). */
	TArray<FString> DefaultPocBComponentRoles();

	/**
	 * Default precipitation / weather emitter plan for BuildEnvironment rain.
	 * rain → RecycleParticlesInView; mist → HangingParticulates.
	 * [VERIFIED: Engine/Plugins/FX/Niagara/Content/DefaultAssets/Templates/Emitters/]
	 */
	TArray<FString> DefaultPrecipitationComponentRoles();

	/**
	 * Default create_vfx_material purpose for a POC B materials.<role> entry.
	 * WS-08 currently wires elemental_projectile_core|trail only
	 * [VERIFIED: UeremcpMaterialService.cpp — purpose gate].
	 */
	FString DefaultPurposeForMaterialRole(const FString& Role);

	/** Default inline create_spec JSON object for a POC B fireball material role. */
	TSharedPtr<FJsonObject> BuildDefaultFireballMaterialCreateSpec(
		const FString& Role,
		const FString& Element = TEXT("fire"));
}
