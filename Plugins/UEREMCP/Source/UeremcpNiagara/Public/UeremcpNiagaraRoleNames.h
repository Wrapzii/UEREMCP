// UEREMCP — role name conventions shared by create + material bind (WS-07).

#pragma once

#include "CoreMinimal.h"

namespace UeremcpNiagaraRoles
{
	/** Map specification role token to emitter name (e.g. ribbon_trail → RibbonTrail). */
	FString RoleToEmitterName(const FString& Role);
}
