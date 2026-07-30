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

#include "Async/ParallelFor.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
	FUeremcpSecurityMutatorQueueTest,
	"UEREMCP.Security.MutatorQueue.SerializesMutators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpSecurityMutatorQueueTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString ProjectA = TEXT("C:/Projects/RE/RE.uproject");
	const FString ProjectB = TEXT("C:/Projects/Other/Other.uproject");
	const FString ProjectC = TEXT("C:/Projects/Parallel/Parallel.uproject");
	TestTrue(TEXT("mutator queue implemented"), FUeremcpMutatorQueue::IsImplemented());

	const auto Read = FUeremcpMutatorQueue::TryAcquire(
		ProjectA, TEXT("read-1"), EUeremcpPermissionTier::Read);
	TestTrue(TEXT("read bypasses queue"), Read.bAcquired);
	TestFalse(TEXT("read does not activate mutator"), FUeremcpMutatorQueue::IsActive(ProjectA));

	const auto Writer1 = FUeremcpMutatorQueue::TryAcquire(
		ProjectA, TEXT("write-1"), EUeremcpPermissionTier::Write);
	TestTrue(TEXT("first writer acquires"), Writer1.bAcquired);
	TestTrue(TEXT("project A active"), FUeremcpMutatorQueue::IsActive(ProjectA));

	const auto Writer1Again = FUeremcpMutatorQueue::TryAcquire(
		ProjectA, TEXT("write-1"), EUeremcpPermissionTier::Write);
	TestTrue(TEXT("owner reacquire is idempotent"), Writer1Again.bAcquired);

	const auto Writer2 = FUeremcpMutatorQueue::TryAcquire(
		ProjectA, TEXT("write-2"), EUeremcpPermissionTier::Destructive);
	TestFalse(TEXT("second writer waits"), Writer2.bAcquired);
	TestTrue(TEXT("second writer queued"), Writer2.bQueued);
	TestFalse(TEXT("queued writer gets job id"), Writer2.JobId.IsEmpty());

	const auto Writer2Again = FUeremcpMutatorQueue::TryAcquire(
		ProjectA, TEXT("write-2"), EUeremcpPermissionTier::Destructive);
	TestEqual(TEXT("queued retry keeps job id"), Writer2Again.JobId, Writer2.JobId);

	const auto Writer3 = FUeremcpMutatorQueue::TryAcquire(
		ProjectA, TEXT("write-3"), EUeremcpPermissionTier::Write);
	TestTrue(TEXT("third writer queued"), Writer3.bQueued);
	TestEqual(TEXT("two pending writers"), FUeremcpMutatorQueue::PendingCount(ProjectA), 2);

	const auto OtherProjectWriter = FUeremcpMutatorQueue::TryAcquire(
		ProjectB, TEXT("other-1"), EUeremcpPermissionTier::Write);
	TestTrue(TEXT("different project has independent slot"), OtherProjectWriter.bAcquired);
	TestTrue(TEXT("release other project"), FUeremcpMutatorQueue::Release(ProjectB, TEXT("other-1")));

	constexpr int32 ContenderCount = 8;
	TArray<FUeremcpMutatorQueue::FAcquireResult> ConcurrentResults;
	ConcurrentResults.SetNum(ContenderCount);
	ParallelFor(ContenderCount, [&ConcurrentResults, &ProjectC](int32 Index)
	{
		ConcurrentResults[Index] = FUeremcpMutatorQueue::TryAcquire(
			ProjectC,
			FString::Printf(TEXT("parallel-%d"), Index),
			EUeremcpPermissionTier::Write);
	});

	int32 ConcurrentAcquired = 0;
	int32 ConcurrentQueued = 0;
	FString ConcurrentOwner;
	for (int32 Index = 0; Index < ConcurrentResults.Num(); ++Index)
	{
		if (ConcurrentResults[Index].bAcquired)
		{
			++ConcurrentAcquired;
			ConcurrentOwner = FString::Printf(TEXT("parallel-%d"), Index);
		}
		if (ConcurrentResults[Index].bQueued)
		{
			++ConcurrentQueued;
		}
	}
	TestEqual(TEXT("concurrent contenders produce one owner"), ConcurrentAcquired, 1);
	TestEqual(TEXT("remaining concurrent contenders queue"), ConcurrentQueued, ContenderCount - 1);
	TestTrue(TEXT("concurrent owner releases"), FUeremcpMutatorQueue::Release(ProjectC, ConcurrentOwner));
	for (int32 Index = 0; Index < ContenderCount; ++Index)
	{
		const FString RequestId = FString::Printf(TEXT("parallel-%d"), Index);
		if (RequestId != ConcurrentOwner)
		{
			TestTrue(TEXT("queued contender cancels"), FUeremcpMutatorQueue::CancelQueued(ProjectC, RequestId));
		}
	}

	TestFalse(TEXT("non-owner cannot release"), FUeremcpMutatorQueue::Release(ProjectA, TEXT("write-2")));
	TestTrue(TEXT("owner releases"), FUeremcpMutatorQueue::Release(ProjectA, TEXT("write-1")));
	TestFalse(TEXT("release leaves slot unclaimed"), FUeremcpMutatorQueue::IsActive(ProjectA));

	const auto Writer3Early = FUeremcpMutatorQueue::TryAcquire(
		ProjectA, TEXT("write-3"), EUeremcpPermissionTier::Write);
	TestTrue(TEXT("later waiter remains queued"), Writer3Early.bQueued);

	const auto Writer2Promoted = FUeremcpMutatorQueue::TryAcquire(
		ProjectA, TEXT("write-2"), EUeremcpPermissionTier::Destructive);
	TestTrue(TEXT("FIFO head acquires after release"), Writer2Promoted.bAcquired);
	TestTrue(TEXT("second writer releases"), FUeremcpMutatorQueue::Release(ProjectA, TEXT("write-2")));

	const auto Writer3Promoted = FUeremcpMutatorQueue::TryAcquire(
		ProjectA, TEXT("write-3"), EUeremcpPermissionTier::Write);
	TestTrue(TEXT("third writer acquires next"), Writer3Promoted.bAcquired);
	TestTrue(TEXT("third writer releases"), FUeremcpMutatorQueue::Release(ProjectA, TEXT("write-3")));
	TestFalse(TEXT("queue inactive after releases"), FUeremcpMutatorQueue::IsActive(ProjectA));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSecurityAuditLogTest,
	"UEREMCP.Security.Audit.AppendOnlyJsonl",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpSecurityAuditLogTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(TEXT("audit log implemented"), FUeremcpAuditLog::IsImplemented());
	FUeremcpPathPolicyRoots Roots = FUeremcpPathPolicy::RootsFromProject();
	const FString UniqueRoot = FPaths::Combine(
		Roots.ProjectSavedDir,
		TEXT("Automation"),
		TEXT("UeremcpSecurity"),
		FGuid::NewGuid().ToString(EGuidFormats::Digits));
	Roots.ProjectSavedDir = UniqueRoot;

	const FString AuditDir = FUeremcpAuditLog::AuditDirectory(Roots);
	TestTrue(TEXT("audit dir under Saved/UEREMCP"), AuditDir.Contains(TEXT("UEREMCP/audit")));

	const FString Daily = FUeremcpAuditLog::DailyLogFileName(FDateTime(2026, 7, 30));
	TestEqual(TEXT("daily log name"), Daily, TEXT("2026-07-30.jsonl"));

	FString Error;
	FUeremcpAuditRecord Record;
	Record.RequestId = TEXT("audit-1");
	Record.IdempotencyKey = TEXT("idem-1");
	Record.Action = TEXT("submit_graph\nescaped");
	Record.Mode = TEXT("patch");
	Record.Status = TEXT("modified_and_validated");
	Record.TargetAssetPath = TEXT("/Game/Test/BP_Audit");
	Record.ModifiedAssets.Add(TEXT("/Game/Test/BP_Audit"));
	Record.bAtomic = true;
	Record.RequiredTier = EUeremcpPermissionTier::Write;
	Record.ProjectPath = FPaths::GetProjectFilePath();
	TestTrue(TEXT("first audit append succeeds"), FUeremcpAuditLog::Append(Record, Roots, Error));
	TestTrue(TEXT("first append has no error"), Error.IsEmpty());

	Record.RequestId = TEXT("audit-2");
	Record.Status = TEXT("rolled_back");
	Record.bDryRun = true;
	TestTrue(TEXT("second audit append succeeds"), FUeremcpAuditLog::Append(Record, Roots, Error));

	const FString AuditFilePath = FPaths::Combine(
		AuditDir,
		FUeremcpAuditLog::DailyLogFileName());
	FString Contents;
	TestTrue(TEXT("audit file readable"), FFileHelper::LoadFileToString(Contents, *AuditFilePath));
	TArray<FString> Lines;
	Contents.ParseIntoArrayLines(Lines, false);
	TestEqual(TEXT("append preserves both JSONL records"), Lines.Num(), 2);

	if (Lines.Num() == 2)
	{
		TSharedPtr<FJsonObject> FirstObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Lines[0]);
		TestTrue(TEXT("first line is valid JSON"), FJsonSerializer::Deserialize(Reader, FirstObject));
		if (FirstObject.IsValid())
		{
			TestEqual(TEXT("first request id retained"), FirstObject->GetStringField(TEXT("request_id")), TEXT("audit-1"));
			TestEqual(TEXT("tier serialized"), FirstObject->GetStringField(TEXT("required_tier")), TEXT("write"));
			TestEqual(TEXT("escaped newline retained inside JSON"), FirstObject->GetStringField(TEXT("action")), Record.Action);
		}
	}

	IFileManager::Get().DeleteDirectory(*UniqueRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSecuritySettingsDefaultsTest,
	"UEREMCP.Security.Settings.Defaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpSecuritySettingsDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const UUeremcpSecuritySettings* Settings = GetDefault<UUeremcpSecuritySettings>();
	TestFalse(TEXT("unsafe off by default"), Settings->bAllowUnsafe);
	TestEqual(TEXT("audit retention default"), Settings->AuditRetentionDays, 14);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
