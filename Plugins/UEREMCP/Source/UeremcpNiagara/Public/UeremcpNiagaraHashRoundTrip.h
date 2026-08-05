// UEREMCP — Hash round-trip scaffolding (WS-07).
//
// Retrieve → submit → retrieve stability flips round_trip_supported ONLY when
// EvaluateRetrieveSubmitRetrieveStability reports success on real inspect graphs.
// Offline harnesses must keep false.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Outcome of hash scaffold evaluation. */
struct FUeremcpNiagaraHashRoundTripResult
{
	bool bHashesPresent = false;
	bool bRetrieveRetrieveStable = false;
	/** True only when retrieve→submit→retrieve hashes match (live proof). */
	bool bRetrieveSubmitRetrieveStable = false;
	/** When true, callers MAY set fidelity.round_trip_supported=true. */
	bool bRoundTripSupported = false;

	FString Summary;
	TMap<FString, FString> GraphIdToHash;
	TArray<FString> HashMismatches;
	FString FailureMode;

	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;
};

/** Hash round-trip scaffolding for Niagara inspect graphs. */
class FUeremcpNiagaraHashRoundTrip
{
public:
	/** Record hashes from a single inspect pass; skips full round-trip stability. */
	static void RecordPostInspectScaffold(
		const TArray<TSharedPtr<FJsonValue>>& InspectGraphs,
		FUeremcpNiagaraHashRoundTripResult& OutResult);

	/**
	 * Compare two inspect graph sets (retrieve → retrieve, no submit).
	 * Does not flip round_trip_supported.
	 */
	static bool EvaluateRetrieveRetrieveStability(
		const TArray<TSharedPtr<FJsonValue>>& FirstPassGraphs,
		const TArray<TSharedPtr<FJsonValue>>& SecondPassGraphs,
		FUeremcpNiagaraHashRoundTripResult& OutResult);

	/**
	 * Full retrieve → submit → retrieve harness.
	 * PreHashes = inspect before submit; PostHashes = inspect after submit of the
	 * same graphs (identity submit / no-op structural apply). Sets
	 * bRoundTripSupported=true ONLY when all compared graph_id hashes match and
	 * hashes are present. Otherwise records FailureMode and keeps false.
	 */
	static bool EvaluateRetrieveSubmitRetrieveStability(
		const TArray<TSharedPtr<FJsonValue>>& PreSubmitGraphs,
		const TArray<TSharedPtr<FJsonValue>>& PostSubmitGraphs,
		FUeremcpNiagaraHashRoundTripResult& OutResult);

	/** Build diagnostics JSON for response.extra_fields.diagnostics.hash_scaffold. */
	static TSharedPtr<FJsonObject> BuildDiagnosticsObject(
		const FUeremcpNiagaraHashRoundTripResult& Result);
};
