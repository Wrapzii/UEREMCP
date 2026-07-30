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
	static const TMap<FString, FString> RoleTemplates = {
		{ TEXT("core"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain") },
		{ TEXT("flame_shell"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/UpwardMeshBurst") },
		{ TEXT("sparks"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst") },
		{ TEXT("smoke"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain") },
		{ TEXT("ribbon_trail"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon") },
		{ TEXT("impact_burst"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst") },
	};
	if (const FString* Found = RoleTemplates.Find(Key))
	{
		return *Found;
	}
	return RoleTemplates.FindRef(TEXT("sparks"));
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
		|| Key == TEXT("smoke")
		|| Key == TEXT("impact_burst"))
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
