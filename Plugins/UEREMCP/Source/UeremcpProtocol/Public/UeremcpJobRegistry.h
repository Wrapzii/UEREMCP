// UEREMCP — in-process long-running job registry (ADR-0009).
//
// Owner: WS-05. Pure protocol/lifecycle logic; Core owns tool registration and
// domain services own the work and cooperative cancellation callbacks.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Templates/Function.h"
#include "UeremcpEnvelope.h"

enum class EUeremcpJobState : uint8
{
	Queued,
	Running,
	Completed,
	Failed,
	Cancelled,
};

struct UEREMCPPROTOCOL_API FUeremcpJobRegistryConfig
{
	/** Hard in-process bound. New jobs are rejected when every retained job is active. */
	int32 MaxJobs = 1024;

	/** Terminal jobs remain pollable for this long. */
	int64 TerminalRetentionMs = 5 * 60 * 1000;

	/** Active jobs older than this fail instead of remaining immortal. */
	int64 MaxActiveAgeMs = 24 * 60 * 60 * 1000;

	bool IsValid(FString& OutError) const;
};

struct UEREMCPPROTOCOL_API FUeremcpJobSnapshot
{
	FString RequestId;
	FUeremcpJob Job;
	int32 PollCount = 0;
	FDateTime CreatedAt;
	FDateTime UpdatedAt;
	FDateTime TerminalAt;
	bool bHasTerminalAt = false;
};

enum class EUeremcpCancelResult : uint8
{
	Cancelled,
	NotFound,
	NotCancellable,
	AlreadyTerminal,
	CancellationPending,
	RejectedByWorker,
};

/**
 * Thread-safe, process-local registry for ADR-0009 jobs.
 *
 * The registry does not schedule domain work. Callers create a handle before
 * dispatch, report state/progress, then store the verified terminal envelope.
 */
class UEREMCPPROTOCOL_API FUeremcpJobRegistry
{
public:
	explicit FUeremcpJobRegistry(const FUeremcpJobRegistryConfig& InConfig = FUeremcpJobRegistryConfig());

	/** Process-wide registry used by Core's public job actions. */
	static FUeremcpJobRegistry& Get();

	/**
	 * Create a queued job. Cancellation is advertised only when a callback exists
	 * and bCancellable is true.
	 */
	bool CreateJob(
		const FString& RequestId,
		bool bCancellable,
		const FString& InitialProgressMessage,
		FString& OutJobId,
		FString& OutError,
		TFunction<bool()>&& RequestCancel = TFunction<bool()>());

	bool StartJob(const FString& JobId, FString& OutError);

	/** Progress is bounded to [0,1] and cannot move backwards. */
	bool UpdateProgress(
		const FString& JobId,
		double Progress,
		const FString& ProgressMessage,
		FString& OutError);

	/**
	 * Store a terminal, verified response. The response must be envelope-valid and
	 * cannot itself claim partially_completed.
	 */
	bool CompleteJob(
		const FString& JobId,
		const FUeremcpResponse& TerminalResponse,
		FString& OutError);

	/** Terminal failure with an accepted `error` envelope. */
	bool FailJob(const FString& JobId, const FString& Summary, FString& OutError);

	/** Request cooperative cancellation, then transition only when the worker honors it. */
	EUeremcpCancelResult CancelJob(const FString& JobId, FString& OutError);

	/**
	 * Initiating-call timeout behavior. Returns current state without incrementing
	 * PollCount, so the first partially_completed response reports one round trip.
	 */
	bool GetTimeoutResponse(const FString& JobId, FUeremcpResponse& OutResponse, FString& OutError);

	/**
	 * Public get_job_result behavior. Every successful call increments PollCount and
	 * reports metrics.mcp_round_trips as originating call + polls.
	 */
	bool GetJobResult(const FString& JobId, FUeremcpResponse& OutResponse, FString& OutError);

	/** Read state without counting an MCP poll. */
	bool GetSnapshot(const FString& JobId, FUeremcpJobSnapshot& OutSnapshot) const;

	/** Remove expired terminal entries and fail stale active entries. */
	int32 CleanupExpired();

	/** Deterministic clock variant for automation tests. */
	int32 CleanupExpiredAt(const FDateTime& Now);

	int32 Num() const;

	/** Test/module-shutdown utility. Does not cancel domain work. */
	void Clear();

	static FString StateToString(EUeremcpJobState State);
	static bool IsTerminal(EUeremcpJobState State);
	static bool IsValidTransition(EUeremcpJobState From, EUeremcpJobState To);

private:
	struct FEntry
	{
		FString RequestId;
		EUeremcpJobState State = EUeremcpJobState::Queued;
		double Progress = 0.0;
		bool bHasProgress = false;
		FString ProgressMessage;
		bool bCancellable = false;
		bool bCancellationPending = false;
		TSharedPtr<TFunction<bool()>, ESPMode::ThreadSafe> RequestCancel;
		int32 PollCount = 0;
		FDateTime CreatedAt;
		FDateTime UpdatedAt;
		FDateTime TerminalAt;
		bool bHasTerminalAt = false;
		FUeremcpResponse TerminalResponse;
		bool bHasTerminalResponse = false;
	};

	bool TransitionLocked(FEntry& Entry, EUeremcpJobState To, const FDateTime& Now, FString& OutError);
	void AttachJobLocked(const FString& JobId, const FEntry& Entry, FUeremcpResponse& Response) const;
	FUeremcpResponse MakeInFlightResponseLocked(const FString& JobId, const FEntry& Entry) const;
	void SetFailedLocked(
		const FString& JobId,
		FEntry& Entry,
		const FString& Summary,
		const FDateTime& Now);
	int32 CleanupExpiredLocked(const FDateTime& Now);
	bool EnsureCapacityLocked(const FDateTime& Now, FString& OutError);

	FUeremcpJobRegistryConfig Config;
	mutable FCriticalSection Mutex;
	TMap<FString, FEntry> Entries;
};
