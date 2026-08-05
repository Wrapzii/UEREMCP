// Editor automation tests for UeremcpNiagara inspect (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpNiagaraInspect.h"
#include "UeremcpNiagaraProbeAssets.h"
#include "UeremcpNiagaraToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpNiagaraInspectTest
{
	static constexpr const TCHAR* GInspectProbePath =
		TEXT("/Game/__UeremcpTests/NS_WS07_Probe");

	static bool EnsureInspectProbeAsset(FAutomationTestBase& Test)
	{
		if (UeremcpNiagaraProbeAssets::AssetExistsAtPath(GInspectProbePath))
		{
			return true;
		}

		const FString CreateRequest = TEXT(
			R"({"protocol_version":"1.0","request_id":"ws07-inspect-probe-bootstrap","action":"create_niagara_effect","mode":"replace","target":{"asset_path":"/Game/__UeremcpTests/NS_WS07_Probe"},"specification":{"effect_type":"projectile","element":"fire","components":["sparks"],"parameters":{"scale":1.0,"intensity":4.0},"template_system":{"asset_path":"/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight"}},"options":{"dry_run":false,"compile":true,"validate":false,"save":true}})");

		const FString CreateJson = UUeremcpNiagaraToolset::CreateNiagaraEffect(CreateRequest);
		TSharedPtr<FJsonObject> CreateRoot;
		const TSharedRef<TJsonReader<>> CreateReader = TJsonReaderFactory<>::Create(CreateJson);
		if (!FJsonSerializer::Deserialize(CreateReader, CreateRoot) || !CreateRoot.IsValid())
		{
			Test.AddError(TEXT("inspect probe bootstrap returned invalid JSON."));
			return false;
		}

		FString CreateStatus;
		CreateRoot->TryGetStringField(TEXT("status"), CreateStatus);
		if (CreateStatus == TEXT("rejected"))
		{
			FString Summary;
			CreateRoot->TryGetStringField(TEXT("summary"), Summary);
			Test.AddError(FString::Printf(
				TEXT("inspect probe bootstrap rejected: %s"),
				*Summary));
			return false;
		}

		if (!UeremcpNiagaraProbeAssets::AssetExistsAtPath(GInspectProbePath))
		{
			Test.AddError(TEXT("inspect probe bootstrap completed but NS_WS07_Probe is still missing."));
			return false;
		}

		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraInspectPathGuardTest,
	"UEREMCP.Niagara.Inspect.PathGuard",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraInspectPathGuardTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("tests root allowed for mutate"),
		FUeremcpNiagaraInspect::IsAllowedProbePath(TEXT("/Game/__UeremcpTests/NS_WS07_Probe")));
	TestTrue(TEXT("poc root allowed for mutate"),
		FUeremcpNiagaraInspect::IsAllowedProbePath(TEXT("/Game/__UeremcpPoc/Fireball/NS_Fireball")));
	TestTrue(TEXT("production Magecraft allowed for inspect"),
		FUeremcpNiagaraInspect::IsAllowedInspectPath(
			TEXT("/Game/RE/VFX/Magecraft/Spells/NS_Spell_IceWall_Cast")));
	TestTrue(TEXT("Magecraft allowed for mutate/adapt"),
		FUeremcpNiagaraInspect::IsAllowedProbePath(
			TEXT("/Game/RE/VFX/Magecraft/Spells/NS_Spell_IceWall_Cast")));
	TestTrue(TEXT("generic /Game VFX allowed for inspect"),
		FUeremcpNiagaraInspect::IsAllowedInspectPath(TEXT("/Game/VFX/NS_Fireball")));
	TestFalse(TEXT("game content rejected for mutate"),
		FUeremcpNiagaraInspect::IsAllowedProbePath(TEXT("/Game/VFX/NS_Fireball")));
	TestFalse(TEXT("poc prefix trick rejected"),
		FUeremcpNiagaraInspect::IsAllowedProbePath(TEXT("/Game/__UeremcpPocFake/NS_Fireball")));
	TestFalse(TEXT("engine denied for inspect"),
		FUeremcpNiagaraInspect::IsAllowedInspectPath(TEXT("/Engine/Niagara/NS_Foo")));
	TestTrue(
		TEXT("probe inspect skips GetStackIssues for tests root"),
		FUeremcpNiagaraInspect::ShouldSkipStackIssuesForProbe(TEXT("/Game/__UeremcpTests/NS_POCB_FireballProbe")));
	TestTrue(
		TEXT("probe inspect skips GetStackIssues for poc root"),
		FUeremcpNiagaraInspect::ShouldSkipStackIssuesForProbe(TEXT("/Game/__UeremcpPoc/Fireball/NS_Fireball")));
	TestFalse(
		TEXT("non-probe inspect may collect stack issues"),
		FUeremcpNiagaraInspect::ShouldSkipStackIssuesForProbe(TEXT("/Game/VFX/NS_Fireball")));
	TestFalse(
		TEXT("Magecraft inspect may collect stack issues"),
		FUeremcpNiagaraInspect::ShouldSkipStackIssuesForProbe(
			TEXT("/Game/RE/VFX/Magecraft/Spells/NS_Spell_IceWall_Cast")));

	FUeremcpRequest ResolveReq;
	FUeremcpNiagaraInspectSpec ResolveSpec;
	ResolveSpec.Query = TEXT("does_not_exist_ws07_zzzz");
	ResolveSpec.SearchRoot = TEXT("/Game/__UeremcpTests");
	FString Resolved;
	FString ResolveError;
	TArray<FString> Candidates;
	TestFalse(
		TEXT("missing query fails"),
		FUeremcpNiagaraInspect::ResolveTargetPath(ResolveReq, ResolveSpec, Resolved, ResolveError, Candidates));

	ResolveReq.TargetAssetPath = TEXT("/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast");
	TestTrue(
		TEXT("explicit path resolves"),
		FUeremcpNiagaraInspect::ResolveTargetPath(ResolveReq, ResolveSpec, Resolved, ResolveError, Candidates));
	TestEqual(TEXT("explicit path kept"), Resolved, ResolveReq.TargetAssetPath);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraInspectSystemRuntimeTest,
	"UEREMCP.Niagara.Inspect.NS_WS07_Probe",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraInspectSystemRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace UeremcpNiagaraInspectTest;

	if (!EnsureInspectProbeAsset(*this))
	{
		return false;
	}

	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-inspect-1","action":"inspect_system","target":{"asset_path":"/Game/__UeremcpTests/NS_WS07_Probe"},"options":{"response_detail":"complete"}})");

	const FString Json = UUeremcpNiagaraToolset::InspectSystem(Request);

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("inspect returns JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("status partially_completed"), Status, FString(TEXT("partially_completed")));

	const TSharedPtr<FJsonObject>* Result = nullptr;
	TestTrue(TEXT("result present"), Root->TryGetObjectField(TEXT("result"), Result) && Result && Result->IsValid());
	if (!Result || !Result->IsValid())
	{
		return false;
	}

	FString PrimaryAsset;
	TestTrue(
		TEXT("result.primary_asset present"),
		(*Result)->TryGetStringField(TEXT("primary_asset"), PrimaryAsset) && !PrimaryAsset.IsEmpty());

	const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
	TestTrue(TEXT("result.graphs array present"), (*Result)->TryGetArrayField(TEXT("graphs"), Graphs) && Graphs);
	if (!Graphs || Graphs->Num() < 2)
	{
		AddError(TEXT("expected system + emitter graphs under result.graphs"));
		return false;
	}

	const TSharedPtr<FJsonObject>* FidelitySummary = nullptr;
	if ((*Result)->TryGetObjectField(TEXT("fidelity"), FidelitySummary) && FidelitySummary && FidelitySummary->IsValid())
	{
		TestFalse(
			TEXT("result.fidelity.round_trip_supported false"),
			(*FidelitySummary)->GetBoolField(TEXT("round_trip_supported")));
	}

	const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
	if (Root->TryGetObjectField(TEXT("diagnostics"), Diagnostics) && Diagnostics && Diagnostics->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* DiagGraphs = nullptr;
		TestFalse(
			TEXT("graphs not required under diagnostics"),
			(*Diagnostics)->TryGetArrayField(TEXT("graphs"), DiagGraphs) && DiagGraphs && DiagGraphs->Num() > 0);
	}

	bool bFoundSystemGraph = false;
	bool bFoundModuleStack = false;
	bool bFoundRendererOnEmitterGraph = false;
	for (const TSharedPtr<FJsonValue>& GraphValue : *Graphs)
	{
		const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
		if (!Graph.IsValid())
		{
			continue;
		}
		FString GraphType;
		Graph->TryGetStringField(TEXT("graph_type"), GraphType);
		if (GraphType == TEXT("NiagaraSystemGraph"))
		{
			bFoundSystemGraph = true;

			FString ContentHash;
			TestTrue(TEXT("system graph has content_hash"), Graph->TryGetStringField(TEXT("content_hash"), ContentHash));
			TestTrue(TEXT("content_hash sha256 prefix"), ContentHash.StartsWith(TEXT("sha256:")));

			const TSharedPtr<FJsonObject>* FidelityObj = nullptr;
			if (Graph->TryGetObjectField(TEXT("fidelity"), FidelityObj) && FidelityObj && FidelityObj->IsValid())
			{
				TestFalse(TEXT("round_trip_supported false"), (*FidelityObj)->GetBoolField(TEXT("round_trip_supported")));
			}

			const TSharedPtr<FJsonObject>* Extensions = nullptr;
			if (Graph->TryGetObjectField(TEXT("extensions"), Extensions) && Extensions && Extensions->IsValid())
			{
				const TSharedPtr<FJsonObject>* Niagara = nullptr;
				if ((*Extensions)->TryGetObjectField(TEXT("niagara"), Niagara) && Niagara && Niagara->IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* EventHandlers = nullptr;
					TestTrue(
						TEXT("event_handlers array present on system graph"),
						(*Niagara)->TryGetArrayField(TEXT("event_handlers"), EventHandlers));
				}
			}
		}
		if (GraphType == TEXT("NiagaraModuleStack"))
		{
			bFoundModuleStack = true;
		}
		if (GraphType == TEXT("NiagaraEmitterGraph"))
		{
			const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
			if (Graph->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes)
			{
				for (const TSharedPtr<FJsonValue>& NodeValue : *Nodes)
				{
					const TSharedPtr<FJsonObject> Node = NodeValue->AsObject();
					if (!Node.IsValid())
					{
						continue;
					}
					FString SemanticType;
					if (Node->TryGetStringField(TEXT("semantic_type"), SemanticType)
						&& SemanticType == TEXT("niagara_renderer"))
					{
						bFoundRendererOnEmitterGraph = true;
						break;
					}
				}
			}

			const TSharedPtr<FJsonObject>* Fidelity = nullptr;
			if (Graph->TryGetObjectField(TEXT("fidelity"), Fidelity) && Fidelity && Fidelity->IsValid())
			{
				const TArray<TSharedPtr<FJsonValue>>* Lossy = nullptr;
				if ((*Fidelity)->TryGetArrayField(TEXT("lossy_areas"), Lossy) && Lossy)
				{
					bool bHasRendererLossy = false;
					for (const TSharedPtr<FJsonValue>& AreaValue : *Lossy)
					{
						if (AreaValue->AsString() == TEXT("renderer_material_bindings"))
						{
							bHasRendererLossy = true;
							break;
						}
					}
					if (bFoundRendererOnEmitterGraph)
					{
						TestTrue(TEXT("emitter graph lists renderer_material_bindings"), bHasRendererLossy);
					}
				}
			}
		}
	}

	TestTrue(TEXT("NiagaraSystemGraph returned"), bFoundSystemGraph);
	TestTrue(TEXT("NiagaraModuleStack returned"), bFoundModuleStack);

	const TSharedPtr<FJsonObject>* Metrics = nullptr;
	if (Root->TryGetObjectField(TEXT("metrics"), Metrics) && Metrics && Metrics->IsValid())
	{
		const int32 InternalOps = static_cast<int32>((*Metrics)->GetNumberField(TEXT("internal_operations")));
		TestTrue(TEXT("internal_operations > 0"), InternalOps > 0);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
