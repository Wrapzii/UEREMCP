// Blueprint graph replace writer (WS-06 P2). Composes Epic write_graph_dsl.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBlueprint;

struct FUeremcpBlueprintReplaceGraphOptions
{
	FString AssetPath;
	FString GraphId;
	bool bDryRun = false;
	bool bCompile = true;
	bool bValidate = true;
	bool bSave = true;
};

struct FUeremcpBlueprintReplaceGraphResult
{
	bool bSuccess = false;
	FString Error;
	FString DslUsed;
	FString RevisionAfter;
	FString RereadHash;
	bool bCompiled = false;
	bool bSaved = false;
	int32 InternalOperations = 0;
	TArray<FString> LossyAreas;
	TArray<FString> CapabilityNotes;
};

class UEREMCPBLUEPRINT_API FUeremcpBlueprintGraphWriter
{
public:
	/** Scratch writes only — /Game/__UeremcpTests/ (RB-14). */
	static bool IsScratchAssetPath(const FString& AssetPath);

	/** Pre-flight ADR-0004 shape + DSL resolution before load/write (honest failed_validation). */
	static bool ValidateSubmittedGraphForReplace(
		const TSharedPtr<FJsonObject>& SubmittedGraph,
		const FString& ExpectedAssetPath,
		const FString& ExpectedGraphId,
		FString& OutError,
		TArray<FString>& OutCapabilityNotes);

	static bool ResolveWriteDsl(
		const TSharedPtr<FJsonObject>& SubmittedGraph,
		FString& OutDsl,
		TArray<FString>& OutLossyNotes,
		FString& OutError);

	/** True when resolved DSL differs; extensions.blueprint.dsl is part of write intent. */
	static bool WriteIntentDiffers(
		const TSharedPtr<FJsonObject>& SubmittedGraph,
		const TSharedPtr<FJsonObject>& CurrentGraph);

	static bool ReplaceGraph(
		UBlueprint* Blueprint,
		const TSharedPtr<FJsonObject>& SubmittedGraph,
		const FUeremcpBlueprintReplaceGraphOptions& Options,
		FUeremcpBlueprintReplaceGraphResult& OutResult);
};
