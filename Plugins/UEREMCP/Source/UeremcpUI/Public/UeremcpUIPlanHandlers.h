#pragma once

#include "CoreMinimal.h"

class FUeremcpUIPlanHandlers
{
public:
	static bool Register(FString& OutError);
	static void Unregister();
};
