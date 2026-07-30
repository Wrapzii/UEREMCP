// UEREMCP — job-model constraints handed to WS-05 (ADR-0009 input).
// Derived from Epic ModelContextProtocol transport facts in RB-04.

#pragma once

#include "CoreMinimal.h"

/** How UEREMCP completes work relative to a single MCP tools/call HTTP request. */
enum class EUeremcpJobDispatchModel : uint8
{
	/** Tool finishes before the MCP SSE stream closes; result is inline in the tool response. */
	InlineComplete,

	/** Work continues after options.timeout_ms; response carries envelope job block for polling. */
	PollAfterTimeout,
};

/**
 * Frozen transport facts Epic provides versus what UEREMCP must build.
 * Values are sourced from engine headers cited in docs/research/RB-04-transport-and-jobs.md.
 */
struct UEREMCPTRANSPORT_API FUeremcpTransportCapabilityFlags
{
	bool bHttpTransportOnly = true;
	bool bStreamableHttpSse = true;
	bool bStdioTransport = false;
	bool bMcpResourcesSupported = true;
	bool bMcpProgressNotifications = true;
	bool bMcpCancellationNotification = true;
	bool bToolsetRegistryCancelWired = false;
	bool bPersistentServerPushChannel = false;
	bool bEngineJobIds = false;
	bool bEngineAuth = false;
	bool bOriginLocalhostGuard = true;
};

/** Recommended defaults for envelope options.timeout_ms and polling (WS-05). */
struct UEREMCPTRANSPORT_API FUeremcpJobModelDefaults
{
	static constexpr int32 DefaultTimeoutMs = 120000;
	static constexpr int32 MinTimeoutMs = 1000;
	static constexpr int32 MaxTimeoutMs = 600000;
	static constexpr const TCHAR* PollActionName = TEXT("get_job_result");
};

/** Parsed subset of transport_job_handoff.json used for drift detection. */
struct UEREMCPTRANSPORT_API FUeremcpHandoffConstraints
{
	FString HandoffVersion;
	FString RecommendedJobModel;
	FUeremcpTransportCapabilityFlags Capabilities;
	int32 DefaultTimeoutMs = FUeremcpJobModelDefaults::DefaultTimeoutMs;
	int32 MinTimeoutMs = FUeremcpJobModelDefaults::MinTimeoutMs;
	int32 MaxTimeoutMs = FUeremcpJobModelDefaults::MaxTimeoutMs;
	FString PollAction = FUeremcpJobModelDefaults::PollActionName;
	bool bDispatchInlineWhenTimeoutZero = true;
	bool bDispatchPollWhenTimeoutPositive = true;
};

namespace UeremcpTransport
{
	/** Returns the dispatch model for a requested timeout (0 = service default). */
	UEREMCPTRANSPORT_API EUeremcpJobDispatchModel ResolveDispatchModel(int32 TimeoutMs);

	/** Static capability flags from header inspection (no editor required). */
	UEREMCPTRANSPORT_API FUeremcpTransportCapabilityFlags GetStaticCapabilityFlags();

	/** Serialise capability flags + defaults to JSON for WS-05 and automation tests. */
	UEREMCPTRANSPORT_API FString CapabilityFlagsToJson(
		const FUeremcpTransportCapabilityFlags& Flags);

	/** Resolve transport_job_handoff.json from the loaded UEREMCP plugin or module tree. */
	UEREMCPTRANSPORT_API FString ResolveHandoffJsonPath();

	/** Parse handoff JSON. Returns false and sets OutError on malformed input. */
	UEREMCPTRANSPORT_API bool ParseHandoffConstraintsJson(
		const FString& JsonText,
		FUeremcpHandoffConstraints& OutHandoff,
		FString& OutError);

	/** Load handoff JSON from ResolveHandoffJsonPath(). */
	UEREMCPTRANSPORT_API bool LoadHandoffConstraints(
		FUeremcpHandoffConstraints& OutHandoff,
		FString& OutError);

	/** Validate parsed handoff invariants (negative guard for drift / corruption). */
	UEREMCPTRANSPORT_API bool ValidateHandoffConstraints(
		const FUeremcpHandoffConstraints& Handoff,
		FString& OutError);

	/** Compare runtime capability flags + defaults against a parsed handoff file. */
	UEREMCPTRANSPORT_API bool HandoffMatchesRuntimeCapabilities(
		const FUeremcpHandoffConstraints& Handoff,
		FString& OutError);

	/** ADR-0009 / response.schema.json job.state values. */
	UEREMCPTRANSPORT_API bool IsValidJobState(const FString& State);

	/** Terminal poll states: completed | failed | cancelled. */
	UEREMCPTRANSPORT_API bool IsTerminalJobState(const FString& State);

	/** In-process registry transition guard (queued→running→terminal). */
	UEREMCPTRANSPORT_API bool IsValidJobStateTransition(
		const FString& FromState,
		const FString& ToState);
}
