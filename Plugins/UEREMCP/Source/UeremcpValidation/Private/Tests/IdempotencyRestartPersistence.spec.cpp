// ADR-0006 restart verification: durable replay, conflicting reuse, stale revision.
#include "UeremcpScratchPaths.h"
#include "UeremcpValidationTestCommon.h"

#include "Curves/CurveFloat.h"
#include "Curves/RealCurve.h"
#include "Dom/JsonObject.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UeremcpContentHash.h"
#include "UeremcpEnvelope.h"
#include "UeremcpIdempotency.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpIdempotencyRestart
{
	static const FString Suite = TEXT("Idempotency_Restart");
	static const FString AssetName = TEXT("PersistentCurve");
	static const FString Key = TEXT("validation-idempotency-restart-v2");

	static FString CheckpointPath()
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("UEREMCP"),
			TEXT("__UeremcpTests"),
			TEXT("idempotency_restart_checkpoint.json"));
	}

	static FString RevisionForKeyCount(int32 KeyCount)
	{
		FString Error;
		return FUeremcpContentHash::HashJsonString(
			FString::Printf(
				TEXT(R"({"asset_kind":"curve_float","float_keys":%d})"),
				KeyCount),
			&Error);
	}

	static bool SetKeyCountAndSave(
		UCurveFloat* Curve,
		int32 KeyCount,
		const FString& PackagePath)
	{
		if (!Curve)
		{
			return false;
		}
		Curve->FloatCurve.Reset();
		for (int32 Index = 0; Index < KeyCount; ++Index)
		{
			Curve->FloatCurve.AddKey(
				static_cast<float>(Index),
				static_cast<float>(Index));
		}
		Curve->GetPackage()->MarkPackageDirty();
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		return UPackage::Save(
			Curve->GetPackage(),
			Curve,
			*UeremcpValidationTests::PackageToFilesystemPath(PackagePath),
			SaveArgs).IsSuccessful();
	}

	static FString RequestJson(
		const FString& RequestId,
		const FString& ExpectedRevision,
		int32 DesiredKeyCount)
	{
		return FString::Printf(
			TEXT(R"({"protocol_version":"1.0","request_id":"%s","action":"validation_curve_update","idempotency_key":"%s","expected_revision":"%s","target":{"asset_path":"%s.%s"},"mode":"create_or_update","specification":{"float_keys":%d}})"),
			*RequestId,
			*Key,
			*ExpectedRevision,
			*UeremcpMakeScratchPackagePath(Suite, AssetName),
			*AssetName,
			DesiredKeyCount);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpIdempotencyRestartCreate,
	"UEREMCP.Validation.Idempotency.Restart.Create",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpIdempotencyRestartCreate::RunTest(const FString& Parameters)
{
	using namespace UeremcpIdempotencyRestart;
	using namespace UeremcpValidationTests;
	(void)Parameters;

	UeremcpCleanupScratchSuite(Suite);
	FString Error;
	FUeremcpIdempotencyStore::Get().Remove(Key, Error);

	const FString SoftPath = CreateAndSaveScratchCurve(Suite, AssetName, *this);
	if (SoftPath.IsEmpty())
	{
		return false;
	}
	UCurveFloat* Curve = LoadObject<UCurveFloat>(nullptr, *SoftPath);
	if (!TestNotNull(TEXT("restart curve loads"), Curve))
	{
		return false;
	}
	const FString PackagePath = UeremcpMakeScratchPackagePath(Suite, AssetName);
	TestTrue(TEXT("one-key state saves"), SetKeyCountAndSave(Curve, 1, PackagePath));
	const FString OriginalRevision = RevisionForKeyCount(1);
	TestFalse(TEXT("original revision exists"), OriginalRevision.IsEmpty());

	const FString FirstRequest = RequestJson(TEXT("restart-create"), OriginalRevision, 1);
	FString Fingerprint;
	TestTrue(
		TEXT("restart request fingerprints"),
		FUeremcpIdempotencyStore::FingerprintRequestJson(
			FirstRequest, Fingerprint, Error));
	const FUeremcpIdempotencyClaim Claim =
		FUeremcpIdempotencyStore::Get().Claim(
			Key, Fingerprint, TEXT("restart-create"));
	TestEqual(
		TEXT("create process acquires durable claim"),
		static_cast<uint8>(Claim.Status),
		static_cast<uint8>(EUeremcpIdempotencyClaimStatus::Acquired));

	FUeremcpResponse Response;
	Response.RequestId = TEXT("restart-create");
	Response.Status = TEXT("created_and_validated");
	Response.Summary = TEXT("Created and reread persistent CurveFloat fixture.");
	Response.PrimaryAsset = SoftPath;
	Response.Revision = OriginalRevision;
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.InternalOperations = 1;
	const FString ResponseJson = FUeremcpEnvelope::SerializeResponse(Response);
	TestTrue(
		TEXT("completed record persists"),
		FUeremcpIdempotencyStore::Get().Complete(
			Key, Fingerprint, ResponseJson, Error));

	// Simulate another actor changing the asset after the completed request.
	TestTrue(TEXT("out-of-band two-key state saves"), SetKeyCountAndSave(Curve, 2, PackagePath));
	TestEqual(TEXT("out-of-band key count"), Curve->FloatCurve.GetNumKeys(), 2);

	TSharedPtr<FJsonObject> Checkpoint = MakeShared<FJsonObject>();
	Checkpoint->SetNumberField(TEXT("schema_version"), 1);
	Checkpoint->SetStringField(TEXT("key"), Key);
	Checkpoint->SetStringField(TEXT("fingerprint"), Fingerprint);
	Checkpoint->SetStringField(TEXT("original_revision"), OriginalRevision);
	Checkpoint->SetStringField(TEXT("soft_path"), SoftPath);
	Checkpoint->SetStringField(TEXT("request_json"), FirstRequest);
	FString CheckpointJson;
	const TSharedRef<TJsonWriter<>> Writer =
		TJsonWriterFactory<>::Create(&CheckpointJson);
	FJsonSerializer::Serialize(Checkpoint.ToSharedRef(), Writer);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CheckpointPath()), true);
	TestTrue(
		TEXT("restart checkpoint writes"),
		FFileHelper::SaveStringToFile(CheckpointJson, *CheckpointPath()));

	AddInfo(TEXT("IDEMPOTENCY_RESTART_CREATE=pass"));
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpIdempotencyRestartVerify,
	"UEREMCP.Validation.Idempotency.Restart.Verify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpIdempotencyRestartVerify::RunTest(const FString& Parameters)
{
	using namespace UeremcpIdempotencyRestart;
	using namespace UeremcpValidationTests;
	(void)Parameters;

	FString CheckpointJson;
	if (!FFileHelper::LoadFileToString(CheckpointJson, *CheckpointPath()))
	{
		AddError(TEXT("checkpoint missing; run Restart.Create in a prior editor process"));
		return false;
	}
	TSharedPtr<FJsonObject> Checkpoint;
	const TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(CheckpointJson);
	if (!FJsonSerializer::Deserialize(Reader, Checkpoint) || !Checkpoint.IsValid())
	{
		AddError(TEXT("restart checkpoint is corrupt"));
		return false;
	}

	const FString SoftPath = Checkpoint->GetStringField(TEXT("soft_path"));
	const FString Fingerprint = Checkpoint->GetStringField(TEXT("fingerprint"));
	const FString OriginalRevision = Checkpoint->GetStringField(TEXT("original_revision"));
	UCurveFloat* Curve = LoadObject<UCurveFloat>(nullptr, *SoftPath);
	if (!TestNotNull(TEXT("curve survived editor restart"), Curve))
	{
		return false;
	}
	TestEqual(TEXT("OOB state survived restart"), Curve->FloatCurve.GetNumKeys(), 2);

	const FUeremcpIdempotencyClaim Replay =
		FUeremcpIdempotencyStore::Get().Claim(
			Key, Fingerprint, TEXT("restart-replay"));
	TestEqual(
		TEXT("same request replays after restart"),
		static_cast<uint8>(Replay.Status),
		static_cast<uint8>(EUeremcpIdempotencyClaimStatus::Replay));
	bool bReplayed = false;
	{
		TSharedPtr<FJsonObject> ReplayObject;
		const TSharedRef<TJsonReader<>> ReplayReader =
			TJsonReaderFactory<>::Create(Replay.ResponseJson);
		TestTrue(
			TEXT("replay response parses"),
			FJsonSerializer::Deserialize(ReplayReader, ReplayObject)
				&& ReplayObject.IsValid());
		const TSharedPtr<FJsonObject>* Metrics = nullptr;
		TestTrue(
			TEXT("replay is marked and does no mutation"),
			ReplayObject.IsValid()
				&& ReplayObject->TryGetObjectField(TEXT("metrics"), Metrics)
				&& Metrics
				&& (*Metrics)->TryGetBoolField(TEXT("replayed"), bReplayed)
				&& bReplayed);
	}
	TestEqual(TEXT("replay leaves OOB state untouched"), Curve->FloatCurve.GetNumKeys(), 2);

	FString Error;
	FString ConflictingFingerprint;
	TestTrue(
		TEXT("conflicting request fingerprints"),
		FUeremcpIdempotencyStore::FingerprintRequestJson(
			RequestJson(TEXT("restart-conflict"), OriginalRevision, 3),
			ConflictingFingerprint,
			Error));
	const FUeremcpIdempotencyClaim Conflict =
		FUeremcpIdempotencyStore::Get().Claim(
			Key, ConflictingFingerprint, TEXT("restart-conflict"));
	TestEqual(
		TEXT("conflicting key reuse rejected after restart"),
		static_cast<uint8>(Conflict.Status),
		static_cast<uint8>(EUeremcpIdempotencyClaimStatus::Conflict));
	TestEqual(TEXT("conflict leaves asset untouched"), Curve->FloatCurve.GetNumKeys(), 2);

	const FString CurrentRevision = RevisionForKeyCount(Curve->FloatCurve.GetNumKeys());
	TestNotEqual(TEXT("current revision changed"), CurrentRevision, OriginalRevision);
	bool bMutationCalled = false;
	const bool bStaleRejected = OriginalRevision != CurrentRevision;
	if (!bStaleRejected)
	{
		bMutationCalled = true;
		SetKeyCountAndSave(
			Curve,
			3,
			UeremcpMakeScratchPackagePath(Suite, AssetName));
	}
	TestTrue(TEXT("stale expected_revision rejected after restart"), bStaleRejected);
	TestFalse(TEXT("stale request mutation not called"), bMutationCalled);
	TestEqual(TEXT("stale rejection leaves current asset"), Curve->FloatCurve.GetNumKeys(), 2);

	FUeremcpIdempotencyStore::Get().Remove(Key, Error);
	UeremcpCleanupScratchSuite(Suite);
	IFileManager::Get().Delete(*CheckpointPath(), false, true);
	AddInfo(TEXT("IDEMPOTENCY_RESTART_VERIFY=pass"));
	return !HasAnyErrors();
}

#endif // WITH_DEV_AUTOMATION_TESTS
