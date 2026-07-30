#include "UeremcpJobRegistry.h"

#include "Misc/ScopeLock.h"

bool FUeremcpJobRegistryConfig::IsValid(FString& OutError) const
{
	OutError.Reset();
	if (MaxJobs <= 0)
	{
		OutError = TEXT("job registry MaxJobs must be > 0");
		return false;
	}
	if (TerminalRetentionMs < 0)
	{
		OutError = TEXT("job registry TerminalRetentionMs must be >= 0");
		return false;
	}
	if (MaxActiveAgeMs <= 0)
	{
		OutError = TEXT("job registry MaxActiveAgeMs must be > 0");
		return false;
	}
	return true;
}

FUeremcpJobRegistry::FUeremcpJobRegistry(const FUeremcpJobRegistryConfig& InConfig)
	: Config(InConfig)
{
	FString Error;
	if (!Config.IsValid(Error))
	{
		Config = FUeremcpJobRegistryConfig();
	}
}

FUeremcpJobRegistry& FUeremcpJobRegistry::Get()
{
	static FUeremcpJobRegistry Registry;
	return Registry;
}

FString FUeremcpJobRegistry::StateToString(EUeremcpJobState State)
{
	switch (State)
	{
	case EUeremcpJobState::Queued:
		return TEXT("queued");
	case EUeremcpJobState::Running:
		return TEXT("running");
	case EUeremcpJobState::Completed:
		return TEXT("completed");
	case EUeremcpJobState::Failed:
		return TEXT("failed");
	case EUeremcpJobState::Cancelled:
		return TEXT("cancelled");
	default:
		return TEXT("failed");
	}
}

bool FUeremcpJobRegistry::IsTerminal(EUeremcpJobState State)
{
	return State == EUeremcpJobState::Completed
		|| State == EUeremcpJobState::Failed
		|| State == EUeremcpJobState::Cancelled;
}

bool FUeremcpJobRegistry::IsValidTransition(EUeremcpJobState From, EUeremcpJobState To)
{
	if (From == To)
	{
		return true;
	}
	if (From == EUeremcpJobState::Queued)
	{
		return To == EUeremcpJobState::Running
			|| To == EUeremcpJobState::Failed
			|| To == EUeremcpJobState::Cancelled;
	}
	if (From == EUeremcpJobState::Running)
	{
		return To == EUeremcpJobState::Completed
			|| To == EUeremcpJobState::Failed
			|| To == EUeremcpJobState::Cancelled;
	}
	return false;
}

bool FUeremcpJobRegistry::TransitionLocked(
	FEntry& Entry,
	EUeremcpJobState To,
	const FDateTime& Now,
	FString& OutError)
{
	if (!IsValidTransition(Entry.State, To))
	{
		OutError = FString::Printf(
			TEXT("invalid job transition %s -> %s"),
			*StateToString(Entry.State),
			*StateToString(To));
		return false;
	}

	Entry.State = To;
	Entry.UpdatedAt = Now;
	if (IsTerminal(To) && !Entry.bHasTerminalAt)
	{
		Entry.TerminalAt = Now;
		Entry.bHasTerminalAt = true;
		Entry.bCancellable = false;
		Entry.bCancellationPending = false;
		Entry.RequestCancel.Reset();
	}
	OutError.Reset();
	return true;
}

bool FUeremcpJobRegistry::EnsureCapacityLocked(const FDateTime& Now, FString& OutError)
{
	CleanupExpiredLocked(Now);
	if (Entries.Num() < Config.MaxJobs)
	{
		return true;
	}

	FString OldestTerminalId;
	FDateTime OldestTerminalAt = FDateTime::MaxValue();
	for (const TPair<FString, FEntry>& Pair : Entries)
	{
		if (Pair.Value.bHasTerminalAt && Pair.Value.TerminalAt < OldestTerminalAt)
		{
			OldestTerminalId = Pair.Key;
			OldestTerminalAt = Pair.Value.TerminalAt;
		}
	}
	if (!OldestTerminalId.IsEmpty())
	{
		Entries.Remove(OldestTerminalId);
		return true;
	}

	OutError = FString::Printf(
		TEXT("job registry capacity %d reached; all retained jobs are active"),
		Config.MaxJobs);
	return false;
}

bool FUeremcpJobRegistry::CreateJob(
	const FString& RequestId,
	bool bCancellable,
	const FString& InitialProgressMessage,
	FString& OutJobId,
	FString& OutError,
	TFunction<bool()>&& RequestCancel)
{
	OutJobId.Reset();
	OutError.Reset();
	const FDateTime Now = FDateTime::UtcNow();
	FScopeLock Lock(&Mutex);

	if (!EnsureCapacityLocked(Now, OutError))
	{
		return false;
	}

	FEntry Entry;
	Entry.RequestId = RequestId;
	Entry.ProgressMessage = InitialProgressMessage;
	Entry.CreatedAt = Now;
	Entry.UpdatedAt = Now;
	if (bCancellable && RequestCancel)
	{
		Entry.bCancellable = true;
		Entry.RequestCancel = MakeShared<TFunction<bool()>, ESPMode::ThreadSafe>(MoveTemp(RequestCancel));
	}

	do
	{
		OutJobId = FUeremcpJobUtil::NewJobId();
	}
	while (Entries.Contains(OutJobId));

	Entries.Add(OutJobId, MoveTemp(Entry));
	return true;
}

bool FUeremcpJobRegistry::StartJob(const FString& JobId, FString& OutError)
{
	FScopeLock Lock(&Mutex);
	FEntry* Entry = Entries.Find(JobId);
	if (!Entry)
	{
		OutError = TEXT("job not found");
		return false;
	}
	return TransitionLocked(*Entry, EUeremcpJobState::Running, FDateTime::UtcNow(), OutError);
}

bool FUeremcpJobRegistry::UpdateProgress(
	const FString& JobId,
	double Progress,
	const FString& ProgressMessage,
	FString& OutError)
{
	if (Progress < 0.0 || Progress > 1.0)
	{
		OutError = TEXT("job progress must be in [0, 1]");
		return false;
	}

	FScopeLock Lock(&Mutex);
	FEntry* Entry = Entries.Find(JobId);
	if (!Entry)
	{
		OutError = TEXT("job not found");
		return false;
	}
	if (IsTerminal(Entry->State))
	{
		OutError = TEXT("terminal job progress cannot change");
		return false;
	}
	if (Entry->bHasProgress && Progress < Entry->Progress)
	{
		OutError = TEXT("job progress cannot move backwards");
		return false;
	}

	Entry->Progress = Progress;
	Entry->bHasProgress = true;
	Entry->ProgressMessage = ProgressMessage;
	Entry->UpdatedAt = FDateTime::UtcNow();
	OutError.Reset();
	return true;
}

bool FUeremcpJobRegistry::CompleteJob(
	const FString& JobId,
	const FUeremcpResponse& TerminalResponse,
	FString& OutError)
{
	if (TerminalResponse.Status == TEXT("partially_completed"))
	{
		OutError = TEXT("terminal completion cannot use status partially_completed");
		return false;
	}
	if (!FUeremcpEnvelope::ValidateResponse(TerminalResponse, OutError))
	{
		return false;
	}

	FScopeLock Lock(&Mutex);
	FEntry* Entry = Entries.Find(JobId);
	if (!Entry)
	{
		OutError = TEXT("job not found");
		return false;
	}
	if (!TransitionLocked(*Entry, EUeremcpJobState::Completed, FDateTime::UtcNow(), OutError))
	{
		return false;
	}

	Entry->Progress = 1.0;
	Entry->bHasProgress = true;
	Entry->TerminalResponse = TerminalResponse;
	Entry->bHasTerminalResponse = true;
	return true;
}

void FUeremcpJobRegistry::AttachJobLocked(
	const FString& JobId,
	const FEntry& Entry,
	FUeremcpResponse& Response) const
{
	Response.bHasJob = true;
	Response.Job.JobId = JobId;
	Response.Job.State = StateToString(Entry.State);
	Response.Job.Progress = Entry.Progress;
	Response.Job.bHasProgress = Entry.bHasProgress;
	Response.Job.ProgressMessage = Entry.ProgressMessage;
	Response.Job.bCancellable = Entry.bCancellable;
	Response.Job.bHasCancellable = true;
	Response.Job.PollAction = FUeremcpJobDefaults::PollAction();
	Response.Metrics.McpRoundTrips = FMath::Max(
		Response.Metrics.McpRoundTrips,
		1 + Entry.PollCount);
}

FUeremcpResponse FUeremcpJobRegistry::MakeInFlightResponseLocked(
	const FString& JobId,
	const FEntry& Entry) const
{
	FUeremcpResponse Response;
	Response.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	Response.RequestId = Entry.RequestId;
	Response.Status = TEXT("partially_completed");
	Response.Summary = Entry.ProgressMessage.IsEmpty()
		? TEXT("Job is still in progress. Poll get_job_result.")
		: Entry.ProgressMessage;
	AttachJobLocked(JobId, Entry, Response);
	return Response;
}

void FUeremcpJobRegistry::SetFailedLocked(
	const FString& JobId,
	FEntry& Entry,
	const FString& Summary,
	const FDateTime& Now)
{
	FString Ignored;
	if (!TransitionLocked(Entry, EUeremcpJobState::Failed, Now, Ignored))
	{
		return;
	}

	Entry.TerminalResponse = FUeremcpResponse();
	Entry.TerminalResponse.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	Entry.TerminalResponse.RequestId = Entry.RequestId;
	Entry.TerminalResponse.Status = TEXT("error");
	Entry.TerminalResponse.Summary = Summary;
	Entry.TerminalResponse.Metrics.InternalOperations = 0;
	Entry.bHasTerminalResponse = true;
	AttachJobLocked(JobId, Entry, Entry.TerminalResponse);
}

bool FUeremcpJobRegistry::FailJob(
	const FString& JobId,
	const FString& Summary,
	FString& OutError)
{
	if (Summary.IsEmpty())
	{
		OutError = TEXT("failure summary is required");
		return false;
	}

	FScopeLock Lock(&Mutex);
	FEntry* Entry = Entries.Find(JobId);
	if (!Entry)
	{
		OutError = TEXT("job not found");
		return false;
	}
	if (IsTerminal(Entry->State))
	{
		OutError = TEXT("job is already terminal");
		return false;
	}
	SetFailedLocked(JobId, *Entry, Summary, FDateTime::UtcNow());
	OutError.Reset();
	return true;
}

EUeremcpCancelResult FUeremcpJobRegistry::CancelJob(
	const FString& JobId,
	FString& OutError)
{
	TSharedPtr<TFunction<bool()>, ESPMode::ThreadSafe> CancelCallback;
	{
		FScopeLock Lock(&Mutex);
		FEntry* Entry = Entries.Find(JobId);
		if (!Entry)
		{
			OutError = TEXT("job not found");
			return EUeremcpCancelResult::NotFound;
		}
		if (IsTerminal(Entry->State))
		{
			OutError = TEXT("job is already terminal");
			return EUeremcpCancelResult::AlreadyTerminal;
		}
		if (!Entry->bCancellable || !Entry->RequestCancel.IsValid())
		{
			OutError = TEXT("job does not advertise cooperative cancellation");
			return EUeremcpCancelResult::NotCancellable;
		}
		if (Entry->bCancellationPending)
		{
			OutError = TEXT("cooperative cancellation is already pending");
			return EUeremcpCancelResult::CancellationPending;
		}
		Entry->bCancellationPending = true;
		CancelCallback = Entry->RequestCancel;
	}

	if (!(*CancelCallback)())
	{
		FScopeLock Lock(&Mutex);
		if (FEntry* Entry = Entries.Find(JobId))
		{
			if (!IsTerminal(Entry->State))
			{
				Entry->bCancellationPending = false;
			}
		}
		OutError = TEXT("domain worker rejected cancellation");
		return EUeremcpCancelResult::RejectedByWorker;
	}

	FScopeLock Lock(&Mutex);
	FEntry* Entry = Entries.Find(JobId);
	if (!Entry)
	{
		OutError = TEXT("job expired while cancellation was requested");
		return EUeremcpCancelResult::NotFound;
	}
	if (IsTerminal(Entry->State))
	{
		OutError = TEXT("job completed while cancellation was requested");
		return EUeremcpCancelResult::AlreadyTerminal;
	}
	if (!TransitionLocked(*Entry, EUeremcpJobState::Cancelled, FDateTime::UtcNow(), OutError))
	{
		return EUeremcpCancelResult::AlreadyTerminal;
	}

	Entry->TerminalResponse = FUeremcpResponse();
	Entry->TerminalResponse.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	Entry->TerminalResponse.RequestId = Entry->RequestId;
	Entry->TerminalResponse.Status = TEXT("partially_completed");
	Entry->TerminalResponse.Summary =
		TEXT("Job cancelled cooperatively before validated completion.");
	Entry->TerminalResponse.Metrics.InternalOperations = 0;
	Entry->bHasTerminalResponse = true;
	AttachJobLocked(JobId, *Entry, Entry->TerminalResponse);
	OutError.Reset();
	return EUeremcpCancelResult::Cancelled;
}

bool FUeremcpJobRegistry::GetTimeoutResponse(
	const FString& JobId,
	FUeremcpResponse& OutResponse,
	FString& OutError)
{
	FScopeLock Lock(&Mutex);
	const FEntry* Entry = Entries.Find(JobId);
	if (!Entry)
	{
		OutError = TEXT("job not found or expired");
		return false;
	}

	if (Entry->bHasTerminalResponse)
	{
		OutResponse = Entry->TerminalResponse;
		AttachJobLocked(JobId, *Entry, OutResponse);
	}
	else
	{
		OutResponse = MakeInFlightResponseLocked(JobId, *Entry);
	}
	OutError.Reset();
	return true;
}

bool FUeremcpJobRegistry::GetJobResult(
	const FString& JobId,
	FUeremcpResponse& OutResponse,
	FString& OutError)
{
	FScopeLock Lock(&Mutex);
	FEntry* Entry = Entries.Find(JobId);
	if (!Entry)
	{
		OutError = TEXT("job not found or expired");
		return false;
	}

	++Entry->PollCount;
	Entry->UpdatedAt = FDateTime::UtcNow();
	if (Entry->bHasTerminalResponse)
	{
		OutResponse = Entry->TerminalResponse;
		AttachJobLocked(JobId, *Entry, OutResponse);
	}
	else
	{
		OutResponse = MakeInFlightResponseLocked(JobId, *Entry);
	}
	OutError.Reset();
	return true;
}

bool FUeremcpJobRegistry::GetSnapshot(
	const FString& JobId,
	FUeremcpJobSnapshot& OutSnapshot) const
{
	FScopeLock Lock(&Mutex);
	const FEntry* Entry = Entries.Find(JobId);
	if (!Entry)
	{
		return false;
	}

	OutSnapshot.RequestId = Entry->RequestId;
	OutSnapshot.Job.JobId = JobId;
	OutSnapshot.Job.State = StateToString(Entry->State);
	OutSnapshot.Job.Progress = Entry->Progress;
	OutSnapshot.Job.bHasProgress = Entry->bHasProgress;
	OutSnapshot.Job.ProgressMessage = Entry->ProgressMessage;
	OutSnapshot.Job.bCancellable = Entry->bCancellable;
	OutSnapshot.Job.bHasCancellable = true;
	OutSnapshot.Job.PollAction = FUeremcpJobDefaults::PollAction();
	OutSnapshot.PollCount = Entry->PollCount;
	OutSnapshot.CreatedAt = Entry->CreatedAt;
	OutSnapshot.UpdatedAt = Entry->UpdatedAt;
	OutSnapshot.TerminalAt = Entry->TerminalAt;
	OutSnapshot.bHasTerminalAt = Entry->bHasTerminalAt;
	return true;
}

int32 FUeremcpJobRegistry::CleanupExpiredLocked(const FDateTime& Now)
{
	int32 Changed = 0;
	TArray<FString> RemoveIds;
	for (TPair<FString, FEntry>& Pair : Entries)
	{
		FEntry& Entry = Pair.Value;
		if (Entry.bHasTerminalAt)
		{
			if ((Now - Entry.TerminalAt).GetTotalMilliseconds() >= Config.TerminalRetentionMs)
			{
				RemoveIds.Add(Pair.Key);
			}
			continue;
		}

		if ((Now - Entry.CreatedAt).GetTotalMilliseconds() >= Config.MaxActiveAgeMs)
		{
			SetFailedLocked(
				Pair.Key,
				Entry,
				TEXT("Job expired after exceeding the maximum active lifetime."),
				Now);
			++Changed;
		}
	}

	for (const FString& JobId : RemoveIds)
	{
		Entries.Remove(JobId);
		++Changed;
	}
	return Changed;
}

int32 FUeremcpJobRegistry::CleanupExpired()
{
	return CleanupExpiredAt(FDateTime::UtcNow());
}

int32 FUeremcpJobRegistry::CleanupExpiredAt(const FDateTime& Now)
{
	FScopeLock Lock(&Mutex);
	return CleanupExpiredLocked(Now);
}

int32 FUeremcpJobRegistry::Num() const
{
	FScopeLock Lock(&Mutex);
	return Entries.Num();
}

void FUeremcpJobRegistry::Clear()
{
	FScopeLock Lock(&Mutex);
	Entries.Reset();
}
