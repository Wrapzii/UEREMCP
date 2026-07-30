#include "UeremcpPlanActions.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpEnvelope.h"
#include "UeremcpIdempotency.h"
#include "UeremcpJob.h"
#include "UeremcpJobRegistry.h"
#include "UeremcpPlanExecutor.h"

namespace
{
	TFunction<bool()> GForceTimeoutProbe;

	bool ParseObject(const FString& Json, TSharedPtr<FJsonObject>& OutObject)
	{
		OutObject.Reset();
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	FString Reject(const FUeremcpRequest& Request, const FString& Error)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, Error);
	}

	bool IsInFlightPartial(const TSharedPtr<FJsonObject>& Response)
	{
		if (!Response.IsValid())
		{
			return false;
		}
		if (Response->GetStringField(TEXT("status")) != TEXT("partially_completed"))
		{
			return false;
		}
		const TSharedPtr<FJsonObject>* Job = nullptr;
		if (!Response->TryGetObjectField(TEXT("job"), Job) || !Job || !(*Job).IsValid())
		{
			return false;
		}
		const FString State = (*Job)->GetStringField(TEXT("state"));
		return State == TEXT("running") || State == TEXT("queued");
	}

	void StoreIdempotencyIfNeeded(const FUeremcpRequest& Request, const FString& ResponseJson)
	{
		if (Request.IdempotencyKey.IsEmpty() || ResponseJson.IsEmpty())
		{
			return;
		}
		TSharedPtr<FJsonObject> Response;
		if (!ParseObject(ResponseJson, Response) || IsInFlightPartial(Response))
		{
			return;
		}
		FUeremcpIdempotencyStore::Get().Put(Request.IdempotencyKey, ResponseJson);
	}

	bool ExtractStatusSummary(
		const FString& ResponseJson,
		FString& OutStatus,
		FString& OutSummary,
		int32& OutInternalOperations)
	{
		OutStatus.Reset();
		OutSummary.Reset();
		OutInternalOperations = 0;
		TSharedPtr<FJsonObject> Response;
		if (!ParseObject(ResponseJson, Response))
		{
			return false;
		}
		OutStatus = Response->GetStringField(TEXT("status"));
		OutSummary = Response->GetStringField(TEXT("summary"));
		const TSharedPtr<FJsonObject>* Metrics = nullptr;
		if (Response->TryGetObjectField(TEXT("metrics"), Metrics) && Metrics && (*Metrics).IsValid())
		{
			OutInternalOperations = static_cast<int32>(
				(*Metrics)->GetNumberField(TEXT("internal_operations")));
		}
		return !OutStatus.IsEmpty();
	}

	FString RunInline(const FUeremcpRequest& Request, const FString& RequestJson)
	{
		FString ResponseJson;
		FString Error;
		const bool bOk = FUeremcpPlanExecutor::ExecuteRequest(RequestJson, ResponseJson, Error);
		if (!ResponseJson.IsEmpty())
		{
			StoreIdempotencyIfNeeded(Request, ResponseJson);
			return ResponseJson;
		}
		if (!bOk)
		{
			return Reject(
				Request,
				Error.IsEmpty() ? TEXT("execute_plan failed without a response envelope") : Error);
		}
		return Reject(Request, TEXT("execute_plan returned an empty response"));
	}

	FString RunWithTimeout(const FUeremcpRequest& Request, const FString& RequestJson)
	{
		FUeremcpJobRegistry& Registry = FUeremcpJobRegistry::Get();
		FString Error;
		FString JobId;
		if (!Registry.CreateJob(
			Request.RequestId,
			false,
			TEXT("Executing execute_plan"),
			JobId,
			Error))
		{
			return Reject(Request, Error);
		}
		if (!Registry.StartJob(JobId, Error))
		{
			return Reject(Request, Error);
		}

		if (GForceTimeoutProbe && GForceTimeoutProbe())
		{
			FUeremcpResponse Timeout;
			if (!Registry.GetTimeoutResponse(JobId, Timeout, Error))
			{
				return Reject(Request, Error);
			}
			Timeout.RequestId = Request.RequestId;
			// In-flight initiating response — do not store under idempotency_key.
			return FUeremcpEnvelope::SerializeResponse(Timeout);
		}

		// Protocol adapter has no production async scheduler (Core/Transport owns
		// that). When the force-timeout probe is unset, run the plan synchronously
		// and treat completion as "finished within timeout_ms".
		FString ResponseJson;
		FUeremcpPlanExecutor::ExecuteRequest(RequestJson, ResponseJson, Error);
		if (ResponseJson.IsEmpty())
		{
			Registry.FailJob(
				JobId,
				Error.IsEmpty() ? TEXT("execute_plan failed without a response envelope") : Error,
				Error);
			return Reject(
				Request,
				Error.IsEmpty() ? TEXT("execute_plan failed without a response envelope") : Error);
		}

		FString Status;
		FString Summary;
		int32 InternalOperations = 0;
		if (!ExtractStatusSummary(ResponseJson, Status, Summary, InternalOperations))
		{
			Registry.FailJob(JobId, TEXT("execute_plan returned invalid JSON"), Error);
			return Reject(Request, TEXT("execute_plan returned invalid JSON"));
		}

		FUeremcpResponse Terminal;
		Terminal.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
		Terminal.RequestId = Request.RequestId;
		Terminal.Status = Status;
		Terminal.Summary = Summary.IsEmpty()
			? TEXT("execute_plan finished within timeout_ms.")
			: Summary;
		Terminal.UnderstoodAction = TEXT("execute_plan");
		Terminal.Metrics.McpRoundTrips = 1;
		Terminal.Metrics.InternalOperations = InternalOperations;

		// Retain any terminal plan status (including plan-level partially_completed
		// from continue_independent, and rolled_back). job.state distinguishes
		// in-flight vs terminal; response status alone does not.
		if (Status == TEXT("error") || Status == TEXT("rejected"))
		{
			Registry.FailJob(JobId, Terminal.Summary, Error);
		}
		else if (!Registry.CompleteJob(JobId, Terminal, Error))
		{
			Registry.FailJob(
				JobId,
				FString::Printf(
					TEXT("execute_plan finished but job retention failed: %s"), *Error),
				Error);
		}

		StoreIdempotencyIfNeeded(Request, ResponseJson);
		// Return the full consolidated plan envelope from the initiating call.
		// Job retention exists so a later get_job_result poll remains honest.
		return ResponseJson;
	}
}

void FUeremcpPlanActions::SetForceTimeoutForTests(TFunction<bool()>&& Probe)
{
	GForceTimeoutProbe = MoveTemp(Probe);
}

void FUeremcpPlanActions::ClearForceTimeoutForTests()
{
	GForceTimeoutProbe.Reset();
}

FString FUeremcpPlanActions::ExecutePlan(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Error;
	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, Error))
	{
		return FUeremcpEnvelope::MakeRejection(FString(), Error);
	}
	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return Reject(
			Request,
			FString::Printf(
				TEXT("unsupported protocol_version '%s'"), *Request.ProtocolVersion));
	}
	if (Request.Action != TEXT("execute_plan"))
	{
		return Reject(
			Request,
			FString::Printf(
				TEXT("expected action 'execute_plan', received '%s'"), *Request.Action));
	}
	if (!Request.Specification.IsValid())
	{
		return Reject(Request, TEXT("specification.operations is required"));
	}

	if (!Request.IdempotencyKey.IsEmpty())
	{
		FString Replay;
		if (FUeremcpIdempotencyStore::Get().TryGetReplay(
			Request.IdempotencyKey,
			Request.RequestId,
			Replay))
		{
			return Replay;
		}
	}

	if (FUeremcpJobUtil::ShouldDispatchInline(Request.TimeoutMs))
	{
		return RunInline(Request, RequestJson);
	}
	return RunWithTimeout(Request, RequestJson);
}
