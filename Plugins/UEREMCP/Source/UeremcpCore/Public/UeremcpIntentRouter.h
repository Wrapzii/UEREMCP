// UEREMCP — live-registry intent router (WS-03).
//
// Candidates come only from UToolsetRegistry::GetAllToolsetJsonSchemas()
// [VERIFIED: $TR/.../Public/ToolsetRegistry/UToolsetRegistry.h:54-56].
// Structurally impossible to emit a tool name absent from the live registry.
// Ranking is lexical (BM25-ish); enrichment overlay may add vocabulary but never
// invent names.
//
// describe_toolset tool descriptions are harvested from UFUNCTION doc comments via
// UStructToJsonSchemaMetadata / GetToolTipText
// [VERIFIED: Engine/.../JsonSchemaGeneratorEditor.h:66-75]
// [VERIFIED: $TR/.../FunctionLibraryToolset.h:42-50].

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FUeremcpIntentRouterResult
{
	TSharedPtr<FJsonObject> Payload;
	FString Status = TEXT("no_change_required");
	FString Summary;
	TArray<FString> CapabilityNotes;
};

class UEREMCPCORE_API FUeremcpIntentRouter
{
public:
	/** Plain-text intent → ordered plan. Mode recommend | execute_if_complete. */
	static FUeremcpIntentRouterResult ResolveIntent(
		const FString& Intent,
		const FString& Mode,
		const TSharedPtr<FJsonObject>& Context,
		const FString& ExpectedRegistryHash,
		int32 MaxSteps);

	/** Bootstrap briefing + START HERE pointer to ResolveIntent. */
	static FUeremcpIntentRouterResult GetStarted(const FString& Detail);

	/** Schema + example for one registry-verified tool. */
	static FUeremcpIntentRouterResult DescribeOperation(const FString& ToolQuery);

	/** SHA-256 of sorted live qualified names. */
	static FString ComputeLiveRegistryHash();

	/** True when Name exists in the live registry (toolset.tool). */
	static bool LiveRegistryContains(const FString& QualifiedToolName);
};
