// UEREMCP — execute_plan semantic handler for create_spell (WS-09).

#include "UeremcpGameplayPlanHandlers.h"

#include "UeremcpGameplayToolset.h"
#include "UeremcpPlanExecutor.h"

namespace
{
	bool DispatchCreateSpell(
		const FString& RequestJson,
		FString& OutResponseJson,
		FString& OutError)
	{
		OutError.Reset();
		OutResponseJson = UUeremcpGameplayToolset::CreateSpell(RequestJson);
		if (OutResponseJson.IsEmpty())
		{
			OutError = TEXT("create_spell returned an empty response");
			return false;
		}
		return true;
	}
}

const TCHAR* FUeremcpGameplayPlanHandlers::RegisteredActionName()
{
	return TEXT("create_spell");
}

bool FUeremcpGameplayPlanHandlers::Register(FString& OutError)
{
	const bool bRegistered = FUeremcpPlanExecutor::RegisterAction(
		RegisteredActionName(),
		DispatchCreateSpell,
		OutError);
	if (bRegistered)
	{
		OutError.Reset();
	}
	return bRegistered;
}

void FUeremcpGameplayPlanHandlers::Unregister()
{
	FUeremcpPlanExecutor::UnregisterAction(RegisteredActionName());
}
