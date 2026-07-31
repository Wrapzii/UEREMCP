#include "UeremcpNetworkingService.h"

#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/SecureHash.h"
#include "UObject/UnrealType.h"
#include "UeremcpSystemsHelpers.h"

namespace
{
	FString ModeFromFlags(uint64 PropertyFlags, const FName& RepNotifyFunc)
	{
		const bool bNet = (PropertyFlags & CPF_Net) != 0;
		const bool bRepNotify = (PropertyFlags & CPF_RepNotify) != 0 || !RepNotifyFunc.IsNone();
		if (bNet && bRepNotify)
		{
			return TEXT("REP_NOTIFY");
		}
		if (bNet)
		{
			return TEXT("REPLICATED");
		}
		return TEXT("NONE");
	}

	bool ApplyMode(UBlueprint* Blueprint, const FName& VarName, const FString& Mode, FString& OutError)
	{
		uint64* Flags = FBlueprintEditorUtils::GetBlueprintVariablePropertyFlags(Blueprint, VarName);
		if (!Flags)
		{
			OutError = FString::Printf(TEXT("variable '%s' not found on Blueprint"), *VarName.ToString());
			return false;
		}

		*Flags &= ~(CPF_Net | CPF_RepNotify);
		FBlueprintEditorUtils::SetBlueprintVariableRepNotifyFunc(Blueprint, VarName, NAME_None);

		if (Mode.Equals(TEXT("REPLICATED"), ESearchCase::IgnoreCase))
		{
			*Flags |= CPF_Net;
		}
		else if (Mode.Equals(TEXT("REP_NOTIFY"), ESearchCase::IgnoreCase))
		{
			*Flags |= CPF_Net | CPF_RepNotify;
			const FName NotifyName(*FString::Printf(TEXT("OnRep_%s"), *VarName.ToString()));
			FBlueprintEditorUtils::SetBlueprintVariableRepNotifyFunc(Blueprint, VarName, NotifyName);
		}
		else if (!Mode.Equals(TEXT("NONE"), ESearchCase::IgnoreCase))
		{
			OutError = FString::Printf(TEXT("unsupported replication mode '%s'"), *Mode);
			return false;
		}

		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		return true;
	}

	FString HashReport(const FUeremcpReplicationReport& Report)
	{
		FSHA1 Sha;
		auto Feed = [&Sha](const FString& S)
		{
			const FTCHARToUTF8 Utf8(*S);
			Sha.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		};
		Feed(Report.bPatternBValid ? TEXT("1") : TEXT("0"));
		for (const FUeremcpReplicationCheck& Check : Report.VariableChecks)
		{
			Feed(Check.VariableName);
			Feed(Check.ObservedMode);
			Feed(Check.DesiredMode);
			Feed(Check.bMatch ? TEXT("1") : TEXT("0"));
		}
		Sha.Final();
		uint8 Digest[FSHA1::DigestSize];
		Sha.GetHash(Digest);
		return BytesToHex(Digest, FSHA1::DigestSize).ToLower();
	}
}

bool FUeremcpNetworkingService::CheckPatternB(
	const FString& Pattern,
	const FString& Authority,
	const FString& CastPath,
	FString& OutError)
{
	if (Pattern != TEXT("B")
		|| Authority != TEXT("server")
		|| CastPath != TEXT("AuthorityCastAbility"))
	{
		OutError = TEXT("networking must declare RE Pattern B: pattern=B, authority=server, cast_path=AuthorityCastAbility");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FUeremcpNetworkingService::ParseValidateSpec(
	const TSharedPtr<FJsonObject>& Specification,
	TArray<FUeremcpReplicationExpectation>& OutExpectations,
	bool& bOutRequirePatternB,
	FString& OutPattern,
	FString& OutAuthority,
	FString& OutCastPath,
	bool& bOutApplyFixes,
	FString& OutError)
{
	OutExpectations.Reset();
	bOutRequirePatternB = false;
	bOutApplyFixes = false;
	OutPattern.Reset();
	OutAuthority.Reset();
	OutCastPath.Reset();

	if (!Specification.IsValid())
	{
		OutError = TEXT("validate_replication requires specification");
		return false;
	}

	Specification->TryGetBoolField(TEXT("apply_fixes"), bOutApplyFixes);

	const TSharedPtr<FJsonObject>* NetworkingObj = nullptr;
	if (Specification->TryGetObjectField(TEXT("networking"), NetworkingObj) && NetworkingObj && (*NetworkingObj).IsValid())
	{
		bOutRequirePatternB = true;
		(*NetworkingObj)->TryGetStringField(TEXT("pattern"), OutPattern);
		(*NetworkingObj)->TryGetStringField(TEXT("authority"), OutAuthority);
		(*NetworkingObj)->TryGetStringField(TEXT("cast_path"), OutCastPath);
	}

	const TArray<TSharedPtr<FJsonValue>>* Vars = nullptr;
	if (Specification->TryGetArrayField(TEXT("variables"), Vars) && Vars)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Vars)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!Value.IsValid() || !Value->TryGetObject(Obj) || !Obj || !(*Obj).IsValid())
			{
				OutError = TEXT("specification.variables entries must be objects");
				return false;
			}
			FUeremcpReplicationExpectation Expectation;
			if (!(*Obj)->TryGetStringField(TEXT("name"), Expectation.VariableName)
				|| Expectation.VariableName.IsEmpty())
			{
				OutError = TEXT("specification.variables[].name is required");
				return false;
			}
			if (!(*Obj)->TryGetStringField(TEXT("replication"), Expectation.DesiredMode)
				|| Expectation.DesiredMode.IsEmpty())
			{
				OutError = TEXT("specification.variables[].replication is required (NONE|REPLICATED|REP_NOTIFY)");
				return false;
			}
			OutExpectations.Add(MoveTemp(Expectation));
		}
	}

	if (!bOutRequirePatternB && OutExpectations.Num() == 0)
	{
		OutError = TEXT("validate_replication requires specification.networking and/or specification.variables");
		return false;
	}
	return true;
}

bool FUeremcpNetworkingService::ValidateBlueprintReplication(
	const FString& BlueprintAssetPath,
	const TArray<FUeremcpReplicationExpectation>& Expectations,
	bool bRequirePatternB,
	const FString& Pattern,
	const FString& Authority,
	const FString& CastPath,
	bool bApplyFixes,
	bool bDryRun,
	FUeremcpReplicationReport& OutReport,
	FString& OutError)
{
	OutReport = FUeremcpReplicationReport();

	if (bRequirePatternB)
	{
		OutReport.bPatternBDeclared = true;
		OutReport.bPatternBValid = CheckPatternB(Pattern, Authority, CastPath, OutReport.PatternBError);
	}

	const FString ObjectPath = UeremcpSystems::ResolveObjectPath(BlueprintAssetPath);
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath);
	if (!Blueprint)
	{
		OutError = FString::Printf(TEXT("Blueprint '%s' could not be loaded"), *BlueprintAssetPath);
		return false;
	}

	bool bModified = false;
	for (const FUeremcpReplicationExpectation& Expectation : Expectations)
	{
		FUeremcpReplicationCheck Check;
		Check.VariableName = Expectation.VariableName;
		Check.DesiredMode = Expectation.DesiredMode;

		const FName VarName(*Expectation.VariableName);
		const uint64* Flags = FBlueprintEditorUtils::GetBlueprintVariablePropertyFlags(Blueprint, VarName);
		if (!Flags)
		{
			Check.ObservedMode = TEXT("MISSING");
			Check.bMatch = false;
			OutReport.MismatchCount++;
			OutReport.VariableChecks.Add(Check);
			continue;
		}

		const FName RepNotify = FBlueprintEditorUtils::GetBlueprintVariableRepNotifyFunc(Blueprint, VarName);
		Check.ObservedMode = ModeFromFlags(*Flags, RepNotify);
		Check.bMatch = Check.ObservedMode.Equals(Check.DesiredMode, ESearchCase::IgnoreCase);

		if (!Check.bMatch && bApplyFixes && !bDryRun)
		{
			FString ApplyError;
			if (!ApplyMode(Blueprint, VarName, Check.DesiredMode, ApplyError))
			{
				OutError = ApplyError;
				return false;
			}
			bModified = true;
			const uint64* NewFlags = FBlueprintEditorUtils::GetBlueprintVariablePropertyFlags(Blueprint, VarName);
			const FName NewNotify = FBlueprintEditorUtils::GetBlueprintVariableRepNotifyFunc(Blueprint, VarName);
			Check.ObservedMode = ModeFromFlags(NewFlags ? *NewFlags : 0, NewNotify);
			Check.bMatch = Check.ObservedMode.Equals(Check.DesiredMode, ESearchCase::IgnoreCase);
		}

		if (Check.bMatch)
		{
			OutReport.MatchCount++;
		}
		else
		{
			OutReport.MismatchCount++;
		}
		OutReport.VariableChecks.Add(Check);
	}

	if (bModified)
	{
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
	}

	OutReport.ContentHash = HashReport(OutReport);
	return true;
}
