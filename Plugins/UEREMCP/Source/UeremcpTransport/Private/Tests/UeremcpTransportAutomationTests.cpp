// WS-04 transport automation tests (ADR-0009 constraints + drift guard).

#include "UeremcpJobConstraints.h"
#include "UeremcpTransportProbe.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpTransportTest
{
	static void SkipMissingApi(
		FAutomationTestBase& Test,
		const TCHAR* MissingApi,
		const TCHAR* AdrSection)
	{
		Test.AddInfo(FString::Printf(
			TEXT("SKIP: %s not implemented — %s deferred until registry lands (WS-03/WS-05)."),
			MissingApi,
			AdrSection));
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
	FUeremcpTransportJobRegistryPollSkipTest,
	"UEREMCP.Transport.JobRegistry.Poll",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpTransportJobRegistryPollSkipTest::RunTest(const FString& Parameters)
{
	UeremcpTransportTest::SkipMissingApi(
		*this,
		TEXT("FUeremcpJobRegistry / get_job_result tool"),
		TEXT("ADR-0009 integration: poll until terminal + metrics.mcp_round_trips"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTransportJobRegistryCancelSkipTest,
	"UEREMCP.Transport.JobRegistry.Cancel",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpTransportJobRegistryCancelSkipTest::RunTest(const FString& Parameters)
{
	UeremcpTransportTest::SkipMissingApi(
		*this,
		TEXT("Cooperative cancel wiring (MCP notifications/cancelled -> job.state cancelled)"),
		TEXT("ADR-0009 cancel verification"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpTransportTimeoutPartialSkipTest,
	"UEREMCP.Transport.Timeout.PartiallyCompleted",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpTransportTimeoutPartialSkipTest::RunTest(const FString& Parameters)
{
	UeremcpTransportTest::SkipMissingApi(
		*this,
		TEXT("timeout_ms enforcement returning partially_completed + job handle"),
		TEXT("ADR-0009 unit: SSE closes before work completes"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
