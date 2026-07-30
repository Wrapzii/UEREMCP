// Editor automation tests for UeremcpNiagara graph content_hash (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"

#include "UeremcpNiagaraGraphHash.h"
#include "UeremcpNiagaraHashRoundTrip.h"
#include "UeremcpNiagaraRoleNames.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSharedPtr<FJsonObject> MakeSampleModuleStackGraph()
	{
		TSharedPtr<FJsonObject> Graph = MakeShared<FJsonObject>();
		Graph->SetStringField(TEXT("asset_path"), TEXT("/Game/__UeremcpTests/NS_WS07_Probe"));
		Graph->SetStringField(TEXT("graph_id"), TEXT("/Game/__UeremcpTests/NS_WS07_Probe::ProbeBurst::ParticleUpdateScript"));
		Graph->SetStringField(TEXT("graph_name"), TEXT("ParticleUpdateScript"));
		Graph->SetStringField(TEXT("graph_type"), TEXT("NiagaraModuleStack"));
		Graph->SetStringField(TEXT("schema_version"), TEXT("1.0"));

		TSharedPtr<FJsonObject> Fidelity = MakeShared<FJsonObject>();
		Fidelity->SetBoolField(TEXT("round_trip_supported"), false);
		Graph->SetObjectField(TEXT("fidelity"), Fidelity);

		TArray<TSharedPtr<FJsonValue>> Nodes;
		TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
		Node->SetStringField(TEXT("node_id"), TEXT("ephemeral-guid-1"));
		Node->SetStringField(TEXT("semantic_id"), TEXT("ProbeBurst/ParticleUpdateScript/ParticleState"));
		Node->SetStringField(TEXT("semantic_type"), TEXT("niagara_module"));
		Nodes.Add(MakeShared<FJsonValueObject>(Node));
		Graph->SetArrayField(TEXT("nodes"), Nodes);

		TSharedPtr<FJsonObject> ExtRoot = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> Niagara = MakeShared<FJsonObject>();
		Niagara->SetStringField(TEXT("emitter_name"), TEXT("ProbeBurst"));
		Niagara->SetStringField(TEXT("script_usage"), TEXT("ParticleUpdateScript"));
		ExtRoot->SetObjectField(TEXT("niagara"), Niagara);
		Graph->SetObjectField(TEXT("extensions"), ExtRoot);

		return Graph;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraGraphHashOfflineTest,
	"UEREMCP.Niagara.Hash.ContentHashOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraGraphHashOfflineTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Graph = MakeSampleModuleStackGraph();

	FString Error;
	TestTrue(TEXT("hash applies"), FUeremcpNiagaraGraphHash::ApplyContentHashToGraph(Graph, Error));
	TestTrue(TEXT("content_hash sha256 prefix"), Graph->GetStringField(TEXT("content_hash")).StartsWith(TEXT("sha256:")));
	TestEqual(TEXT("revision equals content_hash"), Graph->GetStringField(TEXT("revision")), Graph->GetStringField(TEXT("content_hash")));

	const bool bRoundTripSupported = Graph->GetObjectField(TEXT("fidelity"))->GetBoolField(TEXT("round_trip_supported"));
	TestFalse(TEXT("round_trip_supported stays false"), bRoundTripSupported);

	const FString FirstHash = Graph->GetStringField(TEXT("content_hash"));

	TArray<TSharedPtr<FJsonValue>> PassA;
	PassA.Add(MakeShared<FJsonValueObject>(Graph));

	TSharedPtr<FJsonObject> GraphCopy = MakeSampleModuleStackGraph();
	FUeremcpNiagaraGraphHash::ApplyContentHashToGraph(GraphCopy, Error);
	TArray<TSharedPtr<FJsonValue>> PassB;
	PassB.Add(MakeShared<FJsonValueObject>(GraphCopy));

	FUeremcpNiagaraHashRoundTripResult Stability;
	TestTrue(
		TEXT("retrieve-retrieve stable for identical semantic graph"),
		FUeremcpNiagaraHashRoundTrip::EvaluateRetrieveRetrieveStability(PassA, PassB, Stability));
	TestTrue(TEXT("retrieve_retrieve_stable"), Stability.bRetrieveRetrieveStable);

	Graph->SetStringField(TEXT("graph_name"), TEXT("MutatedStack"));
	FUeremcpNiagaraGraphHash::ApplyContentHashToGraph(Graph, Error);
	TestNotEqual(TEXT("semantic change changes hash"), Graph->GetStringField(TEXT("content_hash")), FirstHash);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraGraphHashPocBManifestOfflineTest,
	"UEREMCP.Niagara.Hash.PocBManifestOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraGraphHashPocBManifestOfflineTest::RunTest(const FString& Parameters)
{
	const FString AssetPath = TEXT("/Game/__UeremcpTests/NS_POCB_FireballProbe");
	TArray<TSharedPtr<FJsonValue>> Graphs;

	TSharedPtr<FJsonObject> SystemGraph = MakeShared<FJsonObject>();
	SystemGraph->SetStringField(TEXT("asset_path"), AssetPath);
	SystemGraph->SetStringField(TEXT("graph_id"), AssetPath + TEXT("::System"));
	SystemGraph->SetStringField(TEXT("graph_name"), TEXT("NS_POCB_FireballProbe"));
	SystemGraph->SetStringField(TEXT("graph_type"), TEXT("NiagaraSystemGraph"));
	Graphs.Add(MakeShared<FJsonValueObject>(SystemGraph));

	for (const FString& Role : UeremcpNiagaraRoles::DefaultPocBComponentRoles())
	{
		const FString EmitterName = UeremcpNiagaraRoles::RoleToEmitterName(Role);
		TSharedPtr<FJsonObject> EmitterGraph = MakeShared<FJsonObject>();
		EmitterGraph->SetStringField(TEXT("asset_path"), AssetPath);
		EmitterGraph->SetStringField(TEXT("graph_id"), AssetPath + TEXT("::") + EmitterName);
		EmitterGraph->SetStringField(TEXT("graph_name"), EmitterName);
		EmitterGraph->SetStringField(TEXT("graph_type"), TEXT("NiagaraEmitterGraph"));
		Graphs.Add(MakeShared<FJsonValueObject>(EmitterGraph));
	}

	TestEqual(TEXT("seven POC B graphs"), Graphs.Num(), 7);

	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;
	TestEqual(
		TEXT("hash all graphs"),
		FUeremcpNiagaraGraphHash::ApplyContentHashesToGraphs(Graphs, ChecksPerformed, ChecksSkipped),
		7);
	FUeremcpNiagaraGraphHash::EnsureRoundTripUnsupportedOnGraphs(Graphs);

	for (const TSharedPtr<FJsonValue>& GraphValue : Graphs)
	{
		const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
		TestTrue(TEXT("content_hash present"), Graph->HasField(TEXT("content_hash")));
		TestFalse(
			TEXT("round_trip_supported false"),
			Graph->GetObjectField(TEXT("fidelity"))->GetBoolField(TEXT("round_trip_supported")));
	}

	TArray<TSharedPtr<FJsonValue>> PassB;
	for (const TSharedPtr<FJsonValue>& GraphValue : Graphs)
	{
		const TSharedPtr<FJsonObject> Copy = MakeShared<FJsonObject>(*GraphValue->AsObject());
		PassB.Add(MakeShared<FJsonValueObject>(Copy));
	}

	FUeremcpNiagaraHashRoundTripResult Scaffold;
	FUeremcpNiagaraHashRoundTrip::RecordPostInspectScaffold(Graphs, Scaffold);
	TestTrue(TEXT("hash manifest present"), Scaffold.bHashesPresent);
	TestEqual(TEXT("seven hashes recorded"), Scaffold.GraphIdToHash.Num(), 7);
	TestFalse(TEXT("retrieve retrieve not claimed"), Scaffold.bRetrieveRetrieveStable);

	FUeremcpNiagaraHashRoundTripResult Stability;
	TestTrue(
		TEXT("retrieve-retrieve stable for cloned manifest"),
		FUeremcpNiagaraHashRoundTrip::EvaluateRetrieveRetrieveStability(Graphs, PassB, Stability));
	TestTrue(TEXT("seven graph stable compare"), Stability.bRetrieveRetrieveStable);

	const TSharedPtr<FJsonObject> Diagnostics =
		FUeremcpNiagaraHashRoundTrip::BuildDiagnosticsObject(Stability);
	bool bRoundTripSupported = true;
	TestTrue(TEXT("diagnostics round_trip field"), Diagnostics->TryGetBoolField(TEXT("round_trip_supported"), bRoundTripSupported));
	TestFalse(TEXT("round_trip stays false"), bRoundTripSupported);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
