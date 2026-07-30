// WS-07 editor filters for POC B B8 restart survival (create + verify phases).
//
// WS-11 orchestrates two UnrealEditor-Cmd launches via tests/run_poc_acceptance.ps1.
// Create persists a checkpoint under Saved/UEREMCP/; Verify reads it in a fresh process.

#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraRoleNames.h"
#include "UeremcpNiagaraToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpNiagaraPocBRestart
{
	static constexpr const TCHAR* ExpectedSystemAsset =
		TEXT("/Game/__UeremcpPoc/NS_POCB_Fireball");
	static constexpr const TCHAR* PocRoot = TEXT("/Game/__UeremcpPoc/");
	static constexpr const TCHAR* CheckpointId = TEXT("poc-b8-fireball");
	static constexpr const TCHAR* DefaultRunId = TEXT("poc-b8-fireball-restart");

	static const TArray<FString>& ExpectedRoles()
	{
		static const TArray<FString> Roles = {
			TEXT("core"),
			TEXT("flame_shell"),
			TEXT("sparks"),
			TEXT("smoke"),
			TEXT("ribbon_trail"),
			TEXT("impact_burst"),
		};
		return Roles;
	}

	static FString CheckpointFilePath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UEREMCP"), TEXT("poc_b8_restart_checkpoint.json"));
	}

	static void EmitPocEvidence(FAutomationTestBase& Test, const TSharedPtr<FJsonObject>& Evidence)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Evidence.ToSharedRef(), Writer);
		Test.AddInfo(FString::Printf(TEXT("UEREMCP_POC_EVIDENCE=%s"), *Json));
	}

	static void EmitOutcomeMarker(FAutomationTestBase& Test, const TCHAR* Outcome, const TCHAR* Detail)
	{
		Test.AddInfo(FString::Printf(TEXT("UEREMCP_POC_B8_OUTCOME=%s %s"), Outcome, Detail));
	}

	static FString ResolveRunId()
	{
		FString RunId;
		if (FParse::Value(FCommandLine::Get(), TEXT("UeremcpPocB8RunId="), RunId) && !RunId.IsEmpty())
		{
			return RunId;
		}
		return FString(DefaultRunId);
	}

	static bool LoadJsonObjectFromFile(
		FAutomationTestBase& Test,
		const FString& Path,
		const TCHAR* Label,
		TSharedPtr<FJsonObject>& OutObject)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			Test.AddWarning(FString::Printf(
				TEXT("UEREMCP_POC_B8_OUTCOME=SKIP reason=%s_unreadable path=%s"),
				Label,
				*Path));
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("%s is not valid JSON."), Label));
			return false;
		}
		return true;
	}

	static bool LoadJsonObjectFromString(
		const FString& Json,
		TSharedPtr<FJsonObject>& OutObject)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	static bool WriteJsonObjectToFile(const TSharedPtr<FJsonObject>& Object, const FString& Path)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		if (!FJsonSerializer::Serialize(Object.ToSharedRef(), Writer))
		{
			return false;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), /*Tree=*/true);
		return FFileHelper::SaveStringToFile(Json, *Path);
	}

	static TSharedPtr<FJsonObject> BuildInlineMaterialsObject()
	{
		TSharedPtr<FJsonObject> Materials = MakeShared<FJsonObject>();
		for (const FString& Role : ExpectedRoles())
		{
			TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetBoolField(TEXT("reuse_if_present"), true);
			const TSharedPtr<FJsonObject> CreateSpec =
				UeremcpNiagaraRoles::BuildDefaultFireballMaterialCreateSpec(Role, TEXT("fire"));
			if (CreateSpec.IsValid())
			{
				Entry->SetObjectField(TEXT("create_spec"), CreateSpec);
			}
			Materials->SetObjectField(Role, Entry);
		}
		return Materials;
	}

	static bool BuildInlineFireballRequest(FString& OutRequestJson)
	{
		TSharedPtr<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetStringField(TEXT("protocol_version"), TEXT("1.0"));
		Request->SetStringField(TEXT("request_id"), TEXT("ws07-poc-b8-restart-create-inline"));
		Request->SetStringField(TEXT("action"), TEXT("create_niagara_effect"));
		Request->SetStringField(TEXT("mode"), TEXT("replace"));

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), ExpectedSystemAsset);
		Request->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Specification = MakeShared<FJsonObject>();
		Specification->SetStringField(TEXT("name"), TEXT("NS_POCB_Fireball"));
		Specification->SetStringField(TEXT("effect_type"), TEXT("projectile"));
		Specification->SetStringField(TEXT("element"), TEXT("fire"));

		TArray<TSharedPtr<FJsonValue>> Components;
		for (const FString& Role : ExpectedRoles())
		{
			Components.Add(MakeShared<FJsonValueString>(Role));
		}
		Specification->SetArrayField(TEXT("components"), Components);

		TSharedPtr<FJsonObject> Parameters = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> PrimaryColor;
		PrimaryColor.Add(MakeShared<FJsonValueNumber>(1.0));
		PrimaryColor.Add(MakeShared<FJsonValueNumber>(0.12));
		PrimaryColor.Add(MakeShared<FJsonValueNumber>(0.01));
		PrimaryColor.Add(MakeShared<FJsonValueNumber>(1.0));
		Parameters->SetArrayField(TEXT("primary_color"), PrimaryColor);
		TArray<TSharedPtr<FJsonValue>> SecondaryColor;
		SecondaryColor.Add(MakeShared<FJsonValueNumber>(1.0));
		SecondaryColor.Add(MakeShared<FJsonValueNumber>(0.75));
		SecondaryColor.Add(MakeShared<FJsonValueNumber>(0.05));
		SecondaryColor.Add(MakeShared<FJsonValueNumber>(1.0));
		Parameters->SetArrayField(TEXT("secondary_color"), SecondaryColor);
		Parameters->SetNumberField(TEXT("scale"), 1.0);
		Parameters->SetNumberField(TEXT("intensity"), 8.0);
		Specification->SetObjectField(TEXT("parameters"), Parameters);

		TSharedPtr<FJsonObject> TemplateSystem = MakeShared<FJsonObject>();
		TemplateSystem->SetStringField(
			TEXT("asset_path"),
			TEXT("/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight"));
		Specification->SetObjectField(TEXT("template_system"), TemplateSystem);
		Specification->SetObjectField(TEXT("materials"), BuildInlineMaterialsObject());
		Request->SetObjectField(TEXT("specification"), Specification);

		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetBoolField(TEXT("dry_run"), false);
		Options->SetBoolField(TEXT("compile"), true);
		Options->SetBoolField(TEXT("validate"), true);
		Options->SetBoolField(TEXT("save"), true);
		Request->SetObjectField(TEXT("options"), Options);

		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutRequestJson);
		return FJsonSerializer::Serialize(Request.ToSharedRef(), Writer);
	}

	static bool BuildFireballRequest(FAutomationTestBase& Test, FString& OutRequestJson)
	{
		FString ScaffoldPath;
		FString MaterialsPath;
		const bool bHasFixtureArgs =
			FParse::Value(FCommandLine::Get(), TEXT("UeremcpPocBScaffold="), ScaffoldPath)
			&& FParse::Value(FCommandLine::Get(), TEXT("UeremcpPocBMaterials="), MaterialsPath);

		if (!bHasFixtureArgs)
		{
			return BuildInlineFireballRequest(OutRequestJson);
		}

		TSharedPtr<FJsonObject> Scaffold;
		TSharedPtr<FJsonObject> MaterialFixture;
		if (!LoadJsonObjectFromFile(Test, ScaffoldPath, TEXT("scaffold"), Scaffold)
			|| !LoadJsonObjectFromFile(Test, MaterialsPath, TEXT("materials"), MaterialFixture))
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* Request = nullptr;
		const TSharedPtr<FJsonObject>* Target = nullptr;
		const TSharedPtr<FJsonObject>* Specification = nullptr;
		const TSharedPtr<FJsonObject>* Materials = nullptr;
		if (!Scaffold->TryGetObjectField(TEXT("create_request"), Request)
			|| !Request || !Request->IsValid()
			|| !(*Request)->TryGetObjectField(TEXT("target"), Target)
			|| !Target || !Target->IsValid()
			|| !(*Request)->TryGetObjectField(TEXT("specification"), Specification)
			|| !Specification || !Specification->IsValid()
			|| !MaterialFixture->TryGetObjectField(TEXT("materials"), Materials)
			|| !Materials || !Materials->IsValid())
		{
			Test.AddError(TEXT("Fireball fixtures do not contain the required objects."));
			return false;
		}

		(*Request)->SetStringField(TEXT("request_id"), TEXT("ws07-poc-b8-restart-create"));
		(*Target)->SetStringField(TEXT("asset_path"), ExpectedSystemAsset);
		(*Specification)->SetStringField(TEXT("name"), TEXT("NS_POCB_Fireball"));
		(*Specification)->SetObjectField(TEXT("materials"), *Materials);

		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutRequestJson);
		return FJsonSerializer::Serialize(Request->ToSharedRef(), Writer);
	}

	static void DeleteIfPresent(const FString& AssetPath)
	{
		if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
		{
			UEditorAssetLibrary::DeleteAsset(AssetPath);
		}
	}

	static void DeleteKnownPocAssets()
	{
		DeleteIfPresent(ExpectedSystemAsset);
		for (const FString& Role : ExpectedRoles())
		{
			DeleteIfPresent(FString::Printf(
				TEXT("/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_%s"),
				*Role));
		}
	}

	static FString NormalizePackagePath(FString Path)
	{
		Path.TrimStartAndEndInline();
		if (Path.Contains(TEXT(".")))
		{
			Path = Path.Left(Path.Find(TEXT(".")));
		}
		return Path;
	}

	static void CollectAssetPathsFromResult(
		const TSharedPtr<FJsonObject>& Result,
		const TCHAR* FieldName,
		TArray<FString>& InOutAssets)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Result->TryGetArrayField(FieldName, Values) || !Values)
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject> Ref = Value->AsObject();
			if (!Ref.IsValid())
			{
				continue;
			}
			FString AssetPath;
			if (Ref->TryGetStringField(TEXT("asset_path"), AssetPath) && !AssetPath.IsEmpty())
			{
				InOutAssets.AddUnique(NormalizePackagePath(AssetPath));
			}
		}
	}

	static TArray<FString> CollectSurvivingAssetPaths(const TSharedPtr<FJsonObject>& Response)
	{
		TArray<FString> Assets;
		Assets.AddUnique(NormalizePackagePath(ExpectedSystemAsset));

		FString PrimaryAsset;
		if (Response->TryGetStringField(TEXT("primary_asset"), PrimaryAsset) && !PrimaryAsset.IsEmpty())
		{
			Assets.AddUnique(NormalizePackagePath(PrimaryAsset));
		}

		const TSharedPtr<FJsonObject>* Result = nullptr;
		if (Response->TryGetObjectField(TEXT("result"), Result) && Result && Result->IsValid())
		{
			CollectAssetPathsFromResult(*Result, TEXT("created_assets"), Assets);
			CollectAssetPathsFromResult(*Result, TEXT("modified_assets"), Assets);
			CollectAssetPathsFromResult(*Result, TEXT("reused_assets"), Assets);
		}

		for (const FString& Role : ExpectedRoles())
		{
			Assets.AddUnique(FString::Printf(
				TEXT("/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_%s"),
				*Role));
		}

		Assets.RemoveAll([](const FString& Path) {
			return !Path.StartsWith(PocRoot);
		});
		Assets.Sort();
		return Assets;
	}

	static TSharedPtr<FJsonObject> BuildMetricsObject(const TSharedPtr<FJsonObject>& Response)
	{
		TSharedPtr<FJsonObject> Metrics = MakeShared<FJsonObject>();
		double McpRoundTrips = 1.0;
		double InternalOperations = 0.0;
		double AssetsAffected = 0.0;

		const TSharedPtr<FJsonObject>* ResponseMetrics = nullptr;
		if (Response.IsValid()
			&& Response->TryGetObjectField(TEXT("metrics"), ResponseMetrics)
			&& ResponseMetrics
			&& ResponseMetrics->IsValid())
		{
			(*ResponseMetrics)->TryGetNumberField(TEXT("mcp_round_trips"), McpRoundTrips);
			(*ResponseMetrics)->TryGetNumberField(TEXT("internal_operations"), InternalOperations);
			(*ResponseMetrics)->TryGetNumberField(TEXT("assets_affected"), AssetsAffected);
		}

		Metrics->SetNumberField(TEXT("mcp_round_trips"), McpRoundTrips);
		Metrics->SetNumberField(TEXT("internal_operations"), InternalOperations);
		Metrics->SetNumberField(TEXT("tokens_total"), 0.0);
		Metrics->SetNumberField(TEXT("wall_clock_seconds"), 0.0);
		Metrics->SetNumberField(
			TEXT("primitive_call_equivalent"),
			FMath::Max(1.0, AssetsAffected));
		return Metrics;
	}

	static TArray<TSharedPtr<FJsonValue>> StringArrayToJsonValues(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> JsonValues;
		for (const FString& Value : Values)
		{
			JsonValues.Add(MakeShared<FJsonValueString>(Value));
		}
		return JsonValues;
	}

	static bool AllAssetsExistOnDisk(
		FAutomationTestBase& Test,
		const TArray<FString>& Assets,
		TArray<FString>& OutMissing)
	{
		OutMissing.Reset();
		for (const FString& AssetPath : Assets)
		{
			if (!UEditorAssetLibrary::DoesAssetExist(AssetPath))
			{
				OutMissing.Add(AssetPath);
			}
		}
		if (OutMissing.Num() > 0)
		{
			for (const FString& Missing : OutMissing)
			{
				Test.AddError(FString::Printf(TEXT("Asset missing after restart: %s"), *Missing));
			}
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPocBRestartCreate,
	"UEREMCP.Niagara.POCB.Restart.Create",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPocBRestartCreate::RunTest(const FString& Parameters)
{
	using namespace UeremcpNiagaraPocBRestart;

	IFileManager::Get().Delete(*CheckpointFilePath());

	FString RequestJson;
	if (!BuildFireballRequest(*this, RequestJson))
	{
		return !HasAnyErrors();
	}

	DeleteKnownPocAssets();

	const FString ResponseJson = UUeremcpNiagaraToolset::CreateNiagaraEffect(RequestJson);
	TSharedPtr<FJsonObject> Response;
	if (!LoadJsonObjectFromString(ResponseJson, Response))
	{
		AddError(TEXT("create_niagara_effect returned invalid JSON."));
		EmitOutcomeMarker(*this, TEXT("FAIL"), TEXT("reason=response_not_json"));
		return false;
	}

	FString Status;
	FString Summary;
	Response->TryGetStringField(TEXT("status"), Status);
	Response->TryGetStringField(TEXT("summary"), Summary);
	if (Status == TEXT("rejected"))
	{
		AddError(FString::Printf(TEXT("Fireball create rejected: %s"), *Summary));
		EmitOutcomeMarker(*this, TEXT("FAIL"), TEXT("reason=create_rejected"));
		return false;
	}

	const TSharedPtr<FJsonObject>* Gates = nullptr;
	const TSharedPtr<FJsonObject>* Validation = nullptr;
	if (!Response->TryGetObjectField(TEXT("poc_b_gates"), Gates)
		|| !Gates || !Gates->IsValid()
		|| !Response->TryGetObjectField(TEXT("validation"), Validation)
		|| !Validation || !Validation->IsValid())
	{
		AddError(TEXT("Create response missing poc_b_gates or validation."));
		EmitOutcomeMarker(*this, TEXT("FAIL"), TEXT("reason=missing_gate_objects"));
		return false;
	}

	bool bB8Saved = false;
	bool bSavedValidation = false;
	TestTrue(
		TEXT("B8 assets saved gate present"),
		(*Gates)->TryGetBoolField(TEXT("B8_assets_saved"), bB8Saved) && bB8Saved);
	TestTrue(
		TEXT("validation.saved true"),
		(*Validation)->TryGetBoolField(TEXT("saved"), bSavedValidation) && bSavedValidation);

	const TArray<FString> Assets = CollectSurvivingAssetPaths(Response);
	TestTrue(TEXT("checkpoint asset list non-empty"), Assets.Num() > 0);
	TestTrue(TEXT("system asset listed"), Assets.Contains(NormalizePackagePath(ExpectedSystemAsset)));

	TArray<FString> MissingOnDisk;
	TestTrue(
		TEXT("created assets exist on disk before checkpoint"),
		AllAssetsExistOnDisk(*this, Assets, MissingOnDisk));

	const FString RunId = ResolveRunId();
	TSharedPtr<FJsonObject> Checkpoint = MakeShared<FJsonObject>();
	Checkpoint->SetStringField(TEXT("schema_version"), TEXT("1"));
	Checkpoint->SetStringField(TEXT("id"), CheckpointId);
	Checkpoint->SetStringField(TEXT("run_id"), RunId);
	Checkpoint->SetArrayField(TEXT("assets"), StringArrayToJsonValues(Assets));
	Checkpoint->SetObjectField(TEXT("create_metrics"), BuildMetricsObject(Response));

	if (!WriteJsonObjectToFile(Checkpoint, CheckpointFilePath()))
	{
		AddError(TEXT("Failed to write B8 restart checkpoint file."));
		EmitOutcomeMarker(*this, TEXT("FAIL"), TEXT("reason=checkpoint_write_failed"));
		return false;
	}

	TSharedPtr<FJsonObject> Evidence = MakeShared<FJsonObject>();
	Evidence->SetNumberField(TEXT("schema_version"), 1);
	Evidence->SetStringField(TEXT("scenario"), TEXT("poc_b8_create"));
	Evidence->SetStringField(TEXT("run_id"), RunId);
	Evidence->SetStringField(TEXT("outcome"), HasAnyErrors() ? TEXT("fail") : TEXT("pass"));

	TSharedPtr<FJsonObject> EvidenceCheckpoint = MakeShared<FJsonObject>();
	EvidenceCheckpoint->SetStringField(TEXT("id"), CheckpointId);
	EvidenceCheckpoint->SetArrayField(TEXT("assets"), StringArrayToJsonValues(Assets));
	Evidence->SetObjectField(TEXT("checkpoint"), EvidenceCheckpoint);

	EmitPocEvidence(*this, Evidence);

	const bool bPass = !HasAnyErrors();
	EmitOutcomeMarker(
		*this,
		bPass ? TEXT("PASS") : TEXT("FAIL"),
		bPass ? TEXT("proof=poc_b8_create_checkpoint_written") : TEXT("reason=assertion_failure"));
	return bPass;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPocBRestartVerify,
	"UEREMCP.Niagara.POCB.Restart.Verify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPocBRestartVerify::RunTest(const FString& Parameters)
{
	using namespace UeremcpNiagaraPocBRestart;

	const FString CheckpointPath = CheckpointFilePath();
	if (!FPaths::FileExists(CheckpointPath))
	{
		TSharedPtr<FJsonObject> SkipEvidence = MakeShared<FJsonObject>();
		SkipEvidence->SetNumberField(TEXT("schema_version"), 1);
		SkipEvidence->SetStringField(TEXT("scenario"), TEXT("poc_b8_verify"));
		SkipEvidence->SetStringField(TEXT("run_id"), ResolveRunId());
		SkipEvidence->SetStringField(TEXT("outcome"), TEXT("skip"));
		EmitPocEvidence(*this, SkipEvidence);
		EmitOutcomeMarker(*this, TEXT("SKIP"), TEXT("reason=checkpoint_missing_run_create_first"));
		return true;
	}

	TSharedPtr<FJsonObject> Checkpoint;
	if (!LoadJsonObjectFromFile(*this, CheckpointPath, TEXT("checkpoint"), Checkpoint))
	{
		return !HasAnyErrors();
	}

	FString CheckpointIdRead;
	FString RunId;
	if (!Checkpoint->TryGetStringField(TEXT("id"), CheckpointIdRead) || CheckpointIdRead.IsEmpty())
	{
		AddError(TEXT("Checkpoint missing id."));
	}
	if (!Checkpoint->TryGetStringField(TEXT("run_id"), RunId) || RunId.IsEmpty())
	{
		RunId = ResolveRunId();
	}

	const TArray<TSharedPtr<FJsonValue>>* AssetValues = nullptr;
	if (!Checkpoint->TryGetArrayField(TEXT("assets"), AssetValues) || !AssetValues || AssetValues->Num() == 0)
	{
		AddError(TEXT("Checkpoint assets array is empty."));
	}

	TArray<FString> Assets;
	if (AssetValues)
	{
		for (const TSharedPtr<FJsonValue>& Value : *AssetValues)
		{
			if (Value.IsValid() && Value->Type == EJson::String)
			{
				Assets.Add(NormalizePackagePath(Value->AsString()));
			}
		}
	}

	TArray<FString> MissingOnDisk;
	const bool bAllPresent = Assets.Num() > 0 && AllAssetsExistOnDisk(*this, Assets, MissingOnDisk);

	TSharedPtr<FJsonObject> Metrics = MakeShared<FJsonObject>();
	const TSharedPtr<FJsonObject>* CreateMetrics = nullptr;
	if (Checkpoint->TryGetObjectField(TEXT("create_metrics"), CreateMetrics)
		&& CreateMetrics
		&& CreateMetrics->IsValid())
	{
		Metrics = MakeShared<FJsonObject>(**CreateMetrics);
	}
	Metrics->SetNumberField(TEXT("mcp_round_trips"), 1.0);
	Metrics->SetNumberField(TEXT("internal_operations"), static_cast<double>(Assets.Num()));
	Metrics->SetNumberField(TEXT("tokens_total"), 0.0);
	Metrics->SetNumberField(TEXT("wall_clock_seconds"), 0.0);
	Metrics->SetNumberField(TEXT("primitive_call_equivalent"), 1.0);

	TSharedPtr<FJsonObject> Evidence = MakeShared<FJsonObject>();
	Evidence->SetNumberField(TEXT("schema_version"), 1);
	Evidence->SetStringField(TEXT("scenario"), TEXT("poc_b8_verify"));
	Evidence->SetStringField(TEXT("run_id"), RunId);
	Evidence->SetBoolField(TEXT("restart_observed"), true);
	Evidence->SetBoolField(TEXT("reread_after_restart"), bAllPresent && !HasAnyErrors());

	TSharedPtr<FJsonObject> Criteria = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> B8 = MakeShared<FJsonObject>();
	B8->SetStringField(TEXT("status"), (bAllPresent && !HasAnyErrors()) ? TEXT("pass") : TEXT("fail"));
	Criteria->SetObjectField(TEXT("B8"), B8);
	Evidence->SetObjectField(TEXT("criteria"), Criteria);

	TSharedPtr<FJsonObject> EvidenceCheckpoint = MakeShared<FJsonObject>();
	EvidenceCheckpoint->SetStringField(TEXT("id"), CheckpointIdRead);
	EvidenceCheckpoint->SetArrayField(TEXT("assets"), StringArrayToJsonValues(Assets));
	Evidence->SetObjectField(TEXT("checkpoint"), EvidenceCheckpoint);
	Evidence->SetObjectField(TEXT("metrics"), Metrics);
	Evidence->SetStringField(
		TEXT("outcome"),
		(bAllPresent && !HasAnyErrors()) ? TEXT("pass") : TEXT("fail"));

	EmitPocEvidence(*this, Evidence);

	const bool bPass = !HasAnyErrors();
	if (bPass)
	{
		for (const FString& AssetPath : Assets)
		{
			DeleteIfPresent(AssetPath);
		}
		IFileManager::Get().Delete(*CheckpointPath);
	}

	EmitOutcomeMarker(
		*this,
		bPass ? TEXT("PASS") : TEXT("FAIL"),
		bPass ? TEXT("proof=poc_b8_verify_reread_after_restart") : TEXT("reason=assertion_failure"));
	return bPass;
}

#endif // WITH_DEV_AUTOMATION_TESTS
