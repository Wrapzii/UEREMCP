// Editor automation tests for UeremcpBlueprint toolset (WS-06 P0).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpBlueprintToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintToolsetPingTest,
	"UeremcpBlueprint.Toolset.Ping",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintToolsetPingTest::RunTest(const FString& Parameters)
{
	const FString Json = UUeremcpBlueprintToolset::Ping();

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("Ping returns parseable JSON object"),
		FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	TestTrue(TEXT("status present"), Root->TryGetStringField(TEXT("status"), Status));
	TestEqual(TEXT("status is no_change_required"), Status, FString(TEXT("no_change_required")));

	const TSharedPtr<FJsonObject>* Metrics = nullptr;
	TestTrue(TEXT("metrics present"), Root->TryGetObjectField(TEXT("metrics"), Metrics) && Metrics && Metrics->IsValid());
	if (Metrics && Metrics->IsValid())
	{
		TestEqual(TEXT("mcp_round_trips == 1"),
			static_cast<int32>((*Metrics)->GetNumberField(TEXT("mcp_round_trips"))), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintToolsetEchoTest,
	"UeremcpBlueprint.Toolset.Echo",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintToolsetEchoTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"bp-echo-1","action":"read_graph","target":{"asset_path":"/Game/__UeremcpTests/None"}})");
	const FString Json = UUeremcpBlueprintToolset::Echo(Request);

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("Echo returns parseable JSON"),
		FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	FString RequestId;
	Root->TryGetStringField(TEXT("status"), Status);
	Root->TryGetStringField(TEXT("request_id"), RequestId);
	TestEqual(TEXT("status"), Status, FString(TEXT("no_change_required")));
	TestEqual(TEXT("request_id echoed"), RequestId, FString(TEXT("bp-echo-1")));

	const TSharedPtr<FJsonObject>* Understood = nullptr;
	TestTrue(TEXT("understood present"),
		Root->TryGetObjectField(TEXT("understood"), Understood) && Understood && Understood->IsValid());
	if (Understood && Understood->IsValid())
	{
		FString Action;
		TestTrue(TEXT("understood.action"), (*Understood)->TryGetStringField(TEXT("action"), Action));
		TestEqual(TEXT("understood action"), Action, FString(TEXT("read_graph")));
	}

	const FString Rejected = UUeremcpBlueprintToolset::Echo(TEXT("not-json"));
	TSharedPtr<FJsonObject> RejectRoot;
	const TSharedRef<TJsonReader<>> RejectReader = TJsonReaderFactory<>::Create(Rejected);
	TestTrue(TEXT("reject parseable"),
		FJsonSerializer::Deserialize(RejectReader, RejectRoot) && RejectRoot.IsValid());
	if (RejectRoot.IsValid())
	{
		FString RejectStatus;
		RejectRoot->TryGetStringField(TEXT("status"), RejectStatus);
		TestEqual(TEXT("malformed -> rejected"), RejectStatus, FString(TEXT("rejected")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpBlueprintToolsetRegisterTest,
	"UeremcpBlueprint.Toolset.Register",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpBlueprintToolsetRegisterTest::RunTest(const FString& Parameters)
{
	if (!UToolsetRegistry::IsToolsetClassRegistered(UUeremcpBlueprintToolset::StaticClass()))
	{
		UToolsetRegistry::RegisterToolsetClass(UUeremcpBlueprintToolset::StaticClass());
	}

	TestTrue(TEXT("toolset class registered"),
		UToolsetRegistry::IsToolsetClassRegistered(UUeremcpBlueprintToolset::StaticClass()));

	const FString SchemaJson =
		UToolsetRegistry::GetToolsetJsonSchema(UUeremcpBlueprintToolset::StaticClass());
	TestFalse(TEXT("schema non-empty"), SchemaJson.IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
