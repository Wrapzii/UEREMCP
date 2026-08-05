// Editor automation tests for adapt_niagara_effect (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpNiagaraAdapt.h"
#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraToolset.h"
#include "UeremcpEnvelope.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraAdaptPathGuardTest,
	"UEREMCP.Niagara.Adapt.PathGuard",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraAdaptPathGuardTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("Magecraft mutate allowed"),
		UeremcpNiagaraPaths::IsAllowedMutatePath(
			TEXT("/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast")));
	TestFalse(
		TEXT("off-root mutate denied"),
		UeremcpNiagaraPaths::IsAllowedMutatePath(TEXT("/Game/VFX/NS_Fireball")));

	const FString RejectJson = UUeremcpNiagaraToolset::AdaptNiagaraEffect(TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-adapt-offroot","action":"adapt_niagara_effect","target":{"asset_path":"/Game/VFX/NS_Fireball"},"specification":{"parameters":{"scale":1.0}},"options":{"dry_run":true}})"));
	TSharedPtr<FJsonObject> RejectRoot;
	const TSharedRef<TJsonReader<>> RejectReader = TJsonReaderFactory<>::Create(RejectJson);
	TestTrue(TEXT("off-root adapt JSON"), FJsonSerializer::Deserialize(RejectReader, RejectRoot) && RejectRoot.IsValid());
	if (RejectRoot.IsValid())
	{
		TestEqual(TEXT("rejected"), RejectRoot->GetStringField(TEXT("status")), FString(TEXT("rejected")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraAdaptDryRunTest,
	"UEREMCP.Niagara.Adapt.DryRun",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraAdaptDryRunTest::RunTest(const FString& Parameters)
{
	FUeremcpNiagaraAdaptSpec Spec;
	FString SpecError;
	TSharedPtr<FJsonObject> SpecObj = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetNumberField(TEXT("dirtiness"), 0.25);
	Params->SetBoolField(TEXT("include_adaptation"), true);
	SpecObj->SetObjectField(TEXT("parameters"), Params);
	TestTrue(TEXT("parse adapt spec"), FUeremcpNiagaraAdapt::ParseSpecification(SpecObj, Spec, SpecError));

	const FString Json = UUeremcpNiagaraToolset::AdaptNiagaraEffect(TEXT(
		R"({"protocol_version":"1.0","request_id":"ws07-adapt-dry","action":"adapt_niagara_effect","target":{"asset_path":"/Game/RE/VFX/Magecraft/Spells/Adapted/NS_nature_xl_cast"},"specification":{"parameters":{"include_adaptation":true,"dirtiness":0.25}},"options":{"dry_run":true}})"));
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	TestTrue(TEXT("adapt dry_run JSON"), FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
	if (!Root.IsValid())
	{
		return false;
	}
	TestEqual(TEXT("dry_run status"), Root->GetStringField(TEXT("status")), FString(TEXT("no_change_required")));
	TestTrue(TEXT("summary mentions adapt"), Root->GetStringField(TEXT("summary")).Contains(TEXT("adapt")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraAdaptSpecRequiresPayloadTest,
	"UEREMCP.Niagara.Adapt.SpecRequiresPayload",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraAdaptSpecRequiresPayloadTest::RunTest(const FString& Parameters)
{
	FUeremcpNiagaraAdaptSpec Spec;
	FString SpecError;
	TSharedPtr<FJsonObject> Empty = MakeShared<FJsonObject>();
	TestFalse(TEXT("empty spec rejected"), FUeremcpNiagaraAdapt::ParseSpecification(Empty, Spec, SpecError));
	TestTrue(TEXT("error mentions parameters"), SpecError.Contains(TEXT("parameters")));
	return true;
}

#endif
