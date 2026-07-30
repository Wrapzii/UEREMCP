// POC E3/E4 on the real Niagara create_niagara_effect pipeline.
#include "UeremcpScratchPaths.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpNiagaraToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpDomainNiagaraIdemRev
{
	static const FString SuiteName = TEXT("Domain_Niagara_IdemRev");

	static FString SoftPath(const FString& AssetName)
	{
		// Niagara create accepts package or soft object paths; use package form.
		return UeremcpMakeScratchPackagePath(SuiteName, AssetName);
	}

	static TSharedPtr<FJsonObject> ParseJson(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Root);
		return Root;
	}

	static FString MakeCreateRequest(
		const FString& RequestId,
		const FString& AssetPath,
		const FString& ExpectedRevision,
		const bool bIncludeExpectedRevision)
	{
		if (bIncludeExpectedRevision)
		{
			return FString::Printf(
				TEXT(R"({"protocol_version":"1.0","request_id":"%s","action":"create_niagara_effect",)")
				TEXT(R"("mode":"replace","expected_revision":"%s",)")
				TEXT(R"("target":{"asset_path":"%s"},)")
				TEXT(R"("specification":{"effect_type":"projectile","element":"fire","components":["sparks"],)")
				TEXT(R"("parameters":{"scale":1.0,"intensity":4.0},)")
				TEXT(R"("template_system":{"asset_path":"/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight"}},)")
				TEXT(R"("options":{"dry_run":false,"allow_destructive":true,"validate":false,"compile":true,"save":true}})"),
				*RequestId,
				*ExpectedRevision,
				*AssetPath);
		}
		return FString::Printf(
			TEXT(R"({"protocol_version":"1.0","request_id":"%s","action":"create_niagara_effect",)")
			TEXT(R"("mode":"replace",)")
			TEXT(R"("target":{"asset_path":"%s"},)")
			TEXT(R"("specification":{"effect_type":"projectile","element":"fire","components":["sparks"],)")
			TEXT(R"("parameters":{"scale":1.0,"intensity":4.0},)")
			TEXT(R"("template_system":{"asset_path":"/Niagara/DefaultAssets/Templates/Systems/MinimalLightweight"}},)")
			TEXT(R"("options":{"dry_run":false,"allow_destructive":true,"validate":false,"compile":true,"save":true}})"),
			*RequestId,
			*AssetPath);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpDomainNiagaraIdempotency,
	"UEREMCP.Validation.Domain.Niagara.IdempotencyRepeatedCreate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpDomainNiagaraIdempotency::RunTest(const FString& Parameters)
{
	using namespace UeremcpDomainNiagaraIdemRev;
	(void)Parameters;

	FUeremcpScratchGuard Guard(SuiteName);

	static const FString AssetName = TEXT("NS_Domain_Idem");
	const FString AssetPath = SoftPath(AssetName);

	const TSharedPtr<FJsonObject> SeedRoot = ParseJson(
		UUeremcpNiagaraToolset::CreateNiagaraEffect(
			MakeCreateRequest(TEXT("niag-idem-seed"), AssetPath, FString(), false)));
	if (!TestTrue(TEXT("seed parseable"), SeedRoot.IsValid()))
	{
		return false;
	}
	FString SeedStatus;
	SeedRoot->TryGetStringField(TEXT("status"), SeedStatus);
	TestTrue(
		TEXT("seed created or validated"),
		SeedStatus == TEXT("created_and_validated")
			|| SeedStatus == TEXT("modified_and_validated")
			|| SeedStatus == TEXT("partially_completed")
			|| SeedStatus == TEXT("created_with_warnings"));

	FString Revision;
	SeedRoot->TryGetStringField(TEXT("revision"), Revision);
	if (!TestTrue(TEXT("seed returns revision"), !Revision.IsEmpty()))
	{
		return false;
	}

	for (int32 Attempt = 1; Attempt <= 3; ++Attempt)
	{
		const FString ReqId = FString::Printf(TEXT("niag-idem-%d"), Attempt);
		const TSharedPtr<FJsonObject> Root = ParseJson(
			UUeremcpNiagaraToolset::CreateNiagaraEffect(
				MakeCreateRequest(ReqId, AssetPath, Revision, true)));
		if (!TestTrue(TEXT("repeat parseable"), Root.IsValid()))
		{
			return false;
		}
		FString Status;
		Root->TryGetStringField(TEXT("status"), Status);
		TestEqual(
			FString::Printf(TEXT("attempt %d is no_change_required"), Attempt),
			Status,
			FString(TEXT("no_change_required")));
	}

	AddInfo(TEXT("POC_E E3 domain Niagara: repeated identical create is no_change_required (3x)"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpDomainNiagaraRevisionStale,
	"UEREMCP.Validation.Domain.Niagara.RevisionStaleRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpDomainNiagaraRevisionStale::RunTest(const FString& Parameters)
{
	using namespace UeremcpDomainNiagaraIdemRev;
	(void)Parameters;

	FUeremcpScratchGuard Guard(SuiteName);

	static const FString AssetName = TEXT("NS_Domain_Rev");
	const FString AssetPath = SoftPath(AssetName);

	const TSharedPtr<FJsonObject> SeedRoot = ParseJson(
		UUeremcpNiagaraToolset::CreateNiagaraEffect(
			MakeCreateRequest(TEXT("niag-rev-seed"), AssetPath, FString(), false)));
	if (!TestTrue(TEXT("seed parseable"), SeedRoot.IsValid()))
	{
		return false;
	}
	FString Revision;
	SeedRoot->TryGetStringField(TEXT("revision"), Revision);
	if (!TestTrue(TEXT("seed returns revision"), !Revision.IsEmpty()))
	{
		return false;
	}

	const FString Stale =
		TEXT("sha256:0000000000000000000000000000000000000000000000000000000000000000");
	const TSharedPtr<FJsonObject> Root = ParseJson(
		UUeremcpNiagaraToolset::CreateNiagaraEffect(
			MakeCreateRequest(TEXT("niag-rev-stale"), AssetPath, Stale, true)));
	if (!TestTrue(TEXT("stale submit parseable"), Root.IsValid()))
	{
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("stale rejected"), Status, FString(TEXT("rejected")));
	FString ReturnedRevision;
	Root->TryGetStringField(TEXT("revision"), ReturnedRevision);
	TestEqual(TEXT("returns current revision"), ReturnedRevision, Revision);

	AddInfo(TEXT("POC_E E4 domain Niagara: stale expected_revision rejected with no mutation"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
