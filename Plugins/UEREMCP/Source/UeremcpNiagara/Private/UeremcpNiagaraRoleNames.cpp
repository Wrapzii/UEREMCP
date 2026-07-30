// UEREMCP — role name conventions (WS-07).

#include "UeremcpNiagaraRoleNames.h"

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
		{ TEXT("core"), TEXT("/Niagara/DefaultAssets/Templates/Emitters/Minimal") },
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
