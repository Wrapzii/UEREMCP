// Editor automation tests for UeremcpNiagara create (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpNiagaraCreate.h"
#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraToolset.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCreatePathGuardTest,
	"UEREMCP.Niagara.Create.PathGuard",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCreatePathGuardTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("tests root allowed"),
		UeremcpNiagaraPaths::IsAllowedProbePath(TEXT("/Game/__UeremcpTests/NS_WS07_CreateProbe")));
	TestFalse(TEXT("game content rejected"),
		UeremcpNiagaraPaths::IsAllowedProbePath(TEXT("/Game/VFX/NS_Fireball")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCreateDryRunTest,
	"UEREMCP.Niagara.Create.DryRun",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCreateDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-create-dry","action":"create_niagara_effect","target":{"asset_path":"/Game/__UeremcpTests/NS_WS07_CreateDry"},"specification":{"effect_type":"projectile","element":"fire","components":["sparks"],"parameters":{"scale":1.0,"intensity":4.0}},"options":{"dry_run":true}})");

	const FString Json = UUeremcpNiagaraToolset::CreateNiagaraEffect(Request);

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("create dry-run returns JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("dry_run status"), Status, FString(TEXT("no_change_required")));

	const TArray<TSharedPtr<FJsonValue>>* NotesArr = nullptr;
	TestTrue(TEXT("capability_notes present"), Root->TryGetArrayField(TEXT("capability_notes"), NotesArr));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraCreateReplaceDryRunTest,
	"UEREMCP.Niagara.Create.ReplaceDryRun",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraCreateReplaceDryRunTest::RunTest(const FString& Parameters)
{
	const FString Request = TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-create-replace-dry","action":"create_niagara_effect","mode":"replace","target":{"asset_path":"/Game/__UeremcpTests/NS_WS07_RoundTripProbe"},"specification":{"effect_type":"projectile","element":"fire","components":["sparks"],"parameters":{"scale":1.0,"intensity":4.0}},"options":{"dry_run":true}})");

	const FString Json = UUeremcpNiagaraToolset::CreateNiagaraEffect(Request);

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("replace dry-run returns JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}

	FString Status;
	Root->TryGetStringField(TEXT("status"), Status);
	TestEqual(TEXT("dry_run status"), Status, FString(TEXT("no_change_required")));

	FString Summary;
	Root->TryGetStringField(TEXT("summary"), Summary);
	TestTrue(TEXT("summary mentions replace"), Summary.Contains(TEXT("replace")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
