// UEREMCP — idempotency store (ADR-0006 rule 3).
//
// Session memory plus optional disk durability under
// <ProjectSavedDir>/UEREMCP/idempotency/ (WS-12 accepted root; never Intermediate/
// Sandboxes). Owner: WS-05.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

enum class EUeremcpIdempotencyClaimStatus : uint8
{
	Acquired,
	Replay,
	Conflict,
	InProgress,
	Error
};

struct UEREMCPPROTOCOL_API FUeremcpIdempotencyClaim
{
	EUeremcpIdempotencyClaimStatus Status = EUeremcpIdempotencyClaimStatus::Error;
	FString ResponseJson;
	FString Error;
};

/**
 * Store of versioned (idempotency_key, request_fingerprint -> response JSON) records.
 *
 * Default process store (`Get()`) keeps an in-memory cache and, when durable
 * persistence is enabled, writes each Put under Saved/UEREMCP/idempotency/.
 * TryGet / TryGetReplay hydrate from disk on cache miss so a restarted editor
 * can replay without redoing work.
 *
 * Claim/Complete is the production path. It reserves a key before mutation, rejects
 * conflicting reuse, and writes completion using temp-file + rename replacement.
 */
class UEREMCPPROTOCOL_API FUeremcpIdempotencyStore
{
public:
	FUeremcpIdempotencyStore();

	/**
	 * Reserve a key before domain work. Same key+fingerprint replays a completed
	 * response; same key+different fingerprint conflicts; live claims report
	 * InProgress. A stale claim may be reclaimed after StaleClaimAge.
	 */
	FUeremcpIdempotencyClaim Claim(
		const FString& Key,
		const FString& RequestFingerprint,
		const FString& RequestId);

	/** Atomically turn this process's matching claim into a completed record. */
	bool Complete(
		const FString& Key,
		const FString& RequestFingerprint,
		const FString& ResponseJson,
		FString& OutError);

	/** Remove a matching in-progress claim after work failed before mutation. */
	bool Abandon(
		const FString& Key,
		const FString& RequestFingerprint,
		FString& OutError);

	/** Delete one UEREMCP-owned record. Does not touch assets or other keys. */
	bool Remove(const FString& Key, FString& OutError);

	/** Look up a stored response. Returns true if present in memory or on disk. */
	bool TryGet(const FString& Key, FString& OutResponseJson) const;

	/**
	 * Replay path (ADR-0006): stored response JSON with metrics.replayed=true and
	 * request_id reflecting the current attempt. Performs no domain work.
	 */
	bool TryGetReplay(const FString& Key, const FString& RequestId, FString& OutResponseJson) const;

	/** Legacy unbound record API. Prefer Claim/Complete for production callers. */
	void Put(const FString& Key, const FString& ResponseJson);

	/** Drop the in-memory cache only. Durable files are left intact. */
	void Clear();

	/**
	 * Delete durable record files under the active durable root and clear memory.
	 * Used by automation against a temp root; do not call on the process store
	 * casually in production tools.
	 */
	void PurgeDurable();

	int32 Num() const { return Entries.Num(); }

	/** Process-wide default store for the editor session. */
	static FUeremcpIdempotencyStore& Get();

	/**
	 * Default durable directory: <ProjectSavedDir>/UEREMCP/idempotency/
	 * [VERIFIED: docs/proposals/ws-12-idempotency-store-root.md].
	 */
	static FString DefaultDurableRoot();

	/** Enable or disable disk persistence for this store instance. */
	void SetDurableEnabled(bool bEnabled);
	bool IsDurableEnabled() const { return bDurableEnabled; }

	/**
	 * Override the durable directory (absolute path). Empty restores DefaultDurableRoot().
	 * Intended for automation and isolated worktrees.
	 */
	void SetDurableRootOverride(const FString& AbsoluteDirectory);
	FString GetDurableRoot() const;

	/** Stable lowercase hex filename stem for a key (SHA-256 of UTF-8 bytes). */
	static FString DurableFileStemForKey(const FString& Key);

	/**
	 * Exact request fingerprint after removing retry-only request_id and
	 * idempotency_key. expected_revision remains part of the fingerprint.
	 */
	static bool FingerprintRequestJson(
		const FString& RequestJson,
		FString& OutFingerprint,
		FString& OutError);

	void SetRetention(FTimespan InRetention) { Retention = InRetention; }
	void SetStaleClaimAge(FTimespan InAge) { StaleClaimAge = InAge; }

private:
	struct FRecord
	{
		FString Key;
		FString RequestFingerprint;
		FString State;
		FString ResponseJson;
		FDateTime CreatedAtUtc;
		FDateTime UpdatedAtUtc;
	};

	bool LoadFromDurable(const FString& Key, FString& OutResponseJson) const;
	bool WriteDurable(const FString& Key, const FString& ResponseJson) const;
	bool LoadRecordFromDurable(
		const FString& Key,
		FRecord& OutRecord,
		FString& OutError,
		bool bQuarantineCorrupt) const;
	bool WriteRecordDurable(const FRecord& Record, FString& OutError) const;
	bool DeleteRecordDurable(const FString& Key, FString& OutError) const;
	bool IsExpired(const FRecord& Record, const FDateTime& Now) const;
	bool IsClaimStale(const FRecord& Record, const FDateTime& Now) const;
	FString DurableFilePathForKey(const FString& Key) const;

	mutable TMap<FString, FRecord> Entries;
	mutable FCriticalSection EntriesMutex;
	bool bDurableEnabled = true;
	FString DurableRootOverride;
	FTimespan Retention = FTimespan::FromDays(7);
	FTimespan StaleClaimAge = FTimespan::FromHours(1);
};
