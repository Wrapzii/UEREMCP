// Editor automation tests for blueprints.describe_node_catalog (WS-06).
//
// The contract these tests defend: an agent that calls DescribeNodeCatalog gets
// node_class and pin names it can paste straight into a SubmitGraph payload,
// without an inspect -> guess -> retry loop.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ObjectTools.h"

#include "UeremcpBlueprintToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpNodeCatalogTest
{
	static const TCHAR* TestsRoot = TEXT("/Game/__UeremcpTests");
	static const TCHAR* SuiteName = TEXT("Blueprint_NodeCatalog");

	static FString MakePackagePath(const FString& AssetName)
	{
		return FString::Printf(TEXT("%s/%s/%s"), TestsRoot, SuiteName, *AssetName);
	}

	static void CleanupSuite()
	{
		const FString Root = FString::Printf(TEXT("%s/%s"), TestsRoot, SuiteName);
		TArray<FAssetData> Assets;
		const FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().GetAssetsByPath(FName(*Root), Assets, true);
		if (Assets.Num() > 0)
		{
			TArray<UObject*> Objects;
			for (const FAssetData& Asset : Assets)
			{
				if (UObject* Obj = Asset.GetAsset())
				{
					Objects.Add(Obj);
				}
			}
			ObjectTools::DeleteObjectsUnchecked(Objects);
		}
	}

	struct FScratchGuard
	{
		FScratchGuard() { CleanupSuite(); }
		~FScratchGuard() { CleanupSuite(); }
	};

	static UBlueprint* CreateScratchBlueprint(const FString& AssetName, FAutomationTestBase& Test)
	{
		const FString PackagePath = MakePackagePath(AssetName);
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("CreatePackage"), Package))
		{
			return nullptr;
		}
		Package->FullyLoad();

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			FName(*AssetName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			NAME_None);
		Test.TestNotNull(TEXT("CreateBlueprint"), Blueprint);
		return Blueprint;
	}

	static TSharedPtr<FJsonObject> ParseResponse(const FString& Json, FAutomationTestBase& Test)
	{
		TSharedPtr<FJsonObject> Response;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!Test.TestTrue(TEXT("response parses as JSON"), FJsonSerializer::Deserialize(Reader, Response)))
		{
			return nullptr;
		}
		return Response;
	}

	/** diagnostics.node_catalog, or null with a test failure recorded. */
	static TSharedPtr<FJsonObject> GetCatalog(
		const TSharedPtr<FJsonObject>& Response,
		FAutomationTestBase& Test)
	{
		if (!Response.IsValid())
		{
			return nullptr;
		}
		const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
		if (!Test.TestTrue(
				TEXT("response has diagnostics"),
				Response->TryGetObjectField(TEXT("diagnostics"), Diagnostics)))
		{
			return nullptr;
		}
		const TSharedPtr<FJsonObject>* Catalog = nullptr;
		if (!Test.TestTrue(
				TEXT("diagnostics has node_catalog"),
				(*Diagnostics)->TryGetObjectField(TEXT("node_catalog"), Catalog)))
		{
			return nullptr;
		}
		return *Catalog;
	}

	static FString MakeRequest(const FString& AssetPath, const FString& SpecificationJson)
	{
		return FString::Printf(
			TEXT(R"({"protocol_version":"1.0","request_id":"bp-catalog-1","action":"describe_node_catalog","target":{"asset_path":"%s","graph_id":"EventGraph"},"specification":%s})"),
			*AssetPath,
			*SpecificationJson);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintNodeCatalogSearchTest,
	"UeremcpBlueprint.Toolset.NodeCatalogSearch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintNodeCatalogSearchTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpNodeCatalogTest;

	FScratchGuard Guard;
	static const FString AssetName = TEXT("BP_NodeCatalog_Scratch");
	UBlueprint* Blueprint = CreateScratchBlueprint(AssetName, *this);
	if (!Blueprint)
	{
		return false;
	}

	const FString AssetPath = MakePackagePath(AssetName) + TEXT(".") + AssetName;
	const FString ResponseJson = UUeremcpBlueprintToolset::DescribeNodeCatalog(
		MakeRequest(AssetPath, TEXT(R"({"search":"print string","max_results":25})")));

	TSharedPtr<FJsonObject> Response = ParseResponse(ResponseJson, *this);
	if (!Response.IsValid())
	{
		return false;
	}

	TestEqual(
		TEXT("status is no_change_required"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("no_change_required")));

	TSharedPtr<FJsonObject> Catalog = GetCatalog(Response, *this);
	if (!Catalog.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("graph_id echoed"), Catalog->GetStringField(TEXT("graph_id")), FString(TEXT("EventGraph")));
	TestTrue(TEXT("scanned the action database"), Catalog->GetIntegerField(TEXT("total_scanned")) > 0);

	const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
	if (!TestTrue(TEXT("catalog has entries array"), Catalog->TryGetArrayField(TEXT("entries"), Entries)))
	{
		return false;
	}
	if (!TestTrue(TEXT("search for 'print string' matched at least one node"), Entries->Num() > 0))
	{
		return false;
	}
	TestTrue(TEXT("entries respect max_results"), Entries->Num() <= 25);

	// The payoff: a PrintString entry whose pins carry the real InString name.
	bool bFoundPrintString = false;
	bool bFoundInStringPin = false;
	for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
	{
		const TSharedPtr<FJsonObject> Entry = EntryValue->AsObject();
		if (!Entry.IsValid())
		{
			continue;
		}

		TestTrue(TEXT("entry has node_class"), !Entry->GetStringField(TEXT("node_class")).IsEmpty());
		TestTrue(TEXT("entry has menu_name"), !Entry->GetStringField(TEXT("menu_name")).IsEmpty());

		if (!Entry->GetStringField(TEXT("menu_name")).Equals(TEXT("Print String"), ESearchCase::IgnoreCase))
		{
			continue;
		}
		bFoundPrintString = true;

		TestEqual(
			TEXT("Print String resolves to K2Node_CallFunction"),
			Entry->GetStringField(TEXT("node_class_name")),
			FString(TEXT("K2Node_CallFunction")));
		TestTrue(TEXT("Print String pins resolved"), Entry->GetBoolField(TEXT("pins_resolved")));

		const TArray<TSharedPtr<FJsonValue>>* Pins = nullptr;
		if (!Entry->TryGetArrayField(TEXT("pins"), Pins))
		{
			continue;
		}
		for (const TSharedPtr<FJsonValue>& PinValue : *Pins)
		{
			const TSharedPtr<FJsonObject> Pin = PinValue->AsObject();
			if (Pin.IsValid() && Pin->GetStringField(TEXT("name")).Equals(TEXT("InString")))
			{
				bFoundInStringPin = true;
				TestEqual(
					TEXT("InString is an input pin"),
					Pin->GetStringField(TEXT("direction")),
					FString(TEXT("input")));
				TestEqual(
					TEXT("InString is a string pin"),
					Pin->GetStringField(TEXT("category")),
					FString(TEXT("string")));
			}
		}
	}

	TestTrue(TEXT("catalog surfaced the Print String node"), bFoundPrintString);
	TestTrue(TEXT("catalog surfaced the InString pin name"), bFoundInStringPin);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintNodeCatalogFilterTest,
	"UeremcpBlueprint.Toolset.NodeCatalogFilters",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintNodeCatalogFilterTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpNodeCatalogTest;

	FScratchGuard Guard;
	static const FString AssetName = TEXT("BP_NodeCatalog_Filter");
	UBlueprint* Blueprint = CreateScratchBlueprint(AssetName, *this);
	if (!Blueprint)
	{
		return false;
	}

	const FString AssetPath = MakePackagePath(AssetName) + TEXT(".") + AssetName;

	// node_classes narrows to exactly one class.
	{
		const FString ResponseJson = UUeremcpBlueprintToolset::DescribeNodeCatalog(
			MakeRequest(AssetPath, TEXT(R"({"node_classes":["K2Node_CallFunction"],"include_pins":false,"max_results":40})")));
		TSharedPtr<FJsonObject> Catalog = GetCatalog(ParseResponse(ResponseJson, *this), *this);
		if (!Catalog.IsValid())
		{
			return false;
		}

		const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
		if (!TestTrue(TEXT("filtered catalog has entries"), Catalog->TryGetArrayField(TEXT("entries"), Entries) && Entries->Num() > 0))
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& EntryValue : *Entries)
		{
			const TSharedPtr<FJsonObject> Entry = EntryValue->AsObject();
			if (!Entry.IsValid())
			{
				continue;
			}
			TestEqual(
				TEXT("node_classes filter is respected"),
				Entry->GetStringField(TEXT("node_class_name")),
				FString(TEXT("K2Node_CallFunction")));

			// include_pins=false must not silently claim resolved pins.
			const TArray<TSharedPtr<FJsonValue>>* Pins = nullptr;
			Entry->TryGetArrayField(TEXT("pins"), Pins);
			TestTrue(TEXT("include_pins=false returns no pins"), Pins == nullptr || Pins->Num() == 0);
			TestFalse(TEXT("include_pins=false reports pins_resolved false"), Entry->GetBoolField(TEXT("pins_resolved")));
		}
	}

	// A search that cannot match reports zero entries rather than failing.
	{
		const FString ResponseJson = UUeremcpBlueprintToolset::DescribeNodeCatalog(
			MakeRequest(AssetPath, TEXT(R"({"search":"zzzz_no_such_node_zzzz"})")));
		TSharedPtr<FJsonObject> Catalog = GetCatalog(ParseResponse(ResponseJson, *this), *this);
		if (!Catalog.IsValid())
		{
			return false;
		}
		TestEqual(TEXT("unmatched search returns zero matches"), Catalog->GetIntegerField(TEXT("total_matched")), 0);
		TestTrue(TEXT("unmatched search still scanned the database"), Catalog->GetIntegerField(TEXT("total_scanned")) > 0);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintNodeCatalogRejectionTest,
	"UeremcpBlueprint.Toolset.NodeCatalogRejections",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintNodeCatalogRejectionTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpNodeCatalogTest;

	// Wrong action.
	{
		TSharedPtr<FJsonObject> Response = ParseResponse(
			UUeremcpBlueprintToolset::DescribeNodeCatalog(
				TEXT(R"({"protocol_version":"1.0","action":"read_graph","target":{"asset_path":"/Game/Nope"},"specification":{}})")),
			*this);
		if (Response.IsValid())
		{
			TestEqual(
				TEXT("wrong action is rejected"),
				Response->GetStringField(TEXT("status")),
				FString(TEXT("rejected")));
		}
	}

	// Missing asset.
	{
		TSharedPtr<FJsonObject> Response = ParseResponse(
			UUeremcpBlueprintToolset::DescribeNodeCatalog(
				TEXT(R"({"protocol_version":"1.0","action":"describe_node_catalog","target":{"asset_path":"/Game/__UeremcpTests/DoesNotExist.DoesNotExist"},"specification":{}})")),
			*this);
		if (Response.IsValid())
		{
			TestEqual(
				TEXT("missing asset is rejected"),
				Response->GetStringField(TEXT("status")),
				FString(TEXT("rejected")));
		}
	}

	// Malformed envelope.
	{
		TSharedPtr<FJsonObject> Response = ParseResponse(
			UUeremcpBlueprintToolset::DescribeNodeCatalog(TEXT("{not json")),
			*this);
		if (Response.IsValid())
		{
			TestEqual(
				TEXT("malformed envelope is rejected"),
				Response->GetStringField(TEXT("status")),
				FString(TEXT("rejected")));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
