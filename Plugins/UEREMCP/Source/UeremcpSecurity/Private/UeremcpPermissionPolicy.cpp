#include "UeremcpPermissionPolicy.h"

#include "UeremcpSecuritySettings.h"

namespace
{
	static FString NormaliseMode(const FString& Mode)
	{
		return Mode.ToLower();
	}

	static bool ModeEquals(const FString& Mode, const TCHAR* Literal)
	{
		return NormaliseMode(Mode) == Literal;
	}
}

bool FUeremcpPermissionPolicy::IsDestructiveContext(
	const FString& Mode,
	bool bTargetExists,
	int32 PredictedDeletedAssetCount)
{
	if (ModeEquals(Mode, TEXT("delete")))
	{
		return true;
	}
	if (ModeEquals(Mode, TEXT("replace")) && bTargetExists)
	{
		return true;
	}
	if (ModeEquals(Mode, TEXT("rebuild_from_specification")) && bTargetExists)
	{
		return true;
	}
	if (PredictedDeletedAssetCount > 0)
	{
		return true;
	}
	return false;
}

EUeremcpPermissionTier FUeremcpPermissionPolicy::TierForMode(const FString& Mode, bool bTargetExists)
{
	if (ModeEquals(Mode, TEXT("delete"))
		|| (ModeEquals(Mode, TEXT("replace")) && bTargetExists)
		|| ModeEquals(Mode, TEXT("rebuild_from_specification")))
	{
		return EUeremcpPermissionTier::Destructive;
	}
	return EUeremcpPermissionTier::Write;
}

bool FUeremcpPermissionPolicy::IsUnsafeAction(const FString& Action)
{
	const FString Normalised = Action.ToLower();
	return Normalised == TEXT("execute_tool_script")
		|| Normalised == TEXT("run_console_command")
		|| Normalised == TEXT("execute_python");
}

FUeremcpPermissionVerdict FUeremcpPermissionPolicy::Evaluate(
	const FString& Action,
	const FString& Mode,
	const FUeremcpPermissionOptions& Options,
	bool bTargetExists,
	const UUeremcpSecuritySettings* Settings)
{
	FUeremcpPermissionVerdict Verdict;
	Verdict.RequiredTier = IsUnsafeAction(Action)
		? EUeremcpPermissionTier::Unsafe
		: TierForMode(Mode, bTargetExists);
	Verdict.bEffectiveDryRun = Options.bDryRun;

	if (Options.PredictedDeletedAssetCount > 0
		&& static_cast<uint8>(Verdict.RequiredTier) < static_cast<uint8>(EUeremcpPermissionTier::Destructive))
	{
		Verdict.RequiredTier = EUeremcpPermissionTier::Destructive;
	}

	const UUeremcpSecuritySettings* EffectiveSettings = Settings ? Settings : UUeremcpSecuritySettings::Get();
	const EUeremcpPermissionTier MaxTier = EffectiveSettings
		? EffectiveSettings->MaxProjectTier
		: EUeremcpPermissionTier::Write;
	const bool bUnsafeAllowed = EffectiveSettings && EffectiveSettings->bAllowUnsafe;

	if (Verdict.RequiredTier == EUeremcpPermissionTier::Unsafe && !bUnsafeAllowed)
	{
		Verdict.bAllowed = false;
		Verdict.DenialReason = TEXT("unsafe tier is disabled in project settings");
		return Verdict;
	}

	// Predicted deletes outside delete/replace/rebuild require explicit allow_destructive
	// (ADR-0010 §2). Mode-native destructive ops do not need the flag.
	if (Options.PredictedDeletedAssetCount > 0
		&& !Options.bAllowDestructive
		&& !ModeEquals(Mode, TEXT("delete"))
		&& !ModeEquals(Mode, TEXT("replace"))
		&& !ModeEquals(Mode, TEXT("rebuild_from_specification")))
	{
		Verdict.bAllowed = false;
		Verdict.DenialReason = TEXT("operation predicts deletions but allow_destructive is false");
		return Verdict;
	}

	if (static_cast<uint8>(Verdict.RequiredTier) > static_cast<uint8>(MaxTier)
		&& Verdict.RequiredTier != EUeremcpPermissionTier::Destructive)
	{
		Verdict.bAllowed = false;
		Verdict.DenialReason = TEXT("required tier exceeds project MaxProjectTier");
		return Verdict;
	}

	if (IsDestructiveContext(Mode, bTargetExists, Options.PredictedDeletedAssetCount))
	{
		if (!Options.bDryRunWasExplicit)
		{
			Verdict.bEffectiveDryRun = true;
			Verdict.bDryRunForced = true;
		}
		else
		{
			Verdict.bEffectiveDryRun = Options.bDryRun;
		}
	}

	return Verdict;
}
