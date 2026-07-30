// UEREMCP — Niagara graph content_hash helpers (WS-07).
//
// Uses FUeremcpContentHash (ADR-0004 / WS-05) — does not fork hash semantics.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Apply ADR-0004 content_hash + revision to inspect graph payloads. */
class FUeremcpNiagaraGraphHash
{
public:
	/**
	 * Compute content_hash via FUeremcpContentHash and set content_hash + revision.
	 * Forces fidelity.round_trip_supported=false on the graph.
	 * [VERIFIED: Plugins/UEREMCP/Source/UeremcpProtocol/Public/UeremcpContentHash.h]
	 */
	static bool ApplyContentHashToGraph(const TSharedPtr<FJsonObject>& Graph, FString& OutError);

	/** Apply hashes to every graph object in the array; records check keys. */
	static int32 ApplyContentHashesToGraphs(
		TArray<TSharedPtr<FJsonValue>>& Graphs,
		TArray<FString>& OutChecksPerformed,
		TArray<FString>& OutChecksSkipped);

	/** Collect graph_id → content_hash for graphs that carry a hash. */
	static void CollectGraphHashes(
		const TArray<TSharedPtr<FJsonValue>>& Graphs,
		TMap<FString, FString>& OutGraphIdToHash);
};
