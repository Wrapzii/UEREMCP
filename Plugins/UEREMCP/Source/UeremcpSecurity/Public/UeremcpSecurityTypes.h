// UEREMCP — shared security types (ADR-0010).
#pragma once

#include "CoreMinimal.h"
#include "UeremcpPermissionTier.h"

/** Result of path validation. */
struct UEREMCPSECURITY_API FUeremcpPathValidationResult
{
	bool bAllowed = false;
	FString Reason;

	static FUeremcpPathValidationResult Allowed();
	static FUeremcpPathValidationResult Denied(const FString& InReason);
};

/** Injectable roots for unit tests (no editor required). */
struct UEREMCPSECURITY_API FUeremcpPathPolicyRoots
{
	FString ProjectDir;
	FString ProjectContentDir;
	FString ProjectSavedDir;

	bool IsConfigured() const;
};

/** Parsed permission-relevant options from the request envelope. */
struct UEREMCPSECURITY_API FUeremcpPermissionOptions
{
	bool bDryRun = false;

	/** True when the request JSON included options.dry_run (even when false). */
	bool bDryRunWasExplicit = false;

	/** Future schema field; ADR-0010 open question. */
	bool bAllowDestructive = false;

	int32 PredictedDeletedAssetCount = 0;
};

/** Outcome of FUeremcpPermissionPolicy::Evaluate. */
struct UEREMCPSECURITY_API FUeremcpPermissionVerdict
{
	bool bAllowed = true;
	EUeremcpPermissionTier RequiredTier = EUeremcpPermissionTier::Write;

	/** Effective dry_run after ADR-0010 destructive override. */
	bool bEffectiveDryRun = false;

	/** True when policy forced dry_run on without an explicit caller value. */
	bool bDryRunForced = false;

	FString DenialReason;
};
