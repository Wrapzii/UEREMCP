#include "UeremcpJobActions.h"

#include "UeremcpEnvelope.h"
#include "UeremcpJobRegistry.h"

namespace
{
	bool ParseJobAction(
		const FString& RequestJson,
		const FString& ExpectedAction,
		FUeremcpRequest& OutRequest,
		FString& OutJobId,
		FString& OutError)
	{
		OutJobId.Reset();
		if (!FUeremcpEnvelope::ParseRequest(RequestJson, OutRequest, OutError))
		{
			return false;
		}
		if (!FUeremcpEnvelope::IsProtocolCompatible(OutRequest.ProtocolVersion))
		{
			OutError = FString::Printf(
				TEXT("unsupported protocol_version '%s'"), *OutRequest.ProtocolVersion);
			return false;
		}
		if (OutRequest.Action != ExpectedAction)
		{
			OutError = FString::Printf(
				TEXT("expected action '%s', received '%s'"),
				*ExpectedAction,
				*OutRequest.Action);
			return false;
		}
		if (!OutRequest.Specification.IsValid())
		{
			OutError = TEXT("specification.job_id is required");
			return false;
		}
		for (const auto& Pair : OutRequest.Specification->Values)
		{
			if (FString(Pair.Key) != TEXT("job_id"))
			{
				OutError = FString::Printf(
					TEXT("unknown job action specification field '%s'"),
					*FString(Pair.Key));
				return false;
			}
		}
		if (!OutRequest.Specification->TryGetStringField(TEXT("job_id"), OutJobId)
			|| OutJobId.IsEmpty())
		{
			OutError = TEXT("specification.job_id must be a non-empty string");
			return false;
		}
		return true;
	}

	FString Reject(const FUeremcpRequest& Request, const FString& Error)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Error);
	}

	FString SerializeCurrentJob(
		const FUeremcpRequest& Request,
		const FString& JobId,
		const FString& SummaryOverride,
		const FString& CapabilityNote)
	{
		FUeremcpResponse Response;
		FString Error;
		if (!FUeremcpJobRegistry::Get().GetJobResult(JobId, Response, Error))
		{
			return Reject(Request, Error);
		}
		Response.RequestId = Request.RequestId;
		if (!SummaryOverride.IsEmpty())
		{
			Response.Summary = SummaryOverride;
		}
		if (!CapabilityNote.IsEmpty())
		{
			Response.CapabilityNotes.Add(CapabilityNote);
		}
		return FUeremcpEnvelope::SerializeResponse(Response);
	}
}

FString FUeremcpJobActions::GetJobResult(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString JobId;
	FString Error;
	if (!ParseJobAction(
		RequestJson,
		TEXT("get_job_result"),
		Request,
		JobId,
		Error))
	{
		return Reject(Request, Error);
	}
	return SerializeCurrentJob(Request, JobId, FString(), FString());
}

FString FUeremcpJobActions::CancelJob(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString JobId;
	FString Error;
	if (!ParseJobAction(
		RequestJson,
		TEXT("cancel_job"),
		Request,
		JobId,
		Error))
	{
		return Reject(Request, Error);
	}

	switch (FUeremcpJobRegistry::Get().CancelJob(JobId, Error))
	{
	case EUeremcpCancelResult::Cancelled:
		return SerializeCurrentJob(
			Request,
			JobId,
			TEXT("Job cancelled cooperatively before validated completion."),
			FString());

	case EUeremcpCancelResult::AlreadyTerminal:
		return SerializeCurrentJob(
			Request,
			JobId,
			TEXT("Job was already terminal; returning its retained result."),
			FString());

	case EUeremcpCancelResult::CancellationPending:
		return SerializeCurrentJob(
			Request,
			JobId,
			TEXT("Cooperative cancellation is already pending."),
			TEXT("The domain cancellation checkpoint has not completed yet."));

	case EUeremcpCancelResult::RejectedByWorker:
		return SerializeCurrentJob(
			Request,
			JobId,
			TEXT("The domain worker rejected cooperative cancellation; work continues."),
			TEXT("Cancellation was requested but not honored by the domain worker."));

	case EUeremcpCancelResult::NotCancellable:
		return Reject(
			Request,
			TEXT("Job does not advertise cooperative cancellation."));

	case EUeremcpCancelResult::NotFound:
	default:
		return Reject(Request, Error.IsEmpty() ? TEXT("job not found or expired") : Error);
	}
}
