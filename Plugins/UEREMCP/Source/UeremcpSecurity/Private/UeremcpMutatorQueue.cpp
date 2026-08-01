#include "UeremcpMutatorQueue.h"

#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

namespace UeremcpMutatorQueuePrivate
{
	struct FWaiter
	{
		FString RequestId;
		FString JobId;
		double LastSeenSeconds = 0.0;
	};

	struct FProjectQueue
	{
		FString ActiveRequestId;
		double ActiveAcquiredSeconds = 0.0;
		TArray<FWaiter> Waiters;
	};

	static FCriticalSection& Mutex()
	{
		static FCriticalSection Instance;
		return Instance;
	}

	static TMap<FString, FProjectQueue>& Queues()
	{
		static TMap<FString, FProjectQueue> Instance;
		return Instance;
	}

	static FString NormalizeProjectKey(const FString& ProjectKey)
	{
		if (ProjectKey.IsEmpty())
		{
			return FString();
		}

		FString Normalized = FPaths::ConvertRelativePathToFull(ProjectKey);
		FPaths::NormalizeFilename(Normalized);
		FPaths::CollapseRelativeDirectories(Normalized);
		return Normalized.ToLower();
	}

	/** Drop abandoned waiters and orphaned active holders. Caller holds Mutex. */
	static void ClearStaleLocked(
		FProjectQueue& Queue,
		const double NowSeconds,
		bool& bClearedActive,
		int32& ClearedWaiters)
	{
		bClearedActive = false;
		ClearedWaiters = 0;

		if (!Queue.ActiveRequestId.IsEmpty()
			&& (NowSeconds - Queue.ActiveAcquiredSeconds) > FUeremcpMutatorQueue::StaleActiveSeconds)
		{
			Queue.ActiveRequestId.Reset();
			Queue.ActiveAcquiredSeconds = 0.0;
			bClearedActive = true;
		}

		const int32 Before = Queue.Waiters.Num();
		Queue.Waiters.RemoveAll(
			[NowSeconds](const FWaiter& Waiter)
			{
				return (NowSeconds - Waiter.LastSeenSeconds)
					> FUeremcpMutatorQueue::StaleWaiterSeconds;
			});
		ClearedWaiters = Before - Queue.Waiters.Num();
	}
}

bool FUeremcpMutatorQueue::IsImplemented()
{
	return true;
}

FUeremcpMutatorQueue::FAcquireResult FUeremcpMutatorQueue::TryAcquire(
	const FString& ProjectKey,
	const FString& RequestId,
	EUeremcpPermissionTier Tier)
{
	FAcquireResult Result;
	if (Tier == EUeremcpPermissionTier::Read)
	{
		Result.bAcquired = true;
		Result.Reason = TEXT("read tier bypasses the mutator queue");
		return Result;
	}

	const FString NormalizedProjectKey = UeremcpMutatorQueuePrivate::NormalizeProjectKey(ProjectKey);
	if (NormalizedProjectKey.IsEmpty())
	{
		Result.Reason = TEXT("project key is required for mutator serialization");
		return Result;
	}
	if (RequestId.IsEmpty())
	{
		Result.Reason = TEXT("request id is required for mutator serialization");
		return Result;
	}

	FScopeLock Lock(&UeremcpMutatorQueuePrivate::Mutex());
	UeremcpMutatorQueuePrivate::FProjectQueue& Queue =
		UeremcpMutatorQueuePrivate::Queues().FindOrAdd(NormalizedProjectKey);

	const double Now = FPlatformTime::Seconds();
	bool bClearedActive = false;
	int32 ClearedWaiters = 0;
	UeremcpMutatorQueuePrivate::ClearStaleLocked(Queue, Now, bClearedActive, ClearedWaiters);
	Result.bClearedStale = bClearedActive || ClearedWaiters > 0;
	Result.ClearedWaiters = ClearedWaiters;

	if (Queue.ActiveRequestId == RequestId)
	{
		Queue.ActiveAcquiredSeconds = Now; // heartbeat while owner re-enters
		Result.bAcquired = true;
		Result.Reason = TEXT("request already owns the mutator slot");
		return Result;
	}

	UeremcpMutatorQueuePrivate::FWaiter* ExistingWaiter = Queue.Waiters.FindByPredicate(
		[&RequestId](const UeremcpMutatorQueuePrivate::FWaiter& Waiter)
		{
			return Waiter.RequestId == RequestId;
		});

	if (!ExistingWaiter)
	{
		UeremcpMutatorQueuePrivate::FWaiter& Waiter = Queue.Waiters.AddDefaulted_GetRef();
		Waiter.RequestId = RequestId;
		Waiter.JobId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		Waiter.LastSeenSeconds = Now;
		ExistingWaiter = &Waiter;
	}
	else
	{
		// Heartbeat: abandoned retries with new request ids must not block forever.
		ExistingWaiter->LastSeenSeconds = Now;
	}

	if (Queue.ActiveRequestId.IsEmpty()
		&& Queue.Waiters.Num() > 0
		&& Queue.Waiters[0].RequestId == RequestId)
	{
		Queue.ActiveRequestId = RequestId;
		Queue.ActiveAcquiredSeconds = Now;
		Queue.Waiters.RemoveAt(0);
		Result.bAcquired = true;
		Result.Reason = Result.bClearedStale
			? TEXT("mutator slot acquired after clearing stale FIFO entries")
			: TEXT("mutator slot acquired");
		return Result;
	}

	Result.bQueued = true;
	Result.JobId = ExistingWaiter->JobId;
	Result.Reason = FString::Printf(
		TEXT("another mutator owns the project (active=%s, pending=%d). "
			 "RETRY this same tool call with the SAME request_id within %.0fs — "
			 "do not poll get_job_result forever. Stale waiters auto-clear after %.0fs; "
			 "orphaned active slots after %.0fs."),
		Queue.ActiveRequestId.IsEmpty() ? TEXT("(none)") : *Queue.ActiveRequestId,
		Queue.Waiters.Num(),
		StaleWaiterSeconds,
		StaleWaiterSeconds,
		StaleActiveSeconds);
	return Result;
}

FUeremcpMutatorQueue::FAcquireResult FUeremcpMutatorQueue::TryAcquire(
	const FString& RequestId,
	EUeremcpPermissionTier Tier)
{
	return TryAcquire(FPaths::ProjectDir(), RequestId, Tier);
}

bool FUeremcpMutatorQueue::Release(const FString& ProjectKey, const FString& RequestId)
{
	const FString NormalizedProjectKey = UeremcpMutatorQueuePrivate::NormalizeProjectKey(ProjectKey);
	if (NormalizedProjectKey.IsEmpty() || RequestId.IsEmpty())
	{
		return false;
	}

	FScopeLock Lock(&UeremcpMutatorQueuePrivate::Mutex());
	UeremcpMutatorQueuePrivate::FProjectQueue* Queue =
		UeremcpMutatorQueuePrivate::Queues().Find(NormalizedProjectKey);
	if (!Queue || Queue->ActiveRequestId != RequestId)
	{
		return false;
	}

	Queue->ActiveRequestId.Reset();
	Queue->ActiveAcquiredSeconds = 0.0;
	if (Queue->Waiters.IsEmpty())
	{
		UeremcpMutatorQueuePrivate::Queues().Remove(NormalizedProjectKey);
	}
	return true;
}

void FUeremcpMutatorQueue::Release(const FString& RequestId)
{
	if (RequestId.IsEmpty())
	{
		return;
	}

	FScopeLock Lock(&UeremcpMutatorQueuePrivate::Mutex());
	for (auto It = UeremcpMutatorQueuePrivate::Queues().CreateIterator(); It; ++It)
	{
		UeremcpMutatorQueuePrivate::FProjectQueue& Queue = It.Value();
		if (Queue.ActiveRequestId == RequestId)
		{
			Queue.ActiveRequestId.Reset();
			Queue.ActiveAcquiredSeconds = 0.0;
			if (Queue.Waiters.IsEmpty())
			{
				It.RemoveCurrent();
			}
			return;
		}
	}
}

bool FUeremcpMutatorQueue::CancelQueued(const FString& ProjectKey, const FString& RequestId)
{
	const FString NormalizedProjectKey = UeremcpMutatorQueuePrivate::NormalizeProjectKey(ProjectKey);
	if (NormalizedProjectKey.IsEmpty() || RequestId.IsEmpty())
	{
		return false;
	}

	FScopeLock Lock(&UeremcpMutatorQueuePrivate::Mutex());
	UeremcpMutatorQueuePrivate::FProjectQueue* Queue =
		UeremcpMutatorQueuePrivate::Queues().Find(NormalizedProjectKey);
	if (!Queue)
	{
		return false;
	}

	const int32 RemovedCount = Queue->Waiters.RemoveAll(
		[&RequestId](const UeremcpMutatorQueuePrivate::FWaiter& Waiter)
		{
			return Waiter.RequestId == RequestId;
		});
	if (Queue->ActiveRequestId.IsEmpty() && Queue->Waiters.IsEmpty())
	{
		UeremcpMutatorQueuePrivate::Queues().Remove(NormalizedProjectKey);
	}
	return RemovedCount > 0;
}

int32 FUeremcpMutatorQueue::ClearStale(const FString& ProjectKey, bool& bClearedActive)
{
	bClearedActive = false;
	const FString NormalizedProjectKey = UeremcpMutatorQueuePrivate::NormalizeProjectKey(ProjectKey);
	if (NormalizedProjectKey.IsEmpty())
	{
		return 0;
	}

	FScopeLock Lock(&UeremcpMutatorQueuePrivate::Mutex());
	UeremcpMutatorQueuePrivate::FProjectQueue* Queue =
		UeremcpMutatorQueuePrivate::Queues().Find(NormalizedProjectKey);
	if (!Queue)
	{
		return 0;
	}

	int32 ClearedWaiters = 0;
	UeremcpMutatorQueuePrivate::ClearStaleLocked(
		*Queue, FPlatformTime::Seconds(), bClearedActive, ClearedWaiters);
	if (Queue->ActiveRequestId.IsEmpty() && Queue->Waiters.IsEmpty())
	{
		UeremcpMutatorQueuePrivate::Queues().Remove(NormalizedProjectKey);
	}
	return ClearedWaiters;
}

void FUeremcpMutatorQueue::ForceClear(const FString& ProjectKey)
{
	const FString NormalizedProjectKey = UeremcpMutatorQueuePrivate::NormalizeProjectKey(ProjectKey);
	if (NormalizedProjectKey.IsEmpty())
	{
		return;
	}
	FScopeLock Lock(&UeremcpMutatorQueuePrivate::Mutex());
	UeremcpMutatorQueuePrivate::Queues().Remove(NormalizedProjectKey);
}

bool FUeremcpMutatorQueue::IsActive(const FString& ProjectKey)
{
	const FString NormalizedProjectKey = UeremcpMutatorQueuePrivate::NormalizeProjectKey(ProjectKey);
	FScopeLock Lock(&UeremcpMutatorQueuePrivate::Mutex());
	const UeremcpMutatorQueuePrivate::FProjectQueue* Queue =
		UeremcpMutatorQueuePrivate::Queues().Find(NormalizedProjectKey);
	return Queue && !Queue->ActiveRequestId.IsEmpty();
}

bool FUeremcpMutatorQueue::IsActive()
{
	FScopeLock Lock(&UeremcpMutatorQueuePrivate::Mutex());
	for (const TPair<FString, UeremcpMutatorQueuePrivate::FProjectQueue>& Pair :
		UeremcpMutatorQueuePrivate::Queues())
	{
		if (!Pair.Value.ActiveRequestId.IsEmpty())
		{
			return true;
		}
	}
	return false;
}

int32 FUeremcpMutatorQueue::PendingCount(const FString& ProjectKey)
{
	const FString NormalizedProjectKey = UeremcpMutatorQueuePrivate::NormalizeProjectKey(ProjectKey);
	FScopeLock Lock(&UeremcpMutatorQueuePrivate::Mutex());
	const UeremcpMutatorQueuePrivate::FProjectQueue* Queue =
		UeremcpMutatorQueuePrivate::Queues().Find(NormalizedProjectKey);
	return Queue ? Queue->Waiters.Num() : 0;
}

FString FUeremcpMutatorQueue::ActiveRequestId(const FString& ProjectKey)
{
	const FString NormalizedProjectKey = UeremcpMutatorQueuePrivate::NormalizeProjectKey(ProjectKey);
	FScopeLock Lock(&UeremcpMutatorQueuePrivate::Mutex());
	const UeremcpMutatorQueuePrivate::FProjectQueue* Queue =
		UeremcpMutatorQueuePrivate::Queues().Find(NormalizedProjectKey);
	return Queue ? Queue->ActiveRequestId : FString();
}
