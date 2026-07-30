// UEREMCP — execute_plan semantic handler for create_vfx_material (WS-08).
//
// Contract: docs/proposals/ws-15-plan-handler-registration.md (WS-15 b15658f).
// Registers one action with FUeremcpPlanExecutor; delegates to the existing
// goal-level toolset entry point — one envelope in, one envelope out.

#pragma once

#include "CoreMinimal.h"

class FUeremcpMaterialPlanHandlers
{
public:
	/** Goal-level action registered with FUeremcpPlanExecutor (WS-15 drift guard). */
	static const TCHAR* RegisteredActionName();

	/** Register create_vfx_material. Fails closed if already registered. */
	static bool Register(FString& OutError);

	/** Unregister only create_vfx_material (module shutdown). */
	static void Unregister();
};
