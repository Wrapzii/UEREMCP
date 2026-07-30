// UEREMCP — security module. Registers no AICallable tools; hosts policy helpers.
// Intentionally not a UToolsetDefinition — domains adopt ADR-0010 via Core's
// FUeremcpMutatingDispatch rather than Security exposing tools.

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpSecurity, Log, All);

class FUeremcpSecurityModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogUeremcpSecurity, Log, TEXT("UEREMCP security module started."));
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogUeremcpSecurity, Log, TEXT("UEREMCP security module shut down."));
	}
};

IMPLEMENT_MODULE(FUeremcpSecurityModule, UeremcpSecurity)
