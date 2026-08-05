// Editor automation tests for InspectMaterial / SubmitMaterialGraph (WS-08).

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpMaterialInspect.h"
#include "UeremcpMaterialPaths.h"
#include "UeremcpMaterialToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpMaterialGraphTest
{
	static bool ParseJson(const FString& Json, TSharedPtr<FJsonObject>& OutRoot)
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialInspectPathPolicyTest,
	"UEREMCP.Material.Inspect.PathPolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialInspectPathPolicyTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("/Game inspect allowed"),
		FUeremcpMaterialInspect::IsAllowedInspectPath(TEXT("/Game/RE/VFX/Free_Spells/M_Free_Spells_Flash")));
	TestTrue(TEXT("scratch inspect allowed"),
		FUeremcpMaterialInspect::IsAllowedInspectPath(TEXT("/Game/__UeremcpTests/Materials/M_Probe")));
	TestFalse(TEXT("engine path rejected"),
		FUeremcpMaterialInspect::IsAllowedInspectPath(TEXT("/Engine/EngineMaterials/DefaultMaterial")));
	TestTrue(TEXT("scratch mutate create allowed"),
		UeremcpMaterialPaths::IsAllowedMutateCreatePath(TEXT("/Game/__UeremcpTests/Materials/MI_X")));
	TestFalse(TEXT("production mutate create denied"),
		UeremcpMaterialPaths::IsAllowedMutateCreatePath(TEXT("/Game/RE/VFX/Free_Spells/M_Free_Spells_Flash")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialInspectRequiresTargetOrQueryTest,
	"UEREMCP.Material.Inspect.RequiresTargetOrQuery",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialInspectRequiresTargetOrQueryTest::RunTest(const FString& Parameters)
{
	const FString Json = UUeremcpMaterialToolset::InspectMaterial(TEXT(
		R"({"protocol_version":"1.0","request_id":"mat-inspect-miss","action":"inspect_material","specification":{}})"));

	TSharedPtr<FJsonObject> Root;
	TestTrue(TEXT("json"), UeremcpMaterialGraphTest::ParseJson(Json, Root));
	if (!Root.IsValid())
	{
		return false;
	}
	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("rejected without path/query"), Status, FString(TEXT("rejected")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialInspectScratchMasterTest,
	"UEREMCP.Material.Inspect.ScratchMasterRoundTripShape",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialInspectScratchMasterTest::RunTest(const FString& Parameters)
{
	// Create a scratch master via create_master_material, then inspect + dry_run submit.
	const FString MasterPath = TEXT("/Game/__UeremcpTests/Materials/Masters/M_WS08_GraphInspectProbe");
	const FString CreateReq = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"mat-create-probe","action":"create_master_material","mode":"replace","target":{"asset_path":"%s"},"options":{"dry_run":false},"specification":{"features":["fresnel","dynamic_color","dynamic_intensity"]}})"),
		*MasterPath);
	const FString CreateJson = UUeremcpMaterialToolset::CreateMasterMaterial(CreateReq);
	TSharedPtr<FJsonObject> CreateRoot;
	if (!UeremcpMaterialGraphTest::ParseJson(CreateJson, CreateRoot))
	{
		AddError(TEXT("create_master_material returned invalid JSON"));
		return false;
	}
	FString CreateStatus;
	CreateRoot->TryGetStringField(TEXT("status"), CreateStatus);
	if (CreateStatus == TEXT("rejected"))
	{
		FString Summary;
		CreateRoot->TryGetStringField(TEXT("summary"), Summary);
		AddError(FString::Printf(TEXT("create_master_material rejected: %s"), *Summary));
		return false;
	}

	const FString InspectReq = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"mat-inspect-probe","action":"inspect_material","target":{"asset_path":"%s"},"specification":{}})"),
		*MasterPath);
	const FString InspectJson = UUeremcpMaterialToolset::InspectMaterial(InspectReq);
	TSharedPtr<FJsonObject> InspectRoot;
	TestTrue(TEXT("inspect json"), UeremcpMaterialGraphTest::ParseJson(InspectJson, InspectRoot));
	if (!InspectRoot.IsValid())
	{
		return false;
	}

	FString Status;
	InspectRoot->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("inspect partially_completed"), Status, FString(TEXT("partially_completed")));

	const TSharedPtr<FJsonObject>* Result = nullptr;
	TestTrue(TEXT("result present"), InspectRoot->TryGetObjectField(TEXT("result"), Result) && Result && Result->IsValid());
	if (!Result || !Result->IsValid())
	{
		return false;
	}

	FString AssetClass;
	(*Result)->TryGetStringField(TEXT("asset_class"), AssetClass);
	TestEqual(TEXT("asset_class Material"), AssetClass, FString(TEXT("Material")));

	const TSharedPtr<FJsonObject>* Fidelity = nullptr;
	TestTrue(TEXT("fidelity"), (*Result)->TryGetObjectField(TEXT("fidelity"), Fidelity) && Fidelity);
	if (Fidelity && Fidelity->IsValid())
	{
		TestFalse(
			TEXT("round_trip_supported false"),
			(*Fidelity)->GetBoolField(TEXT("round_trip_supported")));
	}

	const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
	TestTrue(TEXT("graphs[]"), (*Result)->TryGetArrayField(TEXT("graphs"), Graphs) && Graphs && Graphs->Num() > 0);
	if (Graphs && Graphs->Num() > 0)
	{
		const TSharedPtr<FJsonObject> Graph = (*Graphs)[0]->AsObject();
		TestTrue(TEXT("graph object"), Graph.IsValid());
		if (Graph.IsValid())
		{
			FString GraphType;
			Graph->TryGetStringField(TEXT("graph_type"), GraphType);
			TestEqual(TEXT("MaterialGraph"), GraphType, FString(TEXT("MaterialGraph")));
			const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
			TestTrue(TEXT("nodes present"), Graph->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes);
			if (Nodes)
			{
				TestTrue(TEXT("expression nodes > 0"), Nodes->Num() > 0);
			}
		}
	}

	const FString SubmitReq = FString::Printf(
		TEXT(R"({"protocol_version":"1.0","request_id":"mat-submit-dry","action":"submit_material_graph","target":{"asset_path":"%s"},"options":{"dry_run":true},"specification":{"parameters":{"scalar":{"EmissiveScale":3.0}}}})"),
		*MasterPath);
	const FString SubmitJson = UUeremcpMaterialToolset::SubmitMaterialGraph(SubmitReq);
	TSharedPtr<FJsonObject> SubmitRoot;
	TestTrue(TEXT("submit json"), UeremcpMaterialGraphTest::ParseJson(SubmitJson, SubmitRoot));
	if (!SubmitRoot.IsValid())
	{
		return false;
	}
	FString SubmitStatus;
	SubmitRoot->TryGetStringField(TEXT("status"), SubmitStatus);
	TestTrue(
		TEXT("submit dry_run honest status"),
		SubmitStatus == TEXT("no_change_required") || SubmitStatus == TEXT("partially_completed"));
	TestFalse(TEXT("never validated without proof"), SubmitJson.Contains(TEXT("_validated")));

	const TSharedPtr<FJsonObject>* SubmitResult = nullptr;
	if (SubmitRoot->TryGetObjectField(TEXT("result"), SubmitResult) && SubmitResult && SubmitResult->IsValid())
	{
		const TSharedPtr<FJsonObject>* SubmitFidelity = nullptr;
		if ((*SubmitResult)->TryGetObjectField(TEXT("fidelity"), SubmitFidelity) && SubmitFidelity)
		{
			TestFalse(
				TEXT("submit round_trip false"),
				(*SubmitFidelity)->GetBoolField(TEXT("round_trip_supported")));
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpMaterialSubmitRejectsProductionDeleteTest,
	"UEREMCP.Material.Submit.RejectsProductionDelete",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpMaterialSubmitRejectsProductionDeleteTest::RunTest(const FString& Parameters)
{
	const FString Json = UUeremcpMaterialToolset::SubmitMaterialGraph(TEXT(
		R"({"protocol_version":"1.0","request_id":"mat-submit-del","action":"submit_material_graph","mode":"replace","target":{"asset_path":"/Game/RE/VFX/Free_Spells/M_Free_Spells_Flash"},"options":{"dry_run":true},"specification":{"graphs":[{"graph_type":"MaterialGraph","nodes":[],"links":[]}],"apply":{"delete_missing_expressions":true}}})"));

	TSharedPtr<FJsonObject> Root;
	TestTrue(TEXT("json"), UeremcpMaterialGraphTest::ParseJson(Json, Root));
	if (!Root.IsValid())
	{
		return false;
	}
	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("rejected production delete"), Status, FString(TEXT("rejected")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
