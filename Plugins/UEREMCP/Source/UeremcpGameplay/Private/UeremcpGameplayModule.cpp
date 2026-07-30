#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpGameplayToolset.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpGameplay, Log, All);

class FUeremcpGameplayModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Toolset classes require explicit post-engine-init registration.
		// [VERIFIED-RUNTIME: UeremcpCoreModule.cpp registration path, 2026-07-30]
		OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
			this,
			&FUeremcpGameplayModule::RegisterToolset);
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
			UToolsetRegistry::UnregisterToolsetClass(UUeremcpGameplayToolset::StaticClass());
		}
	}

private:
	void RegisterToolset()
	{
		UToolsetRegistry::RegisterToolsetClass(UUeremcpGameplayToolset::StaticClass());
		UE_LOG(
			LogUeremcpGameplay,
			Log,
			TEXT("UEREMCP Gameplay create_spell preflight toolset registered."));
	}

	FDelegateHandle OnPostEngineInitHandle;
};

IMPLEMENT_MODULE(FUeremcpGameplayModule, UeremcpGameplay)
