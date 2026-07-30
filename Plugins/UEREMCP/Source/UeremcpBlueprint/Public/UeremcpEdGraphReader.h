// Shared read-only UEdGraph → graph.schema.json walker (WS-06 / ADR-0004).
//
// Family-specific semantic_id / semantic_type / extensions are supplied via hooks
// so WS-10 (AnimBP) can consume without forking pin/link enumeration.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UEdGraph;
class UEdGraphNode;
class UEdGraphPin;
struct FEdGraphPinType;

/** Optional family-specific semantics; unset hooks use neutral defaults. */
struct FUeremcpEdGraphSemanticHooks
{
	TFunction<FString(const UEdGraphNode*)> ResolveSemanticType;
	TFunction<FString(const UEdGraphNode*)> ResolveSemanticId;
	TFunction<TSharedPtr<FJsonObject>(const UEdGraphNode*)> ResolveProperties;
	TFunction<bool(const UEdGraphNode*)> IsEntryNode;
	TFunction<TArray<const UEdGraphNode*>(const UEdGraph*)> GatherEntryNodes;
	TFunction<bool(const UEdGraphPin*)> IsExecPin;
};

struct FUeremcpEdGraphReadOptions
{
	FString AssetPath;
	FString GraphName;
	/** ADR-0004 graph_type discriminator, e.g. BlueprintEventGraph / AnimBlueprintGraph. */
	FString GraphType;
	bool bEmitNodesAndLinks = true;
	bool bIncludePinDefaults = true;
	bool bRoundTripSupported = false;
	TArray<FString> LossyAreas;
	FString PurposeSummaryPrefix = TEXT("Graph");
};

struct FUeremcpEdGraphReadResult
{
	TSharedPtr<FJsonObject> Graph;
	FString Error;
	TSet<FString> DependencyPaths;
};

class UEREMCPBLUEPRINT_API FUeremcpEdGraphReader
{
public:
	static FString MakeNodeId(const UEdGraphNode* Node);
	static FString MakePinId(const UEdGraphPin* Pin);
	static TSharedPtr<FJsonObject> PinTypeToJson(const FEdGraphPinType& PinType);

	/** Walk UEdGraph nodes/pins/links into graph.schema.json shape (read-only). */
	static bool ReadGraph(
		const UEdGraph* Graph,
		const FUeremcpEdGraphReadOptions& Options,
		const FUeremcpEdGraphSemanticHooks& Hooks,
		FUeremcpEdGraphReadResult& OutResult);
};
