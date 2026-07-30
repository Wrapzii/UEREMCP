// UEREMCP — Material domain module (WS-08).

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpMaterial, Log, All);

class FUeremcpMaterialModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogUeremcpMaterial, Log, TEXT("UEREMCP Material module started (Wave 2 scaffold)."));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogUeremcpMaterial, Log, TEXT("UEREMCP Material module shut down."));
	}
};

IMPLEMENT_MODULE(FUeremcpMaterialModule, UeremcpMaterial)
