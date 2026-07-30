// POC E3/E4 on the real Blueprint pipeline (not CurveFloat scratch harness).
#include "UeremcpScratchPaths.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpBlueprintToolset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpDomainBlueprintIdemRev
{
	static const FString SuiteName = TEXT("Domain_Blueprint_IdemRev");

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

	static bool ReadGraphAndRevision(
		const FString& AssetPath,
		FString& OutRevision,
		TSharedPtr<FJsonObject>& OutGraph,
		FAutomationTestBase& Test)
	{
		const FString ReadJson = FString::Printf(
			TEXT(R"({"protocol_version":"1.0","request_id":"bp-idem-read","action":"read_graph",)")
			TEXT(R"("target":{"asset_path":"%s","graph_id":"EventGraph"},)")
			TEXT(R"("options":{"response_detail":"complete"}})"),
			*AssetPath);
		const TSharedPtr<FJsonObject> Root = ParseJson(UUeremcpBlueprintToolset::ReadGraph(ReadJson));
		if (!Test.TestTrue(TEXT("read_graph parseable"), Root.IsValid()))
		{
			return false;
		}
		Root->TryGetStringField(TEXT("revision"), OutRevision);
		const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
		if (!Test.TestTrue(TEXT("read returns graph"),
				Root->TryGetObjectField(TEXT("diagnostics"), Diagnostics)
				&& Diagnostics
				&& (*Diagnostics)->TryGetArrayField(TEXT("graphs"), Graphs)
				&& Graphs
				&& Graphs->Num() == 1))
		{
			return false;
		}
		OutGraph = (*Graphs)[0]->AsObject();
		return OutGraph.IsValid() && !OutRevision.IsEmpty();
	}

	static FString MakeReplaceRequest(
		const FString& RequestId,
		const FString& AssetPath,
		const TSharedPtr<FJsonObject>& Graph,
		const FString& ExpectedRevision)
	{
		FString GraphJson;
		{
			const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&GraphJson);
			FJsonSerializer::Serialize(Graph.ToSharedRef(), Writer);
		}
		return FString::Printf(
			TEXT(R"({"protocol_version":"1.0","request_id":"%s","action":"submit_graph",)")
			TEXT(R"("mode":"replace","expected_revision":"%s",)")
			TEXT(R"("target":{"asset_path":"%s","graph_id":"EventGraph"},)")
			TEXT(R"("specification":{"graph":%s},)")
			TEXT(R"("options":{"dry_run":false,"validate":true,"compile":true,"save":true}})"),
			*RequestId,
			*ExpectedRevision,
			*AssetPath,
			*GraphJson);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpDomainBlueprintIdempotency,
	"UEREMCP.Validation.Domain.Blueprint.IdempotencyRepeatedReplace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpDomainBlueprintIdempotency::RunTest(const FString& Parameters)
{
	using namespace UeremcpDomainBlueprintIdemRev;
	(void)Parameters;

	FUeremcpScratchGuard Guard(SuiteName);

	static const FString AssetName = TEXT("BP_Domain_Idem");
	if (!CreateMinimalActorBlueprint(AssetName, *this))
	{
		return false;
	}
	const FString AssetPath = SoftPath(AssetName);

	FString Revision;
	TSharedPtr<FJsonObject> Graph;
	if (!ReadGraphAndRevision(AssetPath, Revision, Graph, *this))
	{
		return false;
	}

	for (int32 Attempt = 1; Attempt <= 3; ++Attempt)
	{
		const FString ReqId = FString::Printf(TEXT("bp-idem-%d"), Attempt);
		const TSharedPtr<FJsonObject> Root = ParseJson(
			UUeremcpBlueprintToolset::SubmitGraph(
				MakeReplaceRequest(ReqId, AssetPath, Graph, Revision)));
		if (!TestTrue(TEXT("submit parseable"), Root.IsValid()))
		{
			return false;
		}
		FString Status;
		Root->TryGetStringField(TEXT("status"), Status);
		TestEqual(
			FString::Printf(TEXT("attempt %d is no_change_required"), Attempt),
			Status,
			FString(TEXT("no_change_required")));
	}

	AddInfo(TEXT("POC_E E3 domain Blueprint: repeated identical replace is no_change_required (3x)"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpDomainBlueprintRevisionStale,
	"UEREMCP.Validation.Domain.Blueprint.RevisionStaleRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpDomainBlueprintRevisionStale::RunTest(const FString& Parameters)
{
	using namespace UeremcpDomainBlueprintIdemRev;
	(void)Parameters;

	FUeremcpScratchGuard Guard(SuiteName);

	static const FString AssetName = TEXT("BP_Domain_Rev");
	if (!CreateMinimalActorBlueprint(AssetName, *this))
	{
		return false;
	}
	const FString AssetPath = SoftPath(AssetName);

	FString Revision;
	TSharedPtr<FJsonObject> Graph;
	if (!ReadGraphAndRevision(AssetPath, Revision, Graph, *this))
	{
		return false;
	}

	const FString Stale =
		TEXT("sha256:0000000000000000000000000000000000000000000000000000000000000000");
	const TSharedPtr<FJsonObject> Root = ParseJson(
		UUeremcpBlueprintToolset::SubmitGraph(
			MakeReplaceRequest(TEXT("bp-rev-stale"), AssetPath, Graph, Stale)));
	if (!TestTrue(TEXT("stale submit parseable"), Root.IsValid()))
	{
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("stale rejected"), Status, FString(TEXT("rejected")));
	FString ReturnedRevision;
	Root->TryGetStringField(TEXT("revision"), ReturnedRevision);
	TestEqual(TEXT("returns current revision"), ReturnedRevision, Revision);

	AddInfo(TEXT("POC_E E4 domain Blueprint: stale expected_revision rejected with no mutation"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
