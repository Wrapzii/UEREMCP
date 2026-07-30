// UEREMCP — fail-closed execute_plan interpreter (ADR-0008).
//
// Owner: WS-05. Domain modules register semantic action handlers; an integration
// module supplies the cross-operation transaction callbacks.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

using FUeremcpPlanOperationHandler = TFunction<bool(
	const FString& RequestJson,
	FString& OutResponseJson,
	FString& OutError)>;

struct UEREMCPPROTOCOL_API FUeremcpPlanTransactionCallbacks
{
	TFunction<bool(FString& OutError)> Begin;
	TFunction<bool(FString& OutError)> Commit;
	TFunction<bool(FString& OutError)> Rollback;

	bool IsComplete() const
	{
		return Begin && Commit && Rollback;
	}
};

/**
 * Interprets one complete execute_plan request.
 *
 * This class deliberately does not depend on ToolsetRegistry or domain modules.
 * Integrators register goal-level action handlers and a transaction coordinator.
 * Missing execution capabilities reject before mutation instead of silently
 * weakening atomicity, compile policy, validation, or rollback.
 */
class UEREMCPPROTOCOL_API FUeremcpPlanExecutor
{
public:
	static bool RegisterAction(
		const FString& Action,
		FUeremcpPlanOperationHandler&& Handler,
		FString& OutError);

	static void UnregisterAction(const FString& Action);
	static void ClearActionHandlers();

	static bool SetTransactionCallbacks(
		FUeremcpPlanTransactionCallbacks&& Callbacks,
		FString& OutError);
	static void ClearTransactionCallbacks();

	/** Delegate-compatible entry point for UeremcpTemplates::SetExecutePlanDelegate. */
	static bool ExecuteRequest(
		const FString& RequestJson,
		FString& OutResponseJson,
		FString& OutError);
};
