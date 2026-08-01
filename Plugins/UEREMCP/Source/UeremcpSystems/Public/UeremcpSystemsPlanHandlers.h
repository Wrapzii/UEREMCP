// UEREMCP — execute_plan handlers for Systems actions.
#pragma once

#include "CoreMinimal.h"

class FUeremcpSystemsPlanHandlers
{
public:
	static bool Register(FString& OutError);
	static void Unregister();
};
