#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Misc/CoreDelegates.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpSystemsToolset.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpSystems, Log, All);

class FUeremcpSystemsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogUeremcpSystems, Log, TEXT("UEREMCP Systems module started; deferring toolset registration."));
		OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
			this, &FUeremcpSystemsModule::RegisterToolset);
	}

	virtual void ShutdownModule() override
	{
		if (OnPostEngineInitHandle.IsValid())
		{
			FCoreDelegates::GetOnPostEngineInit().Remove(OnPostEngineInitHandle);
			OnPostEngineInitHandle.Reset();
		}
		if (UObjectInitialized())
		{
			UToolsetRegistry::UnregisterToolsetClass(UUeremcpSystemsToolset::StaticClass());
		}
		UE_LOG(LogUeremcpSystems, Log, TEXT("UEREMCP Systems module shut down."));
	}

private:
	void RegisterToolset()
	{
		UToolsetRegistry::RegisterToolsetClass(UUeremcpSystemsToolset::StaticClass());
		if (UToolsetRegistry::IsToolsetClassRegistered(UUeremcpSystemsToolset::StaticClass()))
		{
			UE_LOG(LogUeremcpSystems, Log, TEXT("UUeremcpSystemsToolset registered (PostEngineInit)."));
		}
		else
		{
			UE_LOG(LogUeremcpSystems, Warning, TEXT("UUeremcpSystemsToolset registration failed at PostEngineInit."));
		}
	}

	FDelegateHandle OnPostEngineInitHandle;
};

IMPLEMENT_MODULE(FUeremcpSystemsModule, UeremcpSystems)
