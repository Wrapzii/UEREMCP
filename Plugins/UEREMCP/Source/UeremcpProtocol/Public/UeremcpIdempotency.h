// UEREMCP — idempotency store (ADR-0006 rule 3).
//
// Minimum bar: in-memory for the editor session. Surviving restart is preferred but
// not required for v1 — see docs/proposals/ws-05-idempotency-store.md.
// Owner: WS-05.

#pragma once

#include "CoreMinimal.h"

/**
 * Session-scoped store of (idempotency_key -> response JSON).
 * Thread-safety: callers must serialise access (editor tools run on game thread).
 */
class UEREMCPPROTOCOL_API FUeremcpIdempotencyStore
{
public:
	/** Look up a stored response. Returns true if present. */
	bool TryGet(const FString& Key, FString& OutResponseJson) const;

	/** Record a completed response. Empty keys are ignored. */
	void Put(const FString& Key, const FString& ResponseJson);

	void Clear();

	int32 Num() const { return Entries.Num(); }

	/** Process-wide default store for the editor session. */
	static FUeremcpIdempotencyStore& Get();

private:
	TMap<FString, FString> Entries;
};
