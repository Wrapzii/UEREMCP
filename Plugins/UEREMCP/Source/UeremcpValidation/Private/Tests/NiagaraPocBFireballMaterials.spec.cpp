// WS-11 single-call editor proof scaffold for POC B inline materials.
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpNiagaraToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpValidationNiagaraFireball
{
	static constexpr const TCHAR* ExpectedAsset =
		TEXT("/Game/__UeremcpPoc/NS_POCB_Fireball");
	static constexpr const TCHAR* PocRoot = TEXT("/Game/__UeremcpPoc/");

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

	static bool LoadJsonObject(
		FAutomationTestBase& Test,
		const FString& Path,
		const TCHAR* Label,
		TSharedPtr<FJsonObject>& OutObject)
	{
		FString Json;
		if (!FFileHelper::LoadFileToString(Json, *Path))
		{
			Test.AddWarning(FString::Printf(
				TEXT("UEREMCP_POC_B_FIREBALL_OUTCOME=SKIP reason=%s_unreadable path=%s"),
				Label,
				*Path));
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
		{
			Test.AddError(FString::Printf(TEXT("%s fixture is not valid JSON."), Label));
			return false;
		}
		return true;
	}

	static bool BuildRequest(
		FAutomationTestBase& Test,
		FString& OutRequestJson)
	{
		FString ScaffoldPath;
		FString MaterialsPath;
		if (!FParse::Value(
				FCommandLine::Get(),
				TEXT("UeremcpPocBScaffold="),
				ScaffoldPath)
			|| !FParse::Value(
				FCommandLine::Get(),
				TEXT("UeremcpPocBMaterials="),
				MaterialsPath))
		{
			Test.AddWarning(TEXT(
				"UEREMCP_POC_B_FIREBALL_OUTCOME=SKIP reason=missing_fixture_arguments"));
			return false;
		}

		TSharedPtr<FJsonObject> Scaffold;
		TSharedPtr<FJsonObject> MaterialFixture;
		if (!LoadJsonObject(Test, ScaffoldPath, TEXT("scaffold"), Scaffold)
			|| !LoadJsonObject(Test, MaterialsPath, TEXT("materials"), MaterialFixture))
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

		for (const FString& Role : ExpectedRoles())
		{
			if (!(*Materials)->HasTypedField<EJson::Object>(Role))
			{
				Test.AddError(FString::Printf(
					TEXT("Inline material fixture is missing role '%s'."),
					*Role));
			}
		}
		if (Test.HasAnyErrors())
		{
			return false;
		}

		(*Request)->SetStringField(TEXT("request_id"), TEXT("ws11-poc-b-fireball-materials"));
		(*Target)->SetStringField(TEXT("asset_path"), ExpectedAsset);
		(*Specification)->SetStringField(TEXT("name"), TEXT("NS_POCB_Fireball"));
		(*Specification)->SetObjectField(TEXT("materials"), *Materials);

		const TSharedRef<TJsonWriter<>> Writer =
			TJsonWriterFactory<>::Create(&OutRequestJson);
		return FJsonSerializer::Serialize(Request->ToSharedRef(), Writer);
	}

	static void DeleteIfPresent(const FString& AssetPath)
	{
		if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
		{
			UEditorAssetLibrary::DeleteAsset(AssetPath);
		}
	}

	static void DeleteKnownAssets()
	{
		DeleteIfPresent(ExpectedAsset);
		for (const FString& Role : ExpectedRoles())
		{
			DeleteIfPresent(FString::Printf(
				TEXT("/Game/__UeremcpPoc/Materials/MI_NS_POCB_Fireball_%s"),
				*Role));
		}
	}

	struct FCleanup
	{
		TArray<FString> ResponseAssets;

		~FCleanup()
		{
			for (const FString& Asset : ResponseAssets)
			{
				DeleteIfPresent(Asset);
			}
			DeleteKnownAssets();
		}
	};

	static void CollectAssetRefs(
		const TSharedPtr<FJsonObject>& Response,
		const TCHAR* Field,
		TSet<FString>& OutMaterialRoles,
		TArray<FString>& OutAssetPaths,
		bool& bOutHasMaterial)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Response->TryGetArrayField(Field, Values) || !Values)
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
			FString AssetClass;
			FString Role;
			Ref->TryGetStringField(TEXT("asset_path"), AssetPath);
			Ref->TryGetStringField(TEXT("asset_class"), AssetClass);
			Ref->TryGetStringField(TEXT("role"), Role);
			if (!AssetPath.IsEmpty())
			{
				OutAssetPaths.AddUnique(AssetPath);
			}
			if (AssetClass.Contains(TEXT("Material"))
				|| AssetPath.Contains(TEXT("/Materials/")))
			{
				bOutHasMaterial = true;
				if (!Role.IsEmpty())
				{
					OutMaterialRoles.Add(Role);
				}
			}
		}
	}

	static bool HasVerifiedRole(
		const TSharedPtr<FJsonObject>& MaterialBindings,
		const FString& Role)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!MaterialBindings->TryGetArrayField(
				TEXT("renderer_bindings_verified"),
				Values)
			|| !Values)
		{
			return false;
		}
		return Values->ContainsByPredicate([&Role](const TSharedPtr<FJsonValue>& Value) {
			return Value.IsValid()
				&& Value->Type == EJson::String
				&& Value->AsString().StartsWith(Role + TEXT("/"));
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPocBFireballMaterials,
	"UEREMCP.Niagara.POCB.FireballInlineMaterials",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPocBFireballMaterials::RunTest(const FString& Parameters)
{
	using namespace UeremcpValidationNiagaraFireball;

	FString RequestJson;
	if (!BuildRequest(*this, RequestJson))
	{
		return !HasAnyErrors();
	}

	FCleanup Cleanup;
	DeleteKnownAssets();

	// Exactly one goal-level create invocation; this is not transport/MCP proof.
	const FString ResponseJson = UUeremcpNiagaraToolset::CreateNiagaraEffect(RequestJson);
	TSharedPtr<FJsonObject> Response;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);
	if (!FJsonSerializer::Deserialize(Reader, Response) || !Response.IsValid())
	{
		AddError(TEXT("create_niagara_effect returned invalid JSON."));
		AddInfo(TEXT("UEREMCP_POC_B_FIREBALL_OUTCOME=FAIL reason=response_not_json"));
		return false;
	}

	FString Status;
	FString Summary;
	Response->TryGetStringField(TEXT("status"), Status);
	Response->TryGetStringField(TEXT("summary"), Summary);
	if (Status == TEXT("rejected")
		&& Summary.Contains(TEXT("/Game/__UeremcpTests")))
	{
		AddWarning(FString::Printf(
			TEXT("UEREMCP_POC_B_FIREBALL_OUTCOME=SKIP reason=poc_root_not_allowed detail=%s"),
			*Summary));
		return true;
	}
	if (Status == TEXT("rejected"))
	{
		AddError(FString::Printf(TEXT("Fireball create rejected: %s"), *Summary));
		AddInfo(TEXT("UEREMCP_POC_B_FIREBALL_OUTCOME=FAIL reason=create_rejected"));
		return false;
	}

	TestEqual(
		TEXT("status remains honest"),
		Status,
		FString(TEXT("partially_completed")));

	TSet<FString> MaterialRoles;
	bool bHasCreatedMaterial = false;
	bool bHasReusedMaterial = false;
	const TSharedPtr<FJsonObject>* Result = nullptr;
	if (!Response->TryGetObjectField(TEXT("result"), Result)
		|| !Result || !Result->IsValid())
	{
		AddError(TEXT("Response has no result object for the merged asset manifest."));
		AddInfo(TEXT("UEREMCP_POC_B_FIREBALL_OUTCOME=FAIL reason=missing_result_manifest"));
		return false;
	}
	CollectAssetRefs(
		*Result,
		TEXT("created_assets"),
		MaterialRoles,
		Cleanup.ResponseAssets,
		bHasCreatedMaterial);
	CollectAssetRefs(
		*Result,
		TEXT("reused_assets"),
		MaterialRoles,
		Cleanup.ResponseAssets,
		bHasReusedMaterial);

	for (const FString& Role : ExpectedRoles())
	{
		TestTrue(
			*FString::Printf(TEXT("merged material manifest contains role %s"), *Role),
			MaterialRoles.Contains(Role));
	}

	for (const FString& Asset : Cleanup.ResponseAssets)
	{
		if (Asset.Contains(TEXT("/Materials/")))
		{
			TestTrue(
				*FString::Printf(TEXT("POC material asset remains under POC root: %s"), *Asset),
				Asset.StartsWith(PocRoot));
		}
	}

	const TSharedPtr<FJsonObject>* Gates = nullptr;
	const TSharedPtr<FJsonObject>* Validation = nullptr;
	const TSharedPtr<FJsonObject>* MaterialBindings = nullptr;
	if (!Response->TryGetObjectField(TEXT("poc_b_gates"), Gates)
		|| !Gates || !Gates->IsValid()
		|| !Response->TryGetObjectField(TEXT("validation"), Validation)
		|| !Validation || !Validation->IsValid()
		|| !Response->TryGetObjectField(TEXT("material_bindings"), MaterialBindings)
		|| !MaterialBindings || !MaterialBindings->IsValid())
	{
		AddError(TEXT("Response is missing POC B material evidence objects."));
		AddInfo(TEXT("UEREMCP_POC_B_FIREBALL_OUTCOME=FAIL reason=missing_evidence_objects"));
		return false;
	}

	bool bReportedCreated = false;
	bool bReportedReused = false;
	bool bB4Gate = false;
	bool bBindingsVerified = false;
	TestTrue(
		TEXT("B2 created-assets verdict present"),
		(*Gates)->TryGetBoolField(TEXT("B2_created_assets_reported"), bReportedCreated));
	TestTrue(
		TEXT("B2 reused-assets verdict present"),
		(*Gates)->TryGetBoolField(TEXT("B2_reused_assets_reported"), bReportedReused));
	TestEqual(TEXT("B2 created verdict matches manifest"), bReportedCreated, bHasCreatedMaterial);
	TestEqual(TEXT("B2 reused verdict matches manifest"), bReportedReused, bHasReusedMaterial);
	TestTrue(TEXT("B2 reports created or reused materials"), bReportedCreated || bReportedReused);
	TestTrue(
		TEXT("B4 gate is true"),
		(*Gates)->TryGetBoolField(TEXT("B4_material_bindings_verified"), bB4Gate)
			&& bB4Gate);
	TestTrue(
		TEXT("validation material binding re-read is true"),
		(*Validation)->TryGetBoolField(TEXT("material_bindings_verified"), bBindingsVerified)
			&& bBindingsVerified);
	for (const FString& Role : ExpectedRoles())
	{
		TestTrue(
			*FString::Printf(TEXT("renderer binding verified for role %s"), *Role),
			HasVerifiedRole(*MaterialBindings, Role));
	}

	const bool bPass = !HasAnyErrors();
	AddInfo(bPass
		? TEXT("UEREMCP_POC_B_FIREBALL_OUTCOME=PASS proof=editor_single_create_inline_materials_b2_b4")
		: TEXT("UEREMCP_POC_B_FIREBALL_OUTCOME=FAIL reason=assertion_failure"));
	return bPass;
}

#endif // WITH_DEV_AUTOMATION_TESTS
