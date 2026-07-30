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

namespace UeremcpTransport
{
	/** Returns the dispatch model for a requested timeout (0 = service default). */
	UEREMCPTRANSPORT_API EUeremcpJobDispatchModel ResolveDispatchModel(int32 TimeoutMs);

	/** Static capability flags from header inspection (no editor required). */
	UEREMCPTRANSPORT_API FUeremcpTransportCapabilityFlags GetStaticCapabilityFlags();

	/** Serialise capability flags + defaults to JSON for WS-05 and automation tests. */
	UEREMCPTRANSPORT_API FString CapabilityFlagsToJson(
		const FUeremcpTransportCapabilityFlags& Flags);
}
