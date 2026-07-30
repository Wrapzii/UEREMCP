// UEREMCP — execute_plan semantic handler for create_niagara_effect (WS-07).

#include "UeremcpNiagaraPlanHandlers.h"

#include "UeremcpNiagaraToolset.h"
#include "UeremcpPlanExecutor.h"

namespace
{
	bool DispatchCreateNiagaraEffect(
		const FString& RequestJson,
		FString& OutResponseJson,
		FString& OutError)
	{
		OutError.Reset();
		OutResponseJson = UUeremcpNiagaraToolset::CreateNiagaraEffect(RequestJson);
		if (OutResponseJson.IsEmpty())
		{
			OutError = TEXT("create_niagara_effect returned an empty response");
			return false;
		}
		return true;
	}
}

const TCHAR* FUeremcpNiagaraPlanHandlers::RegisteredActionName()
{
	return TEXT("create_niagara_effect");
}

bool FUeremcpNiagaraPlanHandlers::Register(FString& OutError)
{
	const bool bRegistered = FUeremcpPlanExecutor::RegisterAction(
		RegisteredActionName(),
		DispatchCreateNiagaraEffect,
		OutError);
	if (bRegistered)
	{
		OutError.Reset();
	}
	return bRegistered;
}

void FUeremcpNiagaraPlanHandlers::Unregister()
{
	FUeremcpPlanExecutor::UnregisterAction(RegisteredActionName());
}
