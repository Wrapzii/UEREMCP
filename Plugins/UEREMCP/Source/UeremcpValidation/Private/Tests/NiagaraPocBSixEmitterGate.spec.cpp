// WS-11 editor proof for the WS-07 POC B B7 scaffold.
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpNiagaraToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpValidationNiagaraPocB
{
	static constexpr const TCHAR* ExpectedAsset =
		TEXT("/Game/__UeremcpTests/NS_POCB_FireballProbe");
	static constexpr const TCHAR* MaterialsDirectory =
		TEXT("/Game/__UeremcpTests/Materials");

	static void DeleteProbeAssets()
	{
		if (UEditorAssetLibrary::DoesAssetExist(ExpectedAsset))
		{
			UEditorAssetLibrary::DeleteAsset(ExpectedAsset);
		}
		if (UEditorAssetLibrary::DoesDirectoryExist(MaterialsDirectory))
		{
			UEditorAssetLibrary::DeleteDirectory(MaterialsDirectory);
		}
	}

	struct FProbeCleanup
	{
		~FProbeCleanup()
		{
			DeleteProbeAssets();
		}
	};

	static bool LoadScaffold(
		FAutomationTestBase& Test,
		TSharedPtr<FJsonObject>& OutRoot,
		FString& OutRequestJson)
	{
		FString ScaffoldPath;
		if (!FParse::Value(
				FCommandLine::Get(),
				TEXT("UeremcpPocBScaffold="),
				ScaffoldPath))
		{
			Test.AddWarning(TEXT(
				"UEREMCP_POC_B_GATE_OUTCOME=SKIP reason=missing_-UeremcpPocBScaffold"));
			return false;
		}

		FString ScaffoldJson;
		if (!FFileHelper::LoadFileToString(ScaffoldJson, *ScaffoldPath))
		{
			Test.AddWarning(FString::Printf(
				TEXT("UEREMCP_POC_B_GATE_OUTCOME=SKIP reason=scaffold_unreadable path=%s"),
				*ScaffoldPath));
			return false;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ScaffoldJson);
		if (!FJsonSerializer::Deserialize(Reader, OutRoot) || !OutRoot.IsValid())
		{
			Test.AddError(TEXT("Scaffold is not valid JSON."));
			return false;
		}

		const TSharedPtr<FJsonObject>* Request = nullptr;
		if (!OutRoot->TryGetObjectField(TEXT("create_request"), Request)
			|| !Request || !Request->IsValid())
		{
			Test.AddError(TEXT("Scaffold has no create_request object."));
			return false;
		}

		const TSharedPtr<FJsonObject>* Target = nullptr;
		FString AssetPath;
		if (!(*Request)->TryGetObjectField(TEXT("target"), Target)
			|| !Target || !Target->IsValid()
			|| !(*Target)->TryGetStringField(TEXT("asset_path"), AssetPath)
			|| AssetPath != ExpectedAsset)
		{
			Test.AddError(TEXT("Scaffold target must be the dedicated POC B scratch asset."));
			return false;
		}

		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutRequestJson);
		return FJsonSerializer::Serialize(Request->ToSharedRef(), Writer);
	}

	static bool IsBooleanOrNull(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field)
	{
		const TSharedPtr<FJsonValue> Value = Object->TryGetField(Field);
		return Value.IsValid()
			&& (Value->Type == EJson::Boolean || Value->Type == EJson::Null);
	}

	static bool StringArrayContains(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		const FString& Expected)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(Field, Values) || !Values)
		{
			return false;
		}
		return Values->ContainsByPredicate([&Expected](const TSharedPtr<FJsonValue>& Value) {
			return Value.IsValid() && Value->Type == EJson::String && Value->AsString() == Expected;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPocBSixEmitterGate,
	"UEREMCP.Niagara.POCB.SixEmitterGateScaffold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPocBSixEmitterGate::RunTest(const FString& Parameters)
{
	using namespace UeremcpValidationNiagaraPocB;

	TSharedPtr<FJsonObject> Scaffold;
	FString RequestJson;
	if (!LoadScaffold(*this, Scaffold, RequestJson))
	{
		// Missing runner input is an explicit skip; malformed supplied input is a failure.
		return !HasAnyErrors();
	}

	FProbeCleanup Cleanup;
	DeleteProbeAssets();

	const FString ResponseJson = UUeremcpNiagaraToolset::CreateNiagaraEffect(RequestJson);
	TSharedPtr<FJsonObject> Response;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);
	if (!FJsonSerializer::Deserialize(Reader, Response) || !Response.IsValid())
	{
		AddError(TEXT("create_niagara_effect returned invalid JSON."));
		AddInfo(TEXT("UEREMCP_POC_B_GATE_OUTCOME=FAIL reason=response_not_json"));
		return false;
	}

	FString Status;
	Response->TryGetStringField(TEXT("status"), Status);
	FString Summary;
	Response->TryGetStringField(TEXT("summary"), Summary);
	if (Status == TEXT("rejected"))
	{
		AddError(FString::Printf(TEXT("create_niagara_effect rejected scaffold: %s"), *Summary));
		AddInfo(TEXT("UEREMCP_POC_B_GATE_OUTCOME=FAIL reason=create_rejected"));
		return false;
	}
	TestEqual(TEXT("status remains honest"), Status, FString(TEXT("partially_completed")));
	TestTrue(TEXT("never created_and_validated"), Status != TEXT("created_and_validated"));
	TestTrue(TEXT("never modified_and_validated"), Status != TEXT("modified_and_validated"));

	const TSharedPtr<FJsonObject>* GatesPtr = nullptr;
	const TSharedPtr<FJsonObject>* ValidationPtr = nullptr;
	if (!Response->TryGetObjectField(TEXT("poc_b_gates"), GatesPtr)
		|| !GatesPtr || !GatesPtr->IsValid())
	{
		AddError(TEXT("Response has no poc_b_gates object."));
		AddInfo(TEXT("UEREMCP_POC_B_GATE_OUTCOME=FAIL reason=missing_poc_b_gates"));
		return false;
	}
	if (!Response->TryGetObjectField(TEXT("validation"), ValidationPtr)
		|| !ValidationPtr || !ValidationPtr->IsValid())
	{
		AddError(TEXT("Response has no validation object."));
		AddInfo(TEXT("UEREMCP_POC_B_GATE_OUTCOME=FAIL reason=missing_validation"));
		return false;
	}

	const TSharedPtr<FJsonObject>& Gates = *GatesPtr;
	const TSharedPtr<FJsonObject>& Validation = *ValidationPtr;
	bool bValue = true;
	TestTrue(TEXT("round_trip_supported present"), Gates->TryGetBoolField(TEXT("round_trip_supported"), bValue));
	TestFalse(TEXT("round trip not overclaimed"), bValue);
	TestTrue(TEXT("B7 emitters present"), Gates->TryGetBoolField(TEXT("B7_emitters_non_empty"), bValue) && bValue);
	TestTrue(TEXT("B7 structural gate is bool/null"), IsBooleanOrNull(Gates, TEXT("B7_structural_match")));
	TestTrue(TEXT("B7 renderers-present gate is bool/null"), IsBooleanOrNull(Gates, TEXT("B7_renderers_present")));
	TestTrue(TEXT("B7 renderers-bound gate is bool/null"), IsBooleanOrNull(Gates, TEXT("B7_renderers_bound")));
	TestTrue(TEXT("B7 data-interface gate is bool/null"), IsBooleanOrNull(Gates, TEXT("B7_data_interfaces_complete")));
	TestTrue(
		TEXT("never_claims lists created_and_validated"),
		StringArrayContains(Gates, TEXT("never_claims"), TEXT("created_and_validated")));
	TestTrue(
		TEXT("never_claims lists modified_and_validated"),
		StringArrayContains(Gates, TEXT("never_claims"), TEXT("modified_and_validated")));

	const TSharedPtr<FJsonValue> Structural = Gates->TryGetField(TEXT("B7_structural_match"));
	if (Structural.IsValid() && Structural->Type == EJson::Boolean)
	{
		TestTrue(TEXT("inspect_fidelity present after inspect"), Gates->HasTypedField<EJson::Object>(TEXT("inspect_fidelity")));
	}

	bool bBindingsVerified = false;
	const bool bHasBindingVerdict =
		Validation->TryGetBoolField(TEXT("material_bindings_verified"), bBindingsVerified);
	bool bRenderersBound = false;
	const bool bHasRendererVerdict =
		Gates->TryGetBoolField(TEXT("B7_renderers_bound"), bRenderersBound);
	TestTrue(
		TEXT("renderer-bound true requires full binding re-read"),
		!bHasRendererVerdict || !bRenderersBound || (bHasBindingVerdict && bBindingsVerified));

	const TArray<TSharedPtr<FJsonValue>>* Performed = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Skipped = nullptr;
	TestTrue(
		TEXT("checks_performed recorded"),
		Validation->TryGetArrayField(TEXT("checks_performed"), Performed) && Performed);
	TestTrue(
		TEXT("checks_skipped recorded"),
		Validation->TryGetArrayField(TEXT("checks_skipped"), Skipped) && Skipped);

	const bool bPass = !HasAnyErrors();
	AddInfo(bPass
		? TEXT("UEREMCP_POC_B_GATE_OUTCOME=PASS proof=editor_create_reread_honesty")
		: TEXT("UEREMCP_POC_B_GATE_OUTCOME=FAIL reason=assertion_failure"));
	return bPass;
}

#endif // WITH_DEV_AUTOMATION_TESTS
