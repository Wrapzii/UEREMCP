// UEREMCP — per-project single active mutator queue (ADR-0010 §3.4, ADR-0009).

#pragma once

#include "CoreMinimal.h"
#include "UeremcpPermissionTier.h"

class UEREMCPSECURITY_API FUeremcpMutatorQueue
{
public:
	struct FAcquireResult
	{
		bool bAcquired = false;
		bool bQueued = false;
		/** Stable while this request waits; the dispatcher maps it to ADR-0009 polling. */
		FString JobId;
		FString Reason;
		/** True when stale active/waiters were cleared during this acquire. */
		bool bClearedStale = false;
		int32 ClearedWaiters = 0;
	};

	/** Waiters that do not re-TryAcquire within this window are dropped. */
	static constexpr double StaleWaiterSeconds = 45.0;
	/** Active owner that never Releases within this window is force-cleared. */
	static constexpr double StaleActiveSeconds = 180.0;

	static bool IsImplemented();

	/**
	 * Try to become the active mutator for ProjectKey.
	 * Read-tier requests bypass the queue. Mutators acquire in FIFO order.
	 * A queued caller retries with the same request id until bAcquired is true.
	 * Stale waiters / orphaned active slots are cleared so agents never hang forever.
	 */
	static FAcquireResult TryAcquire(
		const FString& ProjectKey,
		const FString& RequestId,
		EUeremcpPermissionTier Tier);

	/** Uses the current project directory as ProjectKey. */
	static FAcquireResult TryAcquire(const FString& RequestId, EUeremcpPermissionTier Tier);

	/** Release the mutator slot only when ProjectKey and RequestId own it. */
	static bool Release(const FString& ProjectKey, const FString& RequestId);

	/** Searches project queues for RequestId; prefer the explicit overload. */
	static void Release(const FString& RequestId);

	/** Remove a waiting request without disturbing the active owner. */
	static bool CancelQueued(const FString& ProjectKey, const FString& RequestId);

	/**
	 * Drop stale waiters and force-release a stale active owner.
	 * Safe to call before retry storms; returns how many waiters were removed.
	 */
	static int32 ClearStale(const FString& ProjectKey, bool& bClearedActive);

	/** Emergency: clear active + all waiters for the project. */
	static void ForceClear(const FString& ProjectKey);

	static bool IsActive(const FString& ProjectKey);

	/** True while any project has an active mutator. */
	static bool IsActive();

	static int32 PendingCount(const FString& ProjectKey);

	/** Active owner's request id (empty if none). For diagnostics. */
	static FString ActiveRequestId(const FString& ProjectKey);
};
