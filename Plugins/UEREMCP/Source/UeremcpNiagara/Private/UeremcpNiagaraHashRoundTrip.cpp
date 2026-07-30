// UEREMCP — Hash round-trip scaffolding (WS-07).

#include "UeremcpNiagaraHashRoundTrip.h"

#include "UeremcpNiagaraGraphHash.h"

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
			TEXT("Content hashes recorded for %d graph(s). retrieve→replace→retrieve stability not proven; fidelity.round_trip_supported remains false."),
			OutResult.GraphIdToHash.Num());
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("niagara.content_hash_manifest"));
		OutResult.Summary = TEXT("No content_hash values present on inspect graphs.");
	}

	OutResult.ChecksSkipped.Add(TEXT("niagara.content_hash_round_trip_stability"));
	OutResult.bRetrieveRetrieveStable = false;
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

	if (FirstHashes.Num() != SecondHashes.Num())
	{
		OutResult.HashMismatches.Add(FString::Printf(
			TEXT("graph count differs: first=%d second=%d"),
			FirstHashes.Num(),
			SecondHashes.Num()));
	}

	for (const TPair<FString, FString>& Pair : FirstHashes)
	{
		const FString* SecondHash = SecondHashes.Find(Pair.Key);
		if (!SecondHash)
		{
			OutResult.HashMismatches.Add(FString::Printf(TEXT("missing in second pass: %s"), *Pair.Key));
			continue;
		}
		if (*SecondHash != Pair.Value)
		{
			OutResult.HashMismatches.Add(FString::Printf(
				TEXT("hash mismatch for %s: %s vs %s"),
				*Pair.Key,
				*Pair.Value,
				**SecondHash));
		}
	}

	for (const TPair<FString, FString>& Pair : SecondHashes)
	{
		if (!FirstHashes.Contains(Pair.Key))
		{
			OutResult.HashMismatches.Add(FString::Printf(TEXT("extra in second pass: %s"), *Pair.Key));
		}
	}

	OutResult.bRetrieveRetrieveStable = OutResult.bHashesPresent && OutResult.HashMismatches.Num() == 0;
	OutResult.ChecksPerformed.Add(TEXT("niagara.content_hash_retrieve_retrieve_compare"));

	// Honest: retrieve→replace→retrieve and round_trip_supported flip require WS-11 runtime proof.
	OutResult.ChecksSkipped.Add(TEXT("niagara.content_hash_round_trip_stability"));
	OutResult.Summary = OutResult.bRetrieveRetrieveStable
		? TEXT("Retrieve→retrieve content_hash stable for compared graphs. replace pass not executed; round_trip_supported stays false.")
		: FString::Printf(
			TEXT("Retrieve→retrieve hash mismatches: %s"),
			*FString::Join(OutResult.HashMismatches, TEXT("; ")));

	return true;
}

TSharedPtr<FJsonObject> FUeremcpNiagaraHashRoundTrip::BuildDiagnosticsObject(
	const FUeremcpNiagaraHashRoundTripResult& Result)
{
	TSharedPtr<FJsonObject> Diagnostics = MakeShared<FJsonObject>();
	Diagnostics->SetBoolField(TEXT("round_trip_supported"), false);
	Diagnostics->SetBoolField(TEXT("retrieve_retrieve_stable"), Result.bRetrieveRetrieveStable);
	Diagnostics->SetBoolField(TEXT("hashes_present"), Result.bHashesPresent);

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
