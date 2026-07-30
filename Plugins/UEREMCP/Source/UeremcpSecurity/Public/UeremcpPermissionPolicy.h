// UEREMCP — envelope permission gate (ADR-0010 §2–§4).
//
// Central policy table keyed by (action, mode, target_exists). Domains call this;
// they do not fork destructive dry_run logic.

#pragma once

#include "CoreMinimal.h"
#include "UeremcpSecurityTypes.h"

class UEREMCPSECURITY_API FUeremcpPermissionPolicy
{
public:
	/**
	 * Evaluate tier requirement and destructive dry_run override.
	 *
	 * @param Action Envelope action string (e.g. create_blueprint_graph).
	 * @param Mode Envelope mode (create, delete, replace, …).
	 * @param Options Parsed options including dry_run explicitness.
	 * @param bTargetExists Whether the target asset already exists.
	 * @param Settings Project settings; nullptr uses defaults (unsafe off).
	 */
	static FUeremcpPermissionVerdict Evaluate(
		const FString& Action,
		const FString& Mode,
		const FUeremcpPermissionOptions& Options,
		bool bTargetExists,
		const class UUeremcpSecuritySettings* Settings = nullptr);

	static EUeremcpPermissionTier TierForMode(const FString& Mode, bool bTargetExists);

	/**
	 * True for console/OS escape hatches (ADR-0010 §2). Requests cannot self-elevate;
	 * only UUeremcpSecuritySettings::bAllowUnsafe can admit these actions.
	 */
	static bool IsUnsafeAction(const FString& Action);

	static bool IsDestructiveContext(const FString& Mode, bool bTargetExists, int32 PredictedDeletedAssetCount);
};
