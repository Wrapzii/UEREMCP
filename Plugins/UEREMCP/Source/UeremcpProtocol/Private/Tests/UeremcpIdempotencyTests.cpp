// ADR-0006 idempotency store replay annotation and durable reload (WS-05).

#include "UeremcpIdempotency.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TSharedPtr<FJsonObject> ParseObject(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Root);
		return Root;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpIdempotencyReplayAnnotation,
	"UEREMCP.Protocol.Idempotency.ReplayAnnotation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpIdempotencyReplayAnnotation::RunTest(const FString& Parameters)
{
	FUeremcpIdempotencyStore Store;
	Store.SetDurableEnabled(false);

	const FString Stored = TEXT(
		R"({"protocol_version":"1.0","request_id":"req-1","status":"created_and_validated","summary":"ok","metrics":{"mcp_round_trips":1,"internal_operations":1}})");
	Store.Put(TEXT("key1"), Stored);

	FString Replay;
	TestTrue(TEXT("TryGetReplay finds entry"), Store.TryGetReplay(TEXT("key1"), TEXT("req-2"), Replay));

	const TSharedPtr<FJsonObject> Root = ParseObject(Replay);
	TestTrue(TEXT("replay JSON parses"), Root.IsValid());

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

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpIdempotencyDurableReload,
	"UEREMCP.Protocol.Idempotency.DurableReload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpIdempotencyDurableReload::RunTest(const FString& Parameters)
{
	const FString TempRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("UEREMCP"),
		TEXT("__UeremcpTests"),
		TEXT("idempotency_durable")));

	IFileManager::Get().DeleteDirectory(*TempRoot, false, true);
	IFileManager::Get().MakeDirectory(*TempRoot, true);

	const FString Stored = TEXT(
		R"({"protocol_version":"1.0","request_id":"req-durable-1","status":"created_and_validated","summary":"persisted","metrics":{"mcp_round_trips":1,"internal_operations":2}})");
	const FString Key = TEXT("ws05-durable-key/with spaces");

	{
		FUeremcpIdempotencyStore Writer;
		Writer.SetDurableRootOverride(TempRoot);
		Writer.SetDurableEnabled(true);
		Writer.Put(Key, Stored);
		TestEqual(TEXT("writer caches one entry"), Writer.Num(), 1);
		TestTrue(
			TEXT("durable file written"),
			FPaths::FileExists(FPaths::Combine(
				TempRoot,
				FUeremcpIdempotencyStore::DurableFileStemForKey(Key) + TEXT(".json"))));
	}

	FUeremcpIdempotencyStore Reader;
	Reader.SetDurableRootOverride(TempRoot);
	Reader.SetDurableEnabled(true);
	TestEqual(TEXT("reader starts empty"), Reader.Num(), 0);

	FString Loaded;
	TestTrue(TEXT("reader hydrates from disk"), Reader.TryGet(Key, Loaded));
	TestEqual(TEXT("reader caches after hydrate"), Reader.Num(), 1);
	TestEqual(TEXT("hydrated JSON matches"), Loaded, Stored);

	FString Replay;
	TestTrue(
		TEXT("durable replay annotates request id"),
		Reader.TryGetReplay(Key, TEXT("req-durable-2"), Replay));
	const TSharedPtr<FJsonObject> ReplayRoot = ParseObject(Replay);
	TestTrue(TEXT("durable replay parses"), ReplayRoot.IsValid());
	TestEqual(
		TEXT("durable replay request_id"),
		ReplayRoot->GetStringField(TEXT("request_id")),
		FString(TEXT("req-durable-2")));
	TestTrue(
		TEXT("durable replay marks metrics.replayed"),
		ReplayRoot->GetObjectField(TEXT("metrics"))->GetBoolField(TEXT("replayed")));

	FUeremcpIdempotencyStore WrongKey;
	WrongKey.SetDurableRootOverride(TempRoot);
	FString Missing;
	TestFalse(
		TEXT("unknown key does not invent a durable hit"),
		WrongKey.TryGet(TEXT("other-key"), Missing));

	Reader.PurgeDurable();
	TestEqual(TEXT("purge clears memory"), Reader.Num(), 0);
	TestFalse(
		TEXT("purge removes durable file"),
		FPaths::FileExists(FPaths::Combine(
			TempRoot,
			FUeremcpIdempotencyStore::DurableFileStemForKey(Key) + TEXT(".json"))));

	IFileManager::Get().DeleteDirectory(*TempRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpIdempotencyDurableRoot,
	"UEREMCP.Protocol.Idempotency.DurableRoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpIdempotencyDurableRoot::RunTest(const FString& Parameters)
{
	const FString DefaultRoot = FUeremcpIdempotencyStore::DefaultDurableRoot();
	TestTrue(
		TEXT("default root is under ProjectSavedDir/UEREMCP/idempotency"),
		DefaultRoot.Contains(TEXT("UEREMCP"))
			&& DefaultRoot.EndsWith(TEXT("idempotency")));
	TestFalse(
		TEXT("default root is never Intermediate/Sandboxes"),
		DefaultRoot.Contains(TEXT("Intermediate"))
			|| DefaultRoot.Contains(TEXT("Sandboxes")));

	const FString Stem = FUeremcpIdempotencyStore::DurableFileStemForKey(TEXT("abc"));
	TestEqual(TEXT("stem is 64 hex chars"), Stem.Len(), 64);
	TestEqual(
		TEXT("stem is stable"),
		Stem,
		FUeremcpIdempotencyStore::DurableFileStemForKey(TEXT("abc")));
	TestNotEqual(
		TEXT("distinct keys get distinct stems"),
		Stem,
		FUeremcpIdempotencyStore::DurableFileStemForKey(TEXT("abd")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
