// UEREMCP — Core pre-domain security dispatch gate (ADR-0010, WS-12 proposal).
//
// Owner: WS-03. Domains call this before mutating work; they do not fork permission,
// path, mutator-queue, or audit logic.

#pragma once

#include "CoreMinimal.h"
#include "UeremcpEnvelope.h"
#include "UeremcpSecurityTypes.h"

/**
 * RAII gate: permission + path validation, FIFO mutator serialization, audit append,
 * and guaranteed mutator release on every terminal path.
 */
class UEREMCPCORE_API FUeremcpMutatingDispatch
{
public:
	FUeremcpMutatingDispatch();
	~FUeremcpMutatingDispatch();

	FUeremcpMutatingDispatch(const FUeremcpMutatingDispatch&) = delete;
	FUeremcpMutatingDispatch& operator=(const FUeremcpMutatingDispatch&) = delete;

	/**
	 * Run the pre-domain security gate.
	 *
	 * @param RequestJson Full request envelope JSON.
	 * @param bTargetExists Whether the target asset already exists.
	 * @param PredictedDeletedAssetCount Deletions the domain expects (permission input).
	 * @param bReadOnlyOperation When true, mutator queue is bypassed (read tier).
	 * @param OutBlockingResponseJson Set when false is returned; caller must return this JSON.
	 * @return true when domain work may proceed; false when blocked (rejection or queued).
	 */
	bool TryBegin(
		const FString& RequestJson,
		bool bTargetExists,
		int32 PredictedDeletedAssetCount,
		bool bReadOnlyOperation,
		FString& OutBlockingResponseJson);

	/** Append audit, release mutator, serialize the terminal response. */
	FString Complete(const FUeremcpResponse& Response);

	const FUeremcpRequest& GetRequest() const { return Request; }
	const FUeremcpPermissionVerdict& GetVerdict() const { return Verdict; }
	bool IsEffectiveDryRun() const { return Verdict.bEffectiveDryRun; }
	bool HoldsMutatorSlot() const { return bMutatorHeld; }

private:
	static FUeremcpPermissionOptions BuildPermissionOptions(
		const FString& RequestJson,
		const FUeremcpRequest& ParsedRequest,
		int32 PredictedDeletedAssetCount);

	static FString MakeMutatorQueuedResponse(
		const FUeremcpRequest& Request,
		const FString& JobId,
		const FString& Reason);

	void ReleaseMutator();
	bool AppendAuditForResponse(const FUeremcpResponse& Response, TArray<FString>& OutNotes);

	FUeremcpRequest Request;
	FUeremcpPermissionVerdict Verdict;
	FUeremcpPathPolicyRoots PathRoots;
	FString ProjectKey;
	bool bMutatorHeld = false;
	bool bGateOpen = false;
};
