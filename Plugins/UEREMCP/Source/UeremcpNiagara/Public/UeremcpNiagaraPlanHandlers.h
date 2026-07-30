// UEREMCP — execute_plan semantic handler for create_niagara_effect (WS-07).
//
// Contract: docs/proposals/ws-15-plan-handler-registration.md (WS-15 b15658f).
// Registers one action with FUeremcpPlanExecutor; delegates to the existing
// goal-level toolset entry point — one envelope in, one envelope out.

#pragma once

#include "CoreMinimal.h"

class FUeremcpNiagaraPlanHandlers
{
public:
	/** Register create_niagara_effect. Fails closed if already registered. */
	static bool Register(FString& OutError);

	/** Unregister only create_niagara_effect (module shutdown). */
	static void Unregister();
};
