#include "UeremcpJob.h"

#include "Misc/Guid.h"

const TCHAR* FUeremcpJobDefaults::PollAction()
{
	return TEXT("get_job_result");
}

bool FUeremcpJob::IsValidState(const FString& InState)
{
	static const TArray<FString> States = {
		TEXT("queued"), TEXT("running"), TEXT("completed"), TEXT("failed"), TEXT("cancelled")
	};
	return States.Contains(InState);
}

bool FUeremcpJob::IsValid(FString& OutError) const
{
	OutError.Reset();
	if (JobId.IsEmpty())
	{
		OutError = TEXT("job.job_id is required");
		return false;
	}
	if (!IsValidState(State))
	{
		OutError = FString::Printf(TEXT("invalid job.state '%s'"), *State);
		return false;
	}
	if (bHasProgress && (Progress < 0.0 || Progress > 1.0))
	{
		OutError = TEXT("job.progress must be in [0, 1]");
		return false;
	}
	return true;
}

bool FUeremcpJobUtil::ShouldDispatchInline(int32 TimeoutMs)
{
	return TimeoutMs == 0;
}

int32 FUeremcpJobUtil::NormaliseTimeoutMs(int32 TimeoutMs)
{
	if (TimeoutMs <= 0)
	{
		return 0;
	}
	if (TimeoutMs < FUeremcpJobDefaults::MinTimeoutMs)
	{
		return FUeremcpJobDefaults::MinTimeoutMs;
	}
	if (TimeoutMs > FUeremcpJobDefaults::MaxTimeoutMs)
	{
		return FUeremcpJobDefaults::MaxTimeoutMs;
	}
	return TimeoutMs;
}

FString FUeremcpJobUtil::NewJobId()
{
	return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
}
