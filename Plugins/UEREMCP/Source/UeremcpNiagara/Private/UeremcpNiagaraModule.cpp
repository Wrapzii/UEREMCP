// UEREMCP — Niagara domain module (WS-07).

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpNiagara, Log, All);

class FUeremcpNiagaraModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogUeremcpNiagara, Log, TEXT("UEREMCP Niagara module started (Wave 2 scaffold)."));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogUeremcpNiagara, Log, TEXT("UEREMCP Niagara module shut down."));
	}
};

IMPLEMENT_MODULE(FUeremcpNiagaraModule, UeremcpNiagara)
