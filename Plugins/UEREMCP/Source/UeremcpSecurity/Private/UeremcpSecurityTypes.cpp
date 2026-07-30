#include "UeremcpSecurityTypes.h"

#include "Misc/Paths.h"

FUeremcpPathValidationResult FUeremcpPathValidationResult::Allowed()
{
	FUeremcpPathValidationResult Result;
	Result.bAllowed = true;
	return Result;
}

FUeremcpPathValidationResult FUeremcpPathValidationResult::Denied(const FString& InReason)
{
	FUeremcpPathValidationResult Result;
	Result.bAllowed = false;
	Result.Reason = InReason;
	return Result;
}

bool FUeremcpPathPolicyRoots::IsConfigured() const
{
	return !ProjectDir.IsEmpty() && !ProjectContentDir.IsEmpty() && !ProjectSavedDir.IsEmpty();
}
