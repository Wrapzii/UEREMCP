// UEREMCP — Blueprint graph read path (WS-06 P1, ADR-0004).
//
// Walks UEdGraph nodes in C++ with structured FEdGraphPinType (not Epic display strings).
// Optionally attaches extensions.blueprint.dsl via Epic BlueprintTools.read_graph_dsl.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UBlueprint;
class UEdGraph;

struct FUeremcpBlueprintReadGraphOptions
{
	/** Graph within the asset; empty selects the primary event graph. */
	FString GraphId;

	/** When true, call Epic read_graph_dsl for extensions.blueprint.dsl (debug only). */
	bool bIncludeDsl = false;

	/** From request.options.response_detail — gates payload size. */
	FString ResponseDetail = TEXT("complete");
};

struct FUeremcpBlueprintReadGraphResult
{
	bool bSuccess = false;
	FString Error;

	TSharedPtr<FJsonObject> Graph;
	FString ContentHash;

	int32 InternalOperations = 0;
};

/** Reads one Blueprint graph into graph.schema.json shape + diagnostics + fidelity. */
class UEREMCPBLUEPRINT_API FUeremcpBlueprintGraphReader
{
public:
	static bool ReadGraph(
		UBlueprint* Blueprint,
		const FString& AssetPath,
		const FUeremcpBlueprintReadGraphOptions& Options,
		FUeremcpBlueprintReadGraphResult& OutResult);

	static TArray<FString> DefaultLossyAreas();
};
