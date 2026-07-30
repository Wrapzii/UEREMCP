// UEREMCP — execute_plan semantic handler for create_vfx_material (WS-08).

#include "UeremcpMaterialPlanHandlers.h"

#include "UeremcpMaterialToolset.h"
#include "UeremcpPlanExecutor.h"

namespace
{
	bool DispatchCreateVfxMaterial(
		const FString& RequestJson,
		FString& OutResponseJson,
		FString& OutError)
	{
		OutError.Reset();
		OutResponseJson = UUeremcpMaterialToolset::CreateVfxMaterial(RequestJson);
		if (OutResponseJson.IsEmpty())
		{
			OutError = TEXT("create_vfx_material returned an empty response");
			return false;
		}
		return true;
	}
}

const TCHAR* FUeremcpMaterialPlanHandlers::RegisteredActionName()
{
	return TEXT("create_vfx_material");
}

bool FUeremcpMaterialPlanHandlers::Register(FString& OutError)
{
	const bool bRegistered = FUeremcpPlanExecutor::RegisterAction(
		RegisteredActionName(),
		DispatchCreateVfxMaterial,
		OutError);
	if (bRegistered)
	{
		OutError.Reset();
	}
	return bRegistered;
}

void FUeremcpMaterialPlanHandlers::Unregister()
{
	FUeremcpPlanExecutor::UnregisterAction(RegisteredActionName());
}
