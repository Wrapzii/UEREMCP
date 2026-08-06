#include "UeremcpVoxelModule.h"

#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpVoxelPlanHandlers.h"
#include "UeremcpVoxelToolset.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpVoxel, Log, All);

void FUeremcpVoxelModule::StartupModule()
{
	UE_LOG(LogUeremcpVoxel, Log, TEXT("UEREMCP Voxel module started; deferring toolset registration."));
	OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
		this, &FUeremcpVoxelModule::RegisterToolsets);
}

void FUeremcpVoxelModule::ShutdownModule()
{
	FUeremcpVoxelPlanHandlers::Unregister();
	if (OnPostEngineInitHandle.IsValid())
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(OnPostEngineInitHandle);
		OnPostEngineInitHandle.Reset();
	}
	if (UObjectInitialized())
	{
		UToolsetRegistry::UnregisterToolsetClass(UUeremcpVoxelToolset::StaticClass());
	}
	UE_LOG(LogUeremcpVoxel, Log, TEXT("UEREMCP Voxel module shut down."));
}

void FUeremcpVoxelModule::RegisterToolsets()
{
	UToolsetRegistry::RegisterToolsetClass(UUeremcpVoxelToolset::StaticClass());
	UE_LOG(LogUeremcpVoxel, Log,
		TEXT("UUeremcpVoxelToolset registration: %s"),
		UToolsetRegistry::IsToolsetClassRegistered(UUeremcpVoxelToolset::StaticClass())
			? TEXT("ok")
			: TEXT("FAILED"));

	FString PlanError;
	if (FUeremcpVoxelPlanHandlers::Register(PlanError))
	{
		UE_LOG(LogUeremcpVoxel, Log, TEXT("Voxel plan actions registered with ExecutePlan."));
	}
	else
	{
		UE_LOG(LogUeremcpVoxel, Warning, TEXT("Voxel plan action registration failed: %s"), *PlanError);
	}
}

IMPLEMENT_MODULE(FUeremcpVoxelModule, UeremcpVoxel)
