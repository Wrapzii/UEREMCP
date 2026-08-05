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
		{ TEXT("core"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain") },
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
		TEXT("crystalline role uses sparks archetype"),
		UeremcpNiagaraRoles::ResolveEmitterTemplatePath(TEXT("crystalline")),
		ExpectedTemplates.FindRef(TEXT("sparks")));
	TestEqual(
		TEXT("ice impact role uses burst archetype"),
		UeremcpNiagaraRoles::ResolveEmitterTemplatePath(TEXT("ice_impact")),
		ExpectedTemplates.FindRef(TEXT("impact_burst")));

	TestEqual(
		TEXT("rain role uses RecycleParticlesInView"),
		UeremcpNiagaraRoles::ResolveEmitterTemplatePath(TEXT("rain")),
		FString(TEXT("/Niagara/DefaultAssets/Templates/Emitters/RecycleParticlesInView")));
	TestEqual(
		TEXT("precipitation role aliases rain template"),
		UeremcpNiagaraRoles::ResolveEmitterTemplatePath(TEXT("precipitation")),
		FString(TEXT("/Niagara/DefaultAssets/Templates/Emitters/RecycleParticlesInView")));
	TestEqual(
		TEXT("mist role uses HangingParticulates"),
		UeremcpNiagaraRoles::ResolveEmitterTemplatePath(TEXT("mist")),
		FString(TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates")));
	TestEqual(
		TEXT("ice_creep uses BlowingParticles"),
		UeremcpNiagaraRoles::ResolveEmitterTemplatePath(TEXT("ice_creep")),
		FString(TEXT("/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles")));
	TestEqual(
		TEXT("freeze_dome uses HangingParticulates"),
		UeremcpNiagaraRoles::ResolveEmitterTemplatePath(TEXT("freeze_dome")),
		FString(TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates")));
	TestEqual(
		TEXT("rain emitter name"),
		UeremcpNiagaraRoles::RoleToEmitterName(TEXT("rain")),
		FString(TEXT("Rain")));
	const TArray<FString> PrecipRoles = UeremcpNiagaraRoles::DefaultPrecipitationComponentRoles();
	TestEqual(TEXT("precipitation default role count"), PrecipRoles.Num(), 2);
	TestTrue(TEXT("precipitation includes rain"), PrecipRoles.Contains(TEXT("rain")));
	TestTrue(TEXT("precipitation includes mist"), PrecipRoles.Contains(TEXT("mist")));
	const TArray<FString> IceRoles = UeremcpNiagaraRoles::DefaultIceFreezeComponentRoles();
	TestEqual(TEXT("ice default role count"), IceRoles.Num(), 3);
	TestTrue(TEXT("ice includes creep"), IceRoles.Contains(TEXT("ice_creep")));
	TestTrue(TEXT("ice includes dome"), IceRoles.Contains(TEXT("freeze_dome")));
	TestTrue(TEXT("ice includes sparks"), IceRoles.Contains(TEXT("sparks")));
	const TArray<FString> IceEffectDefaults =
		UeremcpNiagaraRoles::DefaultComponentRolesForEffectType(TEXT("freeze"));
	TestEqual(TEXT("effect_type=freeze defaults to ice plan"), IceEffectDefaults.Num(), 3);
	TestEqual(
		TEXT("rain material purpose"),
		UeremcpNiagaraRoles::DefaultPurposeForMaterialRole(TEXT("rain")),
		FString(TEXT("elemental_projectile_core")));

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

	const TSharedPtr<FJsonObject> TrailSpec =
		UeremcpNiagaraRoles::BuildDefaultFireballMaterialCreateSpec(TEXT("ribbon_trail"));
	TestTrue(TEXT("ribbon_trail create_spec"), TrailSpec.IsValid());
	const TSharedPtr<FJsonObject>* TrailTextures = nullptr;
	TestTrue(
		TEXT("ribbon_trail default FlowMap textures"),
		TrailSpec->TryGetObjectField(TEXT("textures"), TrailTextures)
			&& TrailTextures
			&& (*TrailTextures)->HasField(TEXT("FlowMap")));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
