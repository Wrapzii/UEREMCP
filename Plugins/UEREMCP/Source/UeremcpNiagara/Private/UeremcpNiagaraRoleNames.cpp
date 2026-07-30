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
