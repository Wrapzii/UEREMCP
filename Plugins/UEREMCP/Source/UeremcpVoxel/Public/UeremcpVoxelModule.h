#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FUeremcpVoxelModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterToolsets();
	FDelegateHandle OnPostEngineInitHandle;
};
