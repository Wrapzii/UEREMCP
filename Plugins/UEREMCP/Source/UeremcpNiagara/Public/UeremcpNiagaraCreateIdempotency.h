// UEREMCP — ADR-0006 create idempotency / revision helpers for Niagara (WS-07).
//
// Revision is a content hash over a stable structural fingerprint of an existing
// UNiagaraSystem (emitter names + user parameter names). Repeated create with the
// same specification returns no_change_required when the live system already
// satisfies the requested component roles.

#pragma once

#include "CoreMinimal.h"
#include "UeremcpNiagaraCreate.h"

namespace UeremcpNiagaraCreateIdempotency
{
	/** Soft/object path → package/object path used by create (folder/name). */
	FString CreatedAssetPathFromRequest(const FString& TargetAssetPath, const FString& SpecName);

	/**
	 * Compute sha256 revision of an existing Niagara system.
	 * [VERIFIED: FUeremcpContentHash::HashJsonObject — UeremcpContentHash.h]
	 */
	bool TryComputeAssetRevision(const FString& AssetPath, FString& OutRevision, FString& OutError);

	/** True when every Spec.ComponentRoles emitter already exists on the live system. */
	bool ExistingSatisfiesSpec(const FString& AssetPath, const FUeremcpNiagaraCreateSpec& Spec);

	/** Envelope on_revision_conflict bypasses (replace|force). */
	bool ShouldBypassRevisionConflict(const FString& OnRevisionConflict);
}
