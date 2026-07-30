// Epic BlueprintTools bridge (WS-06). Internal — not agent-facing.

#pragma once

#include "CoreMinimal.h"

class UBlueprint;
class UEdGraph;

/** Sync calls into editor_toolset.toolsets.blueprint.BlueprintTools via ToolsetRegistry. */
class UEREMCPBLUEPRINT_API FUeremcpBlueprintEpicBridge
{
public:
	static bool ExecuteToolSync(
		const FString& ToolName,
		const FString& JsonInput,
		FString& OutJsonResult,
		FString& OutError);

	static bool WriteGraphDsl(UEdGraph* Graph, const FString& DslCode, FString& OutError);

	static bool ValidateWriteGraphDsl(const FString& DslCode, FString& OutError);

	static bool CompileBlueprint(UBlueprint* Blueprint, FString& OutError);

	static UEdGraph* ResolveGraph(UBlueprint* Blueprint, const FString& GraphId, FString& OutGraphName);
};
