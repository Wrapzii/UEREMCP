// Editor automation tests for UeremcpNiagara graph content_hash (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"

#include "UeremcpNiagaraGraphHash.h"
#include "UeremcpNiagaraHashRoundTrip.h"

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

#endif // WITH_DEV_AUTOMATION_TESTS
