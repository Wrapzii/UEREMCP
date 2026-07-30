#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpAnimationToolset.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpAnimation, Log, All);

class FUeremcpAnimationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
			this, &FUeremcpAnimationModule::RegisterToolset);
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
			UToolsetRegistry::UnregisterToolsetClass(UUeremcpAnimationToolset::StaticClass());
		}
	}

private:
	void RegisterToolset()
	{
		// Explicit post-engine-init registration is required for domain toolsets.
		// [VERIFIED-RUNTIME: UeremcpCoreModule.cpp registration gate, 2026-07-30]
		UToolsetRegistry::RegisterToolsetClass(UUeremcpAnimationToolset::StaticClass());
		if (!UToolsetRegistry::IsToolsetClassRegistered(UUeremcpAnimationToolset::StaticClass()))
		{
			UE_LOG(LogUeremcpAnimation, Warning, TEXT("UUeremcpAnimationToolset registration failed."));
		}
	}

	FDelegateHandle OnPostEngineInitHandle;
};

IMPLEMENT_MODULE(FUeremcpAnimationModule, UeremcpAnimation)
