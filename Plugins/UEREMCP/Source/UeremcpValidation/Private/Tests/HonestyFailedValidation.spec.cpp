// POC E6 — deliberately broken request → failed_validation with actionable diagnostics.
#include "UeremcpHonestyContract.h"
#include "UeremcpScratchPaths.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpBlueprintToolset.h"
#include "UeremcpEnvelope.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpHonestyFailedValidation
{
	static const FString SuiteName = TEXT("Honesty_FailedValidation");

	static FString SoftPath(const FString& AssetName)
	{
		return UeremcpMakeScratchPackagePath(SuiteName, AssetName) + TEXT(".") + AssetName;
	}

	static UBlueprint* CreateMinimalActorBlueprint(const FString& AssetName, FAutomationTestBase& Test)
	{
		const FString PackagePath = UeremcpMakeScratchPackagePath(SuiteName, AssetName);
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("CreatePackage"), Package))
		{
			return nullptr;
		}
		Package->FullyLoad();

		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
			AActor::StaticClass(),
			Package,
			FName(*AssetName),
			BPTYPE_Normal,
			UBlueprint::StaticClass(),
			UBlueprintGeneratedClass::StaticClass(),
			NAME_None);
		if (!Test.TestNotNull(TEXT("CreateBlueprint"), Blueprint))
		{
			return nullptr;
		}

		FAssetRegistryModule::AssetCreated(Blueprint);
		Package->MarkPackageDirty();

		FString Filename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(
				PackagePath, Filename, FPackageName::GetAssetPackageExtension()))
		{
			Test.AddError(TEXT("package path conversion failed"));
			return nullptr;
		}
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		const FSavePackageResultStruct Result = UPackage::Save(Package, Blueprint, *Filename, SaveArgs);
		if (!Test.TestTrue(TEXT("save blueprint"), Result.Result == ESavePackageResult::Success))
		{
			return nullptr;
		}
		return Blueprint;
	}

	static TSharedPtr<FJsonObject> ParseJson(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Root);
		return Root;
	}

	static void EmitEvidence(FAutomationTestBase& Test, bool bPass, const FString& Detail)
	{
		const FString Marker = FString::Printf(
			TEXT("UEREMCP_POC_EVIDENCE={\"schema_version\":1,\"scenario\":\"poc_e_e6\","
				"\"run_id\":\"poc-e-e6-failed-validation\",\"outcome\":\"%s\","
				"\"criteria\":{\"E6\":{\"status\":\"%s\",\"detail\":\"%s\"}}}"),
			bPass ? TEXT("pass") : TEXT("fail"),
			bPass ? TEXT("pass") : TEXT("fail"),
			*Detail);
		Test.AddInfo(Marker);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpHonestyFailedValidation,
	"UEREMCP.Validation.Honesty.BrokenRequestFailedValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpHonestyFailedValidation::RunTest(const FString& Parameters)
{
	using namespace UeremcpHonestyFailedValidation;
	(void)Parameters;

	FUeremcpScratchGuard Guard(SuiteName);

	// --- Protocol-level: forge a failed_validation response with actionable text ---
	FUeremcpResponse Failure;
	Failure.RequestId = TEXT("e6-protocol");
	Failure.Status = TEXT("failed_validation");
	Failure.Summary = TEXT("Submitted graph is missing required nodes array — cannot write.");
	Failure.CapabilityNotes.Add(TEXT("blueprint.submitted_graph_structure"));
	TestTrue(TEXT("honest failure status"),
		UeremcpHonestyContract::IsHonestFailureStatus(Failure.Status));
	TestTrue(TEXT("actionable diagnostics"),
		UeremcpHonestyContract::HasActionableDiagnostics(Failure));
	TestFalse(TEXT("validated is not honest failure"),
		UeremcpHonestyContract::IsHonestFailureStatus(TEXT("created_and_validated")));

	const FString FailureJson = FUeremcpEnvelope::SerializeResponse(Failure);
	TestTrue(TEXT("serializes failed_validation"), FailureJson.Contains(TEXT("\"failed_validation\"")));
	TestTrue(TEXT("serializes summary"), FailureJson.Contains(TEXT("missing required nodes")));

	// --- Real Blueprint domain: structurally broken replace ---
	static const FString AssetName = TEXT("BP_E6_BrokenSubmit");
	if (!CreateMinimalActorBlueprint(AssetName, *this))
	{
		EmitEvidence(*this, false, TEXT("blueprint_create_failed"));
		return false;
	}
	const FString AssetPath = SoftPath(AssetName);

	// Deliberately incomplete graph (missing graph_type/fidelity) → failed_validation.
	const FString BrokenSubmit = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"e6-broken","action":"submit_graph",)"
			 TEXT(R"("mode":"replace",)"
			 TEXT(R"("target":{"asset_path":"%s","graph_id":"EventGraph"},)"
			 TEXT(R"("specification":{"graph":{"schema_version":"1.0","asset_path":"%s",)"
			 TEXT(R"("graph_id":"EventGraph","nodes":[],"links":[]}},)"
			 TEXT(R"("options":{"dry_run":false,"validate":true,"compile":true,"save":true}})"),
		*AssetPath,
		*AssetPath);

	const TSharedPtr<FJsonObject> Root =
		ParseJson(UUeremcpBlueprintToolset::SubmitGraph(BrokenSubmit));
	if (!TestTrue(TEXT("broken submit parseable"), Root.IsValid()))
	{
		EmitEvidence(*this, false, TEXT("submit_unparseable"));
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestTrue(
		TEXT("E6: failed_validation or rejected (not success)"),
		UeremcpHonestyContract::IsHonestFailureStatus(Status));
	TestFalse(TEXT("E6: not created_and_validated"),
		Status.Equals(TEXT("created_and_validated"), ESearchCase::CaseSensitive));
	TestFalse(TEXT("E6: not modified_and_validated"),
		Status.Equals(TEXT("modified_and_validated"), ESearchCase::CaseSensitive));

	FString Summary;
	Root->TryGetStringField(TEXT("summary"), Summary);
	const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	bool bHasDiagItems = Root->TryGetObjectField(TEXT("diagnostics"), Diagnostics)
		&& Diagnostics
		&& (*Diagnostics)->TryGetArrayField(TEXT("items"), Items)
		&& Items
		&& Items->Num() > 0;
	const TArray<TSharedPtr<FJsonValue>>* Notes = nullptr;
	bool bHasNotes = Root->TryGetArrayField(TEXT("capability_notes"), Notes)
		&& Notes
		&& Notes->Num() > 0;
	TestTrue(
		TEXT("E6: actionable summary or diagnostics/notes"),
		!Summary.IsEmpty() || bHasDiagItems || bHasNotes);

	const bool bPass = !HasAnyErrors();
	EmitEvidence(*this, bPass, Status.IsEmpty() ? TEXT("empty_status") : Status);
	AddInfo(bPass
		? TEXT("POC_E E6 PASS: broken submit_graph yields honest failure with diagnostics")
		: TEXT("POC_E E6 FAIL"));
	return bPass;
}

#endif // WITH_DEV_AUTOMATION_TESTS
