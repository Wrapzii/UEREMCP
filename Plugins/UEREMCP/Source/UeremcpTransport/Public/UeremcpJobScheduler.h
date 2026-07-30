// UEREMCP — production timeout scheduler for ADR-0009 jobs.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeBool.h"
#include "UeremcpEnvelope.h"

/**
 * Domain work dispatched by the timeout scheduler.
 *
 * JobId is populated for poll-after-timeout work and empty for inline work.
 * Cooperative implementations must stop before validated completion when
 * CancelRequested becomes true.
 */
using FUeremcpScheduledJobWork = TFunction<FUeremcpResponse(
	const FString& JobId,
	const TSharedRef<FThreadSafeBool, ESPMode::ThreadSafe>& CancelRequested)>;

/** Receives exactly one initiating tools/call response. */
using FUeremcpScheduledResponse = TFunction<void(const FUeremcpResponse& Response)>;

/**
 * Runs domain work without blocking the MCP/game thread and enforces timeout_ms.
 *
 * The callback may run on any thread. This matches Epic's MCP result callback
 * contract [VERIFIED: IModelContextProtocolTool.h:30-32].
 */
class UEREMCPTRANSPORT_API FUeremcpJobScheduler
{
public:
	/**
	 * timeout_ms == 0 completes inline through Callback with no job handle.
	 * timeout_ms > 0 registers a process-local job; the first of completion or
	 * timeout wins Callback. Timed-out work remains pollable in JobRegistry.
	 */
	static bool Dispatch(
		const FString& RequestId,
		int32 TimeoutMs,
		bool bCancellable,
		const FString& InitialProgressMessage,
		FUeremcpScheduledJobWork&& Work,
		FUeremcpScheduledResponse&& Callback,
		FString& OutJobId,
		FString& OutError);
};
