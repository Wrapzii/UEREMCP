// POC E3/E4 on the real Material create_vfx_material pipeline.
#include "UeremcpScratchPaths.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UeremcpMaterialToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace UeremcpDomainMaterialIdemRev
{
	static const FString SuiteName = TEXT("Domain_Material_IdemRev");

	static FString PackagePath(const FString& AssetName)
	{
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
				TEXT(R"({"protocol_version":"1.0","request_id":"%s","action":"create_vfx_material",)")
				TEXT(R"("mode":"create","expected_revision":"%s",)")
				TEXT(R"("target":{"asset_path":"%s"},)")
				TEXT(R"("specification":{"purpose":"elemental_projectile_core","element":"fire",)")
				TEXT(R"("features":["radial_falloff","animated_noise","fresnel","dynamic_color","dynamic_intensity"]},)")
				TEXT(R"("options":{"dry_run":false,"validate":true,"compile":true,"save":true}})"),
				*RequestId,
				*ExpectedRevision,
				*AssetPath);
		}
		return FString::Printf(
			TEXT(R"({"protocol_version":"1.0","request_id":"%s","action":"create_vfx_material",)")
			TEXT(R"("mode":"create",)")
			TEXT(R"("target":{"asset_path":"%s"},)")
			TEXT(R"("specification":{"purpose":"elemental_projectile_core","element":"fire",)")
			TEXT(R"("features":["radial_falloff","animated_noise","fresnel","dynamic_color","dynamic_intensity"]},)")
			TEXT(R"("options":{"dry_run":false,"validate":true,"compile":true,"save":true}})"),
			*RequestId,
			*AssetPath);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpDomainMaterialIdempotency,
	"UEREMCP.Validation.Domain.Material.IdempotencyRepeatedCreate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpDomainMaterialIdempotency::RunTest(const FString& Parameters)
{
	using namespace UeremcpDomainMaterialIdemRev;
	(void)Parameters;

	FUeremcpScratchGuard Guard(SuiteName);

	static const FString AssetName = TEXT("MI_Domain_Idem");
	const FString AssetPath = PackagePath(AssetName);

	const TSharedPtr<FJsonObject> SeedRoot = ParseJson(
		UUeremcpMaterialToolset::CreateVfxMaterial(
			MakeCreateRequest(TEXT("mat-idem-seed"), AssetPath, FString(), false)));
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
		const FString ReqId = FString::Printf(TEXT("mat-idem-%d"), Attempt);
		const TSharedPtr<FJsonObject> Root = ParseJson(
			UUeremcpMaterialToolset::CreateVfxMaterial(
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

	AddInfo(TEXT("POC_E E3 domain Material: repeated identical create is no_change_required (3x)"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpDomainMaterialRevisionStale,
	"UEREMCP.Validation.Domain.Material.RevisionStaleRejected",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpDomainMaterialRevisionStale::RunTest(const FString& Parameters)
{
	using namespace UeremcpDomainMaterialIdemRev;
	(void)Parameters;

	FUeremcpScratchGuard Guard(SuiteName);

	static const FString AssetName = TEXT("MI_Domain_Rev");
	const FString AssetPath = PackagePath(AssetName);

	const TSharedPtr<FJsonObject> SeedRoot = ParseJson(
		UUeremcpMaterialToolset::CreateVfxMaterial(
			MakeCreateRequest(TEXT("mat-rev-seed"), AssetPath, FString(), false)));
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
		UUeremcpMaterialToolset::CreateVfxMaterial(
			MakeCreateRequest(TEXT("mat-rev-stale"), AssetPath, Stale, true)));
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

	AddInfo(TEXT("POC_E E4 domain Material: stale expected_revision rejected with no mutation"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
