// UEREMCP — single active mutator queue (ADR-0010 §3.4, ADR-0009).
//
// TODO (Wave 2): game-thread FIFO for write|destructive|unsafe; compose with
// FUeremcpJob poll handles when waiters exceed timeout_ms.

#pragma once

#include "CoreMinimal.h"
#include "UeremcpPermissionTier.h"

class UEREMCPSECURITY_API FUeremcpMutatorQueue
{
public:
	struct FAcquireResult
	{
		bool bAcquired = false;
		/** Set when queued per ADR-0009; empty when acquired immediately. */
		FString JobId;
		FString Reason;
	};

	/** Returns false until queue wiring lands (stub locks API shape). */
	static bool IsImplemented();

	/**
	 * Try to become the active mutator for this project.
	 * TODO: enforce on game thread; return job id when queued.
	 */
	static FAcquireResult TryAcquire(const FString& RequestId, EUeremcpPermissionTier Tier);

	/** Release the mutator slot owned by RequestId. */
	static void Release(const FString& RequestId);

	/** True while any write+ tier holds the lock (stub: always false). */
	static bool IsActive();
};
