#include "UeremcpTransport.h"
#include "Modules/ModuleInterface.h"

DEFINE_LOG_CATEGORY(LogUeremcpTransport);

class FUeremcpTransportModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogUeremcpTransport, Log,
			TEXT("UEREMCP transport module loaded (capability probe + job constraints only; Epic owns HTTP/SSE)."));
	}

	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FUeremcpTransportModule, UeremcpTransport)
