// UEREMCP — role name conventions (WS-07).

#include "UeremcpNiagaraRoleNames.h"

#include "Dom/JsonObject.h"

FString UeremcpNiagaraRoles::RoleToEmitterName(const FString& Role)
{
	FString Out;
	TArray<FString> Parts;
	Role.ParseIntoArray(Parts, TEXT("_"), true);
	for (FString& Part : Parts)
	{
		if (Part.Len() > 0)
		{
			Part[0] = FChar::ToUpper(Part[0]);
		}
		Out += Part;
	}
	return Out.IsEmpty() ? Role : Out;
}

FString UeremcpNiagaraRoles::ResolveEmitterTemplatePath(const FString& Role)
{
	const FString Key = Role.ToLower();
	// Soft package paths — Create/Submit append .AssetName when needed.
	// [VERIFIED: Engine/Plugins/FX/Niagara/Content/DefaultAssets/Templates/Emitters/*.uasset]
	static const TMap<FString, FString> RoleTemplates = {
		{ TEXT("core"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain") },
		{ TEXT("flame_shell"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst") },
		{ TEXT("sparks"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst") },
		{ TEXT("spark_burst"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst") },
		{ TEXT("smoke"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain") },
		{ TEXT("ribbon_trail"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon") },
		{ TEXT("trail"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon") },
		{ TEXT("impact_burst"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst") },
		{ TEXT("burst"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst") },
		{ TEXT("explosion"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst") },
		{ TEXT("crystalline"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst") },
		{ TEXT("ice_impact"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst") },
		// Ice / freeze topologies (creep + dome + sparks one-shot)
		{ TEXT("creep"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles") },
		{ TEXT("ice_creep"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles") },
		{ TEXT("ground_creep"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles") },
		{ TEXT("dome"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates") },
		{ TEXT("freeze_dome"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates") },
		{ TEXT("ice_dome"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates") },
		{ TEXT("frost"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst") },
		{ TEXT("freeze"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst") },
		// Precipitation / weather
		{ TEXT("rain"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/RecycleParticlesInView") },
		{ TEXT("precipitation"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/RecycleParticlesInView") },
		{ TEXT("mist"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates") },
		{ TEXT("fog"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates") },
		{ TEXT("hanging"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates") },
		// Free_Spells-like / spell FX patterns
		{ TEXT("circle"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/ConfettiBurst") },
		{ TEXT("confetti"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/ConfettiBurst") },
		{ TEXT("mesh"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst") },
		{ TEXT("ray"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/DirectionalBurst") },
		{ TEXT("directional"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/DirectionalBurst") },
		{ TEXT("beam"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/DynamicBeam") },
		{ TEXT("dynamic_beam"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/DynamicBeam") },
		{ TEXT("static_beam"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/StaticBeam") },
		{ TEXT("fountain"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain") },
		{ TEXT("blowing"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles") },
		{ TEXT("wind"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/BlowingParticles") },
		{ TEXT("single"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/SingleLoopingParticle") },
		{ TEXT("single_loop"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/SingleLoopingParticle") },
		{ TEXT("minimal"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal") },
		{ TEXT("aura"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates") },
	};
	if (const FString* Found = RoleTemplates.Find(Key))
	{
		return *Found;
	}
	return RoleTemplates.FindRef(TEXT("sparks"));
}

TArray<FString> UeremcpNiagaraRoles::DefaultPrecipitationComponentRoles()
{
	return {
		TEXT("rain"),
		TEXT("mist"),
	};
}

TArray<FString> UeremcpNiagaraRoles::DefaultPocBComponentRoles()
{
	return {
		TEXT("core"),
		TEXT("flame_shell"),
		TEXT("sparks"),
		TEXT("smoke"),
		TEXT("ribbon_trail"),
		TEXT("impact_burst"),
	};
}

TArray<FString> UeremcpNiagaraRoles::DefaultBurstComponentRoles()
{
	return {
		TEXT("sparks"),
		TEXT("impact_burst"),
	};
}

TArray<FString> UeremcpNiagaraRoles::DefaultSpellFxComponentRoles()
{
	return {
		TEXT("circle"),
		TEXT("sparks"),
		TEXT("mesh"),
	};
}

TArray<FString> UeremcpNiagaraRoles::DefaultIceFreezeComponentRoles()
{
	return {
		TEXT("ice_creep"),
		TEXT("freeze_dome"),
		TEXT("sparks"),
	};
}

TArray<FString> UeremcpNiagaraRoles::DefaultComponentRolesForEffectType(const FString& EffectType)
{
	const FString Key = EffectType.ToLower();
	if (Key == TEXT("precipitation") || Key == TEXT("rain") || Key == TEXT("weather"))
	{
		return DefaultPrecipitationComponentRoles();
	}
	if (Key == TEXT("projectile") || Key == TEXT("fireball") || Key == TEXT("missile"))
	{
		return DefaultPocBComponentRoles();
	}
	if (Key == TEXT("explosion") || Key == TEXT("burst") || Key == TEXT("impact")
		|| Key == TEXT("spark_burst") || Key == TEXT("sparks"))
	{
		return DefaultBurstComponentRoles();
	}
	if (Key == TEXT("ice") || Key == TEXT("freeze") || Key == TEXT("frost")
		|| Key == TEXT("ice_creep") || Key == TEXT("freeze_dome"))
	{
		return DefaultIceFreezeComponentRoles();
	}
	if (Key == TEXT("spell") || Key == TEXT("spell_fx") || Key == TEXT("magecraft")
		|| Key == TEXT("free_spells") || Key == TEXT("circle") || Key == TEXT("cast"))
	{
		return DefaultSpellFxComponentRoles();
	}
	if (Key == TEXT("beam") || Key == TEXT("ray") || Key == TEXT("laser"))
	{
		return { TEXT("beam"), TEXT("sparks") };
	}
	if (Key == TEXT("aura") || Key == TEXT("mist") || Key == TEXT("fog"))
	{
		return { TEXT("mist"), TEXT("sparks") };
	}
	if (Key == TEXT("helix") || Key == TEXT("trail"))
	{
		return { TEXT("core"), TEXT("ribbon_trail"), TEXT("sparks") };
	}
	if (Key == TEXT("mesh") || Key == TEXT("mesh_burst"))
	{
		return { TEXT("mesh"), TEXT("sparks") };
	}
	// Unknown effect_type: still author at least one emitter (never empty shell).
	return { TEXT("sparks") };
}

FString UeremcpNiagaraRoles::DefaultPurposeForMaterialRole(const FString& Role)
{
	const FString Key = Role.ToLower();
	if (Key == TEXT("ribbon_trail") || Key == TEXT("trail") || Key == TEXT("trail_material"))
	{
		return TEXT("elemental_projectile_trail");
	}
	if (Key == TEXT("core")
		|| Key == TEXT("core_material")
		|| Key == TEXT("flame_shell")
		|| Key == TEXT("sparks")
		|| Key == TEXT("spark_burst")
		|| Key == TEXT("smoke")
		|| Key == TEXT("impact_burst")
		|| Key == TEXT("burst")
		|| Key == TEXT("explosion")
		|| Key == TEXT("crystalline")
		|| Key == TEXT("ice_impact")
		|| Key == TEXT("creep")
		|| Key == TEXT("ice_creep")
		|| Key == TEXT("ground_creep")
		|| Key == TEXT("dome")
		|| Key == TEXT("freeze_dome")
		|| Key == TEXT("ice_dome")
		|| Key == TEXT("frost")
		|| Key == TEXT("freeze")
		|| Key == TEXT("rain")
		|| Key == TEXT("precipitation")
		|| Key == TEXT("mist")
		|| Key == TEXT("fog")
		|| Key == TEXT("hanging")
		|| Key == TEXT("circle")
		|| Key == TEXT("confetti")
		|| Key == TEXT("mesh")
		|| Key == TEXT("ray")
		|| Key == TEXT("directional")
		|| Key == TEXT("beam")
		|| Key == TEXT("dynamic_beam")
		|| Key == TEXT("static_beam")
		|| Key == TEXT("fountain")
		|| Key == TEXT("blowing")
		|| Key == TEXT("wind")
		|| Key == TEXT("aura")
		|| Key == TEXT("single")
		|| Key == TEXT("single_loop")
		|| Key == TEXT("minimal"))
	{
		return TEXT("elemental_projectile_core");
	}
	return FString();
}

TSharedPtr<FJsonObject> UeremcpNiagaraRoles::BuildDefaultFireballMaterialCreateSpec(
	const FString& Role,
	const FString& Element)
{
	const FString Purpose = DefaultPurposeForMaterialRole(Role);
	if (Purpose.IsEmpty())
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> CreateSpec = MakeShared<FJsonObject>();
	CreateSpec->SetStringField(TEXT("purpose"), Purpose);
	CreateSpec->SetStringField(TEXT("element"), Element);

	if (Purpose == TEXT("elemental_projectile_trail"))
	{
		TArray<TSharedPtr<FJsonValue>> Features;
		Features.Add(MakeShared<FJsonValueString>(TEXT("panning_textures")));
		Features.Add(MakeShared<FJsonValueString>(TEXT("erosion")));
		Features.Add(MakeShared<FJsonValueString>(TEXT("depth_fade")));
		Features.Add(MakeShared<FJsonValueString>(TEXT("dynamic_color")));
		CreateSpec->SetArrayField(TEXT("features"), Features);

		TSharedPtr<FJsonObject> Textures = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> FlowMap = MakeShared<FJsonObject>();
		FlowMap->SetStringField(TEXT("generate"), TEXT("flow_map"));
		TArray<TSharedPtr<FJsonValue>> Dimensions;
		Dimensions.Add(MakeShared<FJsonValueNumber>(256));
		Dimensions.Add(MakeShared<FJsonValueNumber>(256));
		FlowMap->SetArrayField(TEXT("dimensions"), Dimensions);
		Textures->SetObjectField(TEXT("FlowMap"), FlowMap);
		CreateSpec->SetObjectField(TEXT("textures"), Textures);
	}
	else
	{
		TArray<TSharedPtr<FJsonValue>> Features;
		Features.Add(MakeShared<FJsonValueString>(TEXT("radial_falloff")));
		Features.Add(MakeShared<FJsonValueString>(TEXT("animated_noise")));
		Features.Add(MakeShared<FJsonValueString>(TEXT("fresnel")));
		Features.Add(MakeShared<FJsonValueString>(TEXT("dynamic_color")));
		Features.Add(MakeShared<FJsonValueString>(TEXT("dynamic_intensity")));
		CreateSpec->SetArrayField(TEXT("features"), Features);
	}

	return CreateSpec;
}
