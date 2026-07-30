// ADR-0006 idempotency store replay annotation (WS-05).

#include "UeremcpIdempotency.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpIdempotencyReplayAnnotation,
	"UEREMCP.Protocol.Idempotency.ReplayAnnotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpIdempotencyReplayAnnotation::RunTest(const FString& Parameters)
{
	FUeremcpIdempotencyStore& Store = FUeremcpIdempotencyStore::Get();
	Store.Clear();

	const FString Stored = TEXT(
		R"({"protocol_version":"1.0","request_id":"req-1","status":"created_and_validated","summary":"ok","metrics":{"mcp_round_trips":1,"internal_operations":1}})");
	Store.Put(TEXT("key1"), Stored);

	FString Replay;
	TestTrue(TEXT("TryGetReplay finds entry"), Store.TryGetReplay(TEXT("key1"), TEXT("req-2"), Replay));

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Replay);
	TestTrue(
		TEXT("replay JSON parses"),
		FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());

	FString RequestId;
	TestTrue(
		TEXT("request_id reflects current attempt"),
		Root->TryGetStringField(TEXT("request_id"), RequestId) && RequestId == TEXT("req-2"));

	const TSharedPtr<FJsonObject>* Metrics = nullptr;
	TestTrue(
		TEXT("metrics object present"),
		Root->TryGetObjectField(TEXT("metrics"), Metrics) && Metrics && Metrics->IsValid());

	bool bReplayed = false;
	TestTrue(
		TEXT("metrics.replayed is true"),
		(*Metrics)->TryGetBoolField(TEXT("replayed"), bReplayed) && bReplayed);

	Store.Clear();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
