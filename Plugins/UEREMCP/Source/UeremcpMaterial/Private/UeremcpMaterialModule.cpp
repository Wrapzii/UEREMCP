// UEREMCP — Material domain module (WS-08).

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Misc/CoreDelegates.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpMaterialToolset.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpMaterial, Log, All);

class FUeremcpMaterialModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogUeremcpMaterial, Log, TEXT("UEREMCP Material module started; deferring toolset registration."));
		OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
			this, &FUeremcpMaterialModule::RegisterMaterialToolset);
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
			UToolsetRegistry::UnregisterToolsetClass(UUeremcpMaterialToolset::StaticClass());
		}
		UE_LOG(LogUeremcpMaterial, Log, TEXT("UEREMCP Material module shut down."));
	}

private:
	void RegisterMaterialToolset()
	{
		// Domain toolsets register after engine initialization.
		// [VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Private/UeremcpCoreModule.cpp:31-53]
		UToolsetRegistry::RegisterToolsetClass(UUeremcpMaterialToolset::StaticClass());

		if (UToolsetRegistry::IsToolsetClassRegistered(UUeremcpMaterialToolset::StaticClass()))
		{
			UE_LOG(LogUeremcpMaterial, Log, TEXT("UUeremcpMaterialToolset registered (PostEngineInit)."));
		}
		else
		{
			UE_LOG(LogUeremcpMaterial, Warning,
				TEXT("UUeremcpMaterialToolset registration failed at PostEngineInit."));
		}
	}

	FDelegateHandle OnPostEngineInitHandle;
};

IMPLEMENT_MODULE(FUeremcpMaterialModule, UeremcpMaterial)
