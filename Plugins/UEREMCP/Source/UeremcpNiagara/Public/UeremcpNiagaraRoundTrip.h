// UEREMCP — Post-create inspect round-trip helpers (WS-07).
//
// Structural re-read after create — honest partially_completed only.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"
#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraHashRoundTrip.h"
#include "UeremcpNiagaraInspect.h"

/** Outcome of create → inspect structural comparison (not ADR-0004 hash round-trip). */
struct FUeremcpNiagaraRoundTripResult
{
	bool bInspectSucceeded = false;
	bool bStructuralMatch = false;

	FString Summary;
	TArray<FString> Mismatches;

	TArray<TSharedPtr<FJsonValue>> InspectGraphs;
	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;

	FUeremcpNiagaraHashRoundTripResult HashScaffold;

	int32 InternalOperations = 0;
};

/** Post-create inspect helpers — never report *_validated. */
class FUeremcpNiagaraRoundTrip
{
public:
	/**
	 * Run inspect_system on CreatedAssetPath and compare emitter / user-var names
	 * against the create result. Sets bStructuralMatch when counts and names align.
	 */
	static bool ValidateCreateResult(
		const FUeremcpRequest& OriginalRequest,
		const FUeremcpNiagaraCreateResult& CreateResult,
		FUeremcpNiagaraRoundTripResult& OutResult);

	/**
	 * Testable structural matcher — compares create emitters/user vars to inspect graphs.
	 * Does not touch the editor.
	 */
	static bool EvaluateStructuralMatch(
		const FUeremcpNiagaraCreateResult& CreateResult,
		const TArray<TSharedPtr<FJsonValue>>& InspectGraphs,
		TArray<FString>& OutMismatches);
};
