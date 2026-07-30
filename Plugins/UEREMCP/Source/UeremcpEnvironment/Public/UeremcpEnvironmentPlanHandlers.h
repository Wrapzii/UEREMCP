// UEREMCP — execute_plan handlers for environment actions (COVERAGE_PLAN Part III).
#pragma once

#include "CoreMinimal.h"

class FUeremcpEnvironmentPlanHandlers
{
public:
	static bool Register(FString& OutError);
	static void Unregister();
};
