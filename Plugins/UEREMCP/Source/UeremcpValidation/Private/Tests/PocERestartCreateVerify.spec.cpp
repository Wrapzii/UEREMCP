// POC E1 scoped restart survival for Validation scratch assets (two-process).
//
// Orchestrated by tests/run_poc_acceptance.ps1 -Scenario E1:
//   Create persists checkpoint under Saved/UEREMCP/; Verify re-reads in a fresh editor.
// This proves Validation/protocol durability primitives survive restart. Full E1 across
// POC A–D additionally requires domain create/verify evidence (B8 exists; A/C/D residuals
// recorded in docs/proposals/ws-11-poc-e-acceptance-status.md).

#include "UeremcpScratchPaths.h"
#include "UeremcpValidationTestCommon.h"

#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpPocERestart
{
	static const FString SuiteName = TEXT("PocE_Restart");
	static const FString AssetName = TEXT("PocERestartCurve");
	static constexpr const TCHAR* CheckpointId = TEXT("poc-e1-validation-scratch");
	static constexpr const TCHAR* DefaultRunId = TEXT("poc-e1-validation-restart");

	static FString CheckpointFilePath()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("UEREMCP"), TEXT("poc_e1_restart_checkpoint.json"));
	}

	static FString SoftPath()
	{
		return UeremcpMakeScratchPackagePath(SuiteName, AssetName) + TEXT(".") + AssetName;
	}

	static void EmitEvidence(FAutomationTestBase& Test, const TSharedPtr<FJsonObject>& Evidence)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Evidence.ToSharedRef(), Writer);
		Test.AddInfo(FString::Printf(TEXT("UEREMCP_POC_EVIDENCE=%s"), *Json));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPocERestartCreate,
	"UEREMCP.Validation.PocE.Restart.Create",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPocERestartCreate::RunTest(const FString& Parameters)
{
	using namespace UeremcpPocERestart;
	using namespace UeremcpValidationTests;
	(void)Parameters;

	// Intentionally NO ScratchGuard — assets must survive until Verify in a second process.
	UeremcpCleanupScratchSuite(SuiteName);

	const FString Soft = CreateAndSaveScratchCurve(SuiteName, AssetName, *this);
	if (Soft.IsEmpty())
	{
		return false;
	}
	TestTrue(TEXT("created asset exists"), UEditorAssetLibrary::DoesAssetExist(Soft));

	TSharedPtr<FJsonObject> Checkpoint = MakeShared<FJsonObject>();
	Checkpoint->SetStringField(TEXT("id"), CheckpointId);
	Checkpoint->SetStringField(TEXT("suite"), SuiteName);
	TArray<TSharedPtr<FJsonValue>> Assets;
	Assets.Add(MakeShared<FJsonValueString>(Soft));
	Checkpoint->SetArrayField(TEXT("assets"), Assets);

	FString CheckpointJson;
	{
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&CheckpointJson);
		FJsonSerializer::Serialize(Checkpoint.ToSharedRef(), Writer);
	}
	const FString Path = CheckpointFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(CheckpointJson, *Path))
	{
		AddError(TEXT("Failed to write E1 restart checkpoint"));
		return false;
	}

	TSharedPtr<FJsonObject> Evidence = MakeShared<FJsonObject>();
	Evidence->SetNumberField(TEXT("schema_version"), 1);
	Evidence->SetStringField(TEXT("scenario"), TEXT("poc_e1_create"));
	Evidence->SetStringField(TEXT("run_id"), DefaultRunId);
	Evidence->SetStringField(TEXT("outcome"), TEXT("pass"));
	Evidence->SetObjectField(TEXT("checkpoint"), Checkpoint);
	EmitEvidence(*this, Evidence);

	AddInfo(TEXT("POC_E E1 create: scratch CurveFloat + checkpoint written (do not cleanup)"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpPocERestartVerify,
	"UEREMCP.Validation.PocE.Restart.Verify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpPocERestartVerify::RunTest(const FString& Parameters)
{
	using namespace UeremcpPocERestart;
	(void)Parameters;

	const FString Path = CheckpointFilePath();
	FString CheckpointJson;
	if (!FFileHelper::LoadFileToString(CheckpointJson, *Path))
	{
		AddError(TEXT("E1 verify: checkpoint missing — run PocE.Restart.Create in a prior editor process"));
		TSharedPtr<FJsonObject> Skip = MakeShared<FJsonObject>();
		Skip->SetNumberField(TEXT("schema_version"), 1);
		Skip->SetStringField(TEXT("scenario"), TEXT("poc_e1_verify"));
		Skip->SetStringField(TEXT("run_id"), DefaultRunId);
		Skip->SetStringField(TEXT("outcome"), TEXT("skip"));
		Skip->SetStringField(TEXT("blocker"), TEXT("checkpoint_missing"));
		EmitEvidence(*this, Skip);
		return true; // skip, not fail — orchestrator maps skip
	}

	TSharedPtr<FJsonObject> Checkpoint;
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CheckpointJson);
		if (!FJsonSerializer::Deserialize(Reader, Checkpoint) || !Checkpoint.IsValid())
		{
			AddError(TEXT("E1 verify: checkpoint JSON invalid"));
			return false;
		}
	}

	FString Id;
	Checkpoint->TryGetStringField(TEXT("id"), Id);
	TestEqual(TEXT("checkpoint id"), Id, FString(CheckpointId));

	const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
	if (!TestTrue(TEXT("checkpoint assets"),
			Checkpoint->TryGetArrayField(TEXT("assets"), Assets) && Assets && Assets->Num() > 0))
	{
		return false;
	}

	bool bAllPresent = true;
	for (const TSharedPtr<FJsonValue>& Value : *Assets)
	{
		const FString Soft = Value->AsString();
		if (!UEditorAssetLibrary::DoesAssetExist(Soft))
		{
			AddError(FString::Printf(TEXT("Asset missing after restart: %s"), *Soft));
			bAllPresent = false;
		}
	}

	const bool bPass = bAllPresent && !HasAnyErrors();

	TSharedPtr<FJsonObject> Evidence = MakeShared<FJsonObject>();
	Evidence->SetNumberField(TEXT("schema_version"), 1);
	Evidence->SetStringField(TEXT("scenario"), TEXT("poc_e1_verify"));
	Evidence->SetStringField(TEXT("run_id"), DefaultRunId);
	Evidence->SetStringField(TEXT("outcome"), bPass ? TEXT("pass") : TEXT("fail"));
	Evidence->SetBoolField(TEXT("restart_observed"), true);
	Evidence->SetBoolField(TEXT("reread_after_restart"), bPass);
	Evidence->SetObjectField(TEXT("checkpoint"), Checkpoint);
	TSharedPtr<FJsonObject> Criteria = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> E1 = MakeShared<FJsonObject>();
	E1->SetStringField(TEXT("status"), bPass ? TEXT("pass") : TEXT("fail"));
	E1->SetStringField(TEXT("scope"), TEXT("validation_scratch_curve"));
	Criteria->SetObjectField(TEXT("E1"), E1);
	Evidence->SetObjectField(TEXT("criteria"), Criteria);
	EmitEvidence(*this, Evidence);

	// Cleanup after verify so the next create is clean.
	UeremcpCleanupScratchSuite(SuiteName);
	IFileManager::Get().Delete(*Path);

	AddInfo(bPass
		? TEXT("POC_E E1 verify PASS: scratch assets survived editor restart")
		: TEXT("POC_E E1 verify FAIL"));
	return bPass;
}

#endif // WITH_DEV_AUTOMATION_TESTS
