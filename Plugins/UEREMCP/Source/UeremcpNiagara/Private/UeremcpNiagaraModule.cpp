// UEREMCP — Niagara domain module (WS-07).

#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Misc/CoreDelegates.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpNiagaraPlanHandlers.h"
#include "UeremcpNiagaraToolset.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpNiagara, Log, All);

class FUeremcpNiagaraModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogUeremcpNiagara, Log, TEXT("UEREMCP Niagara module started; deferring toolset registration."));
		OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
			this, &FUeremcpNiagaraModule::RegisterNiagaraToolset);
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
			FUeremcpNiagaraPlanHandlers::Unregister();
			UToolsetRegistry::UnregisterToolsetClass(UUeremcpNiagaraToolset::StaticClass());
		}
		UE_LOG(LogUeremcpNiagara, Log, TEXT("UEREMCP Niagara module shut down."));
	}

private:
	void RegisterNiagaraToolset()
	{
		// Domain toolsets register after engine initialization.
		// [VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Private/UeremcpCoreModule.cpp:31-53]
		UToolsetRegistry::RegisterToolsetClass(UUeremcpNiagaraToolset::StaticClass());

		if (UToolsetRegistry::IsToolsetClassRegistered(UUeremcpNiagaraToolset::StaticClass()))
		{
			UE_LOG(LogUeremcpNiagara, Log, TEXT("UUeremcpNiagaraToolset registered (PostEngineInit)."));
		}
		else
		{
			UE_LOG(LogUeremcpNiagara, Warning,
				TEXT("UUeremcpNiagaraToolset registration failed at PostEngineInit."));
		}

		FString PlanHandlerError;
		if (FUeremcpNiagaraPlanHandlers::Register(PlanHandlerError))
		{
			UE_LOG(LogUeremcpNiagara, Log,
				TEXT("Niagara execute_plan handler registered (create_niagara_effect)."));
		}
		else
		{
			UE_LOG(LogUeremcpNiagara, Warning,
				TEXT("Niagara execute_plan handler registration failed: %s"), *PlanHandlerError);
		}
	}

	FDelegateHandle OnPostEngineInitHandle;
};

IMPLEMENT_MODULE(FUeremcpNiagaraModule, UeremcpNiagara)
