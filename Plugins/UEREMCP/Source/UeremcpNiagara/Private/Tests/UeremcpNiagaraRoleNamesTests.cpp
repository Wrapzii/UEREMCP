// Editor automation tests for UeremcpNiagara role names (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UeremcpNiagaraRoleNames.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraPocBEmitterPlanOfflineTest,
	"UEREMCP.Niagara.Create.PocBEmitterPlanOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraPocBEmitterPlanOfflineTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Roles = UeremcpNiagaraRoles::DefaultPocBComponentRoles();
	TestEqual(TEXT("six POC B roles"), Roles.Num(), 6);

	static const TMap<FString, FString> ExpectedEmitterNames = {
		{ TEXT("core"), TEXT("Core") },
		{ TEXT("flame_shell"), TEXT("FlameShell") },
		{ TEXT("sparks"), TEXT("Sparks") },
		{ TEXT("smoke"), TEXT("Smoke") },
		{ TEXT("ribbon_trail"), TEXT("RibbonTrail") },
		{ TEXT("impact_burst"), TEXT("ImpactBurst") },
	};

	static const TMap<FString, FString> ExpectedTemplates = {
		{ TEXT("core"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal") },
		{ TEXT("flame_shell"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst") },
		{ TEXT("sparks"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst") },
		{ TEXT("smoke"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain") },
		{ TEXT("ribbon_trail"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon") },
		{ TEXT("impact_burst"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst") },
	};

	for (const FString& Role : Roles)
	{
		const FString* ExpectedName = ExpectedEmitterNames.Find(Role);
		const FString* ExpectedTemplate = ExpectedTemplates.Find(Role);
		TestNotNull(*FString::Printf(TEXT("%s expected emitter name"), *Role), ExpectedName);
		TestNotNull(*FString::Printf(TEXT("%s expected template"), *Role), ExpectedTemplate);
		if (ExpectedName)
		{
			TestEqual(
				*FString::Printf(TEXT("%s emitter name"), *Role),
				UeremcpNiagaraRoles::RoleToEmitterName(Role),
				*ExpectedName);
		}
		if (ExpectedTemplate)
		{
			TestEqual(
				*FString::Printf(TEXT("%s template path"), *Role),
				UeremcpNiagaraRoles::ResolveEmitterTemplatePath(Role),
				*ExpectedTemplate);
		}
	}

	TestEqual(
		TEXT("unknown role falls back to sparks template"),
		UeremcpNiagaraRoles::ResolveEmitterTemplatePath(TEXT("unknown_role")),
		ExpectedTemplates.FindRef(TEXT("sparks")));

	TestEqual(
		TEXT("ribbon_trail material purpose"),
		UeremcpNiagaraRoles::DefaultPurposeForMaterialRole(TEXT("ribbon_trail")),
		FString(TEXT("elemental_projectile_trail")));
	TestEqual(
		TEXT("flame_shell material purpose"),
		UeremcpNiagaraRoles::DefaultPurposeForMaterialRole(TEXT("flame_shell")),
		FString(TEXT("elemental_projectile_core")));

	const TSharedPtr<FJsonObject> CoreSpec =
		UeremcpNiagaraRoles::BuildDefaultFireballMaterialCreateSpec(TEXT("core"));
	TestTrue(TEXT("core create_spec"), CoreSpec.IsValid());
	FString Purpose;
	TestTrue(TEXT("core purpose field"), CoreSpec->TryGetStringField(TEXT("purpose"), Purpose));
	TestEqual(TEXT("core purpose value"), Purpose, FString(TEXT("elemental_projectile_core")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
