// UEREMCP — execute_plan cross-operation transaction callbacks (ADR-0005).
//
// Owner: WS-03. Wires FUeremcpPlanExecutor transaction hooks to FileSandbox +
// editor undo bracketing via ToolsetRegistry.

#pragma once

#include "CoreMinimal.h"

class UEREMCPCORE_API FUeremcpPlanTransactionCoordinator
{
public:
	/** Register begin/commit/rollback callbacks with FUeremcpPlanExecutor. */
	static bool RegisterWithExecutor(FString& OutError);

	static void UnregisterFromExecutor();

	static bool IsSessionActive();

	/** ADR-0005 outer layer: Enter sandbox and sample undo depth. */
	static bool Begin(FString& OutError);

	/** Persist sandbox changes and leave; detect undo leaks. */
	static bool Commit(FString& OutError);

	/** Discard sandbox changes, leave, and unwind editor undo delta. */
	static bool Rollback(FString& OutError);
};
