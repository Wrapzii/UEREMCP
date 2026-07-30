// POC E5 — options.validate=false never yields *_validated.
#include "UeremcpHonestyContract.h"
#include "UeremcpScratchPaths.h"
#include "UeremcpValidationTestCommon.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpBlueprintToolset.h"
#include "UeremcpEnvelope.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpHonestyValidateFalse
{
	static const FString SuiteName = TEXT("Honesty_ValidateFalse");

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
		// Detail is a short machine token (no quotes) so the marker stays one-line JSON.
		const FString Marker = FString::Printf(
			TEXT("UEREMCP_POC_EVIDENCE={\"schema_version\":1,\"scenario\":\"poc_e_e5\","
				"\"run_id\":\"poc-e-e5-validate-false\",\"outcome\":\"%s\","
				"\"criteria\":{\"E5\":{\"status\":\"%s\",\"detail\":\"%s\"}}}"),
			bPass ? TEXT("pass") : TEXT("fail"),
			bPass ? TEXT("pass") : TEXT("fail"),
			*Detail);
		Test.AddInfo(Marker);
	}
}

/**
 * E5 contract: validate=false → partially_completed; never *_validated.
 * Covers protocol helper + real Blueprint submit_graph path.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpHonestyValidateFalse,
	"UEREMCP.Validation.Honesty.ValidateFalseForbidsValidated",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpHonestyValidateFalse::RunTest(const FString& Parameters)
{
	using namespace UeremcpHonestyValidateFalse;
	(void)Parameters;

	FUeremcpScratchGuard Guard(SuiteName);

	// --- Protocol helper: forged *_validated must be demoted ---
	FUeremcpRequest Request;
	Request.ProtocolVersion = FUeremcpEnvelope::ProtocolVersion();
	Request.RequestId = TEXT("e5-helper");
	Request.Action = TEXT("submit_graph");
	Request.bValidate = false;

	TestEqual(
		TEXT("created_and_validated demoted"),
		UeremcpHonestyContract::ResolveStatusHonoringValidateFlag(
			Request, TEXT("created_and_validated")),
		FString(TEXT("partially_completed")));
	TestEqual(
		TEXT("modified_and_validated demoted"),
		UeremcpHonestyContract::ResolveStatusHonoringValidateFlag(
			Request, TEXT("modified_and_validated")),
		FString(TEXT("partially_completed")));
	TestEqual(
		TEXT("failed_validation preserved"),
		UeremcpHonestyContract::ResolveStatusHonoringValidateFlag(
			Request, TEXT("failed_validation")),
		FString(TEXT("failed_validation")));

	FUeremcpResponse Forged;
	Forged.RequestId = TEXT("e5-forged");
	Forged.Status = TEXT("created_and_validated");
	Forged.Summary = TEXT("would-be lie");
	UeremcpHonestyContract::ApplyValidateFlagToResponse(Request, Forged);
	TestEqual(TEXT("Apply demotes status"), Forged.Status, FString(TEXT("partially_completed")));
	TestTrue(TEXT("Apply adds capability note"), Forged.CapabilityNotes.Num() > 0);

	const FString ForgedJson = FUeremcpEnvelope::SerializeResponse(Forged);
	TestTrue(TEXT("serialized status is partially_completed"),
		ForgedJson.Contains(TEXT("\"partially_completed\"")));
	TestFalse(TEXT("serialized status is not created_and_validated"),
		ForgedJson.Contains(TEXT("\"created_and_validated\"")));

	// --- Envelope parse: options.validate=false ---
	FUeremcpRequest Parsed;
	FString ParseError;
	const FString ValidateFalseJson =
		TEXT(R"({"protocol_version":"1.0","request_id":"e5-parse","action":"submit_graph",)")
		TEXT(R"("options":{"validate":false}})");
	TestTrue(TEXT("parse validate=false"),
		FUeremcpEnvelope::ParseRequest(ValidateFalseJson, Parsed, ParseError));
	TestFalse(TEXT("bValidate false"), Parsed.bValidate);

	// --- Real Blueprint domain path ---
	static const FString AssetName = TEXT("BP_E5_ValidateFalse");
	UBlueprint* Blueprint = CreateMinimalActorBlueprint(AssetName, *this);
	if (!Blueprint)
	{
		EmitEvidence(*this, false, TEXT("blueprint_create_failed"));
		return false;
	}
	const FString AssetPath = SoftPath(AssetName);

	const FString ReadJson = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"e5-read","action":"read_graph",)")
		TEXT(R"("target":{"asset_path":"%s","graph_id":"EventGraph"},)")
		TEXT(R"("options":{"response_detail":"complete"}})"),
		*AssetPath);
	const TSharedPtr<FJsonObject> ReadRoot = ParseJson(UUeremcpBlueprintToolset::ReadGraph(ReadJson));
	if (!TestTrue(TEXT("read_graph parseable"), ReadRoot.IsValid()))
	{
		EmitEvidence(*this, false, TEXT("read_graph_failed"));
		return false;
	}
	FString Revision;
	ReadRoot->TryGetStringField(TEXT("revision"), Revision);
	const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
	if (!TestTrue(TEXT("read returns graph"),
			ReadRoot->TryGetObjectField(TEXT("diagnostics"), Diagnostics)
			&& Diagnostics
			&& (*Diagnostics)->TryGetArrayField(TEXT("graphs"), Graphs)
			&& Graphs
			&& Graphs->Num() == 1))
	{
		EmitEvidence(*this, false, TEXT("read_graph_missing"));
		return false;
	}

	FString GraphJson;
	{
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&GraphJson);
		FJsonSerializer::Serialize((*Graphs)[0]->AsObject().ToSharedRef(), Writer);
	}

	const FString SubmitJson = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"e5-submit","action":"submit_graph",)")
		TEXT(R"("mode":"replace","expected_revision":"%s",)")
		TEXT(R"("target":{"asset_path":"%s","graph_id":"EventGraph"},)")
		TEXT(R"("specification":{"graph":%s},)")
		TEXT(R"("options":{"dry_run":false,"validate":false,"compile":true,"save":true}})"),
		*Revision,
		*AssetPath,
		*GraphJson);

	const TSharedPtr<FJsonObject> SubmitRoot =
		ParseJson(UUeremcpBlueprintToolset::SubmitGraph(SubmitJson));
	if (!TestTrue(TEXT("submit_graph parseable"), SubmitRoot.IsValid()))
	{
		EmitEvidence(*this, false, TEXT("submit_graph_failed"));
		return false;
	}
	FString Status;
	SubmitRoot->TryGetStringField(TEXT("status"), Status);
	TestFalse(TEXT("E5: not created_and_validated"),
		Status.Equals(TEXT("created_and_validated"), ESearchCase::CaseSensitive));
	TestFalse(TEXT("E5: not modified_and_validated"),
		Status.Equals(TEXT("modified_and_validated"), ESearchCase::CaseSensitive));
	const bool bHonest =
		Status.Equals(TEXT("partially_completed"), ESearchCase::CaseSensitive)
		|| Status.Equals(TEXT("no_change_required"), ESearchCase::CaseSensitive);
	TestTrue(TEXT("E5: partially_completed or no_change_required"), bHonest);

	const bool bPass = !HasAnyErrors();
	EmitEvidence(*this, bPass, Status);
	AddInfo(bPass
		? TEXT("POC_E E5 PASS: validate=false forbids *_validated (helper + Blueprint submit_graph)")
		: TEXT("POC_E E5 FAIL"));
	return bPass;
}

#endif // WITH_DEV_AUTOMATION_TESTS
