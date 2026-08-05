// UEREMCP — Niagara renderer material binding (WS-07).
//
// Composes GetRendererData / SetRendererData on UNiagaraExternalEditUtilities.
// Inline create_spec delegates to UeremcpMaterialNiagaraExport (WS-08).

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"

struct FNiagaraExternalEditContext;
class UNiagaraMeshRendererProperties;
class UNiagaraSystem;

/** One specification.materials entry. */
struct FUeremcpNiagaraMaterialRequest
{
	FString Role;
	FString ExistingAssetPath;
	TSharedPtr<FJsonObject> CreateSpec;
	bool bReuseIfPresent = true;
};

/** WS-08 inline create_spec outcome for one materials.<role> entry. */
struct FUeremcpNiagaraInlineMaterialCreate
{
	FString Role;
	bool bSuccess = false;
	bool bShortCircuitedReuse = false;
	FString Status;
	FString Summary;
	FString PrimaryAsset;
	TArray<FUeremcpAssetRef> CreatedAssets;
	TArray<FUeremcpAssetRef> ModifiedAssets;
	TArray<FUeremcpAssetRef> ReusedAssets;
	TArray<FString> CapabilityNotes;
};

/** Outcome of material resolution + renderer binding. */
struct FUeremcpNiagaraMaterialBindingResult
{
	bool bAttempted = false;
	bool bAllRequestedVerified = false;
	bool bAnyBindingFailedReread = false;

	TMap<FString, FString> ResolvedMaterialPaths;
	TArray<FString> RendererBindingsApplied;
	TArray<FString> RendererBindingsVerified;
	TArray<FString> UnresolvedMaterialBindings;
	TArray<FUeremcpNiagaraInlineMaterialCreate> InlineMaterialCreates;

	/** Soft paths for inline materials deferred to WS-08 create_vfx_material (may be empty). */
	TArray<FString> CreatedMaterialAssetsPendingWs08;

	FString Error;
};

enum class EUeremcpNiagaraRendererMaterialKind : uint8
{
	Unsupported,
	Sprite,
	Ribbon,
	Mesh,
};

/** Parse, patch, apply, and verify renderer material bindings. */
class FUeremcpNiagaraMaterialBinding
{
public:
	static bool ParseMaterialRequests(
		const TSharedPtr<FJsonObject>& Specification,
		TArray<FUeremcpNiagaraMaterialRequest>& OutRequests,
		FString& OutError);

	/** Validate direct asset paths under probe root and canonicalize object paths (no UObject load). */
	static bool ResolveDirectMaterialPaths(
		const TArray<FUeremcpNiagaraMaterialRequest>& Requests,
		TMap<FString, FString>& OutRoleToCanonicalPath,
		TArray<FString>& OutUnresolved,
		FString& OutError);

	/** Resolve direct paths and inline create_spec via UeremcpMaterialNiagaraExport (probe root only). */
	static bool ResolveMaterialPaths(
		const FString& NiagaraSystemPackagePath,
		const TArray<FUeremcpNiagaraMaterialRequest>& Requests,
		bool bCompile,
		bool bValidate,
		bool bSave,
		TMap<FString, FString>& OutRoleToCanonicalPath,
		TArray<FUeremcpNiagaraInlineMaterialCreate>& OutInlineCreates,
		TArray<FString>& OutUnresolved,
		int32& InOutInternalOperations,
		FString& OutError);

	static EUeremcpNiagaraRendererMaterialKind ClassifyRenderer(
		const FString& RendererClassPath);

	/** Pure JSON patch helpers (unit-tested offline). */
	static bool HasValidUserMaterialBinding(
		const TSharedPtr<FJsonObject>& PropertyValues,
		const FString& BindingFieldName);

	static bool PatchSpriteOrRibbonMaterial(
		TSharedPtr<FJsonObject>& PropertyValues,
		const FString& CanonicalMaterialPath,
		FString& OutConflictReason);

	static bool EnableMeshMaterialOverrides(
		TSharedPtr<FJsonObject>& PropertyValues);

	static bool PatchMeshRendererMaterial(
		TSharedPtr<FJsonObject>& PropertyValues,
		const FString& CanonicalMaterialPath,
		FString& OutConflictReason);

	/** Enable bOverrideMaterials on mesh renderers whose templates ship OverrideMaterials slots. */
	static void NormalizeMeshRendererOverrideFlags(
		UNiagaraSystem* System,
		FNiagaraExternalEditContext& Context,
		int32& InOutInternalOperations);

	/**
	 * Mesh renderer inspect/bind path that avoids GetRendererData/SetRendererData.
	 * GetAllObjectProperties evaluates FNiagaraMeshMaterialOverride::ExplicitMat edit
	 * conditions in struct scope and LogErrors on bOverrideMaterials
	 * [VERIFIED: NiagaraMeshRendererProperties.h:70-74,227,273-274].
	 */
	static TSharedPtr<FJsonObject> BuildMeshRendererObservabilityPropertyValues(
		UNiagaraMeshRendererProperties* MeshProps);

	static FString ExtractMaterialPathFromMeshRenderer(
		UNiagaraMeshRendererProperties* MeshProps);

	static bool MeshRendererMaterialMatchesExpected(
		UNiagaraMeshRendererProperties* MeshProps,
		const FString& ExpectedCanonicalPath);

	/** Compare material object paths with FSoftObjectPath equivalence. */
	static bool MaterialObjectPathsMatch(
		const FString& ActualPath,
		const FString& ExpectedPath);

	static bool MaterialMatchesExpectedAfterReread(
		const FString& PropertyValuesJson,
		EUeremcpNiagaraRendererMaterialKind Kind,
		const FString& ExpectedCanonicalPath);

	/** Merge role purpose defaults and WS-08-required texture specs into inline create_spec. */
	static TSharedPtr<FJsonObject> PrepareInlineCreateSpec(
		const FString& Role,
		const TSharedPtr<FJsonObject>& CreateSpec);

	static bool ApplyRoleMaterialBindings(
		UNiagaraSystem* System,
		const TArray<FString>& EmittersAdded,
		const TMap<FString, FString>& RoleToCanonicalMaterialPath,
		const TArray<FUeremcpNiagaraMaterialRequest>& Requests,
		FNiagaraExternalEditContext& Context,
		FUeremcpNiagaraMaterialBindingResult& OutResult,
		int32& InOutInternalOperations);
};
