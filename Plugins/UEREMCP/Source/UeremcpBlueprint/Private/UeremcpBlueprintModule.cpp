#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Misc/CoreDelegates.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpBlueprintToolset.h"
#include "UeremcpBlueprintPlanHandlers.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpBlueprint, Log, All);

/**
 * UEREMCP Blueprint editor module (WS-06).
 *
 * Registers UUeremcpBlueprintToolset after PostEngineInit — same pattern as UeremcpCore
 * [VERIFIED: UeremcpCoreModule.cpp].
 */
class FUeremcpBlueprintModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogUeremcpBlueprint, Log, TEXT("UEREMCP Blueprint module started; deferring toolset registration."));
		OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
			this, &FUeremcpBlueprintModule::RegisterBlueprintToolset);
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
			UToolsetRegistry::UnregisterToolsetClass(UUeremcpBlueprintToolset::StaticClass());
		}
		UE_LOG(LogUeremcpBlueprint, Log, TEXT("UEREMCP Blueprint module shut down."));
	}

private:
	void RegisterBlueprintToolset()
	{
		UToolsetRegistry::RegisterToolsetClass(UUeremcpBlueprintToolset::StaticClass());

		if (UToolsetRegistry::IsToolsetClassRegistered(UUeremcpBlueprintToolset::StaticClass()))
		{
			UE_LOG(LogUeremcpBlueprint, Log, TEXT("UUeremcpBlueprintToolset registered (PostEngineInit)."));
		}
		else
		{
			UE_LOG(LogUeremcpBlueprint, Warning,
				TEXT("UUeremcpBlueprintToolset registration failed at PostEngineInit."));
		}

		FString PlanError;
		if (FUeremcpBlueprintPlanHandlers::Register(PlanError))
		{
			UE_LOG(LogUeremcpBlueprint, Log, TEXT("Blueprint plan actions registered with ExecutePlan."));
		}
		else
		{
			UE_LOG(LogUeremcpBlueprint, Warning,
				TEXT("Blueprint plan action registration failed: %s"), *PlanError);
		}
	}

	FDelegateHandle OnPostEngineInitHandle;
};

IMPLEMENT_MODULE(FUeremcpBlueprintModule, UeremcpBlueprint)
