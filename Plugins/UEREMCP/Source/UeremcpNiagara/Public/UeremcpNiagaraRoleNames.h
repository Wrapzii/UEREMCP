// UEREMCP — role name conventions shared by create + material bind (WS-07).

#pragma once

#include "CoreMinimal.h"

namespace UeremcpNiagaraRoles
{
	/** Map specification role token to emitter name (e.g. ribbon_trail → RibbonTrail). */
	FString RoleToEmitterName(const FString& Role);

	/** POC B default emitter template soft path for a component role. */
	FString ResolveEmitterTemplatePath(const FString& Role);

	/** Canonical six-emitter POC B projectile plan (docs/POC_ACCEPTANCE.md B3). */
	TArray<FString> DefaultPocBComponentRoles();
}
