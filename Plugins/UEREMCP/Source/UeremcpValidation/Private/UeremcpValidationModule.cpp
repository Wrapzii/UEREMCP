// UEREMCP — validation module. Registers no AICallable tools; hosts automation
// tests and scratch-path helpers only (RB-14).

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpValidation, Log, All);

class FUeremcpValidationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogUeremcpValidation, Log, TEXT("UEREMCP validation module started."));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogUeremcpValidation, Log, TEXT("UEREMCP validation module shut down."));
	}
};

IMPLEMENT_MODULE(FUeremcpValidationModule, UeremcpValidation)
