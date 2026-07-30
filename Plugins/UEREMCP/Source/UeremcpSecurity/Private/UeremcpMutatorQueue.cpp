#include "UeremcpMutatorQueue.h"

#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

namespace UeremcpMutatorQueuePrivate
{
	struct FWaiter
	{
		FString RequestId;
		FString JobId;
	};

	struct FProjectQueue
	{
		FString ActiveRequestId;
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

	if (Queue.ActiveRequestId == RequestId)
	{
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
		ExistingWaiter = &Waiter;
	}

	if (Queue.ActiveRequestId.IsEmpty()
		&& Queue.Waiters.Num() > 0
		&& Queue.Waiters[0].RequestId == RequestId)
	{
		Queue.ActiveRequestId = RequestId;
		Queue.Waiters.RemoveAt(0);
		Result.bAcquired = true;
		Result.Reason = TEXT("mutator slot acquired");
		return Result;
	}

	Result.bQueued = true;
	Result.JobId = ExistingWaiter->JobId;
	Result.Reason = TEXT("another mutator owns the project; request queued FIFO");
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
