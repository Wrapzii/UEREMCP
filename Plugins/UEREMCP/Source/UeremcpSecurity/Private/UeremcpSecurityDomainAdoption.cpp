#include "UeremcpSecurityDomainAdoption.h"

const TCHAR* FUeremcpSecurityDomainAdoption::PreferredGateHeader()
{
	return TEXT("UeremcpMutatingDispatch.h");
}

int32 FUeremcpSecurityDomainAdoption::PredictedDeletedForDestructiveReplace(
	const bool bTargetExists,
	const bool bDestructiveMode)
{
	return (bDestructiveMode && bTargetExists) ? 1 : 0;
}

FUeremcpPermissionOptions FUeremcpSecurityDomainAdoption::MakePermissionOptions(
	const bool bDryRun,
	const bool bDryRunWasExplicit,
	const bool bAllowDestructive,
	const int32 PredictedDeletedAssetCount)
{
	FUeremcpPermissionOptions Options;
	Options.bDryRun = bDryRun;
	Options.bDryRunWasExplicit = bDryRunWasExplicit;
	Options.bAllowDestructive = bAllowDestructive;
	Options.PredictedDeletedAssetCount = PredictedDeletedAssetCount;
	return Options;
}

FUeremcpPathValidationResult FUeremcpSecurityDomainAdoption::ValidateWriteSoftPath(
	const FString& SoftPath,
	const FUeremcpPathPolicyRoots* Roots)
{
	return FUeremcpPathPolicy::ValidateSoftPath(SoftPath, true, Roots);
}

FUeremcpPermissionVerdict FUeremcpSecurityDomainAdoption::EvaluatePermission(
	const FString& Action,
	const FString& Mode,
	const FUeremcpPermissionOptions& Options,
	const bool bTargetExists,
	const UUeremcpSecuritySettings* Settings)
{
	return FUeremcpPermissionPolicy::Evaluate(Action, Mode, Options, bTargetExists, Settings);
}
