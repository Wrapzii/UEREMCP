// Runtime introspection of Epic's in-editor MCP server (public API only).

#pragma once

#include "UeremcpJobConstraints.h"

#include "CoreMinimal.h"

/** Snapshot of live transport state; safe to call from editor after MCP modules load. */
struct UEREMCPTRANSPORT_API FUeremcpTransportProbeResult
{
	bool bMcpModuleLoaded = false;
	bool bServerRunning = false;
	uint32 ServerPort = 0;
	FString ServerUrlPath;
	FString NegotiatedProtocolVersion;
	bool bAutoStartConfigured = false;
	FString ClientEndpointUrl;
	FUeremcpTransportCapabilityFlags Capabilities;
	TArray<FString> LimitationNotes;
};

namespace UeremcpTransport
{
	/**
	 * Probe Epic MCP transport without starting or stopping the server.
	 * Returns limitation notes when modules are absent (editor not ready, plugin disabled).
	 */
	UEREMCPTRANSPORT_API FUeremcpTransportProbeResult ProbeEpicTransport();
}
