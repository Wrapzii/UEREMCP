// Editor automation tests for UeremcpNiagara post-create round-trip (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"

#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraRoundTrip.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSharedPtr<FJsonObject> MakeEmitterGraph(const FString& EmitterName)
	{
		TSharedPtr<FJsonObject> Graph = MakeShared<FJsonObject>();
		Graph->SetStringField(TEXT("graph_type"), TEXT("NiagaraEmitterGraph"));
		Graph->SetStringField(TEXT("graph_name"), EmitterName);
		return Graph;
	}

	TSharedPtr<FJsonObject> MakeSystemGraphWithUserVars(const TArray<FString>& UserVars)
	{
		TSharedPtr<FJsonObject> Graph = MakeShared<FJsonObject>();
		Graph->SetStringField(TEXT("graph_type"), TEXT("NiagaraSystemGraph"));

		TSharedPtr<FJsonObject> ExtRoot = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> Niagara = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Params;
		for (const FString& Name : UserVars)
		{
			TSharedPtr<FJsonObject> Param = MakeShared<FJsonObject>();
			Param->SetStringField(TEXT("name"), Name);
			Params.Add(MakeShared<FJsonValueObject>(Param));
		}
		Niagara->SetArrayField(TEXT("user_parameters"), Params);
		ExtRoot->SetObjectField(TEXT("niagara"), Niagara);
		Graph->SetObjectField(TEXT("extensions"), ExtRoot);
		return Graph;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraRoundTripStructuralMatchTest,
	"UEREMCP.Niagara.RoundTrip.StructuralMatchOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraRoundTripStructuralMatchTest::RunTest(const FString& Parameters)
{
	FUeremcpNiagaraCreateResult CreateResult;
	CreateResult.EmittersAdded = {TEXT("Sparks"), TEXT("ImpactBurst")};
	CreateResult.UserVariablesAdded = {TEXT("User.Scale"), TEXT("User.Intensity")};

	TArray<TSharedPtr<FJsonValue>> Graphs;
	Graphs.Add(MakeShared<FJsonValueObject>(MakeSystemGraphWithUserVars(CreateResult.UserVariablesAdded)));
	Graphs.Add(MakeShared<FJsonValueObject>(MakeEmitterGraph(TEXT("Sparks"))));
	Graphs.Add(MakeShared<FJsonValueObject>(MakeEmitterGraph(TEXT("ImpactBurst"))));

	TArray<FString> Mismatches;
	TestTrue(
		TEXT("structural match succeeds"),
		FUeremcpNiagaraRoundTrip::EvaluateStructuralMatch(CreateResult, Graphs, Mismatches));
	TestEqual(TEXT("no mismatches"), Mismatches.Num(), 0);

	CreateResult.UserVariablesAdded.Add(TEXT("User.Missing"));
	TestFalse(
		TEXT("missing user var fails match"),
		FUeremcpNiagaraRoundTrip::EvaluateStructuralMatch(CreateResult, Graphs, Mismatches));
	TestTrue(TEXT("mismatch recorded"), Mismatches.Num() > 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
