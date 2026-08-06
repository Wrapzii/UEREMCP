#include "UeremcpBlueprintNodeCatalog.h"

#include "BlueprintActionDatabase.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

namespace UeremcpNodeCatalog
{
	// EEdGraphPinDirection [VERIFIED: Runtime/Engine/Classes/EdGraph/EdGraphNode.h:99-101]
	static FString DirectionToString(EEdGraphPinDirection Direction)
	{
		return Direction == EGPD_Input ? TEXT("input") : TEXT("output");
	}

	// EPinContainerType [VERIFIED: Runtime/Engine/Classes/EdGraph/EdGraphNode.h:123-129]
	static FString ContainerTypeToString(EPinContainerType ContainerType)
	{
		switch (ContainerType)
		{
		case EPinContainerType::Array: return TEXT("array");
		case EPinContainerType::Set:   return TEXT("set");
		case EPinContainerType::Map:   return TEXT("map");
		default:                       return TEXT("none");
		}
	}

	static bool MatchesSearch(
		const FString& Search,
		const FString& MenuName,
		const FString& Category,
		const FString& Keywords)
	{
		if (Search.IsEmpty())
		{
			return true;
		}
		return MenuName.Contains(Search, ESearchCase::IgnoreCase)
			|| Category.Contains(Search, ESearchCase::IgnoreCase)
			|| Keywords.Contains(Search, ESearchCase::IgnoreCase);
	}

	static bool MatchesCategories(const TArray<FString>& Categories, const FString& Category)
	{
		if (Categories.Num() == 0)
		{
			return true;
		}
		for (const FString& Wanted : Categories)
		{
			if (Category.StartsWith(Wanted, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	static bool MatchesNodeClasses(const TArray<FString>& NodeClassNames, const FString& ClassName)
	{
		if (NodeClassNames.Num() == 0)
		{
			return true;
		}
		for (const FString& Wanted : NodeClassNames)
		{
			if (ClassName.Equals(Wanted, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * Reads pin signatures off a template node. The template is owned and cached
	 * by the spawner; we only read from it.
	 * [VERIFIED: Editor/BlueprintGraph/Public/BlueprintNodeSpawner.h:235]
	 * [VERIFIED: Runtime/Engine/Classes/EdGraph/EdGraphNode.h:293]
	 */
	static bool ReadTemplatePins(
		const UBlueprintNodeSpawner& Spawner,
		UEdGraph* ContextGraph,
		TArray<FUeremcpNodeCatalogPin>& OutPins)
	{
		const UEdGraphNode* Template = Spawner.GetTemplateNode(ContextGraph);
		if (!Template)
		{
			return false;
		}

		for (const UEdGraphPin* Pin : Template->Pins)
		{
			if (!Pin)
			{
				continue;
			}

			FUeremcpNodeCatalogPin Entry;
			// [VERIFIED: Runtime/Engine/Classes/EdGraph/EdGraphPin.h:306,312,373,383,386]
			Entry.Name = Pin->PinName.ToString();
			if (!Pin->PinFriendlyName.IsEmpty())
			{
				const FString Friendly = Pin->PinFriendlyName.ToString();
				if (!Friendly.Equals(Entry.Name, ESearchCase::CaseSensitive))
				{
					Entry.FriendlyName = Friendly;
				}
			}
			Entry.Direction = DirectionToString(static_cast<EEdGraphPinDirection>(Pin->Direction.GetValue()));
			Entry.DefaultValue = Pin->DefaultValue;

			// FEdGraphPinType [VERIFIED: Runtime/Engine/Classes/EdGraph/EdGraphPin.h:82,86,90,101,111]
			const FEdGraphPinType& PinType = Pin->PinType;
			Entry.Category = PinType.PinCategory.ToString();
			Entry.SubCategory = PinType.PinSubCategory.ToString();
			if (const UObject* SubObject = PinType.PinSubCategoryObject.Get())
			{
				Entry.SubCategoryObject = SubObject->GetPathName();
			}
			Entry.ContainerType = ContainerTypeToString(PinType.ContainerType);
			Entry.bIsReference = PinType.bIsReference != 0;

			OutPins.Add(MoveTemp(Entry));
		}

		return true;
	}
}

bool FUeremcpBlueprintNodeCatalog::Query(
	UEdGraph* ContextGraph,
	const FUeremcpNodeCatalogQuery& InQuery,
	FUeremcpNodeCatalogResult& OutResult,
	FString& OutError)
{
	if (!ContextGraph)
	{
		OutError = TEXT("describe_node_catalog requires a context graph; pin signatures depend on the owning graph schema.");
		return false;
	}

	const int32 MaxResults = FMath::Clamp(InQuery.MaxResults, 1, 500);

	// FBlueprintActionDatabase is editor-only and is the same source the palette
	// uses, so the catalog cannot drift from what the editor will actually place.
	// [VERIFIED: Editor/BlueprintGraph/Public/BlueprintActionDatabase.h:66,94]
	FBlueprintActionDatabase& Database = FBlueprintActionDatabase::Get();
	const FBlueprintActionDatabase::FActionRegistry& Registry = Database.GetAllActions();

	// Two passes: filter on the cheap UI signature first, and only instantiate
	// template nodes for entries that survive the cap. Priming every spawner in
	// the database would cost thousands of node allocations per call.
	for (const TPair<FObjectKey, FBlueprintActionDatabase::FActionList>& RegistryPair : Registry)
	{
		for (const TObjectPtr<UBlueprintNodeSpawner>& SpawnerPtr : RegistryPair.Value)
		{
			const UBlueprintNodeSpawner* Spawner = SpawnerPtr.Get();
			if (!Spawner)
			{
				continue;
			}

			++OutResult.TotalScanned;

			// [VERIFIED: Editor/BlueprintGraph/Public/BlueprintNodeSpawner.h:149]
			const UClass* NodeClass = Spawner->NodeClass.Get();
			if (!NodeClass)
			{
				continue;
			}

			const FString NodeClassName = NodeClass->GetName();
			if (!UeremcpNodeCatalog::MatchesNodeClasses(InQuery.NodeClassNames, NodeClassName))
			{
				continue;
			}

			// PrimeDefaultUiSpec fills MenuName/Category/Tooltip for spawners that
			// defer them. [VERIFIED: Editor/BlueprintGraph/Public/BlueprintNodeSpawner.h:178]
			const FBlueprintActionUiSpec& UiSpec = Spawner->PrimeDefaultUiSpec(ContextGraph);
			const FString MenuName = UiSpec.MenuName.ToString();
			const FString Category = UiSpec.Category.ToString();
			const FString Keywords = UiSpec.Keywords.ToString();

			if (MenuName.IsEmpty())
			{
				continue;
			}
			if (!UeremcpNodeCatalog::MatchesCategories(InQuery.Categories, Category))
			{
				continue;
			}
			if (!UeremcpNodeCatalog::MatchesSearch(InQuery.Search, MenuName, Category, Keywords))
			{
				continue;
			}

			++OutResult.TotalMatched;

			if (OutResult.Entries.Num() >= MaxResults)
			{
				OutResult.bTruncated = true;
				continue;
			}

			FUeremcpNodeCatalogEntry Entry;
			Entry.NodeClass = NodeClass->GetPathName();
			Entry.NodeClassName = NodeClassName;
			Entry.MenuName = MenuName;
			Entry.Category = Category;
			Entry.Tooltip = UiSpec.Tooltip.ToString();
			Entry.Keywords = Keywords;

			if (InQuery.bIncludePins)
			{
				Entry.bPinsResolved =
					UeremcpNodeCatalog::ReadTemplatePins(*Spawner, ContextGraph, Entry.Pins);
				if (!Entry.bPinsResolved)
				{
					++OutResult.PinsUnresolved;
				}
			}

			OutResult.Entries.Add(MoveTemp(Entry));
		}
	}

	return true;
}
