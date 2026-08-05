// UEREMCP — Material submit from edited MaterialGraph / parameters JSON (WS-08).
//
// Prefer in-place production edits. Never silent-delete user masters.
// round_trip_supported stays false until proven.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"

struct FUeremcpMaterialSubmitApply
{
	bool bParameters = true;
	bool bLinks = true;
	bool bPropertyInputs = true;
	bool bCreateMissingExpressions = false;
	bool bDeleteMissingExpressions = false;
};

struct FUeremcpMaterialSubmitSpec
{
	TArray<TSharedPtr<FJsonValue>> Graphs;
	TSharedPtr<FJsonObject> Parameters;
	FUeremcpMaterialSubmitApply Apply;
	FString ParentMaterial;
};

struct FUeremcpMaterialSubmitResult
{
	bool bSuccess = false;
	FString Status;
	FString Summary;
	FString Error;

	TArray<TSharedPtr<FJsonValue>> PlannedChanges;
	TArray<TSharedPtr<FJsonValue>> AppliedChanges;
	TArray<FString> Errors;
	TArray<FString> InterpretationNotes;
	TArray<FString> CapabilityNotes;

	TArray<FUeremcpAssetRef> CreatedAssets;
	TArray<FUeremcpAssetRef> ModifiedAssets;

	TSharedPtr<FJsonObject> ResultPayload;
	int32 InternalOperations = 0;
};

class FUeremcpMaterialSubmit
{
public:
	static bool ParseSpecification(
		const TSharedPtr<FJsonObject>& Spec,
		FUeremcpMaterialSubmitSpec& OutSpec,
		FString& OutError);

	static bool Run(
		const FUeremcpRequest& Request,
		const FUeremcpMaterialSubmitSpec& Spec,
		FUeremcpMaterialSubmitResult& OutResult);
};
