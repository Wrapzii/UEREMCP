#include "UeremcpUIModule.h"

#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpUIPlanHandlers.h"
#include "UeremcpUIToolset.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpUI, Log, All);

void FUeremcpUIModule::StartupModule()
{
	UE_LOG(LogUeremcpUI, Log, TEXT("UEREMCP UI module started; deferring toolset registration."));
	OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
		this, &FUeremcpUIModule::RegisterToolsets);
}

void FUeremcpUIModule::ShutdownModule()
{
	FUeremcpUIPlanHandlers::Unregister();
	if (OnPostEngineInitHandle.IsValid())
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(OnPostEngineInitHandle);
		OnPostEngineInitHandle.Reset();
	}
	if (UObjectInitialized())
	{
		UToolsetRegistry::UnregisterToolsetClass(UUeremcpUIToolset::StaticClass());
	}
	UE_LOG(LogUeremcpUI, Log, TEXT("UEREMCP UI module shut down."));
}

void FUeremcpUIModule::RegisterToolsets()
{
	UToolsetRegistry::RegisterToolsetClass(UUeremcpUIToolset::StaticClass());
	UE_LOG(LogUeremcpUI, Log,
		TEXT("UUeremcpUIToolset registration: %s"),
		UToolsetRegistry::IsToolsetClassRegistered(UUeremcpUIToolset::StaticClass())
			? TEXT("ok")
			: TEXT("FAILED"));

	FString PlanError;
	if (FUeremcpUIPlanHandlers::Register(PlanError))
	{
		UE_LOG(LogUeremcpUI, Log, TEXT("UI plan actions registered with ExecutePlan."));
	}
	else
	{
		UE_LOG(LogUeremcpUI, Warning,
			TEXT("UI plan action registration failed: %s"), *PlanError);
	}
}

IMPLEMENT_MODULE(FUeremcpUIModule, UeremcpUI)
