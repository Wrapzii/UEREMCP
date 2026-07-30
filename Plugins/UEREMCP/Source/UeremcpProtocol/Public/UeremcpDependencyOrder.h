// UEREMCP — batch dependency topological sort (schemas/batch/plan.schema.json).
// Owner: WS-05. Cycles are rejected before ANY operation runs.

#pragma once

#include "CoreMinimal.h"

/** One node in a dependency graph (operation id + depends_on). */
struct UEREMCPPROTOCOL_API FUeremcpDependencyNode
{
	FString Id;
	TArray<FString> DependsOn;
};

/**
 * Topologically sort operations by depends_on.
 * Stable among ties: original input order is preserved (Kahn with FIFO of ready ids
 * inserted in first-seen order).
 */
class UEREMCPPROTOCOL_API FUeremcpDependencyOrder
{
public:
	/**
	 * @return true and fills OutOrderedIds on success.
	 *         false with OutError on duplicate ids, missing dependency, or cycle.
	 */
	static bool TopologicalSort(
		const TArray<FUeremcpDependencyNode>& Nodes,
		TArray<FString>& OutOrderedIds,
		FString& OutError);
};
