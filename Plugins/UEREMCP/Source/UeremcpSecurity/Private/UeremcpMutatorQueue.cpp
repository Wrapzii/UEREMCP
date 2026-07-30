#include "UeremcpMutatorQueue.h"

bool FUeremcpMutatorQueue::IsImplemented()
{
	return false;
}

FUeremcpMutatorQueue::FAcquireResult FUeremcpMutatorQueue::TryAcquire(
	const FString& RequestId,
	EUeremcpPermissionTier Tier)
{
	(void)RequestId;
	(void)Tier;

	FAcquireResult Result;
	Result.bAcquired = false;
	Result.Reason = TEXT("FUeremcpMutatorQueue is not implemented yet (ADR-0010 Wave 2 stub)");
	return Result;
}

void FUeremcpMutatorQueue::Release(const FString& RequestId)
{
	(void)RequestId;
}

bool FUeremcpMutatorQueue::IsActive()
{
	return false;
}
