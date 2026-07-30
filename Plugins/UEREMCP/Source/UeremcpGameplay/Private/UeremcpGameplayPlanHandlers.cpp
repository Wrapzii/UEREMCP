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

	bool DispatchCreateSpellVariation(
		const FString& RequestJson,
		FString& OutResponseJson,
		FString& OutError)
	{
		OutError.Reset();
		OutResponseJson = UUeremcpGameplayToolset::CreateSpellVariation(RequestJson);
		if (OutResponseJson.IsEmpty())
		{
			OutError = TEXT("create_spell_variation returned an empty response");
			return false;
		}
		return true;
	}
}

const TCHAR* FUeremcpGameplayPlanHandlers::RegisteredActionName()
{
	return TEXT("create_spell");
}

const TCHAR* FUeremcpGameplayPlanHandlers::RegisteredVariationActionName()
{
	return TEXT("create_spell_variation");
}

bool FUeremcpGameplayPlanHandlers::Register(FString& OutError)
{
	const bool bSpellRegistered = FUeremcpPlanExecutor::RegisterAction(
		RegisteredActionName(),
		DispatchCreateSpell,
		OutError);
	if (!bSpellRegistered)
	{
		return false;
	}
	if (!FUeremcpPlanExecutor::RegisterAction(
		RegisteredVariationActionName(),
		DispatchCreateSpellVariation,
		OutError))
	{
		FUeremcpPlanExecutor::UnregisterAction(RegisteredActionName());
		return false;
	}
	OutError.Reset();
	return true;
}

void FUeremcpGameplayPlanHandlers::Unregister()
{
	FUeremcpPlanExecutor::UnregisterAction(RegisteredActionName());
	FUeremcpPlanExecutor::UnregisterAction(RegisteredVariationActionName());
}
