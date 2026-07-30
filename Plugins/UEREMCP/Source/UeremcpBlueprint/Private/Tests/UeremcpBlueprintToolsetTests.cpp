// Editor automation tests for blueprints.read_graph (WS-06 P1).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "EdGraphSchema_K2_Actions.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ObjectTools.h"
#include "UObject/SavePackage.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpBlueprintToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpBlueprintReadGraphTest
{
	static const TCHAR* TestsRoot = TEXT("/Game/__UeremcpTests");
	static const TCHAR* SuiteName = TEXT("Blueprint_ReadGraph");

	static FString MakePackagePath(const FString& AssetName)
	{
		return FString::Printf(TEXT("%s/%s/%s"), TestsRoot, SuiteName, *AssetName);
	}

	static FString PackageToFilesystemPath(const FString& PackagePath)
	{
		FString Path = PackagePath;
		Path.RemoveFromStart(TEXT("/Game/"));
		return FPaths::ProjectContentDir() / Path + TEXT(".uasset");
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

	static UEdGraphNode* SpawnNodeFromTemplate(UK2Node* Template, UEdGraph* Graph, const FVector2f& Pos, UEdGraphPin* ConnectPin = nullptr)
	{
		TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action = MakeShared<FEdGraphSchemaAction_K2NewNode>();
		Action->NodeTemplate = Template;
		return Action->PerformAction(Graph, ConnectPin, Pos, false);
	}

	static UK2Node* MakePrintStringTemplate(UObject* Outer)
	{
		UK2Node_CallFunction* CallFn = NewObject<UK2Node_CallFunction>(Outer);
		UFunction* Fn = FindFieldChecked<UFunction>(UKismetSystemLibrary::StaticClass(), TEXT("PrintString"));
		CallFn->FunctionReference.SetFromField<UFunction>(Fn, false);
		return CallFn;
	}

	static UEdGraphNode* AddBeginPlayEvent(UBlueprint* Blueprint, UEdGraph* Graph)
	{
		UEdGraph* TempOuter = NewObject<UEdGraph>(Blueprint);
		TempOuter->SetFlags(RF_Transient);

		UK2Node_Event* EventTemplate = NewObject<UK2Node_Event>(TempOuter);
		EventTemplate->EventReference.SetExternalMember(FName(TEXT("ReceiveBeginPlay")), AActor::StaticClass());
		EventTemplate->bOverrideFunction = true;

		if (UK2Node_Event* Existing = FBlueprintEditorUtils::FindOverrideForFunction(
				Blueprint,
				EventTemplate->EventReference.GetMemberParentClass(EventTemplate->GetBlueprintClassFromNode()),
				EventTemplate->EventReference.GetMemberName()))
		{
			return Existing;
		}

		return SpawnNodeFromTemplate(EventTemplate, Graph, FVector2f(0.f, 0.f));
	}

	static void ConnectPins(UEdGraphNode* From, const FName FromPin, UEdGraphNode* To, const FName ToPin)
	{
		if (UEdGraphPin* A = From->FindPin(FromPin))
		{
			if (UEdGraphPin* B = To->FindPin(ToPin))
			{
				A->MakeLinkTo(B);
			}
		}
	}

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
		if (!Test.TestNotNull(TEXT("CreateBlueprint"), Blueprint))
		{
			return nullptr;
		}

		UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
		if (!Test.TestNotNull(TEXT("EventGraph"), Graph))
		{
			return nullptr;
		}

		Graph->Modify();
		Blueprint->Modify();

		UEdGraph* TempOuter = NewObject<UEdGraph>(Blueprint);
		TempOuter->SetFlags(RF_Transient);

		UEdGraphNode* BeginPlay = AddBeginPlayEvent(Blueprint, Graph);
		UEdGraphNode* ConnectedPrint = SpawnNodeFromTemplate(
			MakePrintStringTemplate(TempOuter),
			Graph,
			FVector2f(300.f, 0.f));
		if (BeginPlay && ConnectedPrint)
		{
			ConnectPins(BeginPlay, UEdGraphSchema_K2::PN_Then, ConnectedPrint, UEdGraphSchema_K2::PN_Execute);
		}

		// Dead node: orphan PrintString with no links.
		SpawnNodeFromTemplate(
			MakePrintStringTemplate(TempOuter),
			Graph,
			FVector2f(300.f, 200.f));

		// Disconnected subgraph: custom event -> print, not linked to BeginPlay.
		UK2Node_CustomEvent* CustomTemplate = NewObject<UK2Node_CustomEvent>(TempOuter);
		CustomTemplate->CustomFunctionName = TEXT("UeremcpOrphanEvent");
		UEdGraphNode* CustomEvent = SpawnNodeFromTemplate(CustomTemplate, Graph, FVector2f(0.f, 400.f));
		UEdGraphNode* IslandPrint = SpawnNodeFromTemplate(
			MakePrintStringTemplate(TempOuter),
			Graph,
			FVector2f(300.f, 400.f));
		if (CustomEvent && IslandPrint)
		{
			ConnectPins(CustomEvent, UEdGraphSchema_K2::PN_Then, IslandPrint, UEdGraphSchema_K2::PN_Execute);
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		const FSavePackageResultStruct SaveResult =
			UPackage::Save(Package, Blueprint, *PackageToFilesystemPath(PackagePath), SaveArgs);
		Test.TestTrue(TEXT("Save scratch Blueprint"), SaveResult.Result == ESavePackageResult::Success);

		return Blueprint;
	}

	static bool ParseResponse(const FString& Json, TSharedPtr<FJsonObject>& OutRoot, FAutomationTestBase& Test)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		const bool bOk = FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
		Test.TestTrue(TEXT("response JSON parseable"), bOk);
		return bOk;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintReadGraphRoundTripTest,
	"UeremcpBlueprint.Toolset.ReadGraphRoundTrip",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintReadGraphRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpBlueprintReadGraphTest;

	FScratchGuard Guard;
	static const FString AssetName = TEXT("BP_ReadGraph_Scratch");
	UBlueprint* Blueprint = CreateScratchBlueprint(AssetName, *this);
	if (!Blueprint)
	{
		return false;
	}

	const FString AssetPath = MakePackagePath(AssetName) + TEXT(".") + AssetName;
	const FString Request = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"bp-read-1","action":"read_graph","target":{"asset_path":"%s","graph_id":"EventGraph"},"options":{"response_detail":"complete"}})"),
		*AssetPath);

	const FString ResponseJson = UUeremcpBlueprintToolset::ReadGraph(Request);
	TSharedPtr<FJsonObject> Root;
	if (!ParseResponse(ResponseJson, Root, *this))
	{
		return false;
	}

	FString Status;
	TestTrue(TEXT("status present"), Root->TryGetStringField(TEXT("status"), Status));
	TestEqual(TEXT("read status"), Status, FString(TEXT("no_change_required")));

	FString Revision;
	TestTrue(TEXT("revision present"), Root->TryGetStringField(TEXT("revision"), Revision));
	TestTrue(TEXT("revision is content hash"), Revision.StartsWith(TEXT("sha256:")));

	const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
	TestTrue(TEXT("diagnostics envelope present"),
		Root->TryGetObjectField(TEXT("diagnostics"), Diagnostics) && Diagnostics && Diagnostics->IsValid());

	const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
	TestTrue(TEXT("diagnostics.graphs present"),
		(*Diagnostics)->TryGetArrayField(TEXT("graphs"), Graphs) && Graphs && Graphs->Num() == 1);

	const TSharedPtr<FJsonObject> Graph = (*Graphs)[0]->AsObject();
	TestTrue(TEXT("graph object"), Graph.IsValid());

	FString GraphType;
	TestTrue(TEXT("graph_type"), Graph->TryGetStringField(TEXT("graph_type"), GraphType));
	TestEqual(TEXT("graph_type value"), GraphType, FString(TEXT("BlueprintEventGraph")));

	FString ContentHash;
	TestTrue(TEXT("graph content_hash"), Graph->TryGetStringField(TEXT("content_hash"), ContentHash));
	TestEqual(TEXT("graph hash matches revision"), ContentHash, Revision);

	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	TestTrue(TEXT("nodes array"), Graph->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes && Nodes->Num() > 0);

	const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
	TestTrue(TEXT("links array"), Graph->TryGetArrayField(TEXT("links"), Links) && Links && Links->Num() > 0);

	const TSharedPtr<FJsonObject>* GraphDiagnostics = nullptr;
	TestTrue(TEXT("graph.diagnostics present"),
		Graph->TryGetObjectField(TEXT("diagnostics"), GraphDiagnostics) && GraphDiagnostics && GraphDiagnostics->IsValid());

	const TArray<TSharedPtr<FJsonValue>>* DeadNodes = nullptr;
	TestTrue(TEXT("dead_nodes present"),
		(*GraphDiagnostics)->TryGetArrayField(TEXT("dead_nodes"), DeadNodes) && DeadNodes && DeadNodes->Num() >= 1);

	const TArray<TSharedPtr<FJsonValue>>* Islands = nullptr;
	TestTrue(TEXT("disconnected_subgraphs present"),
		(*GraphDiagnostics)->TryGetArrayField(TEXT("disconnected_subgraphs"), Islands) && Islands && Islands->Num() >= 1);

	const TSharedPtr<FJsonObject>* Fidelity = nullptr;
	TestTrue(TEXT("fidelity present"),
		Graph->TryGetObjectField(TEXT("fidelity"), Fidelity) && Fidelity && Fidelity->IsValid());
	bool bRoundTrip = true;
	(*Fidelity)->TryGetBoolField(TEXT("round_trip_supported"), bRoundTrip);
	TestFalse(TEXT("round_trip_supported honest"), bRoundTrip);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintSubmitGraphStubTest,
	"UeremcpBlueprint.Toolset.SubmitGraphStub",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintSubmitGraphStubTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"bp-submit-1","action":"submit_graph","target":{"asset_path":"/Game/__UeremcpTests/None"},"mode":"replace"})");
	const FString ResponseJson = UUeremcpBlueprintToolset::SubmitGraph(Request);

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);
	TestTrue(TEXT("parseable"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("submit_graph stub status"), Status, FString(TEXT("partially_completed")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintToolsetPingTest,
	"UeremcpBlueprint.Toolset.Ping",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintToolsetPingTest::RunTest(const FString& Parameters)
{
	const FString Json = UUeremcpBlueprintToolset::Ping();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("Ping returns parseable JSON object"),
		FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	TestTrue(TEXT("status present"), Root->TryGetStringField(TEXT("status"), Status));
	TestEqual(TEXT("status is no_change_required"), Status, FString(TEXT("no_change_required")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintToolsetRegisterTest,
	"UeremcpBlueprint.Toolset.Register",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintToolsetRegisterTest::RunTest(const FString& Parameters)
{
	if (!UToolsetRegistry::IsToolsetClassRegistered(UUeremcpBlueprintToolset::StaticClass()))
	{
		UToolsetRegistry::RegisterToolsetClass(UUeremcpBlueprintToolset::StaticClass());
	}

	TestTrue(TEXT("toolset class registered"),
		UToolsetRegistry::IsToolsetClassRegistered(UUeremcpBlueprintToolset::StaticClass()));

	const FString SchemaJson =
		UToolsetRegistry::GetToolsetJsonSchema(UUeremcpBlueprintToolset::StaticClass());
	TestFalse(TEXT("schema non-empty"), SchemaJson.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
