// UEREMCP — idempotency store (ADR-0006 rule 3).
//
// Session memory plus optional disk durability under
// <ProjectSavedDir>/UEREMCP/idempotency/ (WS-12 accepted root; never Intermediate/
// Sandboxes). Owner: WS-05.

#pragma once

#include "CoreMinimal.h"

/**
 * Store of (idempotency_key -> response JSON).
 *
 * Default process store (`Get()`) keeps an in-memory cache and, when durable
 * persistence is enabled, writes each Put under Saved/UEREMCP/idempotency/.
 * TryGet / TryGetReplay hydrate from disk on cache miss so a restarted editor
 * can replay without redoing work.
 *
 * Thread-safety: callers must serialise access (editor tools run on game thread).
 */
class UEREMCPPROTOCOL_API FUeremcpIdempotencyStore
{
public:
	FUeremcpIdempotencyStore();

	/** Look up a stored response. Returns true if present in memory or on disk. */
	bool TryGet(const FString& Key, FString& OutResponseJson) const;

	/**
	 * Replay path (ADR-0006): stored response JSON with metrics.replayed=true and
	 * request_id reflecting the current attempt. Performs no domain work.
	 */
	bool TryGetReplay(const FString& Key, const FString& RequestId, FString& OutResponseJson) const;

	/** Record a completed response. Empty keys are ignored. Persists when durable. */
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

private:
	bool LoadFromDurable(const FString& Key, FString& OutResponseJson) const;
	bool WriteDurable(const FString& Key, const FString& ResponseJson) const;
	FString DurableFilePathForKey(const FString& Key) const;

	/** Mutable so const TryGet can hydrate the session cache from durable files. */
	mutable TMap<FString, FString> Entries;
	bool bDurableEnabled = true;
	FString DurableRootOverride;
};
