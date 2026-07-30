// Blueprint domain adapter for FUeremcpMutatingDispatch (orch UeremcpCore).
// Compile with UEREMCP_BLUEPRINT_MUTATING_DISPATCH=1 after orch merge lands Core header.

#pragma once

#include "CoreMinimal.h"
#include "UeremcpEnvelope.h"

class UEREMCPBLUEPRINT_API FUeremcpBlueprintMutatingGate
{
public:
	/** read_graph — permission/path gate; read tier bypasses mutator queue when enabled. */
	bool TryBeginRead(const FString& RequestJson, FString& OutBlockingResponseJson);

	/** submit_graph live replace — mutator + permission gate (caller skips when dry_run). */
	bool TryBeginMutating(
		const FString& RequestJson,
		bool bTargetExists,
		FString& OutBlockingResponseJson);

	bool IsActive() const { return bActive; }
	bool IsEffectiveDryRun() const { return bEffectiveDryRun; }

	/** Audit + mutator release when Core dispatch is enabled; else plain serialize. */
	FString Complete(const FUeremcpResponse& Response);

private:
	bool bActive = false;
	bool bEffectiveDryRun = false;

#if UEREMCP_BLUEPRINT_MUTATING_DISPATCH
	class FDispatchHolder;
	TUniquePtr<FDispatchHolder> Dispatch;
#endif
};
