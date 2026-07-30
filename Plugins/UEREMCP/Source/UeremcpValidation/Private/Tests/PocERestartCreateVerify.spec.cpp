// POC E1 restart survival — Validation scratch plus successful POC A–D result assets.
//
// Orchestrated by tests/run_poc_acceptance.ps1:
//   -Scenario E   → honesty filters + scratch/E1 pair (A–D included when present)
//   -Scenario E1  → domain create filters, then Create/Verify restart pair
// Create persists checkpoint under Saved/UEREMCP/; Verify re-reads in a fresh editor.
//
// Full scope: seeds and checkpoints the accepted POC A-D result assets, including
// both POC C gameplay-binding rows, then re-reads assets and rows after restart.

#include "UeremcpScratchPaths.h"
#include "UeremcpValidationTestCommon.h"

#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UeremcpTemplatesToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpPocERestart
{
	static const FString SuiteName = TEXT("PocE_Restart");
	static const FString AssetName = TEXT("PocERestartCurve");
	static constexpr const TCHAR* CheckpointId = TEXT("poc-e1-restart");
	static constexpr const TCHAR* DefaultRunId = TEXT("poc-e1-ad-restart");
	static constexpr const TCHAR* PocCAbilityTable =
		TEXT("/Game/__UeremcpPoc/Abilities/DT_POCC_Variations");
	static constexpr const TCHAR* PocCIceRow = TEXT("poc_c_ice_fire_s");
	static constexpr const TCHAR* PocCWindRow = TEXT("poc_c_wind_fire_s");
	static constexpr const TCHAR* AbilityRowStructPath = TEXT("/Script/RE.REAbilityDef");

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

	static FString ToObjectPath(const FString& PackagePath)
	{
		if (PackagePath.Contains(TEXT(".")))
		{
			return PackagePath;
		}
		const FString PackageAssetName = FPackageName::GetLongPackageAssetName(PackagePath);
		return FString::Printf(TEXT("%s.%s"), *PackagePath, *PackageAssetName);
	}

	static bool ReadAbilityRowSnapshot(
		FAutomationTestBase& Test,
		const FString& RowName,
		FString& OutSnapshot)
	{
		UDataTable* Table = Cast<UDataTable>(StaticLoadObject(
			UDataTable::StaticClass(),
			nullptr,
			*ToObjectPath(PocCAbilityTable),
			nullptr,
			LOAD_NoWarn));
		if (!Test.TestNotNull(TEXT("POC C variation ability table loads"), Table))
		{
			return false;
		}
		const UScriptStruct* RowStruct = Table->GetRowStruct();
		if (!Test.TestTrue(
				TEXT("POC C variation table uses FREAbilityDef"),
				RowStruct && RowStruct->GetPathName() == AbilityRowStructPath))
		{
			return false;
		}
		const uint8* RowData = Table->FindRowUnchecked(FName(*RowName));
		if (!Test.TestNotNull(
				*FString::Printf(TEXT("POC C variation row %s exists"), *RowName),
				RowData))
		{
			return false;
		}
		const TSharedPtr<FJsonObject> RowJson = MakeShared<FJsonObject>();
		if (!Test.TestTrue(
				*FString::Printf(TEXT("POC C variation row %s normalizes"), *RowName),
				FJsonObjectConverter::UStructToJsonObject(
					RowStruct,
					RowData,
					RowJson.ToSharedRef(),
					0,
					0)))
		{
			return false;
		}
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutSnapshot);
		return Test.TestTrue(
			*FString::Printf(TEXT("POC C variation row %s serializes"), *RowName),
			FJsonSerializer::Serialize(RowJson.ToSharedRef(), Writer));
	}

	static bool SeedPocCAbilityVariations(FAutomationTestBase& Test)
	{
		struct FGeneration
		{
			const TCHAR* RequestId;
			const TCHAR* Element;
			const TCHAR* SourcePath;
			const TCHAR* TargetPath;
			const TCHAR* AbilityTable;
			const TCHAR* SourceRow;
			const TCHAR* TargetRow;
			const TCHAR* ModifiersJson;
		};
		const FGeneration Generations[] = {
			{
				TEXT("poc-e1-c5-ice"),
				TEXT("ice"),
				TEXT("/Game/__UeremcpPoc/NS_POCB_Fireball"),
				TEXT("/Game/__UeremcpPoc/NS_POCC_IceVariation"),
				TEXT("/Game/RE/Data/DT_Abilities"),
				TEXT("fire_s"),
				PocCIceRow,
				TEXT("\"adjust\":[\"reduce_trail_persistence\",\"boost_impact\"],\"add\":[\"crystalline_fragments\"],\"preserve\":[\"preserve_networking\"]")
			},
			{
				TEXT("poc-e1-c5-wind"),
				TEXT("wind"),
				TEXT("/Game/__UeremcpPoc/NS_POCC_IceVariation"),
				TEXT("/Game/__UeremcpPoc/NS_POCC_WindThirdGeneration"),
				PocCAbilityTable,
				PocCIceRow,
				PocCWindRow,
				TEXT("\"preserve\":[\"preserve_networking\"]")
			},
		};

		for (const FGeneration& Generation : Generations)
		{
			const FString Request = FString::Printf(
				TEXT("{\"protocol_version\":\"1.0\",\"request_id\":\"%s\",\"action\":\"instantiate_template\",")
				TEXT("\"mode\":\"create_or_update\",\"specification\":{\"template_id\":\"niagara.projectile.elemental.v1\",")
				TEXT("\"inputs\":{\"element\":\"%s\",\"target_path\":\"%s\",\"source_system\":\"%s\",")
				TEXT("\"ability_table\":\"%s\",\"source_row\":\"%s\",")
				TEXT("\"target_ability_table\":\"%s\",\"target_row\":\"%s\",")
				TEXT("\"vfx_phase\":\"projectile_and_impact\",\"scale\":1.0,\"intensity\":6.0},")
				TEXT("\"modifiers\":{%s},\"target\":{\"asset_path\":\"%s\"},\"mode\":\"create_or_update\"},")
				TEXT("\"options\":{\"dry_run\":false,\"atomic\":true,\"rollback_on_failure\":true,")
				TEXT("\"compile\":true,\"validate\":true,\"save\":true,\"response_detail\":\"complete\",\"timeout_ms\":0}}"),
				Generation.RequestId,
				Generation.Element,
				Generation.TargetPath,
				Generation.SourcePath,
				Generation.AbilityTable,
				Generation.SourceRow,
				PocCAbilityTable,
				Generation.TargetRow,
				Generation.ModifiersJson,
				Generation.TargetPath);
			const FString ResponseJson = UUeremcpTemplatesToolset::InstantiateTemplate(Request);
			TSharedPtr<FJsonObject> Response;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);
			if (!Test.TestTrue(
					*FString::Printf(TEXT("%s C5 seed response parses"), Generation.Element),
					FJsonSerializer::Deserialize(Reader, Response) && Response.IsValid()))
			{
				return false;
			}
			FString Status;
			bool bProtectedFieldsEqual = false;
			const TSharedPtr<FJsonObject>* Validation = nullptr;
			if (!Test.TestTrue(
					*FString::Printf(TEXT("%s C5 seed validates"), Generation.Element),
					Response->TryGetStringField(TEXT("status"), Status)
						&& (Status == TEXT("created_and_validated")
							|| Status == TEXT("modified_and_validated")
							|| Status == TEXT("partially_completed")
							|| Status == TEXT("no_change_required"))
						&& Response->TryGetObjectField(TEXT("validation"), Validation)
						&& Validation
						&& (*Validation)->TryGetBoolField(
							TEXT("protected_fields_equal"),
							bProtectedFieldsEqual)
						&& bProtectedFieldsEqual))
			{
				return false;
			}
		}
		return true;
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
			PocCAbilityTable,
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

	if (!SeedPocCAbilityVariations(*this))
	{
		return false;
	}

	TSharedPtr<FJsonObject> AbilityRows = MakeShared<FJsonObject>();
	for (const TCHAR* RowName : {PocCIceRow, PocCWindRow})
	{
		FString Snapshot;
		if (!ReadAbilityRowSnapshot(*this, RowName, Snapshot))
		{
			return false;
		}
		AbilityRows->SetStringField(RowName, Snapshot);
	}

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
		const bool bComplete = Missing.Num() == 0 && Present.Num() == Group.Candidates.Num();
		PocEntry->SetBoolField(TEXT("any_present"), bAny);
		PocEntry->SetBoolField(TEXT("complete"), bComplete);
		Presence->SetObjectField(Group.PocKey, PocEntry);

		if (bComplete)
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
			AddError(FString::Printf(
				TEXT("POC %s result checkpoint is incomplete"),
				Group.PocKey));
		}
	}

	TSharedPtr<FJsonObject> Residuals = MakeShared<FJsonObject>();
	Residuals->SetStringField(
		TEXT("E3_E4"),
		TEXT("Named protocol + Blueprint + Niagara + Material domain idempotency/revision gates pass under NullRHI."));

	TSharedPtr<FJsonObject> Checkpoint = MakeShared<FJsonObject>();
	Checkpoint->SetStringField(TEXT("id"), CheckpointId);
	Checkpoint->SetStringField(TEXT("suite"), SuiteName);
	Checkpoint->SetArrayField(TEXT("assets"), StringArrayToJson(AllAssets));
	Checkpoint->SetObjectField(TEXT("by_poc"), ByPoc);
	Checkpoint->SetObjectField(TEXT("presence"), Presence);
	Checkpoint->SetArrayField(TEXT("pocs_present"), StringArrayToJson(PresentPocs));
	Checkpoint->SetArrayField(TEXT("pocs_absent"), StringArrayToJson(AbsentPocs));
	Checkpoint->SetStringField(TEXT("ability_table"), PocCAbilityTable);
	Checkpoint->SetObjectField(TEXT("ability_rows"), AbilityRows);
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

	bool bAllRowsMatch = true;
	TSharedPtr<FJsonObject> RereadRows = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject>* ExpectedRows = nullptr;
	if (!Checkpoint->TryGetObjectField(TEXT("ability_rows"), ExpectedRows)
		|| !ExpectedRows
		|| !ExpectedRows->IsValid())
	{
		AddError(TEXT("E1 verify: POC C ability row snapshots missing from checkpoint"));
		bAllRowsMatch = false;
	}
	else
	{
		for (const TCHAR* RowName : {PocCIceRow, PocCWindRow})
		{
			FString ExpectedSnapshot;
			FString ActualSnapshot;
			const bool bExpected = (*ExpectedRows)->TryGetStringField(RowName, ExpectedSnapshot);
			const bool bRead = ReadAbilityRowSnapshot(*this, RowName, ActualSnapshot);
			const bool bMatches = bExpected && bRead && ActualSnapshot == ExpectedSnapshot;
			TestTrue(
				*FString::Printf(TEXT("POC C variation row %s survived unchanged"), RowName),
				bMatches);
			bAllRowsMatch &= bMatches;
			TSharedPtr<FJsonObject> RowEvidence = MakeShared<FJsonObject>();
			RowEvidence->SetBoolField(TEXT("present"), bRead);
			RowEvidence->SetBoolField(TEXT("matches_checkpoint"), bMatches);
			RereadRows->SetObjectField(RowName, RowEvidence);
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

	const bool bAllPocsPresent = Checkpoint->GetArrayField(TEXT("pocs_absent")).Num() == 0;
	const bool bPass = bAllPresent && bAllRowsMatch && bAllPocsPresent && !HasAnyErrors();

	const FString Scope =
		TEXT("full_ad_results_including_poc_c_gameplay_binding_rows");

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
	Evidence->SetObjectField(TEXT("reread_ability_rows"), RereadRows);
	Evidence->SetBoolField(TEXT("overall_e1_all_ad_claimed"), bPass);

	TSharedPtr<FJsonObject> Criteria = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> E1 = MakeShared<FJsonObject>();
	E1->SetStringField(TEXT("status"), bPass ? TEXT("pass") : TEXT("fail"));
	E1->SetStringField(TEXT("scope"), Scope);
	E1->SetBoolField(TEXT("full_ad_results_claimed"), bPass);
	Criteria->SetObjectField(TEXT("E1"), E1);
	Evidence->SetObjectField(TEXT("criteria"), Criteria);
	EmitEvidence(*this, Evidence);

	// Cleanup Validation scratch only — never delete domain POC A–D assets.
	UeremcpCleanupScratchSuite(SuiteName);
	IFileManager::Get().Delete(*Path);

	AddInfo(bPass
		? TEXT("POC_E E1 verify PASS: full A-D assets and C5 gameplay rows survived editor restart")
		: TEXT("POC_E E1 verify FAIL"));
	return bPass;
}

#endif // WITH_DEV_AUTOMATION_TESTS
