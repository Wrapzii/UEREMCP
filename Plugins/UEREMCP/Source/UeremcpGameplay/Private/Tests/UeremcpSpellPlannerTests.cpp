#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpGameplayToolset.h"
#include "UeremcpSpellPlanner.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
TSharedPtr<FJsonObject> ParseObject(const FString& Json)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	FJsonSerializer::Deserialize(Reader, Object);
	return Object;
}

const TCHAR* ValidSpecificationJson = TEXT(R"({
	"name":"Fireball UEREMCP",
	"row_name":"Fireball_Ueremcp",
	"element":"Fire",
	"element_color":[1.0,0.12,0.01,1.0],
	"delivery":{"type":"projectile","speed":2400,"range":3200},
	"impact":{"damage":45,"aoe_radius":175,"status":"Burn","status_duration":3},
	"presentation":{
		"projectile_effect":"/Game/__UeremcpTests/VFX/NS_Fireball",
		"impact_effect":"/Game/__UeremcpTests/VFX/NS_Fireball_Impact"
	},
	"networking":{"pattern":"B","authority":"server","cast_path":"AuthorityCastAbility"}
})");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSpellPlannerValidTest,
	"UEREMCP.Gameplay.SpellPlanner.ValidRESpell",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpSpellPlannerValidTest::RunTest(const FString& Parameters)
{
	FUeremcpSpellPlan Plan;
	FString Error;
	TestTrue(
		TEXT("valid create_spell specification plans"),
		FUeremcpSpellPlanner::BuildPlan(ParseObject(ValidSpecificationJson), Plan, Error));
	TestEqual(TEXT("stable row name"), Plan.RowName, FString(TEXT("Fireball_Ueremcp")));
	TestTrue(TEXT("row payload exists"), Plan.RowPayload.IsValid());
	if (Plan.RowPayload.IsValid())
	{
		TestEqual(
			TEXT("AbilityId echoes row identity"),
			Plan.RowPayload->GetStringField(TEXT("AbilityId")),
			FString(TEXT("Fireball_Ueremcp")));
		TestEqual(
			TEXT("semantic delivery maps to RE enum"),
			Plan.RowPayload->GetStringField(TEXT("CastType")),
			FString(TEXT("Projectile")));
		TestEqual(
			TEXT("RE status enum retained"),
			Plan.RowPayload->GetStringField(TEXT("ImpactStatus")),
			FString(TEXT("Burn")));
	}
	TestEqual(TEXT("both VFX dependencies returned"), Plan.DependencyAssetPaths.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpSpellPlannerRejectsClientAuthorityTest,
	"UEREMCP.Gameplay.SpellPlanner.RejectsClientAuthority",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpSpellPlannerRejectsClientAuthorityTest::RunTest(const FString& Parameters)
{
	TSharedPtr<FJsonObject> Specification = ParseObject(ValidSpecificationJson);
	const TSharedPtr<FJsonObject>* Networking = nullptr;
	if (Specification.IsValid()
		&& Specification->TryGetObjectField(TEXT("networking"), Networking)
		&& Networking && Networking->IsValid())
	{
		(*Networking)->SetStringField(TEXT("authority"), TEXT("client"));
	}

	FUeremcpSpellPlan Plan;
	FString Error;
	TestFalse(
		TEXT("client-authority declaration rejected"),
		FUeremcpSpellPlanner::BuildPlan(Specification, Plan, Error));
	TestTrue(TEXT("error identifies Pattern B"), Error.Contains(TEXT("Pattern B")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpCreateSpellPreflightHonestyTest,
	"UEREMCP.Gameplay.Toolset.CreateSpellPreflightHonesty",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::ProductFilter)

bool FUeremcpCreateSpellPreflightHonestyTest::RunTest(const FString& Parameters)
{
	const FString Request = FString::Printf(
		TEXT(R"({
			"protocol_version":"1.0",
			"request_id":"ws09-preflight",
			"action":"create_spell",
			"mode":"create_or_update",
			"target":{"asset_path":"/Game/__UeremcpTests/Abilities/DT_UeremcpAbilities"},
			"specification":%s
		})"),
		ValidSpecificationJson);

	const TSharedPtr<FJsonObject> Response =
		ParseObject(UUeremcpGameplayToolset::CreateSpell(Request));
	TestTrue(TEXT("response parses"), Response.IsValid());
	if (!Response.IsValid())
	{
		return false;
	}

	TestEqual(
		TEXT("no unverified mutation success"),
		Response->GetStringField(TEXT("status")),
		FString(TEXT("partially_completed")));

	const TSharedPtr<FJsonObject>* Validation = nullptr;
	TestTrue(
		TEXT("validation evidence returned"),
		Response->TryGetObjectField(TEXT("validation"), Validation)
			&& Validation && Validation->IsValid());
	if (Validation && Validation->IsValid())
	{
		TestTrue(
			TEXT("static structure validated"),
			(*Validation)->GetBoolField(TEXT("structurally_valid")));
		TestTrue(
			TEXT("write reread explicitly unperformed"),
			(*Validation)->HasTypedField<EJson::Null>(TEXT("reread_after_write")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
