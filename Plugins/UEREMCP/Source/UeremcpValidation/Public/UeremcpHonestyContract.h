// POC E honesty contract helpers (WS-11).
//
// Enforces AGENTS.md rule 6 / POC_ACCEPTANCE E5–E6:
//   - options.validate=false forfeits any *_validated status
//   - broken requests must surface failed_validation (or rejected) with actionable text
// Domains remain responsible for calling these helpers; Validation locks the contract.

#pragma once

#include "CoreMinimal.h"

struct FUeremcpRequest;
struct FUeremcpResponse;

namespace UeremcpHonestyContract
{
	/** True for created_and_validated / modified_and_validated. */
	bool IsValidatedStatus(const FString& Status);

	/**
	 * When Request.bValidate is false, any *_validated proposed status becomes
	 * partially_completed. Other statuses are left unchanged.
	 */
	FString ResolveStatusHonoringValidateFlag(
		const FUeremcpRequest& Request,
		const FString& ProposedStatus);

	/**
	 * Apply ResolveStatusHonoringValidateFlag to Response.Status and append a
	 * capability note when a validated claim was forfeited.
	 */
	void ApplyValidateFlagToResponse(
		const FUeremcpRequest& Request,
		FUeremcpResponse& Response);

	/** True when status is failed_validation or rejected (honest non-success). */
	bool IsHonestFailureStatus(const FString& Status);

	/** True when Summary or CapabilityNotes contain non-empty actionable text. */
	bool HasActionableDiagnostics(const FUeremcpResponse& Response);
}
