// ADR-0006 idempotency store replay annotation and durable reload (WS-05).

#include "UeremcpIdempotency.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpIdempotencyClaimConflictRestart,
	"UEREMCP.Protocol.Idempotency.ClaimConflictRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpIdempotencyClaimConflictRestart::RunTest(const FString& Parameters)
{
	const FString TempRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("UEREMCP"),
		TEXT("__UeremcpTests"),
		TEXT("idempotency_claim")));
	IFileManager::Get().DeleteDirectory(*TempRoot, false, true);

	const FString RequestA = TEXT(
		R"({"protocol_version":"1.0","request_id":"one","action":"execute_plan","idempotency_key":"claim-key","expected_revision":"sha256:aaa","specification":{"operations":[]}})");
	const FString RequestARetry = TEXT(
		R"({"specification":{"operations":[]},"expected_revision":"sha256:aaa","idempotency_key":"claim-key","action":"execute_plan","request_id":"two","protocol_version":"1.0"})");
	const FString RequestB = TEXT(
		R"({"protocol_version":"1.0","request_id":"three","action":"execute_plan","idempotency_key":"claim-key","expected_revision":"sha256:bbb","specification":{"operations":[]}})");
	FString FingerprintA;
	FString FingerprintARetry;
	FString FingerprintB;
	FString Error;
	TestTrue(
		TEXT("fingerprint first request"),
		FUeremcpIdempotencyStore::FingerprintRequestJson(RequestA, FingerprintA, Error));
	TestTrue(
		TEXT("fingerprint retry"),
		FUeremcpIdempotencyStore::FingerprintRequestJson(RequestARetry, FingerprintARetry, Error));
	TestTrue(
		TEXT("fingerprint conflicting request"),
		FUeremcpIdempotencyStore::FingerprintRequestJson(RequestB, FingerprintB, Error));
	TestEqual(TEXT("retry-only fields do not change fingerprint"), FingerprintA, FingerprintARetry);
	TestNotEqual(TEXT("expected_revision changes fingerprint"), FingerprintA, FingerprintB);

	const FString Stored = TEXT(
		R"({"protocol_version":"1.0","request_id":"one","status":"created_and_validated","summary":"persisted","metrics":{"mcp_round_trips":1,"internal_operations":1}})");
	{
		FUeremcpIdempotencyStore Writer;
		Writer.SetDurableRootOverride(TempRoot);
		const FUeremcpIdempotencyClaim First =
			Writer.Claim(TEXT("claim-key"), FingerprintA, TEXT("one"));
		TestEqual(
			TEXT("first process acquires claim"),
			First.Status,
			EUeremcpIdempotencyClaimStatus::Acquired);
		const FUeremcpIdempotencyClaim Concurrent =
			Writer.Claim(TEXT("claim-key"), FingerprintA, TEXT("concurrent"));
		TestEqual(
			TEXT("concurrent duplicate does not mutate"),
			Concurrent.Status,
			EUeremcpIdempotencyClaimStatus::InProgress);
		TestTrue(
			TEXT("completion is persisted"),
			Writer.Complete(TEXT("claim-key"), FingerprintA, Stored, Error));
	}

	FUeremcpIdempotencyStore Restarted;
	Restarted.SetDurableRootOverride(TempRoot);
	const FUeremcpIdempotencyClaim Replay =
		Restarted.Claim(TEXT("claim-key"), FingerprintARetry, TEXT("two"));
	TestEqual(
		TEXT("fresh store replays after restart"),
		Replay.Status,
		EUeremcpIdempotencyClaimStatus::Replay);
	const TSharedPtr<FJsonObject> ReplayObject = ParseObject(Replay.ResponseJson);
	TestTrue(TEXT("restart replay parses"), ReplayObject.IsValid());
	TestEqual(
		TEXT("restart replay uses current request id"),
		ReplayObject->GetStringField(TEXT("request_id")),
		FString(TEXT("two")));
	TestTrue(
		TEXT("restart replay annotation"),
		ReplayObject->GetObjectField(TEXT("metrics"))->GetBoolField(TEXT("replayed")));

	const FUeremcpIdempotencyClaim Conflict =
		Restarted.Claim(TEXT("claim-key"), FingerprintB, TEXT("three"));
	TestEqual(
		TEXT("conflicting key reuse is rejected after restart"),
		Conflict.Status,
		EUeremcpIdempotencyClaimStatus::Conflict);

	TArray<FString> TempFiles;
	IFileManager::Get().FindFiles(
		TempFiles,
		*FPaths::Combine(TempRoot, TEXT("*.tmp.*")),
		true,
		false);
	TestEqual(TEXT("atomic write leaves no temp files"), TempFiles.Num(), 0);

	Restarted.PurgeDurable();
	IFileManager::Get().DeleteDirectory(*TempRoot, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpIdempotencyCorruptionAndExpiry,
	"UEREMCP.Protocol.Idempotency.CorruptionAndExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpIdempotencyCorruptionAndExpiry::RunTest(const FString& Parameters)
{
	const FString TempRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("UEREMCP"),
		TEXT("__UeremcpTests"),
		TEXT("idempotency_corruption")));
	IFileManager::Get().DeleteDirectory(*TempRoot, false, true);
	IFileManager::Get().MakeDirectory(*TempRoot, true);

	const FString Key = TEXT("corrupt-key");
	const FString Path = FPaths::Combine(
		TempRoot,
		FUeremcpIdempotencyStore::DurableFileStemForKey(Key) + TEXT(".json"));
	TestTrue(
		TEXT("write corrupt fixture"),
		FFileHelper::SaveStringToFile(TEXT("{not-json"), *Path));

	FUeremcpIdempotencyStore Store;
	Store.SetDurableRootOverride(TempRoot);
	const FUeremcpIdempotencyClaim Corrupt =
		Store.Claim(Key, TEXT("sha256:fingerprint"), TEXT("request"));
	TestEqual(
		TEXT("corrupt record fails closed"),
		Corrupt.Status,
		EUeremcpIdempotencyClaimStatus::Error);
	TestFalse(TEXT("corrupt record is quarantined"), FPaths::FileExists(Path));
	TArray<FString> Quarantined;
	IFileManager::Get().FindFiles(
		Quarantined,
		*FPaths::Combine(TempRoot, TEXT("*.corrupt.*")),
		true,
		false);
	TestEqual(TEXT("one corrupt record retained for diagnosis"), Quarantined.Num(), 1);

	const FString ExpiringKey = TEXT("expiring-key");
	const FString Response = TEXT(
		R"({"protocol_version":"1.0","request_id":"expire","status":"no_change_required","summary":"done","metrics":{"mcp_round_trips":1,"internal_operations":0}})");
	FString Error;
	Store.SetRetention(FTimespan(1));
	TestEqual(
		TEXT("expiring claim acquired"),
		Store.Claim(ExpiringKey, TEXT("sha256:expire"), TEXT("expire")).Status,
		EUeremcpIdempotencyClaimStatus::Acquired);
	TestTrue(
		TEXT("expiring claim completes"),
		Store.Complete(ExpiringKey, TEXT("sha256:expire"), Response, Error));
	FPlatformProcess::Sleep(0.01f);
	TestEqual(
		TEXT("expired completed record is reclaimed"),
		Store.Claim(ExpiringKey, TEXT("sha256:expire"), TEXT("retry")).Status,
		EUeremcpIdempotencyClaimStatus::Acquired);

	Store.PurgeDurable();
	IFileManager::Get().DeleteDirectory(*TempRoot, false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
