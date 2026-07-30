#include "UeremcpEnvironmentModule.h"

#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpEnvironmentPlanHandlers.h"
#include "UeremcpEnvironmentToolset.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpEnvironment, Log, All);

void FUeremcpEnvironmentModule::StartupModule()
{
	UE_LOG(LogUeremcpEnvironment, Log, TEXT("UEREMCP environment module started; deferring toolset registration."));
	OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
		this, &FUeremcpEnvironmentModule::RegisterToolsets);
}

void FUeremcpEnvironmentModule::ShutdownModule()
{
	FUeremcpEnvironmentPlanHandlers::Unregister();
	if (OnPostEngineInitHandle.IsValid())
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(OnPostEngineInitHandle);
		OnPostEngineInitHandle.Reset();
	}
	if (UObjectInitialized())
	{
		UToolsetRegistry::UnregisterToolsetClass(UUeremcpEnvironmentToolset::StaticClass());
	}
	UE_LOG(LogUeremcpEnvironment, Log, TEXT("UEREMCP environment module shut down."));
}

void FUeremcpEnvironmentModule::RegisterToolsets()
{
	UToolsetRegistry::RegisterToolsetClass(UUeremcpEnvironmentToolset::StaticClass());
	UE_LOG(LogUeremcpEnvironment, Log,
		TEXT("UUeremcpEnvironmentToolset registration: %s"),
		UToolsetRegistry::IsToolsetClassRegistered(UUeremcpEnvironmentToolset::StaticClass())
			? TEXT("ok")
			: TEXT("FAILED"));

	FString PlanError;
	if (FUeremcpEnvironmentPlanHandlers::Register(PlanError))
	{
		UE_LOG(LogUeremcpEnvironment, Log, TEXT("Environment plan actions registered with ExecutePlan."));
	}
	else
	{
		UE_LOG(LogUeremcpEnvironment, Warning,
			TEXT("Environment plan action registration failed: %s"), *PlanError);
	}
}

IMPLEMENT_MODULE(FUeremcpEnvironmentModule, UeremcpEnvironment)
