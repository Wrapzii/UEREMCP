#pragma once

#include "CoreMinimal.h"
#include "UeremcpEnvelope.h"

struct FUeremcpReplicationExpectation
{
	FString VariableName;
	/** NONE | REPLICATED | REP_NOTIFY */
	FString DesiredMode;
};

struct FUeremcpReplicationCheck
{
	FString VariableName;
	FString ObservedMode;
	FString DesiredMode;
	bool bMatch = false;
};

struct FUeremcpReplicationReport
{
	bool bPatternBDeclared = false;
	bool bPatternBValid = false;
	FString PatternBError;
	TArray<FUeremcpReplicationCheck> VariableChecks;
	int32 MatchCount = 0;
	int32 MismatchCount = 0;
	FString ContentHash;
};

class UEREMCPSYSTEMS_API FUeremcpNetworkingService
{
public:
	static bool ParseValidateSpec(
		const TSharedPtr<FJsonObject>& Specification,
		TArray<FUeremcpReplicationExpectation>& OutExpectations,
		bool& bOutRequirePatternB,
		FString& OutPattern,
		FString& OutAuthority,
		FString& OutCastPath,
		bool& bOutApplyFixes,
		FString& OutError);

	static bool ValidateBlueprintReplication(
		const FString& BlueprintAssetPath,
		const TArray<FUeremcpReplicationExpectation>& Expectations,
		bool bRequirePatternB,
		const FString& Pattern,
		const FString& Authority,
		const FString& CastPath,
		bool bApplyFixes,
		bool bDryRun,
		FUeremcpReplicationReport& OutReport,
		FString& OutError);

	/** Pure Pattern B checklist — unit-testable without loading a Blueprint. */
	static bool CheckPatternB(
		const FString& Pattern,
		const FString& Authority,
		const FString& CastPath,
		FString& OutError);
};
