// WS-04 transport automation tests (ADR-0009 constraints + drift guard).

#include "UeremcpJobConstraints.h"
#include "UeremcpTransportProbe.h"
#include "UeremcpEnvelope.h"
#include "UeremcpJobRegistry.h"
#include "UeremcpReferenceToolset.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/Event.h"
#include "HAL/PooledSyncEvent.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpTransportTest
{
	static FUeremcpResponse MakeTerminalResponse(
		const FString& RequestId,
		const FString& Summary,
		const int32 InternalOperations)
	{
		FUeremcpResponse Response;
		Response.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
		Response.RequestId = RequestId;
		Response.Status = TEXT("created_and_validated");
		Response.Summary = Summary;
		Response.Metrics.McpRoundTrips = 1;
		Response.Metrics.InternalOperations = InternalOperations;
		return Response;
	}

	static FString MakeJobActionRequest(
		const FString& RequestId,
		const FString& Action,
		const FString& JobId)
	{
		return FString::Printf(
			TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"%s\","
				"\"action\":\"%s\",\"specification\":{\"job_id\":\"%s\"}}"),
			*RequestId,
			*Action,
			*JobId);
	}

	static TSharedPtr<FJsonObject> ParseResponseJson(const FString& Json)
	{
		TSharedPtr<FJsonObject> Response;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Response);
		return Response;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTransportHandoffDriftTest,
	"UEREMCP.Transport.Handoff.DriftGuard",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpTransportHandoffDriftTest::RunTest(const FString& Parameters)
{
	FUeremcpHandoffConstraints Handoff;
	FString Error;
	const FString Path = UeremcpTransport::ResolveHandoffJsonPath();
	TestFalse(TEXT("handoff JSON path resolves"), Path.IsEmpty());
	if (Path.IsEmpty())
	{
		return false;
	}

	TestTrue(TEXT("LoadHandoffConstraints succeeds"), UeremcpTransport::LoadHandoffConstraints(Handoff, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
	}
	if (!Error.IsEmpty())
	{
		return false;
	}

	TestTrue(
		TEXT("handoff matches runtime capability flags"),
		UeremcpTransport::HandoffMatchesRuntimeCapabilities(Handoff, Error));
	if (!Error.IsEmpty())
	{
		AddError(Error);
		return false;
	}

	const FString RuntimeJson = UeremcpTransport::CapabilityFlagsToJson(
		UeremcpTransport::GetStaticCapabilityFlags());
	TSharedPtr<FJsonObject> RuntimeRoot;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RuntimeJson);
	TestTrue(TEXT("CapabilityFlagsToJson is parseable"),
		FJsonSerializer::Deserialize(Reader, RuntimeRoot) && RuntimeRoot.IsValid());
	if (!RuntimeRoot.IsValid())
	{
		return false;
	}

	FString RuntimeVersion;
	TestTrue(
		TEXT("CapabilityFlagsToJson handoff_version matches file"),
		RuntimeRoot->TryGetStringField(TEXT("handoff_version"), RuntimeVersion)
			&& RuntimeVersion == Handoff.HandoffVersion);

	const TSharedPtr<FJsonObject>* DefaultsObj = nullptr;
	TestTrue(TEXT("CapabilityFlagsToJson carries job_defaults"),
		RuntimeRoot->TryGetObjectField(TEXT("job_defaults"), DefaultsObj) && DefaultsObj && (*DefaultsObj).IsValid());
	if (DefaultsObj && (*DefaultsObj).IsValid())
	{
		TestEqual(
			TEXT("default_timeout_ms parity"),
			static_cast<int32>((*DefaultsObj)->GetNumberField(TEXT("default_timeout_ms"))),
			Handoff.DefaultTimeoutMs);
		TestEqual(
			TEXT("poll_action parity"),
			(*DefaultsObj)->GetStringField(TEXT("poll_action")),
			Handoff.PollAction);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTransportDispatchModelTest,
	"UEREMCP.Transport.DispatchModel",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpTransportDispatchModelTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("timeout_ms == 0 dispatches inline"),
		UeremcpTransport::ResolveDispatchModel(0),
		EUeremcpJobDispatchModel::InlineComplete);
	TestEqual(
		TEXT("timeout_ms > 0 dispatches poll-after-timeout"),
		UeremcpTransport::ResolveDispatchModel(1),
		EUeremcpJobDispatchModel::PollAfterTimeout);
	TestEqual(
		TEXT("default long-op timeout dispatches poll"),
		UeremcpTransport::ResolveDispatchModel(FUeremcpJobModelDefaults::DefaultTimeoutMs),
		EUeremcpJobDispatchModel::PollAfterTimeout);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTransportJobStateInvariantTest,
	"UEREMCP.Transport.JobState.Invariants",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpTransportJobStateInvariantTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("queued is valid"), UeremcpTransport::IsValidJobState(TEXT("queued")));
	TestTrue(TEXT("running is valid"), UeremcpTransport::IsValidJobState(TEXT("running")));
	TestTrue(TEXT("completed is terminal"), UeremcpTransport::IsTerminalJobState(TEXT("completed")));
	TestTrue(TEXT("failed is terminal"), UeremcpTransport::IsTerminalJobState(TEXT("failed")));
	TestTrue(TEXT("cancelled is terminal"), UeremcpTransport::IsTerminalJobState(TEXT("cancelled")));
	TestFalse(TEXT("running is not terminal"), UeremcpTransport::IsTerminalJobState(TEXT("running")));

	TestTrue(
		TEXT("queued -> running allowed"),
		UeremcpTransport::IsValidJobStateTransition(TEXT("queued"), TEXT("running")));
	TestTrue(
		TEXT("running -> completed allowed"),
		UeremcpTransport::IsValidJobStateTransition(TEXT("running"), TEXT("completed")));
	TestTrue(
		TEXT("running -> cancelled allowed"),
		UeremcpTransport::IsValidJobStateTransition(TEXT("running"), TEXT("cancelled")));
	TestTrue(
		TEXT("identity transition allowed"),
		UeremcpTransport::IsValidJobStateTransition(TEXT("running"), TEXT("running")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTransportJobStateNegativeTest,
	"UEREMCP.Transport.JobState.Negative",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpTransportJobStateNegativeTest::RunTest(const FString& Parameters)
{
	FString Error;
	FUeremcpHandoffConstraints Handoff;

	const FString MalformedJson = TEXT("{\"handoff_version\":\"broken\"}");
	TestFalse(
		TEXT("malformed handoff JSON rejected"),
		UeremcpTransport::ParseHandoffConstraintsJson(MalformedJson, Handoff, Error));
	TestFalse(TEXT("parse error reported"), Error.IsEmpty());

	const FString BadStdioJson = TEXT(R"({
		"handoff_version":"ws04-wave1-1",
		"recommended_job_model":"poll_after_timeout",
		"capabilities":{
			"http_transport_only":true,
			"streamable_http_sse":true,
			"stdio_transport":true,
			"mcp_resources":true,
			"mcp_progress_notifications":true,
			"mcp_cancellation_notification":true,
			"toolset_registry_cancel_wired":false,
			"persistent_server_push":false,
			"engine_job_ids":false,
			"engine_auth":false,
			"origin_localhost_guard":true
		},
		"job_defaults":{"default_timeout_ms":120000,"min_timeout_ms":1000,"max_timeout_ms":600000,"poll_action":"get_job_result"},
		"ws05_constraints":{"dispatch_inline_when_timeout_ms_zero":true,"dispatch_poll_when_timeout_ms_positive":true}
	})");
	Error.Reset();
	TestFalse(
		TEXT("stdio_transport true rejected"),
		UeremcpTransport::ParseHandoffConstraintsJson(BadStdioJson, Handoff, Error));

	TestFalse(TEXT("bogus job state rejected"), UeremcpTransport::IsValidJobState(TEXT("exploding")));
	TestFalse(
		TEXT("terminal -> running rejected"),
		UeremcpTransport::IsValidJobStateTransition(TEXT("completed"), TEXT("running")));
	TestFalse(
		TEXT("running -> queued rejected"),
		UeremcpTransport::IsValidJobStateTransition(TEXT("running"), TEXT("queued")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTransportProbeEpicMcpTest,
	"UEREMCP.Transport.Probe.EpicMcp",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpTransportProbeEpicMcpTest::RunTest(const FString& Parameters)
{
	const FUeremcpTransportProbeResult Probe = UeremcpTransport::ProbeEpicTransport();
	TestTrue(TEXT("static capability flags always populated"),
		Probe.Capabilities.bHttpTransportOnly && Probe.Capabilities.bStreamableHttpSse);
	TestFalse(TEXT("stdio not advertised"), Probe.Capabilities.bStdioTransport);

	if (!Probe.bMcpModuleLoaded)
	{
		AddInfo(TEXT("ModelContextProtocol module not loaded in this Cmd session — probe notes only."));
		return true;
	}

	TestFalse(TEXT("negotiated protocol version empty"), Probe.NegotiatedProtocolVersion.IsEmpty());
	TestFalse(TEXT("server URL path empty"), Probe.ServerUrlPath.IsEmpty());
	TestTrue(TEXT("client endpoint uses loopback"), Probe.ClientEndpointUrl.Contains(TEXT("127.0.0.1")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTransportJobRegistryPollTest,
	"UEREMCP.Transport.JobRegistry.Poll",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpTransportJobRegistryPollTest::RunTest(const FString& Parameters)
{
	FUeremcpJobRegistry Registry;
	FString Error;
	FString JobId;
	const FString RequestId = FString::Printf(TEXT("transport-poll-%s"), *FGuid::NewGuid().ToString());
	int32 FixtureExecutions = 0;

	TestTrue(
		TEXT("deterministic poll fixture registers"),
		Registry.CreateJob(RequestId, false, TEXT("Queued for transport test"), JobId, Error));
	TestFalse(TEXT("registry assigns a stable job id"), JobId.IsEmpty());

	FUeremcpResponse Poll;
	TestTrue(TEXT("queued result is pollable"), Registry.GetJobResult(JobId, Poll, Error));
	TestEqual(TEXT("queued poll is partial"), Poll.Status, FString(TEXT("partially_completed")));
	TestEqual(TEXT("queued state is non-terminal"), Poll.Job.State, FString(TEXT("queued")));
	TestEqual(TEXT("queued poll preserves job id"), Poll.Job.JobId, JobId);
	TestEqual(TEXT("first poll counts originating call"), Poll.Metrics.McpRoundTrips, 2);

	TestTrue(TEXT("fixture enters running state"), Registry.StartJob(JobId, Error));
	FPooledSyncEvent WorkerBlocked(true);
	FPooledSyncEvent ReleaseWorker(true);
	bool bWorkerCompleted = false;
	FString WorkerError;
	TFuture<void> Worker = Async(EAsyncExecution::ThreadPool, [&]()
	{
		WorkerBlocked->Trigger();
		ReleaseWorker->Wait();
		++FixtureExecutions;
		bWorkerCompleted = Registry.CompleteJob(
			JobId,
			UeremcpTransportTest::MakeTerminalResponse(
				RequestId,
				TEXT("Transport poll fixture completed exactly once."),
				4),
			WorkerError);
	});

	const bool bWorkerReachedBarrier = WorkerBlocked->Wait(5000);
	TestTrue(TEXT("worker reaches deterministic barrier"), bWorkerReachedBarrier);
	if (!bWorkerReachedBarrier)
	{
		ReleaseWorker->Trigger();
		Worker.Wait();
		return false;
	}

	TestTrue(TEXT("running result polls before release"), Registry.GetJobResult(JobId, Poll, Error));
	TestEqual(TEXT("running state remains non-terminal"), Poll.Job.State, FString(TEXT("running")));
	TestEqual(TEXT("running poll preserves job id"), Poll.Job.JobId, JobId);
	TestEqual(TEXT("second poll is counted"), Poll.Metrics.McpRoundTrips, 3);

	ReleaseWorker->Trigger();
	Worker.Wait();
	TestTrue(TEXT("released worker stores terminal response"), bWorkerCompleted);
	if (!WorkerError.IsEmpty())
	{
		AddError(WorkerError);
	}

	TestTrue(TEXT("terminal result remains pollable"), Registry.GetJobResult(JobId, Poll, Error));
	TestEqual(TEXT("terminal status retained"), Poll.Status, FString(TEXT("created_and_validated")));
	TestEqual(TEXT("terminal state completed"), Poll.Job.State, FString(TEXT("completed")));
	TestEqual(TEXT("terminal poll preserves job id"), Poll.Job.JobId, JobId);
	TestEqual(TEXT("third poll is counted"), Poll.Metrics.McpRoundTrips, 4);
	TestEqual(TEXT("terminal operations retained"), Poll.Metrics.InternalOperations, 4);
	TestEqual(TEXT("fixture executed once"), FixtureExecutions, 1);

	TestTrue(TEXT("terminal result can be retrieved again"), Registry.GetJobResult(JobId, Poll, Error));
	TestEqual(TEXT("repeat retrieval does not re-execute fixture"), FixtureExecutions, 1);
	TestEqual(TEXT("repeat terminal poll is counted"), Poll.Metrics.McpRoundTrips, 5);

	const int32 NumBeforeInvalidPolls = Registry.Num();
	TestFalse(TEXT("empty job id is rejected"), Registry.GetJobResult(FString(), Poll, Error));
	TestFalse(
		TEXT("unknown job id is rejected"),
		Registry.GetJobResult(TEXT("00000000-0000-0000-0000-000000000000"), Poll, Error));
	TestEqual(TEXT("invalid polls do not mutate registry"), Registry.Num(), NumBeforeInvalidPolls);

	FUeremcpJobRegistry& SharedRegistry = FUeremcpJobRegistry::Get();
	SharedRegistry.Clear();
	FString SharedJobId;
	TestTrue(
		TEXT("adapter fixture registers in process registry"),
		SharedRegistry.CreateJob(
			TEXT("transport-action-origin"),
			false,
			TEXT("Adapter fixture running"),
			SharedJobId,
			Error));
	TestTrue(TEXT("adapter fixture starts"), SharedRegistry.StartJob(SharedJobId, Error));

	const TSharedPtr<FJsonObject> ActionPoll = UeremcpTransportTest::ParseResponseJson(
		UUeremcpReferenceToolset::GetJobResult(UeremcpTransportTest::MakeJobActionRequest(
			TEXT("transport-action-poll"),
			TEXT("get_job_result"),
			SharedJobId)));
	TestTrue(TEXT("get_job_result adapter returns JSON object"), ActionPoll.IsValid());
	if (!ActionPoll.IsValid())
	{
		SharedRegistry.Clear();
		return false;
	}
	TestEqual(
		TEXT("poll adapter rebinds request id"),
		ActionPoll->GetStringField(TEXT("request_id")),
		FString(TEXT("transport-action-poll")));
	TestEqual(
		TEXT("poll adapter returns partial running state"),
		ActionPoll->GetStringField(TEXT("status")),
		FString(TEXT("partially_completed")));
	TestEqual(
		TEXT("poll adapter preserves stable job id"),
		ActionPoll->GetObjectField(TEXT("job"))->GetStringField(TEXT("job_id")),
		SharedJobId);
	TestEqual(
		TEXT("poll adapter includes originating call and poll"),
		static_cast<int32>(
			ActionPoll->GetObjectField(TEXT("metrics"))->GetNumberField(TEXT("mcp_round_trips"))),
		2);

	TestTrue(
		TEXT("adapter fixture completes"),
		SharedRegistry.CompleteJob(
			SharedJobId,
			UeremcpTransportTest::MakeTerminalResponse(
				TEXT("transport-action-origin"),
				TEXT("Adapter fixture terminal result."),
				2),
			Error));
	const TSharedPtr<FJsonObject> TerminalActionPoll = UeremcpTransportTest::ParseResponseJson(
		UUeremcpReferenceToolset::GetJobResult(UeremcpTransportTest::MakeJobActionRequest(
			TEXT("transport-action-terminal-poll"),
			TEXT("get_job_result"),
			SharedJobId)));
	TestTrue(TEXT("terminal adapter poll returns JSON object"), TerminalActionPoll.IsValid());
	if (TerminalActionPoll.IsValid())
	{
		TestEqual(
			TEXT("poll adapter returns retained terminal status"),
			TerminalActionPoll->GetStringField(TEXT("status")),
			FString(TEXT("created_and_validated")));
		TestEqual(
			TEXT("terminal adapter poll reports completed"),
			TerminalActionPoll->GetObjectField(TEXT("job"))->GetStringField(TEXT("state")),
			FString(TEXT("completed")));
		TestEqual(
			TEXT("terminal adapter poll counts both polls"),
			static_cast<int32>(
				TerminalActionPoll->GetObjectField(TEXT("metrics"))
					->GetNumberField(TEXT("mcp_round_trips"))),
			3);
	}

	const TSharedPtr<FJsonObject> UnknownActionPoll = UeremcpTransportTest::ParseResponseJson(
		UUeremcpReferenceToolset::GetJobResult(UeremcpTransportTest::MakeJobActionRequest(
			TEXT("transport-action-missing"),
			TEXT("get_job_result"),
			TEXT("00000000-0000-0000-0000-000000000000"))));
	TestTrue(TEXT("unknown poll adapter result is structured"), UnknownActionPoll.IsValid());
	if (UnknownActionPoll.IsValid())
	{
		TestEqual(
			TEXT("unknown poll adapter rejects"),
			UnknownActionPoll->GetStringField(TEXT("status")),
			FString(TEXT("rejected")));
	}

	const TSharedPtr<FJsonObject> MalformedActionPoll = UeremcpTransportTest::ParseResponseJson(
		UUeremcpReferenceToolset::GetJobResult(
			TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"transport-action-malformed\","
				"\"action\":\"get_job_result\",\"specification\":{}}")));
	TestTrue(TEXT("malformed poll adapter result is structured"), MalformedActionPoll.IsValid());
	if (MalformedActionPoll.IsValid())
	{
		TestEqual(
			TEXT("malformed poll adapter rejects"),
			MalformedActionPoll->GetStringField(TEXT("status")),
			FString(TEXT("rejected")));
	}
	SharedRegistry.Clear();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTransportJobRegistryCancelTest,
	"UEREMCP.Transport.JobRegistry.Cancel",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpTransportJobRegistryCancelTest::RunTest(const FString& Parameters)
{
	FUeremcpJobRegistry Registry;
	FString Error;

	FString NonCancellableId;
	TestTrue(
		TEXT("non-cancellable fixture registers"),
		Registry.CreateJob(
			TEXT("transport-cancel-disabled"),
			false,
			TEXT("Cannot cancel"),
			NonCancellableId,
			Error));
	FUeremcpResponse NonCancellable;
	TestTrue(
		TEXT("non-cancellable fixture is inspectable"),
		Registry.GetTimeoutResponse(NonCancellableId, NonCancellable, Error));
	TestFalse(TEXT("missing callback is never advertised cancellable"), NonCancellable.Job.bCancellable);
	TestEqual(
		TEXT("non-cancellable request is rejected"),
		Registry.CancelJob(NonCancellableId, Error),
		EUeremcpCancelResult::NotCancellable);

	FString JobId;
	int32 CancellationCheckpoints = 0;
	TestTrue(
		TEXT("cancellable fixture registers"),
		Registry.CreateJob(
			TEXT("transport-cancel-enabled"),
			true,
			TEXT("Waiting at cooperative checkpoint"),
			JobId,
			Error,
			[&CancellationCheckpoints]()
			{
				++CancellationCheckpoints;
				return true;
			}));
	TestTrue(TEXT("cancellable fixture starts"), Registry.StartJob(JobId, Error));
	TestEqual(
		TEXT("cooperative cancellation is accepted"),
		Registry.CancelJob(JobId, Error),
		EUeremcpCancelResult::Cancelled);
	TestEqual(TEXT("domain checkpoint observes cancellation once"), CancellationCheckpoints, 1);

	FUeremcpResponse Cancelled;
	TestTrue(TEXT("cancelled job remains pollable"), Registry.GetJobResult(JobId, Cancelled, Error));
	TestEqual(TEXT("cancelled job reaches terminal state"), Cancelled.Job.State, FString(TEXT("cancelled")));
	TestFalse(TEXT("cancelled job stops advertising cancellation"), Cancelled.Job.bCancellable);
	TestEqual(
		TEXT("cancel does not claim validated completion"),
		Cancelled.Status,
		FString(TEXT("partially_completed")));

	TestEqual(
		TEXT("repeat cancellation has explicit terminal result"),
		Registry.CancelJob(JobId, Error),
		EUeremcpCancelResult::AlreadyTerminal);
	TestEqual(TEXT("repeat cancellation does not invoke checkpoint"), CancellationCheckpoints, 1);

	const FUeremcpResponse InvalidCompletion = UeremcpTransportTest::MakeTerminalResponse(
		TEXT("transport-cancel-enabled"),
		TEXT("Must not replace cancellation."),
		1);
	TestFalse(
		TEXT("cancelled work cannot later transition to completed"),
		Registry.CompleteJob(JobId, InvalidCompletion, Error));
	FUeremcpJobSnapshot Snapshot;
	TestTrue(TEXT("cancelled snapshot remains available"), Registry.GetSnapshot(JobId, Snapshot));
	TestEqual(TEXT("cancelled state is stable"), Snapshot.Job.State, FString(TEXT("cancelled")));

	TestEqual(
		TEXT("unknown cancellation has explicit not-found result"),
		Registry.CancelJob(TEXT("00000000-0000-0000-0000-000000000000"), Error),
		EUeremcpCancelResult::NotFound);

	FUeremcpJobRegistry& SharedRegistry = FUeremcpJobRegistry::Get();
	SharedRegistry.Clear();
	FString SharedJobId;
	int32 AdapterCancellationCheckpoints = 0;
	TestTrue(
		TEXT("cancel adapter fixture registers"),
		SharedRegistry.CreateJob(
			TEXT("transport-cancel-action-origin"),
			true,
			TEXT("Adapter cancellation fixture running"),
			SharedJobId,
			Error,
			[&AdapterCancellationCheckpoints]()
			{
				++AdapterCancellationCheckpoints;
				return true;
			}));
	TestTrue(TEXT("cancel adapter fixture starts"), SharedRegistry.StartJob(SharedJobId, Error));

	const TSharedPtr<FJsonObject> ActionCancel = UeremcpTransportTest::ParseResponseJson(
		UUeremcpReferenceToolset::CancelJob(UeremcpTransportTest::MakeJobActionRequest(
			TEXT("transport-cancel-action"),
			TEXT("cancel_job"),
			SharedJobId)));
	TestTrue(TEXT("cancel adapter returns JSON object"), ActionCancel.IsValid());
	if (!ActionCancel.IsValid())
	{
		SharedRegistry.Clear();
		return false;
	}
	TestEqual(TEXT("cancel adapter invokes checkpoint once"), AdapterCancellationCheckpoints, 1);
	TestEqual(
		TEXT("cancel adapter rebinds request id"),
		ActionCancel->GetStringField(TEXT("request_id")),
		FString(TEXT("transport-cancel-action")));
	TestEqual(
		TEXT("cancel adapter keeps honest status"),
		ActionCancel->GetStringField(TEXT("status")),
		FString(TEXT("partially_completed")));
	TestEqual(
		TEXT("cancel adapter exposes cancelled terminal state"),
		ActionCancel->GetObjectField(TEXT("job"))->GetStringField(TEXT("state")),
		FString(TEXT("cancelled")));

	const TSharedPtr<FJsonObject> RepeatActionCancel = UeremcpTransportTest::ParseResponseJson(
		UUeremcpReferenceToolset::CancelJob(UeremcpTransportTest::MakeJobActionRequest(
			TEXT("transport-cancel-action-repeat"),
			TEXT("cancel_job"),
			SharedJobId)));
	TestTrue(TEXT("repeat cancel adapter result is structured"), RepeatActionCancel.IsValid());
	if (RepeatActionCancel.IsValid())
	{
		TestEqual(
			TEXT("repeat cancel adapter returns retained cancelled result"),
			RepeatActionCancel->GetObjectField(TEXT("job"))->GetStringField(TEXT("state")),
			FString(TEXT("cancelled")));
	}
	TestEqual(
		TEXT("repeat cancel adapter does not repeat checkpoint"),
		AdapterCancellationCheckpoints,
		1);

	FString SharedNonCancellableId;
	TestTrue(
		TEXT("adapter non-cancellable fixture registers"),
		SharedRegistry.CreateJob(
			TEXT("transport-cancel-action-disabled"),
			false,
			FString(),
			SharedNonCancellableId,
			Error));
	const TSharedPtr<FJsonObject> RejectedActionCancel = UeremcpTransportTest::ParseResponseJson(
		UUeremcpReferenceToolset::CancelJob(UeremcpTransportTest::MakeJobActionRequest(
			TEXT("transport-cancel-action-rejected"),
			TEXT("cancel_job"),
			SharedNonCancellableId)));
	TestTrue(TEXT("non-cancellable adapter result is structured"), RejectedActionCancel.IsValid());
	if (RejectedActionCancel.IsValid())
	{
		TestEqual(
			TEXT("non-cancellable adapter rejects"),
			RejectedActionCancel->GetStringField(TEXT("status")),
			FString(TEXT("rejected")));
	}

	const TSharedPtr<FJsonObject> UnknownActionCancel = UeremcpTransportTest::ParseResponseJson(
		UUeremcpReferenceToolset::CancelJob(UeremcpTransportTest::MakeJobActionRequest(
			TEXT("transport-cancel-action-missing"),
			TEXT("cancel_job"),
			TEXT("00000000-0000-0000-0000-000000000000"))));
	TestTrue(TEXT("unknown cancel adapter result is structured"), UnknownActionCancel.IsValid());
	if (UnknownActionCancel.IsValid())
	{
		TestEqual(
			TEXT("unknown cancel adapter rejects"),
			UnknownActionCancel->GetStringField(TEXT("status")),
			FString(TEXT("rejected")));
	}
	SharedRegistry.Clear();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTransportTimeoutPartialResponseTest,
	"UEREMCP.Transport.Timeout.PartiallyCompleted",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpTransportTimeoutPartialResponseTest::RunTest(const FString& Parameters)
{
	int32 InlineExecutions = 0;
	const auto ExecuteInline = [&InlineExecutions]()
	{
		++InlineExecutions;
		return UeremcpTransportTest::MakeTerminalResponse(
			TEXT("transport-timeout-inline"),
			TEXT("Inline fixture completed before returning."),
			1);
	};

	TestEqual(
		TEXT("timeout_ms zero selects inline execution"),
		UeremcpTransport::ResolveDispatchModel(0),
		EUeremcpJobDispatchModel::InlineComplete);
	const FUeremcpResponse Inline = ExecuteInline();
	TestEqual(TEXT("inline fixture executes once"), InlineExecutions, 1);
	TestEqual(TEXT("inline fixture returns terminal status"), Inline.Status, FString(TEXT("created_and_validated")));
	TestFalse(TEXT("inline fixture has no timeout job handle"), Inline.bHasJob);

	FUeremcpJobRegistry Registry;
	FString Error;
	FString JobId;
	const FString RequestId = TEXT("transport-timeout-positive");
	TestTrue(
		TEXT("positive-timeout fixture registers"),
		Registry.CreateJob(RequestId, false, TEXT("Blocked beyond timeout"), JobId, Error));
	TestTrue(TEXT("positive-timeout fixture starts"), Registry.StartJob(JobId, Error));
	TestEqual(
		TEXT("positive timeout selects poll-after-timeout"),
		UeremcpTransport::ResolveDispatchModel(1),
		EUeremcpJobDispatchModel::PollAfterTimeout);

	FPooledSyncEvent WorkerBlocked(true);
	FPooledSyncEvent ReleaseWorker(true);
	bool bWorkerCompleted = false;
	int32 WorkerExecutions = 0;
	FString WorkerError;
	TFuture<void> Worker = Async(EAsyncExecution::ThreadPool, [&]()
	{
		WorkerBlocked->Trigger();
		ReleaseWorker->Wait();
		++WorkerExecutions;
		bWorkerCompleted = Registry.CompleteJob(
			JobId,
			UeremcpTransportTest::MakeTerminalResponse(
				RequestId,
				TEXT("Blocked fixture completed after release."),
				3),
			WorkerError);
	});

	const bool bWorkerReachedBarrier = WorkerBlocked->Wait(5000);
	TestTrue(TEXT("positive-timeout worker reaches barrier"), bWorkerReachedBarrier);
	if (!bWorkerReachedBarrier)
	{
		ReleaseWorker->Trigger();
		Worker.Wait();
		return false;
	}

	FUeremcpResponse Timeout;
	TestTrue(
		TEXT("blocked fixture returns timeout response"),
		Registry.GetTimeoutResponse(JobId, Timeout, Error));
	TestEqual(TEXT("timeout response is partial"), Timeout.Status, FString(TEXT("partially_completed")));
	TestEqual(TEXT("timeout response state is running"), Timeout.Job.State, FString(TEXT("running")));
	TestEqual(TEXT("timeout response preserves job id"), Timeout.Job.JobId, JobId);
	TestEqual(
		TEXT("timeout response advertises poll action"),
		Timeout.Job.PollAction,
		FString(FUeremcpJobModelDefaults::PollActionName));
	TestEqual(TEXT("timeout response counts initial call"), Timeout.Metrics.McpRoundTrips, 1);
	TestFalse(TEXT("early response is not terminal success"), UeremcpTransport::IsTerminalJobState(Timeout.Job.State));

	FUeremcpResponse RunningPoll;
	TestTrue(TEXT("blocked fixture can be polled"), Registry.GetJobResult(JobId, RunningPoll, Error));
	TestEqual(TEXT("running poll preserves job id"), RunningPoll.Job.JobId, JobId);
	TestEqual(TEXT("running poll counts initial call plus poll"), RunningPoll.Metrics.McpRoundTrips, 2);

	ReleaseWorker->Trigger();
	Worker.Wait();
	TestTrue(TEXT("released timeout worker completes"), bWorkerCompleted);
	if (!WorkerError.IsEmpty())
	{
		AddError(WorkerError);
	}
	TestEqual(TEXT("released fixture executes once"), WorkerExecutions, 1);

	FUeremcpResponse TerminalPoll;
	TestTrue(TEXT("released fixture reaches terminal poll"), Registry.GetJobResult(JobId, TerminalPoll, Error));
	TestEqual(TEXT("terminal poll returns validated result"), TerminalPoll.Status, FString(TEXT("created_and_validated")));
	TestEqual(TEXT("terminal poll reports completed state"), TerminalPoll.Job.State, FString(TEXT("completed")));
	TestEqual(TEXT("terminal poll preserves stable job id"), TerminalPoll.Job.JobId, JobId);
	TestEqual(TEXT("terminal poll counts initial call plus polls"), TerminalPoll.Metrics.McpRoundTrips, 3);

	AddInfo(TEXT(
		"SKIP residual: no production timeout scheduler currently invokes this registry "
		"lifecycle or closes the MCP SSE stream; deterministic registry lifecycle is covered."));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
