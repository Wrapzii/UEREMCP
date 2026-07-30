// UEREMCP — Niagara graph content_hash helpers (WS-07).

#include "UeremcpNiagaraGraphHash.h"

#include "UeremcpContentHash.h"

namespace
{
	void EnsureRoundTripUnsupported(const TSharedPtr<FJsonObject>& Graph)
	{
		const TSharedPtr<FJsonObject>* FidelityPtr = nullptr;
		TSharedPtr<FJsonObject> Fidelity;
		if (Graph->TryGetObjectField(TEXT("fidelity"), FidelityPtr) && FidelityPtr && FidelityPtr->IsValid())
		{
			Fidelity = *FidelityPtr;
		}
		else
		{
			Fidelity = MakeShared<FJsonObject>();
			Graph->SetObjectField(TEXT("fidelity"), Fidelity);
		}
		Fidelity->SetBoolField(TEXT("round_trip_supported"), false);
	}
}

bool FUeremcpNiagaraGraphHash::ApplyContentHashToGraph(
	const TSharedPtr<FJsonObject>& Graph,
	FString& OutError)
{
	OutError.Reset();
	if (!Graph.IsValid())
	{
		OutError = TEXT("Graph object is null.");
		return false;
	}

	EnsureRoundTripUnsupported(Graph);

	const FString Hash = FUeremcpContentHash::HashJsonObject(Graph, &OutError);
	if (Hash.IsEmpty())
	{
		return false;
	}

	Graph->SetStringField(TEXT("content_hash"), Hash);
	Graph->SetStringField(TEXT("revision"), Hash);
	return true;
}

int32 FUeremcpNiagaraGraphHash::ApplyContentHashesToGraphs(
	TArray<TSharedPtr<FJsonValue>>& Graphs,
	TArray<FString>& OutChecksPerformed,
	TArray<FString>& OutChecksSkipped)
{
	int32 HashedCount = 0;

	for (const TSharedPtr<FJsonValue>& GraphValue : Graphs)
	{
		const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
		if (!Graph.IsValid())
		{
			continue;
		}

		FString GraphId;
		Graph->TryGetStringField(TEXT("graph_id"), GraphId);
		if (GraphId.IsEmpty())
		{
			GraphId = TEXT("unknown");
		}

		FString HashError;
		if (ApplyContentHashToGraph(Graph, HashError))
		{
			++HashedCount;
		}
		else
		{
			OutChecksSkipped.Add(FString::Printf(TEXT("niagara.content_hash_%s"), *GraphId));
		}
	}

	if (HashedCount > 0)
	{
		OutChecksPerformed.Add(TEXT("niagara.content_hash_compute"));
	}

	OutChecksSkipped.Add(TEXT("niagara.content_hash_round_trip_stability"));
	return HashedCount;
}

void FUeremcpNiagaraGraphHash::CollectGraphHashes(
	const TArray<TSharedPtr<FJsonValue>>& Graphs,
	TMap<FString, FString>& OutGraphIdToHash)
{
	OutGraphIdToHash.Reset();

	for (const TSharedPtr<FJsonValue>& GraphValue : Graphs)
	{
		const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
		if (!Graph.IsValid())
		{
			continue;
		}

		FString GraphId;
		FString ContentHash;
		if (Graph->TryGetStringField(TEXT("graph_id"), GraphId)
			&& Graph->TryGetStringField(TEXT("content_hash"), ContentHash)
			&& !GraphId.IsEmpty()
			&& !ContentHash.IsEmpty())
		{
			OutGraphIdToHash.Add(GraphId, ContentHash);
		}
	}
}

void FUeremcpNiagaraGraphHash::EnsureRoundTripUnsupportedOnGraphs(
	TArray<TSharedPtr<FJsonValue>>& Graphs)
{
	for (const TSharedPtr<FJsonValue>& GraphValue : Graphs)
	{
		const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
		if (Graph.IsValid())
		{
			EnsureRoundTripUnsupported(Graph);
		}
	}
}
