// UEREMCP — cross-module export surface for WS-07 Niagara material bindings.
//
// Contract: docs/proposals/ws-07-niagara-material-bindings.md (0f6378b).
// WS-07 calls these helpers + UeremcpMaterialService::ExecuteCreateVfxMaterial.

#pragma once

#include "CoreMinimal.h"
#include "UeremcpEnvelope.h"
#include "UeremcpMaterialPaths.h"
#include "UeremcpMaterialService.h"

namespace UeremcpMaterialNiagaraExport
{
	/** Sanitize Niagara asset / role tokens for stable MI asset names. */
	UEREMCPMATERIAL_API FString SanitizeAssetToken(const FString& Token);

	/**
	 * Deterministic MI package path for inline Niagara material creation.
	 * Pattern: <ScratchContentRoot>/Materials/MI_<NiagaraName>_<Role>
	 */
	UEREMCPMATERIAL_API FString ResolveMaterialInstancePath(
		const FString& NiagaraAssetName,
		const FString& Role,
		const FString& ScratchContentRoot = UeremcpMaterialPaths::TestsContentRoot);

	/** Derive scratch content root from a Niagara system package path and build MI path. */
	UEREMCPMATERIAL_API FString ResolveMaterialInstancePathForNiagaraSystem(
		const FString& NiagaraSystemPackagePath,
		const FString& Role);

	/**
	 * Map Niagara materials.<role> keys to create_vfx_material purpose when omitted
	 * from create_spec. Returns false when role has no default (caller must supply purpose).
	 */
	UEREMCPMATERIAL_API bool ResolvePurposeForNiagaraRole(const FString& Role, FString& OutPurpose);

	/** Build an internal create_vfx_material request from WS-07 create_spec JSON. */
	UEREMCPMATERIAL_API FUeremcpRequest BuildCreateVfxMaterialRequest(
		const FString& TargetAssetPath,
		const TSharedPtr<class FJsonObject>& CreateSpec,
		bool bCompile = true,
		bool bValidate = true,
		bool bSave = true,
		bool bDryRun = false);

	/**
	 * WS-07 acceptance gate: load PrimaryAsset and confirm UMaterialInterface.
	 * PackagePath must be FSoftObjectPath-compatible (e.g. /Game/__UeremcpTests/Materials/MI_Foo).
	 */
	UEREMCPMATERIAL_API bool VerifyPrimaryAssetIsMaterialInterface(
		const FString& PackagePath,
		FString& OutError);

	/**
	 * Convenience: resolve path from Niagara name + role under TestsContentRoot (automation only).
	 * POC/fireball inline creates must use ExecuteCreateVfxMaterialForNiagaraSystem.
	 */
	UEREMCPMATERIAL_API FUeremcpMaterialCreateResult ExecuteCreateVfxMaterialForNiagaraRole(
		const FString& NiagaraAssetName,
		const FString& Role,
		const TSharedPtr<class FJsonObject>& CreateSpec,
		bool bCompile = true,
		bool bValidate = true,
		bool bSave = true,
		bool bDryRun = false);

	/**
	 * Inline create for a Niagara system package path; MIs co-locate under the system's scratch root.
	 * Example: /Game/__UeremcpPoc/NS_POCB_Fireball → /Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_<role>
	 */
	UEREMCPMATERIAL_API FUeremcpMaterialCreateResult ExecuteCreateVfxMaterialForNiagaraSystem(
		const FString& NiagaraSystemPackagePath,
		const FString& Role,
		const TSharedPtr<class FJsonObject>& CreateSpec,
		bool bCompile = true,
		bool bValidate = true,
		bool bSave = true,
		bool bDryRun = false);
}
