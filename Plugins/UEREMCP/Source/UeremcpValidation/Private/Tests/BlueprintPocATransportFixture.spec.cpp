// WS-11 out-of-band fixture lifecycle for the transport-level POC A proof.
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpValidationPocATransportFixture
{
	static constexpr const TCHAR* PackagePath =
		TEXT("/Game/__UeremcpPoc/Blueprint/BP_CompleteRoundTripTransport");
	static constexpr const TCHAR* ObjectPath =
		TEXT("/Game/__UeremcpPoc/Blueprint/BP_CompleteRoundTripTransport."
			"BP_CompleteRoundTripTransport");

	static UEdGraphNode* SpawnNode(
		UK2Node* Template,
		UEdGraph* Graph,
		const FVector2f& Position)
	{
		TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action =
			MakeShared<FEdGraphSchemaAction_K2NewNode>();
		Action->NodeTemplate = Template;
		return Action->PerformAction(Graph, nullptr, Position, false);
	}

	static void DeleteFixture()
	{
		if (UEditorAssetLibrary::DoesAssetExist(ObjectPath))
		{
			UEditorAssetLibrary::DeleteAsset(ObjectPath);
		}
	}

	static bool CreateFixture(FAutomationTestBase& Test)
	{
		DeleteFixture();

		UPackage* Package = CreatePackage(PackagePath);
		if (!Test.TestNotNull(TEXT("Create POC A fixture package"), Package))
		{
			return false;
		}
		Package->FullyLoad();

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			TEXT("BP_CompleteRoundTripTransport"),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			NAME_None);
		if (!Test.TestNotNull(TEXT("Create POC A fixture Blueprint"), Blueprint))
		{
			return false;
		}

		UEdGraph* Graph = FBlueprintEditorUtils::FindEventGraph(Blueprint);
		if (!Test.TestNotNull(TEXT("Find fixture EventGraph"), Graph))
		{
			return false;
		}

		UEdGraph* Templates = NewObject<UEdGraph>(Blueprint);
		Templates->SetFlags(RF_Transient);

		UK2Node_Event* BeginPlayTemplate = NewObject<UK2Node_Event>(Templates);
		BeginPlayTemplate->EventReference.SetExternalMember(
			TEXT("ReceiveBeginPlay"),
			AActor::StaticClass());
		BeginPlayTemplate->bOverrideFunction = true;
		UEdGraphNode* BeginPlay = SpawnNode(
			BeginPlayTemplate,
			Graph,
			FVector2f(0.0f, 0.0f));

		UK2Node_CallFunction* PrintTemplate =
			NewObject<UK2Node_CallFunction>(Templates);
		PrintTemplate->FunctionReference.SetFromField<UFunction>(
			FindFieldChecked<UFunction>(
				UKismetSystemLibrary::StaticClass(),
				TEXT("PrintString")),
			false);
		UEdGraphNode* Print = SpawnNode(
			PrintTemplate,
			Graph,
			FVector2f(300.0f, 0.0f));
		if (BeginPlay && Print)
		{
			UEdGraphPin* Then = BeginPlay->FindPin(UEdGraphSchema_K2::PN_Then);
			UEdGraphPin* Execute = Print->FindPin(UEdGraphSchema_K2::PN_Execute);
			if (Then && Execute)
			{
				Then->MakeLinkTo(Execute);
			}
		}

		// Deliberate dead node retained for A3 continuity with the domain proof.
		UK2Node_CallFunction* DeadPrint =
			NewObject<UK2Node_CallFunction>(Templates);
		DeadPrint->FunctionReference.SetFromField<UFunction>(
			FindFieldChecked<UFunction>(
				UKismetSystemLibrary::StaticClass(),
				TEXT("PrintString")),
			false);
		SpawnNode(DeadPrint, Graph, FVector2f(300.0f, 200.0f));

		// Deliberate disconnected subgraph for aggregate A3 transport evidence.
		UK2Node_CustomEvent* OrphanEventTemplate =
			NewObject<UK2Node_CustomEvent>(Templates);
		OrphanEventTemplate->CustomFunctionName =
			TEXT("UeremcpTransportOrphanEvent");
		UEdGraphNode* OrphanEvent = SpawnNode(
			OrphanEventTemplate,
			Graph,
			FVector2f(0.0f, 400.0f));
		UK2Node_CallFunction* OrphanPrintTemplate =
			NewObject<UK2Node_CallFunction>(Templates);
		OrphanPrintTemplate->FunctionReference.SetFromField<UFunction>(
			FindFieldChecked<UFunction>(
				UKismetSystemLibrary::StaticClass(),
				TEXT("PrintString")),
			false);
		UEdGraphNode* OrphanPrint = SpawnNode(
			OrphanPrintTemplate,
			Graph,
			FVector2f(300.0f, 400.0f));
		if (OrphanEvent && OrphanPrint)
		{
			UEdGraphPin* Then =
				OrphanEvent->FindPin(UEdGraphSchema_K2::PN_Then);
			UEdGraphPin* Execute =
				OrphanPrint->FindPin(UEdGraphSchema_K2::PN_Execute);
			if (Then && Execute)
			{
				Then->MakeLinkTo(Execute);
			}
		}

		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		const FString Filename =
			FPackageName::LongPackageNameToFilename(
				PackagePath,
				FPackageName::GetAssetPackageExtension());
		const FSavePackageResultStruct Saved =
			UPackage::Save(Package, Blueprint, *Filename, SaveArgs);
		return Test.TestTrue(
			TEXT("Save POC A transport fixture"),
			Saved.Result == ESavePackageResult::Success);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintPocATransportFixtureSetup,
	"UEREMCP.Blueprint.POCA.TransportFixture.Setup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintPocATransportFixtureSetup::RunTest(const FString& Parameters)
{
	return UeremcpValidationPocATransportFixture::CreateFixture(*this);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintPocATransportFixtureCleanup,
	"UEREMCP.Blueprint.POCA.TransportFixture.Cleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintPocATransportFixtureCleanup::RunTest(const FString& Parameters)
{
	UeremcpValidationPocATransportFixture::DeleteFixture();
	return TestFalse(
		TEXT("POC A transport fixture removed"),
		UEditorAssetLibrary::DoesAssetExist(
			UeremcpValidationPocATransportFixture::ObjectPath));
}

#endif
