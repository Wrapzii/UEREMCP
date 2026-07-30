// UEREMCP — Core-facing public action adapter for ADR-0008 execute_plan.
//
// Owner: WS-05. Core exposes this through a thin AICallable UFUNCTION wrapper
// (same pattern as FUeremcpJobActions). Protocol stays ToolsetRegistry-free
// [VERIFIED: UeremcpProtocol.Build.cs layering comment].

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

/**
 * Agent-facing execute_plan entry that returns one response envelope string.
 *
 * Parses the frozen request envelope, honours idempotency_key replay (ADR-0006),
 * dispatches through FUeremcpPlanExecutor, and applies ADR-0009 timeout semantics:
 * timeout_ms == 0 completes inline; timeout_ms > 0 may return partially_completed
 * with a job handle when work is still running.
 */
class UEREMCPPROTOCOL_API FUeremcpPlanActions
{
public:
	/** Implements action=execute_plan. Returns response envelope JSON. */
	static FString ExecutePlan(const FString& RequestJson);

	/**
	 * Test-only probe for the positive-timeout initiating path.
	 * When set and timeout_ms > 0, returning true forces GetTimeoutResponse
	 * without running the plan (deterministic partial coverage without a
	 * production Core/Transport scheduler).
	 */
	static void SetForceTimeoutForTests(TFunction<bool()>&& Probe);
	static void ClearForceTimeoutForTests();
};
