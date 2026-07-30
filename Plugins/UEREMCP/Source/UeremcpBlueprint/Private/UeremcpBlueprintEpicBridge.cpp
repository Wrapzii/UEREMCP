#include "UeremcpBlueprintEpicBridge.h"

#include "BlueprintEditorLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProcess.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ToolsetRegistry/ToolCallAsyncResultString.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

namespace
{
	static const FString EpicBlueprintToolsetName =
		TEXT("editor_toolset.toolsets.blueprint.BlueprintTools");

	static bool ParseQuotedDslArgument(
		const FString& DslCode,
		const FString& ArgumentName,
		FString& OutValue)
	{
		const FString Marker = FString::Printf(TEXT(":%s"), *ArgumentName);
		const int32 MarkerIndex = DslCode.Find(Marker, ESearchCase::CaseSensitive);
		if (MarkerIndex == INDEX_NONE)
		{
			return false;
		}

		const int32 QuoteStart = DslCode.Find(
			TEXT("\""),
			ESearchCase::CaseSensitive,
			ESearchDir::FromStart,
			MarkerIndex + Marker.Len());
		if (QuoteStart == INDEX_NONE)
		{
			return false;
		}

		bool bEscaped = false;
		for (int32 Index = QuoteStart + 1; Index < DslCode.Len(); ++Index)
		{
			const TCHAR Character = DslCode[Index];
			if (Character == TEXT('"') && !bEscaped)
			{
				OutValue = DslCode.Mid(QuoteStart + 1, Index - QuoteStart - 1);
				OutValue.ReplaceInline(TEXT("\\\""), TEXT("\""));
				OutValue.ReplaceInline(TEXT("\\\\"), TEXT("\\"));
				return true;
			}
			bEscaped = Character == TEXT('\\') && !bEscaped;
			if (Character != TEXT('\\'))
			{
				bEscaped = false;
			}
		}
		return false;
	}

	static UEdGraphNode* SpawnK2Node(
		UK2Node* Template,
		UEdGraph* Graph,
		const FVector2f& Position)
	{
		TSharedPtr<FEdGraphSchemaAction_K2NewNode> Action =
			MakeShared<FEdGraphSchemaAction_K2NewNode>();
		Action->NodeTemplate = Template;
		return Action->PerformAction(Graph, nullptr, Position, false);
	}

	struct FNativeWriteIntent
	{
		bool bHasBranch = false;
		bool bCondition = false;
		FString PrintString;
	};

	static bool ParseNativeWriteIntent(
		const FString& DslCode,
		FNativeWriteIntent& OutIntent,
		FString& OutError)
	{
		const bool bHasBeginPlay =
			DslCode.Contains(TEXT("(event EventBeginPlay"), ESearchCase::CaseSensitive);
		const bool bConditionTrue =
			DslCode.Contains(TEXT("(if true"), ESearchCase::CaseSensitive);
		const bool bConditionFalse =
			DslCode.Contains(TEXT("(if false"), ESearchCase::CaseSensitive);
		const bool bHasAnyBranch =
			DslCode.Contains(TEXT("(if "), ESearchCase::CaseSensitive);
		const bool bHasPrintString =
			DslCode.Contains(TEXT("(Development|PrintString"), ESearchCase::CaseSensitive);
		if (!bHasBeginPlay
			|| (bHasAnyBranch && bConditionTrue == bConditionFalse)
			|| !bHasPrintString
			|| !ParseQuotedDslArgument(DslCode, TEXT("InString"), OutIntent.PrintString))
		{
			OutError =
				TEXT("native Blueprint writer supports ")
				TEXT("(event EventBeginPlay (Development|PrintString :InString \"...\")) ")
				TEXT("and the same call nested in (if <bool> ...); ")
				TEXT("no Python or Epic BlueprintTools fallback is used");
			return false;
		}

		OutIntent.bHasBranch = bHasAnyBranch;
		OutIntent.bCondition = bConditionTrue;
		return true;
	}
}

bool FUeremcpBlueprintEpicBridge::ExecuteToolSync(
	const FString& ToolName,
	const FString& JsonInput,
	FString& OutJsonResult,
	FString& OutError)
{
	if (!UToolsetRegistry::IsAvailable())
	{
		OutError = TEXT("ToolsetRegistry is not available");
		return false;
	}
	UToolCallAsyncResultString* AsyncResult = UToolsetRegistry::ExecuteTool(
		EpicBlueprintToolsetName,
		ToolName,
		JsonInput);
	if (!AsyncResult)
	{
		OutError = TEXT("ExecuteTool returned null");
		return false;
	}

	const double Deadline = FPlatformTime::Seconds() + 60.0;
	while (!AsyncResult->bIsComplete && FPlatformTime::Seconds() < Deadline)
	{
		FPlatformProcess::Sleep(0.01f);
	}

	if (!AsyncResult->bIsComplete)
	{
		OutError = TEXT("Epic BlueprintTools call timed out");
		return false;
	}

	if (!AsyncResult->Error.IsEmpty())
	{
		OutError = AsyncResult->Error;
		return false;
	}

	OutJsonResult = AsyncResult->GetValueAsJsonString();
	return true;
}

UEdGraph* FUeremcpBlueprintEpicBridge::ResolveGraph(
	UBlueprint* Blueprint,
	const FString& GraphId,
	FString& OutGraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	if (GraphId.IsEmpty())
	{
		UEdGraph* EventGraph = UBlueprintEditorLibrary::FindEventGraph(Blueprint);
		if (EventGraph)
		{
			OutGraphName = EventGraph->GetName();
		}
		return EventGraph;
	}

	const TArray<UEdGraph*> AllGraphs = UBlueprintEditorLibrary::ListGraphs(Blueprint);
	for (UEdGraph* Graph : AllGraphs)
	{
		if (Graph && Graph->GetName().Equals(GraphId, ESearchCase::CaseSensitive))
		{
			OutGraphName = Graph->GetName();
			return Graph;
		}
	}

	return nullptr;
}

bool FUeremcpBlueprintEpicBridge::ValidateWriteGraphDsl(
	const FString& DslCode,
	FString& OutError)
{
	FNativeWriteIntent Intent;
	return ParseNativeWriteIntent(DslCode, Intent, OutError);
}

bool FUeremcpBlueprintEpicBridge::WriteGraphDsl(UEdGraph* Graph, const FString& DslCode, FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("graph is null");
		return false;
	}

	FNativeWriteIntent Intent;
	if (!ParseNativeWriteIntent(DslCode, Intent, OutError))
	{
		return false;
	}

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (!Blueprint)
	{
		OutError = TEXT("graph has no owning Blueprint");
		return false;
	}

	UEdGraph* Templates = NewObject<UEdGraph>(GetTransientPackage(), NAME_None, RF_Transient);
	UK2Node_Event* BeginPlayTemplate = NewObject<UK2Node_Event>(Templates);
	BeginPlayTemplate->EventReference.SetExternalMember(
		TEXT("ReceiveBeginPlay"),
		AActor::StaticClass());
	BeginPlayTemplate->bOverrideFunction = true;

	UK2Node_IfThenElse* BranchTemplate =
		Intent.bHasBranch ? NewObject<UK2Node_IfThenElse>(Templates) : nullptr;

	UFunction* PrintFunction =
		UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString"));
	if (!PrintFunction)
	{
		OutError = TEXT("UKismetSystemLibrary.PrintString was not found");
		return false;
	}
	UK2Node_CallFunction* PrintTemplate = NewObject<UK2Node_CallFunction>(Templates);
	PrintTemplate->FunctionReference.SetFromField<UFunction>(PrintFunction, false);

	Blueprint->Modify();
	Graph->Modify();
	const TArray<TObjectPtr<UEdGraphNode>> ExistingNodes = Graph->Nodes;
	for (UEdGraphNode* Node : ExistingNodes)
	{
		if (Node)
		{
			Node->Modify();
			Node->DestroyNode();
		}
	}

	UEdGraphNode* BeginPlay =
		SpawnK2Node(BeginPlayTemplate, Graph, FVector2f(0.0f, 0.0f));
	UEdGraphNode* Branch = Intent.bHasBranch
		? SpawnK2Node(BranchTemplate, Graph, FVector2f(300.0f, 0.0f))
		: nullptr;
	UEdGraphNode* Print =
		SpawnK2Node(
			PrintTemplate,
			Graph,
			Intent.bHasBranch ? FVector2f(600.0f, 0.0f) : FVector2f(300.0f, 0.0f));
	if (!BeginPlay || (Intent.bHasBranch && !Branch) || !Print)
	{
		OutError = TEXT("native Blueprint writer failed to spawn required K2 nodes");
		return false;
	}

	UEdGraphPin* BeginPlayThen = BeginPlay->FindPin(UEdGraphSchema_K2::PN_Then);
	UEdGraphPin* BranchExecute =
		Branch ? Branch->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr;
	UEdGraphPin* BranchThen =
		Branch ? Branch->FindPin(UEdGraphSchema_K2::PN_Then) : nullptr;
	UEdGraphPin* BranchCondition =
		Branch ? Branch->FindPin(UEdGraphSchema_K2::PN_Condition) : nullptr;
	UEdGraphPin* PrintExecute = Print->FindPin(UEdGraphSchema_K2::PN_Execute);
	UEdGraphPin* PrintValue = Print->FindPin(TEXT("InString"));
	if (!BeginPlayThen
		|| (Intent.bHasBranch && (!BranchExecute || !BranchThen || !BranchCondition))
		|| !PrintExecute
		|| !PrintValue)
	{
		OutError = TEXT("native Blueprint writer could not resolve required K2 pins");
		return false;
	}

	if (BranchCondition)
	{
		BranchCondition->DefaultValue = Intent.bCondition ? TEXT("true") : TEXT("false");
	}
	PrintValue->DefaultValue = Intent.PrintString;
	const UEdGraphSchema* Schema = Graph->GetSchema();
	const bool bConnected = Schema
		&& (Intent.bHasBranch
			? Schema->TryCreateConnection(BeginPlayThen, BranchExecute)
				&& Schema->TryCreateConnection(BranchThen, PrintExecute)
			: Schema->TryCreateConnection(BeginPlayThen, PrintExecute));
	if (!bConnected)
	{
		OutError = TEXT("native Blueprint writer failed to connect the requested execution chain");
		return false;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	return true;
}

bool FUeremcpBlueprintEpicBridge::CompileBlueprint(UBlueprint* Blueprint, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("blueprint is null");
		return false;
	}

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	if (Blueprint->Status == BS_Error)
	{
		OutError = TEXT("Blueprint compile finished with BS_Error");
		return false;
	}

	return Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings;
}
