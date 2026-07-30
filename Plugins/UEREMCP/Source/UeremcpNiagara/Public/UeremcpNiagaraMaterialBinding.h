// UEREMCP — Niagara renderer material binding (WS-07).
//
// Composes GetRendererData / SetRendererData on UNiagaraExternalEditUtilities.
// Inline create_spec delegation blocked until WS-08 exports UeremcpMaterialService.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FNiagaraExternalEditContext;
class UNiagaraSystem;

/** One specification.materials entry. */
struct FUeremcpNiagaraMaterialRequest
{
	FString Role;
	FString ExistingAssetPath;
	TSharedPtr<FJsonObject> CreateSpec;
	bool bReuseIfPresent = true;
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

	/** Resolve direct asset paths to canonical UMaterialInterface object paths (probe root only). */
	static bool ResolveDirectMaterialPaths(
		const TArray<FUeremcpNiagaraMaterialRequest>& Requests,
		TMap<FString, FString>& OutRoleToCanonicalPath,
		TArray<FString>& OutUnresolved,
		TArray<FString>& OutPendingWs08,
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

	static bool PatchMeshRendererMaterial(
		TSharedPtr<FJsonObject>& PropertyValues,
		const FString& CanonicalMaterialPath,
		FString& OutConflictReason);

	static bool MaterialMatchesExpectedAfterReread(
		const FString& PropertyValuesJson,
		EUeremcpNiagaraRendererMaterialKind Kind,
		const FString& ExpectedCanonicalPath);

	static bool ApplyRoleMaterialBindings(
		UNiagaraSystem* System,
		const TArray<FString>& EmittersAdded,
		const TMap<FString, FString>& RoleToCanonicalMaterialPath,
		const TArray<FUeremcpNiagaraMaterialRequest>& Requests,
		FNiagaraExternalEditContext& Context,
		FUeremcpNiagaraMaterialBindingResult& OutResult,
		int32& InOutInternalOperations);
};
