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
	static const TCHAR* TestsRoot = TEXT("/Game/__UeremcpPoc");
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

	static FString SerializeObject(const TSharedPtr<FJsonObject>& Object)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
		return Json;
	}

	static FString MakeSubmitReplaceRequest(
		const FString& RequestId,
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& Graph,
		const FString& ExpectedRevision,
		bool bDryRun = false,
		const TSharedPtr<FJsonObject>& ExpectedAfterWrite = nullptr)
	{
		TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetStringField(TEXT("protocol_version"), TEXT("1.0"));
		Request->SetStringField(TEXT("request_id"), RequestId);
		Request->SetStringField(TEXT("action"), TEXT("submit_graph"));
		Request->SetStringField(TEXT("mode"), TEXT("replace"));
		if (!ExpectedRevision.IsEmpty())
		{
			Request->SetStringField(TEXT("expected_revision"), ExpectedRevision);
		}

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph_id"), TEXT("EventGraph"));
		Request->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetBoolField(TEXT("dry_run"), bDryRun);
		Request->SetObjectField(TEXT("options"), Options);

		TSharedPtr<FJsonObject> Specification = MakeShared<FJsonObject>();
		Specification->SetObjectField(TEXT("graph"), Graph);
		if (ExpectedAfterWrite.IsValid())
		{
			Specification->SetObjectField(TEXT("expected_after_write"), ExpectedAfterWrite);
		}
		Request->SetObjectField(TEXT("specification"), Specification);
		return SerializeObject(Request);
	}

	static TSharedPtr<FJsonObject> MakeBranchExpectedAfterWrite()
	{
		TSharedPtr<FJsonObject> Expected = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Nodes;
		for (const TPair<FString, FString>& Pair : {
				TPair<FString, FString>(TEXT("begin_play"), TEXT("K2Node_Event")),
				TPair<FString, FString>(TEXT("branch"), TEXT("K2Node_IfThenElse")),
				TPair<FString, FString>(TEXT("print"), TEXT("K2Node_CallFunction"))})
		{
			TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
			Node->SetStringField(TEXT("key"), Pair.Key);
			Node->SetStringField(TEXT("node_class"), Pair.Value);
			Nodes.Add(MakeShared<FJsonValueObject>(Node));
		}
		Expected->SetArrayField(TEXT("nodes"), Nodes);

		TArray<TSharedPtr<FJsonValue>> Links;
		auto AddLink = [&Links](
			const TCHAR* From,
			const TCHAR* FromPin,
			const TCHAR* To,
			const TCHAR* ToPin)
		{
			TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
			Link->SetStringField(TEXT("from"), From);
			Link->SetStringField(TEXT("from_pin"), FromPin);
			Link->SetStringField(TEXT("to"), To);
			Link->SetStringField(TEXT("to_pin"), ToPin);
			Links.Add(MakeShared<FJsonValueObject>(Link));
		};
		AddLink(TEXT("begin_play"), TEXT("then"), TEXT("branch"), TEXT("execute"));
		AddLink(TEXT("branch"), TEXT("then"), TEXT("print"), TEXT("execute"));
		Expected->SetArrayField(TEXT("links"), Links);
		return Expected;
	}

	static TSharedPtr<FJsonObject> ExtractSingleGraph(
		const TSharedPtr<FJsonObject>& Root,
		FAutomationTestBase& Test)
	{
		const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
		if (!Test.TestTrue(
				TEXT("response contains one complete graph"),
				Root.IsValid()
					&& Root->TryGetObjectField(TEXT("diagnostics"), Diagnostics)
					&& Diagnostics
					&& (*Diagnostics)->TryGetArrayField(TEXT("graphs"), Graphs)
					&& Graphs
					&& Graphs->Num() == 1))
		{
			return nullptr;
		}
		return (*Graphs)[0]->AsObject();
	}

	static FString MakeSubmitPatchRequest(
		const FString& RequestId,
		const FString& AssetPath,
		const FString& ExpectedRevision)
	{
		TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetStringField(TEXT("protocol_version"), TEXT("1.0"));
		Request->SetStringField(TEXT("request_id"), RequestId);
		Request->SetStringField(TEXT("action"), TEXT("submit_graph"));
		Request->SetStringField(TEXT("mode"), TEXT("patch"));
		Request->SetStringField(TEXT("expected_revision"), ExpectedRevision);

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), AssetPath);
		Target->SetStringField(TEXT("graph_id"), TEXT("EventGraph"));
		Request->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Patch = MakeShared<FJsonObject>();
		Patch->SetStringField(TEXT("base_revision"), ExpectedRevision);
		TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>();
		Op->SetStringField(TEXT("op"), TEXT("ensure_entry"));
		Op->SetStringField(TEXT("entry_kind"), TEXT("event"));
		Op->SetStringField(TEXT("name"), TEXT("EventBeginPlay"));
		TArray<TSharedPtr<FJsonValue>> Ops;
		Ops.Add(MakeShared<FJsonValueObject>(Op));
		Patch->SetArrayField(TEXT("ops"), Ops);

		TSharedPtr<FJsonObject> Specification = MakeShared<FJsonObject>();
		Specification->SetStringField(TEXT("graph_id"), TEXT("EventGraph"));
		Specification->SetObjectField(TEXT("patch"), Patch);
		Request->SetObjectField(TEXT("specification"), Specification);
		return SerializeObject(Request);
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

	const FString SummaryRequest = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"bp-read-summary","action":"read_graph","target":{"asset_path":"%s","graph_id":"EventGraph"},"options":{"response_detail":"summary"}})"),
		*AssetPath);
	TSharedPtr<FJsonObject> SummaryRoot;
	if (ParseResponse(UUeremcpBlueprintToolset::ReadGraph(SummaryRequest), SummaryRoot, *this))
	{
		FString Summary;
		TestTrue(TEXT("summary text present"), SummaryRoot->TryGetStringField(TEXT("summary"), Summary));
		TestFalse(TEXT("summary does not claim replace is unimplemented"),
			Summary.Contains(TEXT("replace not implemented"), ESearchCase::IgnoreCase));
		FString SummaryRevision;
		TestTrue(TEXT("summary revision present"),
			SummaryRoot->TryGetStringField(TEXT("revision"), SummaryRevision));
		TestEqual(TEXT("summary and complete revisions match"), SummaryRevision, Revision);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintSubmitGraphValidationTest,
	"UeremcpBlueprint.Toolset.SubmitGraphValidation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintSubmitGraphValidationTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpBlueprintReadGraphTest;

	FScratchGuard Guard;
	static const FString AssetName = TEXT("BP_SubmitGraph_Scratch");
	UBlueprint* Blueprint = CreateScratchBlueprint(AssetName, *this);
	if (!Blueprint)
	{
		return false;
	}
	const FString AssetPath = MakePackagePath(AssetName) + TEXT(".") + AssetName;
	const FString ReadRequest = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"bp-submit-read","action":"read_graph","target":{"asset_path":"%s","graph_id":"EventGraph"},"options":{"response_detail":"complete"}})"),
		*AssetPath);

	TSharedPtr<FJsonObject> Root;
	if (!ParseResponse(UUeremcpBlueprintToolset::ReadGraph(ReadRequest), Root, *this))
	{
		return false;
	}

	FString Revision;
	TestTrue(TEXT("read revision"), Root->TryGetStringField(TEXT("revision"), Revision));
	const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
	if (!TestTrue(TEXT("read graph available"),
			Root->TryGetObjectField(TEXT("diagnostics"), Diagnostics)
			&& Diagnostics
			&& (*Diagnostics)->TryGetArrayField(TEXT("graphs"), Graphs)
			&& Graphs
			&& Graphs->Num() == 1))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> Graph = (*Graphs)[0]->AsObject();

	TSharedPtr<FJsonObject> NoOpRoot;
	const FString NoOpRequest =
		MakeSubmitReplaceRequest(TEXT("bp-submit-noop"), AssetPath, Graph, Revision);
	if (!ParseResponse(UUeremcpBlueprintToolset::SubmitGraph(NoOpRequest), NoOpRoot, *this))
	{
		return false;
	}
	FString Status;
	NoOpRoot->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("unchanged replace is no-op"), Status, FString(TEXT("no_change_required")));
	FString NoOpRevision;
	NoOpRoot->TryGetStringField(TEXT("revision"), NoOpRevision);
	TestEqual(TEXT("no-op revision unchanged"), NoOpRevision, Revision);
	const TSharedPtr<FJsonObject>* NoOpValidation = nullptr;
	TestTrue(TEXT("no-op validation present"),
		NoOpRoot->TryGetObjectField(TEXT("validation"), NoOpValidation)
		&& NoOpValidation
		&& NoOpValidation->IsValid());
	bool bNoOpReread = true;
	if (NoOpValidation && NoOpValidation->IsValid())
	{
		TestTrue(TEXT("no-op reread evidence present"),
			(*NoOpValidation)->TryGetBoolField(TEXT("reread_after_write"), bNoOpReread));
		TestFalse(TEXT("no-op performs no post-write reread"), bNoOpReread);
	}

	TSharedPtr<FJsonObject> RepeatedNoOpRoot;
	const FString RepeatedNoOpRequest =
		MakeSubmitReplaceRequest(TEXT("bp-submit-noop-repeat"), AssetPath, Graph, Revision);
	if (!ParseResponse(UUeremcpBlueprintToolset::SubmitGraph(RepeatedNoOpRequest), RepeatedNoOpRoot, *this))
	{
		return false;
	}
	RepeatedNoOpRoot->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("repeated identical replace remains idempotent"),
		Status,
		FString(TEXT("no_change_required")));
	const TSharedPtr<FJsonObject>* RepeatedMetrics = nullptr;
	TestTrue(TEXT("repeat metrics present"),
		RepeatedNoOpRoot->TryGetObjectField(TEXT("metrics"), RepeatedMetrics)
		&& RepeatedMetrics
		&& RepeatedMetrics->IsValid());
	double RepeatInternalOperations = -1.0;
	if (RepeatedMetrics && RepeatedMetrics->IsValid())
	{
		TestTrue(TEXT("repeat internal_operations present"),
			(*RepeatedMetrics)->TryGetNumberField(TEXT("internal_operations"), RepeatInternalOperations));
		TestTrue(TEXT("repeat performs read-only comparison"),
			RepeatInternalOperations >= 1.0);
	}

	TSharedPtr<FJsonObject> StaleRoot;
	const FString StaleRequest = MakeSubmitReplaceRequest(
		TEXT("bp-submit-stale"),
		AssetPath,
		Graph,
		TEXT("sha256:0000000000000000000000000000000000000000000000000000000000000000"));
	if (!ParseResponse(UUeremcpBlueprintToolset::SubmitGraph(StaleRequest), StaleRoot, *this))
	{
		return false;
	}
	StaleRoot->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("stale replace rejected"), Status, FString(TEXT("rejected")));
	FString CurrentRevision;
	StaleRoot->TryGetStringField(TEXT("revision"), CurrentRevision);
	TestEqual(TEXT("conflict returns current revision"), CurrentRevision, Revision);
	const TSharedPtr<FJsonObject>* StaleValidation = nullptr;
	bool bStaleReread = true;
	TestTrue(TEXT("stale conflict validation present"),
		StaleRoot->TryGetObjectField(TEXT("validation"), StaleValidation)
		&& StaleValidation
		&& StaleValidation->IsValid());
	if (StaleValidation && StaleValidation->IsValid())
	{
		TestTrue(TEXT("stale reread evidence present"),
			(*StaleValidation)->TryGetBoolField(TEXT("reread_after_write"), bStaleReread));
		TestFalse(TEXT("stale conflict performs no post-write reread"), bStaleReread);
	}

	TSharedPtr<FJsonObject> PatchRoot;
	const FString PatchRequest = MakeSubmitPatchRequest(
		TEXT("bp-submit-patch-rejected"),
		AssetPath,
		Revision);
	if (!ParseResponse(UUeremcpBlueprintToolset::SubmitGraph(PatchRequest), PatchRoot, *this))
	{
		return false;
	}
	PatchRoot->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("undefined patch contract rejected"), Status, FString(TEXT("rejected")));
	const TSharedPtr<FJsonObject>* PatchDiagnostics = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* PatchItems = nullptr;
	TestTrue(TEXT("patch diagnostic items present"),
		PatchRoot->TryGetObjectField(TEXT("diagnostics"), PatchDiagnostics)
		&& PatchDiagnostics
		&& (*PatchDiagnostics)->TryGetArrayField(TEXT("items"), PatchItems)
		&& PatchItems
		&& PatchItems->Num() == 1);
	if (PatchItems && PatchItems->Num() == 1)
	{
		FString DiagnosticCode;
		TestTrue(TEXT("patch diagnostic code present"),
			(*PatchItems)[0]->AsObject()->TryGetStringField(TEXT("code"), DiagnosticCode));
		TestEqual(TEXT("patch diagnostic code stable"),
			DiagnosticCode,
			FString(TEXT("blueprint.patch_contract_undefined")));
	}
	const TSharedPtr<FJsonObject>* PatchMetrics = nullptr;
	double PatchInternalOperations = -1.0;
	TestTrue(TEXT("patch metrics present"),
		PatchRoot->TryGetObjectField(TEXT("metrics"), PatchMetrics)
		&& PatchMetrics
		&& PatchMetrics->IsValid());
	if (PatchMetrics && PatchMetrics->IsValid())
	{
		TestTrue(TEXT("patch internal_operations present"),
			(*PatchMetrics)->TryGetNumberField(TEXT("internal_operations"), PatchInternalOperations));
		TestEqual(TEXT("patch rejected before editor operations"), PatchInternalOperations, 0.0);
	}
	const TSharedPtr<FJsonObject>* PatchValidation = nullptr;
	bool bPatchReread = true;
	TestTrue(TEXT("patch validation present"),
		PatchRoot->TryGetObjectField(TEXT("validation"), PatchValidation)
		&& PatchValidation
		&& PatchValidation->IsValid());
	if (PatchValidation && PatchValidation->IsValid())
	{
		TestTrue(TEXT("patch reread evidence present"),
			(*PatchValidation)->TryGetBoolField(TEXT("reread_after_write"), bPatchReread));
		TestFalse(TEXT("patch rejection performs no post-write reread"), bPatchReread);
	}

	TSharedPtr<FJsonObject> ChangedGraph = MakeShared<FJsonObject>();
	ChangedGraph->Values = Graph->Values;
	TSharedPtr<FJsonObject> Extensions = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> BlueprintExt = MakeShared<FJsonObject>();
	BlueprintExt->SetStringField(
		TEXT("dsl"),
		TEXT("(event EventBeginPlay\n  (Development|PrintString :InString \"dry_run changed\"))"));
	Extensions->SetObjectField(TEXT("blueprint"), BlueprintExt);
	ChangedGraph->SetObjectField(TEXT("extensions"), Extensions);
	ChangedGraph->SetStringField(
		TEXT("content_hash"),
		TEXT("sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));
	ChangedGraph->SetStringField(
		TEXT("revision"),
		TEXT("sha256:bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"));

	TSharedPtr<FJsonObject> DryRunRoot;
	const FString DryRunRequest = MakeSubmitReplaceRequest(
		TEXT("bp-submit-dry-run"),
		AssetPath,
		ChangedGraph,
		Revision,
		true);
	if (!ParseResponse(UUeremcpBlueprintToolset::SubmitGraph(DryRunRequest), DryRunRoot, *this))
	{
		return false;
	}
	DryRunRoot->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("changed replace dry_run is partial"), Status, FString(TEXT("partially_completed")));
	FString DryRunRevision;
	DryRunRoot->TryGetStringField(TEXT("revision"), DryRunRevision);
	TestEqual(TEXT("dry_run leaves revision unchanged"), DryRunRevision, Revision);
	const TSharedPtr<FJsonObject>* DryRunValidation = nullptr;
	bool bDryRunReread = true;
	TestTrue(TEXT("dry_run validation present"),
		DryRunRoot->TryGetObjectField(TEXT("validation"), DryRunValidation)
		&& DryRunValidation
		&& DryRunValidation->IsValid());
	if (DryRunValidation && DryRunValidation->IsValid())
	{
		TestTrue(TEXT("dry_run reread evidence present"),
			(*DryRunValidation)->TryGetBoolField(TEXT("reread_after_write"), bDryRunReread));
		TestFalse(TEXT("dry_run performs no post-write reread"), bDryRunReread);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintPocA6RereadTest,
	"UeremcpBlueprint.Toolset.PocA6Reread",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintPocA6RereadTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpBlueprintReadGraphTest;

	FScratchGuard Guard;
	static const FString AssetName = TEXT("BP_PocA6_Scratch");
	if (!CreateScratchBlueprint(AssetName, *this))
	{
		return false;
	}
	const FString AssetPath = MakePackagePath(AssetName) + TEXT(".") + AssetName;
	const FString ReadRequest = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"poc-a6-read-before","action":"read_graph","target":{"asset_path":"%s","graph_id":"EventGraph"},"options":{"response_detail":"complete"}})"),
		*AssetPath);

	TSharedPtr<FJsonObject> BeforeRoot;
	if (!ParseResponse(UUeremcpBlueprintToolset::ReadGraph(ReadRequest), BeforeRoot, *this))
	{
		return false;
	}
	FString BeforeRevision;
	TestTrue(TEXT("A1 initial read revision"), BeforeRoot->TryGetStringField(TEXT("revision"), BeforeRevision));
	const TSharedPtr<FJsonObject> BeforeGraph = ExtractSingleGraph(BeforeRoot, *this);
	if (!BeforeGraph.IsValid())
	{
		return false;
	}
	const TSharedPtr<FJsonObject>* ReadMetrics = nullptr;
	double ReadRoundTrips = 0.0;
	TestTrue(
		TEXT("A1 read reports one MCP round trip"),
		BeforeRoot->TryGetObjectField(TEXT("metrics"), ReadMetrics)
			&& ReadMetrics
			&& (*ReadMetrics)->TryGetNumberField(TEXT("mcp_round_trips"), ReadRoundTrips)
			&& ReadRoundTrips == 1.0);
	const TArray<TSharedPtr<FJsonValue>>* BeforeNodes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* BeforeLinks = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* BeforeVariables = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* BeforeEntryPoints = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* BeforeDependencies = nullptr;
	TestTrue(TEXT("A2 nodes included"), BeforeGraph->TryGetArrayField(TEXT("nodes"), BeforeNodes) && BeforeNodes);
	TestTrue(TEXT("A2 links included"), BeforeGraph->TryGetArrayField(TEXT("links"), BeforeLinks) && BeforeLinks);
	TestTrue(
		TEXT("A2 variables included even when empty"),
		BeforeGraph->TryGetArrayField(TEXT("variables"), BeforeVariables) && BeforeVariables);
	TestTrue(
		TEXT("A2 entry points included"),
		BeforeGraph->TryGetArrayField(TEXT("entry_points"), BeforeEntryPoints) && BeforeEntryPoints);
	TestTrue(
		TEXT("A2 dependencies included even when empty"),
		BeforeGraph->TryGetArrayField(TEXT("dependencies"), BeforeDependencies) && BeforeDependencies);
	bool bSawPinType = false;
	bool bSawPinDefault = false;
	if (BeforeNodes)
	{
		for (const TSharedPtr<FJsonValue>& NodeValue : *BeforeNodes)
		{
			const TSharedPtr<FJsonObject> Node = NodeValue->AsObject();
			if (!Node.IsValid())
			{
				continue;
			}
			bSawPinDefault |= Node->HasTypedField<EJson::Object>(TEXT("defaults"));
			for (const TCHAR* PinField : {TEXT("input_pins"), TEXT("output_pins")})
			{
				const TArray<TSharedPtr<FJsonValue>>* Pins = nullptr;
				if (Node->TryGetArrayField(PinField, Pins) && Pins)
				{
					for (const TSharedPtr<FJsonValue>& PinValue : *Pins)
					{
						const TSharedPtr<FJsonObject> Pin = PinValue->AsObject();
						bSawPinType |= Pin.IsValid() && Pin->HasTypedField<EJson::Object>(TEXT("pin_type"));
					}
				}
			}
		}
	}
	TestTrue(TEXT("A2 pin types included"), bSawPinType);
	TestTrue(TEXT("A2 pin defaults included"), bSawPinDefault);

	// A4: externally modify the complete JSON's write intent to insert Branch -> PrintString.
	TSharedPtr<FJsonObject> ChangedGraph = MakeShared<FJsonObject>();
	ChangedGraph->Values = BeforeGraph->Values;
	TSharedPtr<FJsonObject> Extensions = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> BlueprintExt = MakeShared<FJsonObject>();
	BlueprintExt->SetStringField(
		TEXT("dsl"),
		TEXT("(event EventBeginPlay\n  (if true\n    (Development|PrintString :InString \"A6 branch confirmed\")))"));
	Extensions->SetObjectField(TEXT("blueprint"), BlueprintExt);
	ChangedGraph->SetObjectField(TEXT("extensions"), Extensions);

	TSharedPtr<FJsonObject> SubmitRoot;
	const FString SubmitRequest = MakeSubmitReplaceRequest(
		TEXT("poc-a6-submit"),
		AssetPath,
		ChangedGraph,
		BeforeRevision,
		false,
		MakeBranchExpectedAfterWrite());
	if (!ParseResponse(UUeremcpBlueprintToolset::SubmitGraph(SubmitRequest), SubmitRoot, *this))
	{
		return false;
	}

	FString Status;
	SubmitRoot->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("A7 status modified_and_validated"), Status, FString(TEXT("modified_and_validated")));
	const TSharedPtr<FJsonObject>* Validation = nullptr;
	bool bRereadAfterWrite = false;
	TestTrue(
		TEXT("A7 validation object"),
		SubmitRoot->TryGetObjectField(TEXT("validation"), Validation) && Validation && Validation->IsValid());
	if (Validation && Validation->IsValid())
	{
		bool bCompiled = false;
		bool bSaved = false;
		TestTrue(
			TEXT("A7 reread_after_write true"),
			(*Validation)->TryGetBoolField(TEXT("reread_after_write"), bRereadAfterWrite)
				&& bRereadAfterWrite);
		TestTrue(
			TEXT("A5 compile succeeded"),
			(*Validation)->TryGetBoolField(TEXT("compiled"), bCompiled) && bCompiled);
		TestTrue(
			TEXT("A5 save succeeded"),
			(*Validation)->TryGetBoolField(TEXT("saved"), bSaved) && bSaved);
	}
	const TSharedPtr<FJsonObject>* SubmitMetrics = nullptr;
	double SubmitRoundTrips = 0.0;
	TestTrue(
		TEXT("A5 submit reports one MCP round trip"),
		SubmitRoot->TryGetObjectField(TEXT("metrics"), SubmitMetrics)
			&& SubmitMetrics
			&& (*SubmitMetrics)->TryGetNumberField(TEXT("mcp_round_trips"), SubmitRoundTrips)
			&& SubmitRoundTrips == 1.0);
	const TSharedPtr<FJsonObject> SubmitRereadGraph = ExtractSingleGraph(SubmitRoot, *this);
	if (!SubmitRereadGraph.IsValid())
	{
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* SubmitNodes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* SubmitLinks = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* SubmitVariables = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* SubmitDependencies = nullptr;
	TestTrue(
		TEXT("A5 submit returns complete reread nodes"),
		SubmitRereadGraph->TryGetArrayField(TEXT("nodes"), SubmitNodes) && SubmitNodes);
	TestTrue(
		TEXT("A5 submit returns complete reread links"),
		SubmitRereadGraph->TryGetArrayField(TEXT("links"), SubmitLinks) && SubmitLinks);
	TestTrue(
		TEXT("A5 submit returns variables"),
		SubmitRereadGraph->TryGetArrayField(TEXT("variables"), SubmitVariables) && SubmitVariables);
	TestTrue(
		TEXT("A5 submit returns dependencies"),
		SubmitRereadGraph->TryGetArrayField(TEXT("dependencies"), SubmitDependencies) && SubmitDependencies);
	const TSharedPtr<FJsonObject>* SubmitFidelity = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* LossyAreas = nullptr;
	TestTrue(
		TEXT("A10 submit returns fidelity"),
		SubmitRereadGraph->TryGetObjectField(TEXT("fidelity"), SubmitFidelity)
			&& SubmitFidelity
			&& (*SubmitFidelity)->TryGetArrayField(TEXT("lossy_areas"), LossyAreas)
			&& LossyAreas);

	const FString AfterReadRequest = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"poc-a6-read-after","action":"read_graph","target":{"asset_path":"%s","graph_id":"EventGraph"},"options":{"response_detail":"complete"}})"),
		*AssetPath);
	TSharedPtr<FJsonObject> AfterRoot;
	if (!ParseResponse(UUeremcpBlueprintToolset::ReadGraph(AfterReadRequest), AfterRoot, *this))
	{
		return false;
	}
	const TSharedPtr<FJsonObject> AfterGraph = ExtractSingleGraph(AfterRoot, *this);
	if (!AfterGraph.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
	TestTrue(TEXT("A6 reread nodes"), AfterGraph->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes);
	TestTrue(TEXT("A6 reread links"), AfterGraph->TryGetArrayField(TEXT("links"), Links) && Links);
	TMap<FString, FString> NodeClassById;
	TSet<FString> NodeClasses;
	if (Nodes)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Nodes)
		{
			const TSharedPtr<FJsonObject> Node = Value->AsObject();
			FString NodeClass;
			FString NodeId;
			if (Node.IsValid()
				&& Node->TryGetStringField(TEXT("node_class"), NodeClass)
				&& Node->TryGetStringField(TEXT("node_id"), NodeId))
			{
				NodeClassById.Add(NodeId, NodeClass);
				NodeClasses.Add(NodeClass);
			}
		}
	}
	TestTrue(TEXT("A6 BeginPlay node exists"), NodeClasses.Contains(TEXT("K2Node_Event")));
	TestTrue(TEXT("A6 Branch node exists"), NodeClasses.Contains(TEXT("K2Node_IfThenElse")));
	TestTrue(TEXT("A6 function-call node exists"), NodeClasses.Contains(TEXT("K2Node_CallFunction")));

	auto HasLink = [&NodeClassById, Links](
		const TCHAR* FromClass,
		const TCHAR* FromPin,
		const TCHAR* ToClass,
		const TCHAR* ToPin) -> bool
	{
		if (!Links)
		{
			return false;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Links)
		{
			const TSharedPtr<FJsonObject> Link = Value->AsObject();
			FString FromNode;
			FString ActualFromPin;
			FString ToNode;
			FString ActualToPin;
			if (Link.IsValid()
				&& Link->TryGetStringField(TEXT("from_node"), FromNode)
				&& Link->TryGetStringField(TEXT("from_pin"), ActualFromPin)
				&& Link->TryGetStringField(TEXT("to_node"), ToNode)
				&& Link->TryGetStringField(TEXT("to_pin"), ActualToPin)
				&& NodeClassById.FindRef(FromNode) == FromClass
				&& ActualFromPin == FromPin
				&& NodeClassById.FindRef(ToNode) == ToClass
				&& ActualToPin == ToPin)
			{
				return true;
			}
		}
		return false;
	};
	TestTrue(
		TEXT("A6 BeginPlay.then -> Branch.execute"),
		HasLink(TEXT("K2Node_Event"), TEXT("then"), TEXT("K2Node_IfThenElse"), TEXT("execute")));
	TestTrue(
		TEXT("A6 Branch.then -> PrintString.execute"),
		HasLink(TEXT("K2Node_IfThenElse"), TEXT("then"), TEXT("K2Node_CallFunction"), TEXT("execute")));

	FString AfterHash;
	TestTrue(TEXT("A8 first reread hash"), AfterGraph->TryGetStringField(TEXT("content_hash"), AfterHash));
	TSharedPtr<FJsonObject> NoOpRoot;
	const FString NoOpRequest =
		MakeSubmitReplaceRequest(TEXT("poc-a8-noop"), AssetPath, AfterGraph, AfterHash);
	if (!ParseResponse(UUeremcpBlueprintToolset::SubmitGraph(NoOpRequest), NoOpRoot, *this))
	{
		return false;
	}
	NoOpRoot->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("A8 unchanged replace is no-op"), Status, FString(TEXT("no_change_required")));
	const TSharedPtr<FJsonObject> NoOpGraph = ExtractSingleGraph(NoOpRoot, *this);
	if (!NoOpGraph.IsValid())
	{
		return false;
	}
	FString NoOpHash;
	TestTrue(TEXT("A8 no-op response includes content hash"), NoOpGraph->TryGetStringField(TEXT("content_hash"), NoOpHash));
	TestEqual(TEXT("A8 no-op response hash identity"), NoOpHash, AfterHash);
	const TSharedPtr<FJsonObject>* NoOpFidelity = nullptr;
	TestTrue(
		TEXT("A10 no-op response includes fidelity"),
		NoOpGraph->TryGetObjectField(TEXT("fidelity"), NoOpFidelity)
			&& NoOpFidelity
			&& NoOpFidelity->IsValid());

	TSharedPtr<FJsonObject> IdentityRoot;
	if (ParseResponse(UUeremcpBlueprintToolset::ReadGraph(AfterReadRequest), IdentityRoot, *this))
	{
		FString IdentityHash;
		IdentityRoot->TryGetStringField(TEXT("revision"), IdentityHash);
		TestEqual(TEXT("A8 unchanged replace hash identity"), IdentityHash, AfterHash);
	}

	TSharedPtr<FJsonObject> RepeatedNoOpRoot;
	const FString RepeatedNoOpRequest =
		MakeSubmitReplaceRequest(TEXT("poc-a11-repeat"), AssetPath, AfterGraph, AfterHash);
	if (!ParseResponse(
			UUeremcpBlueprintToolset::SubmitGraph(RepeatedNoOpRequest),
			RepeatedNoOpRoot,
			*this))
	{
		return false;
	}
	RepeatedNoOpRoot->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("A11 second identical replace is no-op"), Status, FString(TEXT("no_change_required")));
	const TSharedPtr<FJsonObject>* NoOpValidation = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* SkippedChecks = nullptr;
	TestTrue(
		TEXT("A11 no-op validation skips compile"),
		RepeatedNoOpRoot->TryGetObjectField(TEXT("validation"), NoOpValidation)
			&& NoOpValidation
			&& (*NoOpValidation)->TryGetArrayField(TEXT("checks_skipped"), SkippedChecks)
			&& SkippedChecks
			&& SkippedChecks->ContainsByPredicate([](const TSharedPtr<FJsonValue>& Value)
			{
				return Value.IsValid() && Value->AsString() == TEXT("blueprint.compile");
			}));
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
