// UEREMCP — Hash round-trip scaffolding (WS-07).

#include "UeremcpNiagaraHashRoundTrip.h"

#include "UeremcpNiagaraGraphHash.h"

namespace
{
	void CompareHashMaps(
		const TMap<FString, FString>& First,
		const TMap<FString, FString>& Second,
		TArray<FString>& OutMismatches)
	{
		if (First.Num() != Second.Num())
		{
			OutMismatches.Add(FString::Printf(
				TEXT("graph count differs: first=%d second=%d"),
				First.Num(),
				Second.Num()));
		}

		for (const TPair<FString, FString>& Pair : First)
		{
			const FString* SecondHash = Second.Find(Pair.Key);
			if (!SecondHash)
			{
				OutMismatches.Add(FString::Printf(TEXT("missing in second pass: %s"), *Pair.Key));
				continue;
			}
			if (*SecondHash != Pair.Value)
			{
				OutMismatches.Add(FString::Printf(
					TEXT("hash mismatch for %s: %s vs %s"),
					*Pair.Key,
					*Pair.Value,
					**SecondHash));
			}
		}

		for (const TPair<FString, FString>& Pair : Second)
		{
			if (!First.Contains(Pair.Key))
			{
				OutMismatches.Add(FString::Printf(TEXT("extra in second pass: %s"), *Pair.Key));
			}
		}
	}
}

void FUeremcpNiagaraHashRoundTrip::RecordPostInspectScaffold(
	const TArray<TSharedPtr<FJsonValue>>& InspectGraphs,
	FUeremcpNiagaraHashRoundTripResult& OutResult)
{
	OutResult = FUeremcpNiagaraHashRoundTripResult();
	FUeremcpNiagaraGraphHash::CollectGraphHashes(InspectGraphs, OutResult.GraphIdToHash);
	OutResult.bHashesPresent = OutResult.GraphIdToHash.Num() > 0;

	if (OutResult.bHashesPresent)
	{
		OutResult.ChecksPerformed.Add(TEXT("niagara.content_hash_manifest"));
		OutResult.Summary = FString::Printf(
			TEXT("Content hashes recorded for %d graph(s). retrieve→submit→retrieve stability not proven; fidelity.round_trip_supported remains false."),
			OutResult.GraphIdToHash.Num());
		OutResult.FailureMode = TEXT(
			"scaffold_only — call EvaluateRetrieveSubmitRetrieveStability after a live "
			"inspect→submit→inspect pass to flip round_trip_supported");
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.content_hash_manifest"));
		OutResult.Summary = TEXT("No content_hash values present on inspect graphs.");
		OutResult.FailureMode = TEXT("no_content_hash_on_graphs");
	}

	OutResult.ChecksSkipped.Add(TEXT("niagara.content_hash_round_trip_stability"));
	OutResult.bRetrieveRetrieveStable = false;
	OutResult.bRetrieveSubmitRetrieveStable = false;
	OutResult.bRoundTripSupported = false;
}

bool FUeremcpNiagaraHashRoundTrip::EvaluateRetrieveRetrieveStability(
	const TArray<TSharedPtr<FJsonValue>>& FirstPassGraphs,
	const TArray<TSharedPtr<FJsonValue>>& SecondPassGraphs,
	FUeremcpNiagaraHashRoundTripResult& OutResult)
{
	OutResult = FUeremcpNiagaraHashRoundTripResult();

	TMap<FString, FString> FirstHashes;
	TMap<FString, FString> SecondHashes;
	FUeremcpNiagaraGraphHash::CollectGraphHashes(FirstPassGraphs, FirstHashes);
	FUeremcpNiagaraGraphHash::CollectGraphHashes(SecondPassGraphs, SecondHashes);

	OutResult.GraphIdToHash = FirstHashes;
	OutResult.bHashesPresent = FirstHashes.Num() > 0;
	CompareHashMaps(FirstHashes, SecondHashes, OutResult.HashMismatches);

	OutResult.bRetrieveRetrieveStable = OutResult.bHashesPresent && OutResult.HashMismatches.Num() == 0;
	OutResult.ChecksPerformed.Add(TEXT("niagara.content_hash_retrieve_retrieve_compare"));
	OutResult.ChecksSkipped.Add(TEXT("niagara.content_hash_round_trip_stability"));
	OutResult.bRetrieveSubmitRetrieveStable = false;
	OutResult.bRoundTripSupported = false;
	OutResult.FailureMode = OutResult.bRetrieveRetrieveStable
		? TEXT("retrieve_retrieve_ok_but_submit_pass_not_executed")
		: TEXT("retrieve_retrieve_hash_mismatch");
	OutResult.Summary = OutResult.bRetrieveRetrieveStable
		? TEXT("Retrieve→retrieve content_hash stable for compared graphs. replace/submit pass not executed; round_trip_supported stays false.")
		: FString::Printf(
			TEXT("Retrieve→retrieve hash mismatches: %s"),
			*FString::Join(OutResult.HashMismatches, TEXT("; ")));

	return true;
}

bool FUeremcpNiagaraHashRoundTrip::EvaluateRetrieveSubmitRetrieveStability(
	const TArray<TSharedPtr<FJsonValue>>& PreSubmitGraphs,
	const TArray<TSharedPtr<FJsonValue>>& PostSubmitGraphs,
	FUeremcpNiagaraHashRoundTripResult& OutResult)
{
	OutResult = FUeremcpNiagaraHashRoundTripResult();

	TMap<FString, FString> PreHashes;
	TMap<FString, FString> PostHashes;
	FUeremcpNiagaraGraphHash::CollectGraphHashes(PreSubmitGraphs, PreHashes);
	FUeremcpNiagaraGraphHash::CollectGraphHashes(PostSubmitGraphs, PostHashes);

	OutResult.GraphIdToHash = PreHashes;
	OutResult.bHashesPresent = PreHashes.Num() > 0;
	CompareHashMaps(PreHashes, PostHashes, OutResult.HashMismatches);

	OutResult.ChecksPerformed.Add(TEXT("niagara.content_hash_retrieve_submit_retrieve_compare"));
	OutResult.bRetrieveSubmitRetrieveStable =
		OutResult.bHashesPresent && OutResult.HashMismatches.Num() == 0;
	// Flip ONLY when proven.
	OutResult.bRoundTripSupported = OutResult.bRetrieveSubmitRetrieveStable;
	OutResult.bRetrieveRetrieveStable = OutResult.bRetrieveSubmitRetrieveStable;

	if (OutResult.bRoundTripSupported)
	{
		OutResult.FailureMode.Reset();
		OutResult.Summary = FString::Printf(
			TEXT("PROVEN: retrieve→submit→retrieve content_hash stable for %d graph(s). "
				 "fidelity.round_trip_supported may be set true."),
			PreHashes.Num());
	}
	else if (!OutResult.bHashesPresent)
	{
		OutResult.FailureMode = TEXT("no_content_hash_on_pre_submit_graphs");
		OutResult.ChecksSkipped.Add(TEXT("niagara.content_hash_round_trip_stability"));
		OutResult.Summary = TEXT(
			"retrieve→submit→retrieve not proven: pre-submit graphs lack content_hash. "
			"round_trip_supported stays false.");
	}
	else
	{
		OutResult.FailureMode = TEXT(
			"hash_drift_after_submit — inspect canonicalization or submit side-effects "
			"changed content_hash; keep round_trip_supported=false");
		OutResult.Summary = FString::Printf(
			TEXT("retrieve→submit→retrieve FAILED (%d mismatch(es)): %s. "
				 "round_trip_supported stays false."),
			OutResult.HashMismatches.Num(),
			*FString::Join(OutResult.HashMismatches, TEXT("; ")));
	}

	return true;
}

TSharedPtr<FJsonObject> FUeremcpNiagaraHashRoundTrip::BuildDiagnosticsObject(
	const FUeremcpNiagaraHashRoundTripResult& Result)
{
	TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
	Diagnostics->SetBoolField(TEXT("round_trip_supported"), Result.bRoundTripSupported);
	Diagnostics->SetBoolField(TEXT("retrieve_retrieve_stable"), Result.bRetrieveRetrieveStable);
	Diagnostics->SetBoolField(
		TEXT("retrieve_submit_retrieve_stable"),
		Result.bRetrieveSubmitRetrieveStable);
	Diagnostics->SetBoolField(TEXT("hashes_present"), Result.bHashesPresent);
	if (!Result.FailureMode.IsEmpty())
	{
		Diagnostics->SetStringField(TEXT("failure_mode"), Result.FailureMode);
	}

	TSharedPtr<FJsonObject> Manifest = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Result.GraphIdToHash)
	{
		Manifest->SetStringField(Pair.Key, Pair.Value);
	}
	Diagnostics->SetObjectField(TEXT("graph_hashes"), Manifest);

	if (Result.HashMismatches.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> MismatchValues;
		for (const FString& Mismatch : Result.HashMismatches)
		{
			MismatchValues.Add(MakeShared<FJsonValueString>(Mismatch));
		}
		Diagnostics->SetArrayField(TEXT("hash_mismatches"), MismatchValues);
	}

	return Diagnostics;
}
