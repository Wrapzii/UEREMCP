// ADR-0009 registry lifecycle, polling, cancellation, and expiration tests.

#include "UeremcpJobRegistry.h"

#include "Async/ParallelFor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeLock.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpJobRegistryPollTest,
	"UEREMCP.Protocol.JobRegistry.Poll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpJobRegistryPollTest::RunTest(const FString& Parameters)
{
	FUeremcpJobRegistry Registry;
	FString JobId;
	FString Error;
	TestTrue(
		TEXT("queued job created"),
		Registry.CreateJob(TEXT("request-poll"), false, TEXT("Queued"), JobId, Error));
	TestTrue(TEXT("job id assigned"), !JobId.IsEmpty());

	FUeremcpResponse Poll;
	TestTrue(TEXT("queued job polls"), Registry.GetJobResult(JobId, Poll, Error));
	TestEqual(TEXT("queued poll status"), Poll.Status, FString(TEXT("partially_completed")));
	TestEqual(TEXT("queued state"), Poll.Job.State, FString(TEXT("queued")));
	TestEqual(TEXT("originating call plus first poll"), Poll.Metrics.McpRoundTrips, 2);

	TestTrue(TEXT("queued -> running"), Registry.StartJob(JobId, Error));
	TestTrue(TEXT("progress updates"), Registry.UpdateProgress(JobId, 0.4, TEXT("Compiling"), Error));
	TestFalse(TEXT("progress cannot regress"), Registry.UpdateProgress(JobId, 0.2, TEXT("Older"), Error));

	FUeremcpResponse Terminal;
	Terminal.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	Terminal.RequestId = TEXT("request-poll");
	Terminal.Status = TEXT("created_and_validated");
	Terminal.Summary = TEXT("Created and re-read the requested asset.");
	Terminal.Metrics.McpRoundTrips = 1;
	Terminal.Metrics.InternalOperations = 7;
	TestTrue(TEXT("running -> completed"), Registry.CompleteJob(JobId, Terminal, Error));

	TestTrue(TEXT("terminal result polls"), Registry.GetJobResult(JobId, Poll, Error));
	TestEqual(TEXT("terminal status retained"), Poll.Status, FString(TEXT("created_and_validated")));
	TestEqual(TEXT("terminal job state"), Poll.Job.State, FString(TEXT("completed")));
	TestEqual(TEXT("completed progress"), Poll.Job.Progress, 1.0);
	TestEqual(TEXT("two polls counted"), Poll.Metrics.McpRoundTrips, 3);
	TestEqual(TEXT("internal operations retained"), Poll.Metrics.InternalOperations, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpJobRegistryCancelTest,
	"UEREMCP.Protocol.JobRegistry.Cancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpJobRegistryCancelTest::RunTest(const FString& Parameters)
{
	FUeremcpJobRegistry Registry;
	FString Error;
	FString JobId;
	bool bWorkerStopped = false;
	EUeremcpCancelResult NestedCancelResult = EUeremcpCancelResult::NotFound;
	TestTrue(
		TEXT("cancellable job created"),
		Registry.CreateJob(
			TEXT("request-cancel"),
			true,
			TEXT("Working"),
			JobId,
			Error,
			[&Registry, &JobId, &bWorkerStopped, &NestedCancelResult]()
			{
				bWorkerStopped = true;
				FString NestedError;
				NestedCancelResult = Registry.CancelJob(JobId, NestedError);
				return true;
			}));
	TestTrue(TEXT("job starts"), Registry.StartJob(JobId, Error));
	TestEqual(
		TEXT("cooperative cancellation succeeds"),
		Registry.CancelJob(JobId, Error),
		EUeremcpCancelResult::Cancelled);
	TestTrue(TEXT("worker callback honored cancellation"), bWorkerStopped);
	TestEqual(
		TEXT("reentrant cancellation does not invoke callback twice"),
		NestedCancelResult,
		EUeremcpCancelResult::CancellationPending);

	FUeremcpResponse Poll;
	TestTrue(TEXT("cancelled result remains pollable"), Registry.GetJobResult(JobId, Poll, Error));
	TestEqual(TEXT("cancelled terminal state"), Poll.Job.State, FString(TEXT("cancelled")));
	TestEqual(TEXT("cancel is not validated completion"), Poll.Status, FString(TEXT("partially_completed")));
	TestFalse(TEXT("terminal job no longer cancellable"), Poll.Job.bCancellable);
	TestEqual(
		TEXT("second cancellation is terminal"),
		Registry.CancelJob(JobId, Error),
		EUeremcpCancelResult::AlreadyTerminal);

	FString NonCancellableId;
	TestTrue(
		TEXT("non-cancellable job created"),
		Registry.CreateJob(TEXT("no-cancel"), false, FString(), NonCancellableId, Error));
	TestEqual(
		TEXT("missing callback never advertises cancellation"),
		Registry.CancelJob(NonCancellableId, Error),
		EUeremcpCancelResult::NotCancellable);

	FString RejectingId;
	TestTrue(
		TEXT("rejecting worker job created"),
		Registry.CreateJob(
			TEXT("reject-cancel"),
			true,
			FString(),
			RejectingId,
			Error,
			[]()
			{
				return false;
			}));
	TestTrue(TEXT("rejecting worker starts"), Registry.StartJob(RejectingId, Error));
	TestEqual(
		TEXT("worker may reject cancellation"),
		Registry.CancelJob(RejectingId, Error),
		EUeremcpCancelResult::RejectedByWorker);
	FUeremcpJobSnapshot StillRunning;
	TestTrue(TEXT("rejected cancellation snapshot exists"), Registry.GetSnapshot(RejectingId, StillRunning));
	TestEqual(TEXT("rejected cancellation stays active"), StillRunning.Job.State, FString(TEXT("running")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpJobRegistryBoundsTest,
	"UEREMCP.Protocol.JobRegistry.Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpJobRegistryBoundsTest::RunTest(const FString& Parameters)
{
	FUeremcpJobRegistryConfig Config;
	Config.MaxJobs = 1;
	Config.TerminalRetentionMs = 60000;
	Config.MaxActiveAgeMs = 60000;
	FUeremcpJobRegistry Registry(Config);
	FString Error;
	FString FirstJob;
	FString SecondJob;

	TestTrue(
		TEXT("first active job created"),
		Registry.CreateJob(TEXT("first"), false, FString(), FirstJob, Error));
	TestFalse(
		TEXT("capacity rejects when all jobs active"),
		Registry.CreateJob(TEXT("second"), false, FString(), SecondJob, Error));
	TestTrue(TEXT("capacity rejection explains active bound"), Error.Contains(TEXT("capacity")));

	TestTrue(TEXT("first job can fail"), Registry.FailJob(FirstJob, TEXT("Expected test failure."), Error));
	TestTrue(
		TEXT("oldest terminal entry evicted for new work"),
		Registry.CreateJob(TEXT("second"), false, FString(), SecondJob, Error));
	TestEqual(TEXT("registry remains bounded"), Registry.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpJobRegistryConcurrentPollTest,
	"UEREMCP.Protocol.JobRegistry.ConcurrentPoll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpJobRegistryConcurrentPollTest::RunTest(const FString& Parameters)
{
	FUeremcpJobRegistry Registry;
	FString Error;
	FString JobId;
	TestTrue(
		TEXT("concurrent-poll job created"),
		Registry.CreateJob(TEXT("concurrent"), false, FString(), JobId, Error));
	TestTrue(TEXT("concurrent-poll job started"), Registry.StartJob(JobId, Error));

	FCriticalSection ResultMutex;
	int32 SuccessfulPolls = 0;
	ParallelFor(32, [&Registry, &JobId, &ResultMutex, &SuccessfulPolls](int32)
	{
		FUeremcpResponse Response;
		FString PollError;
		if (Registry.GetJobResult(JobId, Response, PollError))
		{
			FScopeLock ResultLock(&ResultMutex);
			++SuccessfulPolls;
		}
	});

	TestEqual(TEXT("all concurrent polls succeed"), SuccessfulPolls, 32);
	FUeremcpJobSnapshot Snapshot;
	TestTrue(TEXT("post-poll snapshot exists"), Registry.GetSnapshot(JobId, Snapshot));
	TestEqual(TEXT("poll count is not lost"), Snapshot.PollCount, 32);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpJobRegistryExpirationTest,
	"UEREMCP.Protocol.JobRegistry.Expiration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpJobRegistryExpirationTest::RunTest(const FString& Parameters)
{
	FUeremcpJobRegistryConfig Config;
	Config.MaxJobs = 4;
	Config.TerminalRetentionMs = 10;
	Config.MaxActiveAgeMs = 20;
	FUeremcpJobRegistry Registry(Config);
	FString Error;
	FString JobId;
	TestTrue(
		TEXT("active job created"),
		Registry.CreateJob(TEXT("expires"), false, FString(), JobId, Error));

	FUeremcpJobSnapshot Before;
	TestTrue(TEXT("snapshot available"), Registry.GetSnapshot(JobId, Before));
	TestEqual(
		TEXT("stale active job transitions to failed"),
		Registry.CleanupExpiredAt(Before.CreatedAt + FTimespan::FromMilliseconds(21)),
		1);

	FUeremcpResponse Poll;
	TestTrue(TEXT("expired active failure is pollable"), Registry.GetJobResult(JobId, Poll, Error));
	TestEqual(TEXT("stale active state"), Poll.Job.State, FString(TEXT("failed")));
	TestEqual(TEXT("stale active envelope status"), Poll.Status, FString(TEXT("error")));

	FUeremcpJobSnapshot Failed;
	TestTrue(TEXT("failed snapshot available"), Registry.GetSnapshot(JobId, Failed));
	TestEqual(
		TEXT("terminal entry removed after retention"),
		Registry.CleanupExpiredAt(Failed.TerminalAt + FTimespan::FromMilliseconds(11)),
		1);
	TestFalse(TEXT("expired terminal no longer polls"), Registry.GetJobResult(JobId, Poll, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpJobTimeoutEnvelopeTest,
	"UEREMCP.Protocol.JobRegistry.TimeoutEnvelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpJobTimeoutEnvelopeTest::RunTest(const FString& Parameters)
{
	FUeremcpJobRegistry Registry;
	FString Error;
	FString JobId;
	TestTrue(
		TEXT("job created before positive timeout dispatch"),
		Registry.CreateJob(TEXT("request-timeout"), false, TEXT("Still running"), JobId, Error));
	TestTrue(TEXT("job started"), Registry.StartJob(JobId, Error));

	FUeremcpResponse Timeout;
	TestTrue(
		TEXT("timeout path returns immediately pollable state"),
		Registry.GetTimeoutResponse(JobId, Timeout, Error));
	TestEqual(TEXT("timeout status is partial"), Timeout.Status, FString(TEXT("partially_completed")));
	TestEqual(TEXT("timeout state is running"), Timeout.Job.State, FString(TEXT("running")));
	TestEqual(TEXT("timeout poll action"), Timeout.Job.PollAction, FString(TEXT("get_job_result")));
	TestEqual(TEXT("initial timeout is one round trip"), Timeout.Metrics.McpRoundTrips, 1);
	TestFalse(TEXT("timeout does not claim cancellability"), Timeout.Job.bCancellable);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
