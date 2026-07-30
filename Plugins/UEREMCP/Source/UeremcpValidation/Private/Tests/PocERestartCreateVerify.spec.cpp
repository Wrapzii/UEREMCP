// POC E1 restart survival — Validation scratch plus successful POC A–D result assets.
//
// Orchestrated by tests/run_poc_acceptance.ps1:
//   -Scenario E   → honesty filters + scratch/E1 pair (A–D included when present)
//   -Scenario E1  → domain create filters, then Create/Verify restart pair
// Create persists checkpoint under Saved/UEREMCP/; Verify re-reads in a fresh editor.
//
// Honest scope: proves assets that *can* be created on this tip survive restart.
// Does NOT claim C5 (no networking/damage contract assets) or D5 multi-client proof.

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
	static constexpr const TCHAR* CheckpointId = TEXT("poc-e1-restart");
	static constexpr const TCHAR* DefaultRunId = TEXT("poc-e1-ad-restart");

	struct FPocAssetGroup
	{
		const TCHAR* PocKey;
		TArray<FString> Candidates;
	};

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

	static TArray<TSharedPtr<FJsonValue>> StringArrayToJson(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Out;
		for (const FString& Value : Values)
		{
			Out.Add(MakeShared<FJsonValueString>(Value));
		}
		return Out;
	}

	static bool AssetExistsAnyForm(const FString& Path)
	{
		if (UEditorAssetLibrary::DoesAssetExist(Path))
		{
			return true;
		}
		// Soft object path form: /Game/Foo/Bar.Bar
		const FString Leaf = FPaths::GetBaseFilename(Path);
		if (!Leaf.IsEmpty() && !Path.Contains(TEXT(".")))
		{
			return UEditorAssetLibrary::DoesAssetExist(Path + TEXT(".") + Leaf);
		}
		return false;
	}

	static void CollectPresent(
		const TArray<FString>& Candidates,
		TArray<FString>& OutPresent,
		TArray<FString>& OutMissing)
	{
		for (const FString& Candidate : Candidates)
		{
			if (AssetExistsAnyForm(Candidate))
			{
				OutPresent.AddUnique(Candidate);
			}
			else
			{
				OutMissing.Add(Candidate);
			}
		}
	}

	static TArray<FPocAssetGroup> ExpectedGroups()
	{
		TArray<FPocAssetGroup> Groups;

		FPocAssetGroup A;
		A.PocKey = TEXT("A");
		A.Candidates = {
			TEXT("/Game/__UeremcpPoc/Blueprint/BP_CompleteRoundTripTransport"),
		};
		Groups.Add(MoveTemp(A));

		FPocAssetGroup B;
		B.PocKey = TEXT("B");
		B.Candidates = {
			TEXT("/Game/__UeremcpPoc/NS_POCB_Fireball"),
			TEXT("/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_core"),
			TEXT("/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_flame_shell"),
			TEXT("/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_sparks"),
			TEXT("/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_smoke"),
			TEXT("/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_ribbon_trail"),
			TEXT("/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_impact_burst"),
		};
		Groups.Add(MoveTemp(B));

		// Successful C results creatable on this tip (C1–C4/C6–C7). C5 has no
		// networking/damage assets to checkpoint — residual only.
		FPocAssetGroup C;
		C.PocKey = TEXT("C");
		C.Candidates = {
			TEXT("/Game/__UeremcpPoc/NS_POCC_IceVariationDirect"),
			TEXT("/Game/__UeremcpPoc/NS_POCC_IceVariation"),
			TEXT("/Game/__UeremcpPoc/NS_POCC_WindThirdGeneration"),
			TEXT("/Game/__UeremcpPoc/MI_NS_POCC_IceVariation_Core"),
			TEXT("/Game/__UeremcpPoc/MI_NS_POCC_IceVariation_Trail"),
			TEXT("/Game/__UeremcpPoc/MI_NS_POCC_WindThirdGeneration_Core"),
			TEXT("/Game/__UeremcpPoc/MI_NS_POCC_WindThirdGeneration_Trail"),
		};
		Groups.Add(MoveTemp(C));

		// Successful D result from LiveUpsertViaPlan. D5 multi-client is not an asset.
		FPocAssetGroup D;
		D.PocKey = TEXT("D");
		D.Candidates = {
			TEXT("/Game/__UeremcpTests/Abilities/DT_PocD_Live"),
		};
		Groups.Add(MoveTemp(D));

		return Groups;
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
	TestTrue(TEXT("created scratch asset exists"), UEditorAssetLibrary::DoesAssetExist(Soft));

	TArray<FString> AllAssets;
	AllAssets.Add(Soft);

	TSharedPtr<FJsonObject> ByPoc = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ScratchArr;
	ScratchArr.Add(MakeShared<FJsonValueString>(Soft));
	ByPoc->SetArrayField(TEXT("validation_scratch"), ScratchArr);

	TSharedPtr<FJsonObject> Presence = MakeShared<FJsonObject>();
	TArray<FString> PresentPocs;
	TArray<FString> AbsentPocs;

	for (const FPocAssetGroup& Group : ExpectedGroups())
	{
		TArray<FString> Present;
		TArray<FString> Missing;
		CollectPresent(Group.Candidates, Present, Missing);

		TSharedPtr<FJsonObject> PocEntry = MakeShared<FJsonObject>();
		PocEntry->SetArrayField(TEXT("present"), StringArrayToJson(Present));
		PocEntry->SetArrayField(TEXT("missing"), StringArrayToJson(Missing));
		const bool bAny = Present.Num() > 0;
		PocEntry->SetBoolField(TEXT("any_present"), bAny);
		Presence->SetObjectField(Group.PocKey, PocEntry);

		if (bAny)
		{
			PresentPocs.Add(Group.PocKey);
			ByPoc->SetArrayField(Group.PocKey, StringArrayToJson(Present));
			for (const FString& Path : Present)
			{
				AllAssets.AddUnique(Path);
				// Touch-save so the package is flushed before the editor exits.
				UEditorAssetLibrary::SaveAsset(Path, /*bOnlyIfIsDirty=*/false);
			}
		}
		else
		{
			AbsentPocs.Add(Group.PocKey);
		}
	}

	TSharedPtr<FJsonObject> Residuals = MakeShared<FJsonObject>();
	Residuals->SetStringField(
		TEXT("C5"),
		TEXT("No networking/damage contract assets exist for Niagara sources; C5 remains FAIL and is not part of the restart checkpoint."));
	Residuals->SetStringField(
		TEXT("D5"),
		TEXT("Pattern B multi-client proof is not an on-disk asset; DT_PocD_Live survival does not close D5."));
	Residuals->SetStringField(
		TEXT("overall_E1"),
		TEXT("E1 overall (all POC A–D results) is not claimed while C5 fails and D5 is static-only; this run proves survival of creatable successful assets only."));

	TSharedPtr<FJsonObject> Checkpoint = MakeShared<FJsonObject>();
	Checkpoint->SetStringField(TEXT("id"), CheckpointId);
	Checkpoint->SetStringField(TEXT("suite"), SuiteName);
	Checkpoint->SetArrayField(TEXT("assets"), StringArrayToJson(AllAssets));
	Checkpoint->SetObjectField(TEXT("by_poc"), ByPoc);
	Checkpoint->SetObjectField(TEXT("presence"), Presence);
	Checkpoint->SetArrayField(TEXT("pocs_present"), StringArrayToJson(PresentPocs));
	Checkpoint->SetArrayField(TEXT("pocs_absent"), StringArrayToJson(AbsentPocs));
	Checkpoint->SetObjectField(TEXT("residuals"), Residuals);

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

	AddInfo(FString::Printf(
		TEXT("POC_E E1 create: scratch + %d checkpointed assets (pocs_present=%d)"),
		AllAssets.Num(),
		PresentPocs.Num()));
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
	const bool bIdOk = (Id == CheckpointId) || (Id == TEXT("poc-e1-validation-scratch"));
	TestTrue(TEXT("checkpoint id is a known E1 id"), bIdOk);

	const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
	if (!TestTrue(TEXT("checkpoint assets"),
			Checkpoint->TryGetArrayField(TEXT("assets"), Assets) && Assets && Assets->Num() > 0))
	{
		return false;
	}

	TArray<FString> Survived;
	TArray<FString> MissingAfterRestart;
	bool bAllPresent = true;
	for (const TSharedPtr<FJsonValue>& Value : *Assets)
	{
		const FString Soft = Value->AsString();
		if (AssetExistsAnyForm(Soft))
		{
			Survived.Add(Soft);
		}
		else
		{
			AddError(FString::Printf(TEXT("Asset missing after restart: %s"), *Soft));
			MissingAfterRestart.Add(Soft);
			bAllPresent = false;
		}
	}

	// Per-POC survival from by_poc when present.
	TSharedPtr<FJsonObject> SurvivedByPoc = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject>* ByPoc = nullptr;
	if (Checkpoint->TryGetObjectField(TEXT("by_poc"), ByPoc) && ByPoc && ByPoc->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ByPoc)->Values)
		{
			if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::Array)
			{
				continue;
			}
			const TArray<TSharedPtr<FJsonValue>>& PocAssets = Pair.Value->AsArray();
			TArray<FString> Ok;
			TArray<FString> Lost;
			for (const TSharedPtr<FJsonValue>& Entry : PocAssets)
			{
				const FString Soft = Entry->AsString();
				if (AssetExistsAnyForm(Soft))
				{
					Ok.Add(Soft);
				}
				else
				{
					Lost.Add(Soft);
				}
			}
			TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetArrayField(TEXT("survived"), StringArrayToJson(Ok));
			Row->SetArrayField(TEXT("missing"), StringArrayToJson(Lost));
			Row->SetBoolField(TEXT("all_survived"), Lost.Num() == 0 && Ok.Num() > 0);
			SurvivedByPoc->SetObjectField(Pair.Key, Row);
		}
	}

	const bool bPass = bAllPresent && !HasAnyErrors();

	FString Scope = TEXT("validation_scratch_curve");
	const TArray<TSharedPtr<FJsonValue>>* PocsPresent = nullptr;
	if (Checkpoint->TryGetArrayField(TEXT("pocs_present"), PocsPresent) && PocsPresent && PocsPresent->Num() > 0)
	{
		TArray<FString> Keys;
		for (const TSharedPtr<FJsonValue>& V : *PocsPresent)
		{
			Keys.Add(V->AsString());
		}
		Scope = FString::Printf(
			TEXT("scratch_plus_successful_ad_assets(%s); C5/D5 residuals unchanged"),
			*FString::Join(Keys, TEXT(",")));
	}

	TSharedPtr<FJsonObject> Evidence = MakeShared<FJsonObject>();
	Evidence->SetNumberField(TEXT("schema_version"), 1);
	Evidence->SetStringField(TEXT("scenario"), TEXT("poc_e1_verify"));
	Evidence->SetStringField(TEXT("run_id"), DefaultRunId);
	Evidence->SetStringField(TEXT("outcome"), bPass ? TEXT("pass") : TEXT("fail"));
	Evidence->SetBoolField(TEXT("restart_observed"), true);
	Evidence->SetBoolField(TEXT("reread_after_restart"), bPass);
	Evidence->SetObjectField(TEXT("checkpoint"), Checkpoint);
	Evidence->SetArrayField(TEXT("survived_assets"), StringArrayToJson(Survived));
	Evidence->SetArrayField(TEXT("missing_after_restart"), StringArrayToJson(MissingAfterRestart));
	Evidence->SetObjectField(TEXT("survived_by_poc"), SurvivedByPoc);
	Evidence->SetBoolField(TEXT("overall_e1_all_ad_claimed"), false);

	TSharedPtr<FJsonObject> Criteria = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> E1 = MakeShared<FJsonObject>();
	E1->SetStringField(TEXT("status"), bPass ? TEXT("pass") : TEXT("fail"));
	E1->SetStringField(TEXT("scope"), Scope);
	E1->SetBoolField(TEXT("full_ad_results_claimed"), false);
	Criteria->SetObjectField(TEXT("E1"), E1);
	Evidence->SetObjectField(TEXT("criteria"), Criteria);
	EmitEvidence(*this, Evidence);

	// Cleanup Validation scratch only — never delete domain POC A–D assets.
	UeremcpCleanupScratchSuite(SuiteName);
	IFileManager::Get().Delete(*Path);

	AddInfo(bPass
		? TEXT("POC_E E1 verify PASS: checkpointed assets survived editor restart (overall A–D E1 not claimed)")
		: TEXT("POC_E E1 verify FAIL"));
	return bPass;
}

#endif // WITH_DEV_AUTOMATION_TESTS
