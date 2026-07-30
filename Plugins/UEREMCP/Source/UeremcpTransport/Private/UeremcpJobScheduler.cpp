#include "UeremcpJobScheduler.h"

#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Misc/ScopeLock.h"
#include "UeremcpJobConstraints.h"
#include "UeremcpJobRegistry.h"

namespace
{
	struct FUeremcpInitialResponseState
	{
		FCriticalSection Mutex;
		bool bDelivered = false;
		FUeremcpScheduledResponse Callback;

		bool TryDeliver(const FUeremcpResponse& Response)
		{
			FUeremcpScheduledResponse CallbackToInvoke;
			{
				FScopeLock Lock(&Mutex);
				if (bDelivered)
				{
					return false;
				}
				bDelivered = true;
				CallbackToInvoke = MoveTemp(Callback);
			}
			CallbackToInvoke(Response);
			return true;
		}
	};

	static FUeremcpResponse MakeDispatchFailure(
		const FString& RequestId,
		const FString& Summary)
	{
		FUeremcpResponse Response;
		Response.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
		Response.RequestId = RequestId;
		Response.Status = TEXT("error");
		Response.Summary = Summary;
		Response.Metrics.McpRoundTrips = 1;
		return Response;
	}
}

bool FUeremcpJobScheduler::Dispatch(
	const FString& RequestId,
	const int32 TimeoutMs,
	const bool bCancellable,
	const FString& InitialProgressMessage,
	FUeremcpScheduledJobWork&& Work,
	FUeremcpScheduledResponse&& Callback,
	FString& OutJobId,
	FString& OutError)
{
	OutJobId.Reset();
	OutError.Reset();
	if (RequestId.IsEmpty())
	{
		OutError = TEXT("scheduler request_id is required");
		return false;
	}
	if (TimeoutMs < 0)
	{
		OutError = TEXT("scheduler timeout_ms must be >= 0");
		return false;
	}
	if (!Work)
	{
		OutError = TEXT("scheduler work callback is required");
		return false;
	}
	if (!Callback)
	{
		OutError = TEXT("scheduler response callback is required");
		return false;
	}

	const TSharedRef<FThreadSafeBool, ESPMode::ThreadSafe> CancelRequested =
		MakeShared<FThreadSafeBool, ESPMode::ThreadSafe>(false);
	const TSharedRef<FUeremcpInitialResponseState, ESPMode::ThreadSafe> ResponseState =
		MakeShared<FUeremcpInitialResponseState, ESPMode::ThreadSafe>();
	ResponseState->Callback = MoveTemp(Callback);

	if (TimeoutMs == 0)
	{
		Async(EAsyncExecution::ThreadPool,
			[RequestId, Work = MoveTemp(Work), CancelRequested, ResponseState]() mutable
			{
				FUeremcpResponse Response = Work(FString(), CancelRequested);
				if (Response.RequestId.IsEmpty())
				{
					Response.RequestId = RequestId;
				}
				ResponseState->TryDeliver(Response);
			});
		return true;
	}

	FUeremcpJobRegistry& Registry = FUeremcpJobRegistry::Get();
	TFunction<bool()> RequestCancel;
	if (bCancellable)
	{
		RequestCancel = [CancelRequested]()
		{
			*CancelRequested = true;
			return true;
		};
	}

	if (!Registry.CreateJob(
		RequestId,
		bCancellable,
		InitialProgressMessage,
		OutJobId,
		OutError,
		MoveTemp(RequestCancel)))
	{
		return false;
	}
	if (!Registry.StartJob(OutJobId, OutError))
	{
		Registry.FailJob(OutJobId, TEXT("Failed to start scheduled job."), OutError);
		return false;
	}

	const FString JobId = OutJobId;
	Async(EAsyncExecution::ThreadPool,
		[RequestId, JobId, Work = MoveTemp(Work), CancelRequested, ResponseState]() mutable
		{
			FUeremcpResponse TerminalResponse = Work(JobId, CancelRequested);
			if (TerminalResponse.RequestId.IsEmpty())
			{
				TerminalResponse.RequestId = RequestId;
			}

			FUeremcpJobRegistry& WorkerRegistry = FUeremcpJobRegistry::Get();
			FString CompletionError;
			if (!WorkerRegistry.CompleteJob(JobId, TerminalResponse, CompletionError))
			{
				FUeremcpJobSnapshot Snapshot;
				if (!WorkerRegistry.GetSnapshot(JobId, Snapshot)
					|| !UeremcpTransport::IsTerminalJobState(Snapshot.Job.State))
				{
					FString FailureError;
					WorkerRegistry.FailJob(
						JobId,
						FString::Printf(
							TEXT("Scheduled work returned an invalid terminal response: %s"),
							*CompletionError),
						FailureError);
				}
			}

			FUeremcpResponse CurrentResponse;
			FString ReadError;
			if (WorkerRegistry.GetTimeoutResponse(JobId, CurrentResponse, ReadError))
			{
				ResponseState->TryDeliver(CurrentResponse);
			}
			else
			{
				ResponseState->TryDeliver(MakeDispatchFailure(
					RequestId,
					FString::Printf(
						TEXT("Scheduled work finished but its job result could not be read: %s"),
						*ReadError)));
			}
		});

	FTSTicker::GetCoreTicker().AddTicker(
		TEXT("UEREMCP.JobTimeout"),
		static_cast<float>(TimeoutMs) / 1000.0f,
		[RequestId, JobId, ResponseState](float)
		{
			FUeremcpResponse TimeoutResponse;
			FString TimeoutError;
			if (FUeremcpJobRegistry::Get().GetTimeoutResponse(JobId, TimeoutResponse, TimeoutError))
			{
				ResponseState->TryDeliver(TimeoutResponse);
			}
			else
			{
				ResponseState->TryDeliver(MakeDispatchFailure(
					RequestId,
					FString::Printf(
						TEXT("Scheduled job timeout response could not be read: %s"),
						*TimeoutError)));
			}
			return false;
		});

	return true;
}
