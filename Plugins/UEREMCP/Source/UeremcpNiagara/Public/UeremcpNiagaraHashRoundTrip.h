// UEREMCP — Hash round-trip scaffolding (WS-07).
//
// Retrieve → replace → retrieve stability is NOT proven here; fidelity stays false.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Outcome of hash scaffold evaluation — never claims round-trip validated. */
struct FUeremcpNiagaraHashRoundTripResult
{
	bool bHashesPresent = false;
	bool bRetrieveRetrieveStable = false;

	FString Summary;
	TMap<FString, FString> GraphIdToHash;
	TArray<FString> HashMismatches;

	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;
};

/** Offline-capable hash round-trip scaffolding for Niagara inspect graphs. */
class FUeremcpNiagaraHashRoundTrip
{
public:
	/** Record hashes from a single inspect pass; skips full round-trip stability. */
	static void RecordPostInspectScaffold(
		const TArray<TSharedPtr<FJsonValue>>& InspectGraphs,
		FUeremcpNiagaraHashRoundTripResult& OutResult);

	/**
	 * Compare two inspect graph sets (simulated retrieve → retrieve).
	 * Does not set round_trip_supported; use only for offline harness / WS-11 prep.
	 */
	static bool EvaluateRetrieveRetrieveStability(
		const TArray<TSharedPtr<FJsonValue>>& FirstPassGraphs,
		const TArray<TSharedPtr<FJsonValue>>& SecondPassGraphs,
		FUeremcpNiagaraHashRoundTripResult& OutResult);

	/** Build diagnostics JSON for response.extra_fields.diagnostics.hash_scaffold. */
	static TSharedPtr<FJsonObject> BuildDiagnosticsObject(
		const FUeremcpNiagaraHashRoundTripResult& Result);
};
