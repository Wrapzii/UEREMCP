// UEREMCP — domain adoption recipe for ADR-0010 (WS-12).
//
// Preferred gate (do not reimplement):
//   #include "UeremcpMutatingDispatch.h"   // UeremcpCore, owner WS-03
//   FUeremcpMutatingDispatch Gate;
//   if (!Gate.TryBegin(RequestJson, bTargetExists, PredictedDeleted, bReadOnly, Blocking))
//       return Blocking;
//   // ... domain mutate + verify ...
//   return Gate.Complete(Response);
//
// Build.cs private deps: "UeremcpCore", "UeremcpSecurity"
//   (MutatingDispatch.h pulls UeremcpSecurityTypes.h)
// [VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Public/UeremcpMutatingDispatch.h]
//
// Reference adopters:
//   UeremcpGameplayToolset::CreateSpell
//   FUeremcpBlueprintMutatingGate (WS-06 adapter)
//   UeremcpMaterialToolset::CreateVfxMaterial / CreateProceduralTexture
//   UeremcpNiagaraToolset::CreateNiagaraEffect
// (see docs/proposals/ws-12-*-mutating-dispatch-handoff.md).

#pragma once

#include "CoreMinimal.h"
#include "UeremcpPathPolicy.h"
#include "UeremcpPermissionPolicy.h"
#include "UeremcpSecurityTypes.h"

/**
 * Helpers that keep domain PredictedDeleted / soft-path checks aligned with policy
 * without forking ADR-0010 logic. Domains still must route live mutators through
 * FUeremcpMutatingDispatch for queue + audit.
 */
class UEREMCPSECURITY_API FUeremcpSecurityDomainAdoption
{
public:
	/** Core header domains must include for the RAII gate. */
	static const TCHAR* PreferredGateHeader();

	/**
	 * Predicted deletions for replace/rebuild when the target already exists.
	 * Pass the result as PredictedDeletedAssetCount into TryBegin / Evaluate.
	 */
	static int32 PredictedDeletedForDestructiveReplace(bool bTargetExists, bool bDestructiveMode);

	/**
	 * Build permission options with dry_run presence preserved (MutatingDispatch does
	 * the same from raw request JSON; use this when options are already parsed).
	 */
	static FUeremcpPermissionOptions MakePermissionOptions(
		bool bDryRun,
		bool bDryRunWasExplicit,
		bool bAllowDestructive = false,
		int32 PredictedDeletedAssetCount = 0);

	/**
	 * Soft-path write gate only (no queue/audit). Useful for unit tests and for
	 * domains that reject before constructing a full response.
	 */
	static FUeremcpPathValidationResult ValidateWriteSoftPath(
		const FString& SoftPath,
		const FUeremcpPathPolicyRoots* Roots = nullptr);

	/**
	 * Full permission evaluate for the same inputs domains feed MutatingDispatch.
	 * Does not acquire the mutator queue — call only for dry_run / preflight probes.
	 */
	static FUeremcpPermissionVerdict EvaluatePermission(
		const FString& Action,
		const FString& Mode,
		const FUeremcpPermissionOptions& Options,
		bool bTargetExists,
		const class UUeremcpSecuritySettings* Settings = nullptr);
};
