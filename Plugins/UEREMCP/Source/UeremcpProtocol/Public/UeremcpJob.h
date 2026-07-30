// UEREMCP — long-running job envelope fields (ADR-0009).
//
// Shape authority: schemas/envelope/response.schema.json `job`.
// Behaviour authority: docs/adr/ADR-0009 (WS-01) + Docs/JOB_MODEL.md (this module).
// Owner: WS-05. No ToolsetRegistry / ModelContextProtocol dependency.

#pragma once

#include "CoreMinimal.h"

/** Defaults from transport_job_handoff.json (handoff_version ws04-wave1-1). */
struct UEREMCPPROTOCOL_API FUeremcpJobDefaults
{
	/** Default timeout_ms for long operations when a positive timeout is chosen. */
	static constexpr int32 DefaultTimeoutMs = 120000;

	/** Practical client SSE risk threshold — do not hold silent SSE past this. */
	static constexpr int32 ClientSseRiskMs = 30000;

	static constexpr int32 MinTimeoutMs = 1000;
	static constexpr int32 MaxTimeoutMs = 600000;

	/** Default job.poll_action. */
	static const TCHAR* PollAction();
};

/** Envelope `job` block. Present when work continues after timeout_ms. */
struct UEREMCPPROTOCOL_API FUeremcpJob
{
	FString JobId;
	/** queued | running | completed | failed | cancelled */
	FString State = TEXT("running");

	/** Semantic progress in [0, 1]. Not an Epic heartbeat counter (ADR-0009). */
	double Progress = 0.0;
	bool bHasProgress = false;

	FString ProgressMessage;

	/**
	 * True only when cooperative cancel is wired and honored.
	 * Wave 1 default: false — do not advertise cancellable early (ADR-0009).
	 */
	bool bCancellable = false;
	bool bHasCancellable = false;

	FString PollAction = TEXT("get_job_result");

	bool IsValid(FString& OutError) const;
	static bool IsValidState(const FString& State);
};

/** Job-related helpers that do not own the in-process registry. */
class UEREMCPPROTOCOL_API FUeremcpJobUtil
{
public:
	/** ADR-0009: timeout_ms == 0 → complete inline on MCP SSE. */
	static bool ShouldDispatchInline(int32 TimeoutMs);

	/** Clamp a positive timeout into [Min, Max]; 0 stays 0 (inline). */
	static int32 NormaliseTimeoutMs(int32 TimeoutMs);

	/** Fresh UEREMCP job id (UUID string). Per editor process; not MCP request id. */
	static FString NewJobId();
};
