// UEREMCP — execute_plan handlers for Templates actions.
#pragma once

#include "CoreMinimal.h"

class FUeremcpTemplatesPlanHandlers
{
public:
	static bool Register(FString& OutError);
	static void Unregister();
};
