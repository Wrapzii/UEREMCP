// UEREMCP — execute_plan handlers for Blueprint actions.
#pragma once

#include "CoreMinimal.h"

class FUeremcpBlueprintPlanHandlers
{
public:
	static bool Register(FString& OutError);
	static void Unregister();
};
