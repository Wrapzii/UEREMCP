#include "UeremcpAuditLog.h"
#include "UeremcpPathPolicy.h"

#include "Misc/Paths.h"

FString FUeremcpAuditLog::AuditDirectory(const FUeremcpPathPolicyRoots& Roots)
{
	return FPaths::Combine(FUeremcpPathPolicy::SavedUeremcpRoot(Roots), TEXT("audit"));
}

FString FUeremcpAuditLog::DailyLogFileName(const FDateTime& UtcNow)
{
	return FString::Printf(TEXT("%04d-%02d-%02d.jsonl"),
		UtcNow.GetYear(), UtcNow.GetMonth(), UtcNow.GetDay());
}

bool FUeremcpAuditLog::IsImplemented()
{
	return false;
}

bool FUeremcpAuditLog::Append(
	const FUeremcpAuditRecord& Record,
	const FUeremcpPathPolicyRoots& Roots,
	FString& OutError)
{
	(void)Record;
	(void)Roots;
	OutError = TEXT("FUeremcpAuditLog::Append is not implemented yet (ADR-0010 Wave 2 stub)");
	return false;
}

int32 FUeremcpAuditLog::PruneOlderThanDays(int32 RetentionDays, const FUeremcpPathPolicyRoots& Roots)
{
	(void)RetentionDays;
	(void)Roots;
	return 0;
}
