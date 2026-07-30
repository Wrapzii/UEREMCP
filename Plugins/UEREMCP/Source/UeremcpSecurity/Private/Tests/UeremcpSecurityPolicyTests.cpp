// Unit/automation tests for UeremcpSecurity policy (ADR-0010).
//
// Run (editor / commandlet — requires the plugin built into a uproject):
//   UnrealEditor-Cmd.exe <Project>.uproject -unattended -NullRHI -nop4
//     -ExecCmds="Automation RunTests UEREMCP.Security;Quit"

#include "UeremcpAuditLog.h"
#include "UeremcpMutatorQueue.h"
#include "UeremcpPathPolicy.h"
#include "UeremcpPermissionPolicy.h"
#include "UeremcpSecuritySettings.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpSecurityTest
{
	static FUeremcpPathPolicyRoots TestRoots()
	{
		FUeremcpPathPolicyRoots Roots;
		Roots.ProjectDir = TEXT("C:/Projects/RE/");
		Roots.ProjectContentDir = TEXT("C:/Projects/RE/Content/");
		Roots.ProjectSavedDir = TEXT("C:/Projects/RE/Saved/");
		return Roots;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSecurityPathPolicySoftTest,
	"UEREMCP.Security.PathPolicy.Soft",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpSecurityPathPolicySoftTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const auto Allowed = FUeremcpPathPolicy::ValidateSoftPath(TEXT("/Game/Foo/Bar"), true);
	TestTrue(TEXT("/Game/ write allowed"), Allowed.bAllowed);

	const auto EngineWrite = FUeremcpPathPolicy::ValidateSoftPath(TEXT("/Engine/Content/Foo"), true);
	TestFalse(TEXT("/Engine/ write rejected"), EngineWrite.bAllowed);

	const auto EngineRead = FUeremcpPathPolicy::ValidateSoftPath(TEXT("/Engine/Content/Foo"), false);
	TestTrue(TEXT("/Engine/ read allowed"), EngineRead.bAllowed);

	const auto Traversal = FUeremcpPathPolicy::ValidateSoftPath(TEXT("/Game/../Secret"), true);
	TestFalse(TEXT("traversal rejected"), Traversal.bAllowed);

	const auto WinAbs = FUeremcpPathPolicy::ValidateSoftPath(TEXT("C:/evil"), true);
	TestFalse(TEXT("windows absolute rejected"), WinAbs.bAllowed);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSecurityPathPolicyFilesystemTest,
	"UEREMCP.Security.PathPolicy.Filesystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpSecurityPathPolicyFilesystemTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FUeremcpPathPolicyRoots Roots = UeremcpSecurityTest::TestRoots();

	const auto ContentWrite = FUeremcpPathPolicy::ValidateFilesystemPath(
		TEXT("C:/Projects/RE/Content/Foo.uasset"), true, &Roots);
	TestTrue(TEXT("Content write allowed"), ContentWrite.bAllowed);

	const auto AuditWrite = FUeremcpPathPolicy::ValidateFilesystemPath(
		TEXT("C:/Projects/RE/Saved/UEREMCP/audit/2026-07-30.jsonl"), true, &Roots);
	TestTrue(TEXT("Saved/UEREMCP write allowed"), AuditWrite.bAllowed);

	const auto SavedOther = FUeremcpPathPolicy::ValidateFilesystemPath(
		TEXT("C:/Projects/RE/Saved/Config/foo.ini"), true, &Roots);
	TestFalse(TEXT("Saved outside UEREMCP write rejected"), SavedOther.bAllowed);

	const auto Outside = FUeremcpPathPolicy::ValidateFilesystemPath(
		TEXT("C:/OtherProject/Content/Evil.uasset"), true, &Roots);
	TestFalse(TEXT("outside project rejected"), Outside.bAllowed);

	const auto Traversal = FUeremcpPathPolicy::ValidateFilesystemPath(
		TEXT("C:/Projects/RE/Content/../Secret"), true, &Roots);
	TestFalse(TEXT("filesystem traversal rejected"), Traversal.bAllowed);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSecurityPermissionPolicyDryRunTest,
	"UEREMCP.Security.PermissionPolicy.DestructiveDryRun",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpSecurityPermissionPolicyDryRunTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FUeremcpPermissionOptions Options;
	Options.bDryRun = false;
	Options.bDryRunWasExplicit = false;

	const auto DeleteDefault = FUeremcpPermissionPolicy::Evaluate(
		TEXT("delete_asset"), TEXT("delete"), Options, true);
	TestTrue(TEXT("delete forces dry_run"), DeleteDefault.bDryRunForced);
	TestTrue(TEXT("delete effective dry_run"), DeleteDefault.bEffectiveDryRun);

	Options.bDryRunWasExplicit = true;
	Options.bDryRun = false;
	const auto DeleteExplicit = FUeremcpPermissionPolicy::Evaluate(
		TEXT("delete_asset"), TEXT("delete"), Options, true);
	TestFalse(TEXT("explicit dry_run false not forced"), DeleteExplicit.bDryRunForced);
	TestFalse(TEXT("explicit dry_run false honoured"), DeleteExplicit.bEffectiveDryRun);

	Options = FUeremcpPermissionOptions();
	const auto ReplaceNew = FUeremcpPermissionPolicy::Evaluate(
		TEXT("replace_graph"), TEXT("replace"), Options, false);
	TestFalse(TEXT("replace on missing target not destructive"), ReplaceNew.bDryRunForced);

	const auto ReplaceExists = FUeremcpPermissionPolicy::Evaluate(
		TEXT("replace_graph"), TEXT("replace"), Options, true);
	TestTrue(TEXT("replace on existing forces dry_run"), ReplaceExists.bDryRunForced);

	Options.PredictedDeletedAssetCount = 2;
	const auto PredictedDeletes = FUeremcpPermissionPolicy::Evaluate(
		TEXT("patch_graph"), TEXT("patch"), Options, false);
	TestTrue(TEXT("predicted deletes force dry_run"), PredictedDeletes.bDryRunForced);

	const auto Create = FUeremcpPermissionPolicy::Evaluate(
		TEXT("create_graph"), TEXT("create_or_update"), FUeremcpPermissionOptions(), false);
	TestFalse(TEXT("create does not force dry_run"), Create.bDryRunForced);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSecurityStubApiTest,
	"UEREMCP.Security.Stubs.ApiSurface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpSecurityStubApiTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestFalse(TEXT("mutator queue not implemented"), FUeremcpMutatorQueue::IsImplemented());
	const auto Acquire = FUeremcpMutatorQueue::TryAcquire(TEXT("req-1"), EUeremcpPermissionTier::Write);
	TestFalse(TEXT("mutator acquire fails until wired"), Acquire.bAcquired);
	TestFalse(TEXT("mutator inactive"), FUeremcpMutatorQueue::IsActive());

	TestFalse(TEXT("audit log not implemented"), FUeremcpAuditLog::IsImplemented());
	const FUeremcpPathPolicyRoots Roots = UeremcpSecurityTest::TestRoots();
	const FString AuditDir = FUeremcpAuditLog::AuditDirectory(Roots);
	TestTrue(TEXT("audit dir under Saved/UEREMCP"), AuditDir.Contains(TEXT("UEREMCP/audit")));

	const FString Daily = FUeremcpAuditLog::DailyLogFileName(FDateTime(2026, 7, 30));
	TestEqual(TEXT("daily log name"), Daily, TEXT("2026-07-30.jsonl"));

	FString Error;
	FUeremcpAuditRecord Record;
	Record.RequestId = TEXT("abc");
	TestFalse(TEXT("append stub returns false"), FUeremcpAuditLog::Append(Record, Roots, Error));

	const UUeremcpSecuritySettings* Settings = GetDefault<UUeremcpSecuritySettings>();
	TestFalse(TEXT("unsafe off by default"), Settings->bAllowUnsafe);
	TestEqual(TEXT("audit retention default"), Settings->AuditRetentionDays, 14);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
