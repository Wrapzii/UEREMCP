#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpCapabilityService.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSharedPtr<FJsonObject> Parse(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, Root) ? Root : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpCapabilityRejectsArbitraryExecutionTest,
	"UeremcpCore.Capabilities.NoArbitraryExecution",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpCapabilityRejectsArbitraryExecutionTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Root = Parse(FUeremcpCapabilityService::ExecutePreparedAction(
		TEXT(R"({"action_id":"not-issued","tool":"EditorToolset.DeleteEverything","overrides":{},"dry_run":false,"confirm":true})")));
	TestTrue(TEXT("arbitrary execution rejection is structured"), Root.IsValid());
	if (!Root.IsValid()) return false;
	TestEqual(TEXT("unknown action is rejected"), Root->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpCapabilityPreparationContractTest,
	"UeremcpCore.Capabilities.ResolveAndPrepare",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpCapabilityPreparationContractTest::RunTest(const FString& Parameters)
{
	if (!UToolsetRegistry::IsAvailable())
	{
		AddWarning(TEXT("ToolsetRegistry unavailable; live prepared-action assertions skipped."));
		return true;
	}

	FUeremcpCapabilityService::ResetForTests();
	const FString Json = FUeremcpCapabilityService::ResolveAndPrepare(
		TEXT(R"({"request_id":"cap-test-1","action":"resolve_and_prepare","goal":"inspect this Niagara system","scope":{"allowed_domains":["niagara"],"asset_paths":[]},"risk_ceiling":"read_only","max_actions":3,"response_detail":"compact"})"));
	const TSharedPtr<FJsonObject> Root = Parse(Json);
	TestTrue(TEXT("preparation returns structured JSON"), Root.IsValid());
	if (!Root.IsValid()) return false;
	TestEqual(TEXT("preparation status"), Root->GetStringField(TEXT("status")), FString(TEXT("prepared")));
	const TSharedPtr<FJsonObject>* Result = nullptr;
	TestTrue(TEXT("preparation result exists"), Root->TryGetObjectField(TEXT("result"), Result) && Result && Result->IsValid());
	if (!Result || !Result->IsValid()) return false;
	TestTrue(TEXT("registry hash is returned"), (*Result)->HasField(TEXT("registry_hash")));
	const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
	TestTrue(TEXT("bounded actions array exists"), (*Result)->TryGetArrayField(TEXT("recommended_actions"), Actions) && Actions);
	if (!Actions || Actions->Num() == 0) return true;
	TestTrue(TEXT("continuation count is bounded"), Actions->Num() <= 3);
	const TSharedPtr<FJsonObject> Action = (*Actions)[0]->AsObject();
	TestTrue(TEXT("action id is server-owned"), Action->GetStringField(TEXT("action_id")).StartsWith(TEXT("action_")));
	TestTrue(TEXT("tool is fully qualified"), Action->GetStringField(TEXT("tool")).Contains(TEXT(".")));
	TestFalse(TEXT("full input schema is not in normal preparation"), Action->HasField(TEXT("input_schema")));
	const FString ActionStatus = Action->GetStringField(TEXT("status"));
	if (ActionStatus != TEXT("ready"))
	{
		TestEqual(TEXT("unbound actions require input"), ActionStatus, FString(TEXT("needs_input")));
		return true;
	}

	const FString ActionId = Action->GetStringField(TEXT("action_id"));
	const FString ContextId = (*Result)->GetStringField(TEXT("context_id"));
	const FString ExecuteJson = FString::Printf(
		TEXT(R"({"request_id":"cap-test-2","action_id":"%s","context_id":"%s","overrides":{},"dry_run":true,"confirm":false})"),
		*ActionId, *ContextId);
	const TSharedPtr<FJsonObject> ExecuteRoot = Parse(FUeremcpCapabilityService::ExecutePreparedAction(ExecuteJson));
	TestTrue(TEXT("dry run returns structured JSON"), ExecuteRoot.IsValid());
	if (ExecuteRoot.IsValid())
	{
		TestEqual(TEXT("dry run status"), ExecuteRoot->GetStringField(TEXT("status")), FString(TEXT("dry_run")));
		TestFalse(TEXT("result is not a nested JSON string"), ExecuteRoot->HasTypedField<EJson::String>(TEXT("result")));
		const TSharedPtr<FJsonObject>* ExecuteResult = nullptr;
		if (ExecuteRoot->TryGetObjectField(TEXT("result"), ExecuteResult) && ExecuteResult && ExecuteResult->IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Continuations = nullptr;
			if ((*ExecuteResult)->TryGetArrayField(TEXT("recommended_actions"), Continuations) && Continuations)
			{
				TestTrue(TEXT("continuations are bounded"), Continuations->Num() <= 3);
				for (const TSharedPtr<FJsonValue>& Value : *Continuations)
				{
					const TSharedPtr<FJsonObject> Continuation = Value->AsObject();
					TestTrue(TEXT("continuation has action id"), Continuation.IsValid() && Continuation->GetStringField(TEXT("action_id")).StartsWith(TEXT("action_")));
					TestTrue(TEXT("continuation has fully qualified tool"), Continuation.IsValid() && Continuation->GetStringField(TEXT("tool")).Contains(TEXT(".")));
					TestTrue(TEXT("continuation has classification"), Continuation.IsValid() && Continuation->HasField(TEXT("continuation_type")));
					TestTrue(TEXT("continuation has confidence"), Continuation.IsValid() && Continuation->HasField(TEXT("confidence")));
					TestTrue(TEXT("continuation has recommendation reason"), Continuation.IsValid() && Continuation->HasField(TEXT("recommendation_reason")));
					TestTrue(TEXT("continuation has target resource"), Continuation.IsValid() && Continuation->HasField(TEXT("target_resource")));
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpCapabilityExplicitMetadataTest,
	"UeremcpCore.Capabilities.ExplicitMetadata",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpCapabilityExplicitMetadataTest::RunTest(const FString& Parameters)
{
	if (!UToolsetRegistry::IsAvailable())
	{
		AddWarning(TEXT("ToolsetRegistry unavailable; explicit metadata assertion skipped."));
		return true;
	}
	const TSharedPtr<FJsonObject> Root = Parse(FUeremcpCapabilityService::SearchCapabilities(
		TEXT(R"({"request_id":"cap-meta-1","query":"InspectSystem","max_results":20})")));
	TestTrue(TEXT("metadata search returns structured JSON"), Root.IsValid());
	if (!Root.IsValid()) return false;
	const TSharedPtr<FJsonObject>* Result = nullptr;
	if (!Root->TryGetObjectField(TEXT("result"), Result) || !Result || !Result->IsValid()) return false;
	const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
	if (!(*Result)->TryGetArrayField(TEXT("results"), Results) || !Results) return false;
	for (const TSharedPtr<FJsonValue>& Value : *Results)
	{
		const TSharedPtr<FJsonObject> Hit = Value->AsObject();
		if (Hit.IsValid() && Hit->GetStringField(TEXT("tool")).EndsWith(TEXT("InspectSystem")))
		{
			TestEqual(TEXT("InspectSystem domain is explicit Niagara"), Hit->GetStringField(TEXT("domain")), FString(TEXT("niagara")));
			TestEqual(TEXT("InspectSystem lifecycle is inspect"), Hit->GetStringField(TEXT("lifecycle")), FString(TEXT("inspect")));
			TestEqual(TEXT("InspectSystem risk is read-only"), Hit->GetStringField(TEXT("risk")), FString(TEXT("read_only")));
			return true;
		}
	}
	AddWarning(TEXT("InspectSystem was not present in this live registry snapshot."));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpCapabilityNoFalseReadyTest,
	"UeremcpCore.Capabilities.NoFalseReady",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpCapabilityNoFalseReadyTest::RunTest(const FString& Parameters)
{
	if (!UToolsetRegistry::IsAvailable())
	{
		AddWarning(TEXT("ToolsetRegistry unavailable; false-ready assertion skipped."));
		return true;
	}
	const TSharedPtr<FJsonObject> Root = Parse(FUeremcpCapabilityService::ResolveAndPrepare(
		TEXT(R"({"request_id":"cap-bind-1","goal":"find Niagara asset NS_MCP_ToolingAudit_Test and inspect it","scope":{"allowed_domains":["niagara"],"asset_search":{"query":"NS_MCP_ToolingAudit_Test","search_root":"/Game"}},"risk_ceiling":"read_only","max_actions":5})")));
	TestTrue(TEXT("asset search response is structured"), Root.IsValid());
	if (!Root.IsValid()) return false;
	const TSharedPtr<FJsonObject>* Result = nullptr;
	if (!Root->TryGetObjectField(TEXT("result"), Result) || !Result || !Result->IsValid()) return false;
	const TArray<TSharedPtr<FJsonValue>>* Resources = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Actions = nullptr;
	(*Result)->TryGetArrayField(TEXT("resources"), Resources);
	(*Result)->TryGetArrayField(TEXT("recommended_actions"), Actions);
	if (!Resources || Resources->Num() == 0)
	{
		if (Actions)
		{
			for (const TSharedPtr<FJsonValue>& Value : *Actions)
				TestNotEqual(TEXT("unresolved asset never yields ready action"), Value->AsObject()->GetStringField(TEXT("status")), FString(TEXT("ready")));
		}
		return true;
	}
	if (Actions)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Actions)
		{
			const TSharedPtr<FJsonObject> Action = Value->AsObject();
			const TSharedPtr<FJsonObject>* Arguments = nullptr;
			if (Action.IsValid() && Action->TryGetObjectField(TEXT("arguments"), Arguments) && Arguments && Arguments->IsValid())
			{
				const TSharedPtr<FJsonObject>* RendererClass = nullptr;
				if ((*Arguments)->TryGetObjectField(TEXT("rendererClass"), RendererClass) && RendererClass)
					TestNotEqual(TEXT("rendererClass is not bound to the asset path"), (*RendererClass)->GetStringField(TEXT("refPath")), FString(TEXT("/Game/RE/VFX/Magecraft/Spells/NS_MCP_ToolingAudit_Test")));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpCapabilityRegistryMismatchTest,
	"UeremcpCore.Capabilities.RegistryMismatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpCapabilityRegistryMismatchTest::RunTest(const FString& Parameters)
{
	const TSharedPtr<FJsonObject> Root = Parse(FUeremcpCapabilityService::ResolveAndPrepare(
		TEXT(R"({"request_id":"cap-test-3","action":"resolve_and_prepare","goal":"inspect Niagara","registry_hash":"sha256:stale","risk_ceiling":"read_only"})")));
	TestTrue(TEXT("stale registry result is structured"), Root.IsValid());
	if (!Root.IsValid()) return false;
	TestEqual(TEXT("stale registry is rejected"), Root->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	return true;
}

#endif
