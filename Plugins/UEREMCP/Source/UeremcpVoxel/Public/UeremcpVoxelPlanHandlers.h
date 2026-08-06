#pragma once

#include "CoreMinimal.h"

class FUeremcpVoxelPlanHandlers
{
public:
	static bool Register(FString& OutError);
	static void Unregister();
};
