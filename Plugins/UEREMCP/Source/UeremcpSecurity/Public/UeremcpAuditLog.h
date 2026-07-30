// UEREMCP — append-only audit trail (ADR-0010 §3.6, RB-13 B8).

#pragma once

#include "CoreMinimal.h"
#include "UeremcpSecurityTypes.h"

/** One audit line — fields from RB-13 B8. */
struct UEREMCPSECURITY_API FUeremcpAuditRecord
{
	FString TimestampUtc;
	FString RequestId;
	FString IdempotencyKey;
	FString Action;
	FString Mode;
	FString Status;
	FString SessionId;
	FString TargetAssetPath;
	TArray<FString> CreatedAssets;
	TArray<FString> ModifiedAssets;
	TArray<FString> DeletedAssets;
	bool bDryRun = false;
	bool bAtomic = true;
	EUeremcpPermissionTier RequiredTier = EUeremcpPermissionTier::Write;
	FString RevisionBefore;
	FString RevisionAfter;
	FString ProjectPath;
};

class UEREMCPSECURITY_API FUeremcpAuditLog
{
public:
	/** <Saved>/UEREMCP/audit/ — outside FileSandbox content mounts. */
	static FString AuditDirectory(const FUeremcpPathPolicyRoots& Roots);

	/** Daily file name pattern: YYYY-MM-DD.jsonl */
	static FString DailyLogFileName(const FDateTime& UtcNow = FDateTime::UtcNow());

	static bool IsImplemented();

	/** Append exactly one condensed JSON object and newline to the daily log. */
	static bool Append(const FUeremcpAuditRecord& Record, const FUeremcpPathPolicyRoots& Roots, FString& OutError);

	/** Delete daily JSONL files whose modification time is outside retention. */
	static int32 PruneOlderThanDays(int32 RetentionDays, const FUeremcpPathPolicyRoots& Roots);
};
