// UEREMCP — POC B inspect fidelity signals for gate scaffolding (WS-07).
//
// Uses post-create inspect graph payloads only. Material paths and DI counts are
// observational — never treated as validated binding/completeness.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Signals extracted from inspect_system graphs (offline-capable). */
struct FUeremcpNiagaraPocBInspectSignals
{
	bool bEvaluated = false;

	int32 ExpectedEmitterCount = 0;
	int32 EmittersWithRendererRefs = 0;
	int32 TotalRendererRefs = 0;
	int32 RenderersWithExtractedMaterialPath = 0;
	TArray<FString> EmittersMissingRenderers;

	bool bDependenciesPresent = false;
	int32 UsedDataInterfaces = 0;

	bool bCompileStatePresent = false;
	bool bCompileHasErrors = false;

	TArray<FString> FidelityNotes;
};

class FUeremcpNiagaraPocBInspectFidelity
{
public:
	static FUeremcpNiagaraPocBInspectSignals Evaluate(
		const TArray<FString>& ExpectedEmitterNames,
		const TArray<TSharedPtr<FJsonValue>>& InspectGraphs);
};
