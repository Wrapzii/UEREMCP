// UEREMCP — role name conventions shared by create + material bind (WS-07).

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace UeremcpNiagaraRoles
{
	/** Map specification role token to emitter name (e.g. ribbon_trail → RibbonTrail). */
	FString RoleToEmitterName(const FString& Role);

	/**
	 * Default emitter template soft path for a component role.
	 * Templates under /Niagara/DefaultAssets/Templates/Emitters/
	 * [VERIFIED: Engine/Plugins/FX/Niagara/Content/DefaultAssets/Templates/Emitters/].
	 */
	FString ResolveEmitterTemplatePath(const FString& Role);

	/** Canonical six-emitter POC B projectile plan (docs/POC_ACCEPTANCE.md B3). */
	TArray<FString> DefaultPocBComponentRoles();

	/**
	 * Default precipitation / weather emitter plan for BuildEnvironment rain.
	 * rain → RecycleParticlesInView; mist → HangingParticulates.
	 */
	TArray<FString> DefaultPrecipitationComponentRoles();

	/**
	 * Default burst / explosion plan (sparks + impact).
	 * sparks → SimpleSpriteBurst; impact_burst → OmnidirectionalBurst.
	 */
	TArray<FString> DefaultBurstComponentRoles();

	/**
	 * Default Free_Spells-like / spell FX plan: circle + sparks + mesh accents.
	 * circle → ConfettiBurst; sparks → SimpleSpriteBurst; mesh → UpwardMeshBurst.
	 */
	TArray<FString> DefaultSpellFxComponentRoles();

	/**
	 * Default ice / freeze plan: ground creep + freeze dome + spark accents.
	 * ice_creep → BlowingParticles; freeze_dome → HangingParticulates; sparks → SimpleSpriteBurst.
	 */
	TArray<FString> DefaultIceFreezeComponentRoles();

	/**
	 * Resolve default component roles when agents omit components/emitters.
	 * Never returns empty for known create effect_types — empty shell is a defect.
	 * Unknown types fall back to sparks so Create still authors a real emitter.
	 */
	TArray<FString> DefaultComponentRolesForEffectType(const FString& EffectType);

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
