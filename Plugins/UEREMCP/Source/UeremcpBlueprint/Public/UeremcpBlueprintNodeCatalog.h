// UEREMCP — Blueprint node catalog (WS-06).
//
// Answers "which node types can I put in this graph, and what are their pins
// called" in one call, so SubmitGraph authors do not have to guess node class
// names or pin names. Read-only; composes Epic's FBlueprintActionDatabase
// rather than re-exposing pin primitives (AGENTS.md rule 2).

#pragma once

#include "CoreMinimal.h"

class UEdGraph;

/** One pin on a catalog entry's template node. */
struct UEREMCPBLUEPRINT_API FUeremcpNodeCatalogPin
{
	/** UEdGraphPin::PinName — the exact string SubmitGraph edges must use. */
	FString Name;

	/** Human-facing pin label when it differs from Name; empty otherwise. */
	FString FriendlyName;

	/** "input" or "output". */
	FString Direction;

	/** FEdGraphPinType::PinCategory, e.g. "exec", "bool", "object". */
	FString Category;

	/** FEdGraphPinType::PinSubCategory; often empty. */
	FString SubCategory;

	/** Path name of PinSubCategoryObject when set, e.g. "/Script/Engine.Actor". */
	FString SubCategoryObject;

	/** "none", "array", "set" or "map". */
	FString ContainerType;

	/** Literal default the node ships with; empty when there is none. */
	FString DefaultValue;

	bool bIsReference = false;
};

/** One node type the agent may place in the context graph. */
struct UEREMCPBLUEPRINT_API FUeremcpNodeCatalogEntry
{
	/** UEdGraphNode subclass path, e.g. "/Script/BlueprintGraph.K2Node_CallFunction". */
	FString NodeClass;

	/** Short class name, e.g. "K2Node_CallFunction" — what graph JSON node_class carries. */
	FString NodeClassName;

	/** Palette title from the spawner's UI spec. */
	FString MenuName;

	/** Palette category, e.g. "Utilities|String". */
	FString Category;

	FString Tooltip;

	FString Keywords;

	/**
	 * True when pins were read off a real template node. False means the
	 * spawner declined to produce a template for this graph and Pins is empty —
	 * reported honestly rather than guessed (AGENTS.md rule 6).
	 */
	bool bPinsResolved = false;

	TArray<FUeremcpNodeCatalogPin> Pins;
};

/** Filters applied before any template node is instantiated. */
struct UEREMCPBLUEPRINT_API FUeremcpNodeCatalogQuery
{
	/** Case-insensitive substring matched against menu name, category and keywords. */
	FString Search;

	/** Exact short class names to keep, e.g. "K2Node_CallFunction". Empty means any. */
	TArray<FString> NodeClassNames;

	/** Case-insensitive category prefixes to keep. Empty means any. */
	TArray<FString> Categories;

	/** Instantiate template nodes to report pin signatures. */
	bool bIncludePins = true;

	/** Hard cap on returned entries; template nodes are built only for these. */
	int32 MaxResults = 100;
};

struct UEREMCPBLUEPRINT_API FUeremcpNodeCatalogResult
{
	TArray<FUeremcpNodeCatalogEntry> Entries;

	/** Spawners the database offered before filtering. */
	int32 TotalScanned = 0;

	/** Spawners that passed the filters, before MaxResults truncation. */
	int32 TotalMatched = 0;

	/** True when TotalMatched exceeded MaxResults. */
	bool bTruncated = false;

	/** Entries whose template node could not be built (bPinsResolved false). */
	int32 PinsUnresolved = 0;
};

/**
 * Queries Epic's Blueprint action database for placeable node types.
 *
 * [VERIFIED: Editor/BlueprintGraph/Public/BlueprintActionDatabase.h:66,94]
 * [VERIFIED: Editor/BlueprintGraph/Public/BlueprintNodeSpawner.h:149,152,178,235]
 */
class UEREMCPBLUEPRINT_API FUeremcpBlueprintNodeCatalog
{
public:
	/**
	 * @param ContextGraph Graph the results must be placeable in. Required —
	 *        pin signatures depend on the owning graph's schema.
	 */
	static bool Query(
		UEdGraph* ContextGraph,
		const FUeremcpNodeCatalogQuery& InQuery,
		FUeremcpNodeCatalogResult& OutResult,
		FString& OutError);
};
