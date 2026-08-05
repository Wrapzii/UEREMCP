// UEREMCP — Material inspect → MaterialGraph JSON (WS-08).
//
// Substrate: UMaterialEditingLibrary [VERIFIED: MaterialEditingLibrary.h:141,
// 317-349, 502-514] — same surface Epic MaterialTools wraps.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"

/** Parsed inspect_material specification. */
struct FUeremcpMaterialInspectSpec
{
	FString Query;
	FString AssetName;
	FString SearchRoot = TEXT("/Game");
	bool bIncludeExpressionGraph = true;
	bool bIncludeParameters = true;
	bool bIncludePropertyInputs = true;
	FString ResponseDetail;
};

/** Result of a read-only material inspection pass. */
struct FUeremcpMaterialInspectResult
{
	bool bSuccess = false;
	FString Error;

	FString Summary;
	FString ResolvedAssetPath;
	FString AssetClass;
	FString ParentMaterialPath;

	TArray<TSharedPtr<FJsonValue>> Graphs;
	TSharedPtr<FJsonObject> Parameters;
	TSharedPtr<FJsonObject> Fidelity;

	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;
	TArray<TSharedPtr<FJsonValue>> ExecutionTrace;
	TArray<FString> Candidates;

	int32 InternalOperations = 0;
	int32 ExpressionCount = 0;
	int32 ParameterCount = 0;
};

class FUeremcpMaterialInspect
{
public:
	static bool ParseSpecification(
		const TSharedPtr<FJsonObject>& Spec,
		FUeremcpMaterialInspectSpec& OutSpec,
		FString& OutError);

	/** Read-only inspect: any /Game/… path (production Free_Spells OK). */
	static bool IsAllowedInspectPath(const FString& AssetPath);

	/**
	 * Resolve target.asset_path from an exact soft path and/or specification.query /
	 * asset_name via AssetRegistry (UMaterial / UMaterialInstanceConstant).
	 */
	static bool ResolveTargetPath(
		const FUeremcpRequest& Request,
		const FUeremcpMaterialInspectSpec& Spec,
		FString& OutAssetPath,
		FString& OutError,
		TArray<FString>& OutCandidates);

	static bool Run(
		const FUeremcpRequest& Request,
		const FUeremcpMaterialInspectSpec& Spec,
		FUeremcpMaterialInspectResult& OutResult);
};
