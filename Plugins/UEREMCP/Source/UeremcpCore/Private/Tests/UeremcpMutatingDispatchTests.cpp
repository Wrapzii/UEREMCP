// Core security dispatch integration tests (WS-03 / WS-12 proposal acceptance).

#include "CoreMinimal.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpAuditLog.h"
#include "UeremcpMutatingDispatch.h"
#include "UeremcpMutatorQueue.h"
#include "UeremcpPathPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpDispatchTest
{
	static FString MutatingRequest(
		const FString& RequestId,
		const FString& Mode,
		const FString& TargetPath = TEXT("/Game/__UeremcpTests/DispatchTarget"))
	{
		return FString::Printf(
			TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"%s\",\"action\":\"submit_graph\","
				"\"project\":{\"path\":\"%s\"},\"target\":{\"asset_path\":\"%s\"},\"mode\":\"%s\"}"),
			*RequestId,
			*FPaths::GetProjectFilePath().ReplaceCharWith(TCHAR('\\'), TCHAR('/')),
			*TargetPath,
			*Mode);
	}

	static TSharedPtr<FJsonObject> ParseJson(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Root);
		return Root;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMutatingDispatchSerializesMutatorsTest,
	"UeremcpCore.MutatingDispatch.SerializesMutators",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMutatingDispatchSerializesMutatorsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FString ProjectKey = FPaths::GetProjectFilePath();
	TestTrue(TEXT("mutator queue available"), FUeremcpMutatorQueue::IsImplemented());

	FUeremcpMutatingDispatch First;
	FString BlockA;
	TestTrue(
		TEXT("first dispatch acquires"),
		First.TryBegin(
			UeremcpDispatchTest::MutatingRequest(TEXT("dispatch-a"), TEXT("create_or_update")),
			false,
			0,
			false,
			BlockA));
	TestTrue(TEXT("first dispatch not blocked"), BlockA.IsEmpty());
	TestTrue(TEXT("first holds mutator"), First.HoldsMutatorSlot());

	FUeremcpMutatingDispatch Second;
	FString BlockB;
	TestFalse(
		TEXT("second dispatch waits"),
		Second.TryBegin(
			UeremcpDispatchTest::MutatingRequest(TEXT("dispatch-b"), TEXT("create_or_update")),
			false,
			0,
			false,
			BlockB));
	TestFalse(TEXT("second blocked response present"), BlockB.IsEmpty());

	const TSharedPtr<FJsonObject> Queued = UeremcpDispatchTest::ParseJson(BlockB);
	TestEqual(
		TEXT("queued status"),
		Queued->GetStringField(TEXT("status")),
		FString(TEXT("partially_completed")));
	TestTrue(TEXT("queued job id present"), Queued->HasTypedField<EJson::Object>(TEXT("job")));

	FUeremcpResponse OkResponse;
	OkResponse.Status = TEXT("no_change_required");
	OkResponse.Summary = TEXT("First dispatch complete.");
	First.Complete(OkResponse);

	TestFalse(TEXT("slot released after complete"), First.HoldsMutatorSlot());

	FUeremcpMutatingDispatch Third;
	FString BlockC;
	TestTrue(
		TEXT("next waiter can acquire after release"),
		Third.TryBegin(
			UeremcpDispatchTest::MutatingRequest(TEXT("dispatch-b"), TEXT("create_or_update")),
			false,
			0,
			false,
			BlockC));
	TestTrue(TEXT("promoted waiter not blocked"), BlockC.IsEmpty());
	Third.Complete(OkResponse);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMutatingDispatchDestructiveDryRunAuditTest,
	"UeremcpCore.MutatingDispatch.DestructiveDryRunAudit",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMutatingDispatchDestructiveDryRunAuditTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(TEXT("audit log available"), FUeremcpAuditLog::IsImplemented());

	const FUeremcpPathPolicyRoots Roots = FUeremcpPathPolicy::RootsFromProject();
	const FString AuditFilePath = FPaths::Combine(
		FUeremcpAuditLog::AuditDirectory(Roots),
		FUeremcpAuditLog::DailyLogFileName());

	FString BeforeContents;
	FFileHelper::LoadFileToString(BeforeContents, *AuditFilePath);
	TArray<FString> BeforeLineArray;
	BeforeContents.ParseIntoArrayLines(BeforeLineArray, false);
	const int32 BeforeLines = BeforeLineArray.Num();

	FUeremcpMutatingDispatch Dispatch;
	FString Block;
	TestTrue(
		TEXT("destructive delete dispatch opens gate"),
		Dispatch.TryBegin(
			UeremcpDispatchTest::MutatingRequest(TEXT("destructive-dry-run"), TEXT("delete")),
			true,
			0,
			false,
			Block));
	TestTrue(TEXT("not blocked"), Block.IsEmpty());
	TestTrue(TEXT("effective dry_run forced"), Dispatch.IsEffectiveDryRun());

	FUeremcpResponse Response;
	Response.Status = TEXT("rolled_back");
	Response.Summary = TEXT("Destructive dry run discarded planned changes.");
	Dispatch.Complete(Response);

	FString AfterContents;
	TestTrue(TEXT("audit file readable"), FFileHelper::LoadFileToString(AfterContents, *AuditFilePath));

	TArray<FString> Lines;
	AfterContents.ParseIntoArrayLines(Lines, false);
	TestTrue(TEXT("audit line appended"), Lines.Num() > BeforeLines);

	bool bFoundDryRun = false;
	for (const FString& Line : Lines)
	{
		TSharedPtr<FJsonObject> Record = UeremcpDispatchTest::ParseJson(Line);
		if (!Record.IsValid())
		{
			continue;
		}
		if (Record->GetStringField(TEXT("request_id")) == TEXT("destructive-dry-run"))
		{
			TestEqual(
				TEXT("audit status"),
				Record->GetStringField(TEXT("status")),
				FString(TEXT("rolled_back")));
			TestTrue(TEXT("audit dry_run flag"), Record->GetBoolField(TEXT("dry_run")));
			bFoundDryRun = true;
		}
	}
	TestTrue(TEXT("destructive dry run audit found"), bFoundDryRun);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
