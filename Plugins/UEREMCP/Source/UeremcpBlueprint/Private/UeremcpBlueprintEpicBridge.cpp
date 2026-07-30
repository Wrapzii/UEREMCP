#include "UeremcpBlueprintEpicBridge.h"

#include "BlueprintEditorLibrary.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "HAL/PlatformProcess.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Modules/ModuleManager.h"
#include "ToolsetRegistry/ToolCallAsyncResultString.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

namespace
{
	static const FString EpicBlueprintToolsetName =
		TEXT("editor_toolset.toolsets.blueprint.BlueprintTools");

	static FString EscapeBlueprintBridgeJsonString(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len() + 8);
		for (const TCHAR C : In)
		{
			switch (C)
			{
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('"'): Out += TEXT("\\\""); break;
			case TEXT('\n'): Out += TEXT("\\n"); break;
			case TEXT('\r'): Out += TEXT("\\r"); break;
			case TEXT('\t'): Out += TEXT("\\t"); break;
			default: Out += C; break;
			}
		}
		return Out;
	}

	static bool EnsureEpicBlueprintToolsRegistered(FString& OutError)
	{
		if (UToolsetRegistry::IsToolsetRegistered(EpicBlueprintToolsetName))
		{
			return true;
		}

		// EditorToolset registers its Python-backed BlueprintTools from init_unreal.py.
		// [VERIFIED: Engine/Plugins/Experimental/Toolsets/EditorToolset/Content/Python/init_unreal.py:3-9]
		FModuleManager::Get().LoadModule(TEXT("PythonScriptPlugin"));

		if (UToolsetRegistry::IsToolsetRegistered(EpicBlueprintToolsetName))
		{
			return true;
		}

		OutError = FString::Printf(
			TEXT("Required Epic toolset '%s' is not registered after loading PythonScriptPlugin. ")
			TEXT("The public MCP operation is toolset 'UeremcpBlueprint.UeremcpBlueprintToolset', ")
			TEXT("tool 'SubmitGraph', with envelope action 'submit_graph' and mode 'replace'."),
			*EpicBlueprintToolsetName);
		return false;
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
	if (!EnsureEpicBlueprintToolsRegistered(OutError))
	{
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

bool FUeremcpBlueprintEpicBridge::WriteGraphDsl(UEdGraph* Graph, const FString& DslCode, FString& OutError)
{
	if (!Graph)
	{
		OutError = TEXT("graph is null");
		return false;
	}

	const FString Input = FString::Printf(
		TEXT("{\"graph\":{\"refPath\":\"%s\"},\"code\":\"%s\"}"),
		*Graph->GetPathName(),
		*EscapeBlueprintBridgeJsonString(DslCode));

	FString Result;
	if (!ExecuteToolSync(TEXT("write_graph_dsl"), Input, Result, OutError))
	{
		return false;
	}

	return true;
}

bool FUeremcpBlueprintEpicBridge::CompileBlueprint(UBlueprint* Blueprint, FString& OutError)
{
	if (!Blueprint)
	{
		OutError = TEXT("blueprint is null");
		return false;
	}

	const FString Input = FString::Printf(
		TEXT("{\"blueprint\":{\"refPath\":\"%s\"}}"),
		*Blueprint->GetPathName());

	FString Result;
	if (!ExecuteToolSync(TEXT("compile_blueprint"), Input, Result, OutError))
	{
		return false;
	}

	if (Blueprint->Status == BS_Error)
	{
		OutError = TEXT("Blueprint compile finished with BS_Error");
		return false;
	}

	return Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings;
}
