#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Misc/CoreDelegates.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpReferenceToolset.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcp, Log, All);

/**
 * UEREMCP core editor module.
 *
 * RB-03 q4 answer: UToolsetDefinition subclasses do NOT self-register.
 * Explicit call required: UToolsetRegistry::RegisterToolsetClass
 * [VERIFIED: $TR/.../Public/ToolsetRegistry/UToolsetRegistry.h:28]
 * [VERIFIED: $TR/.../Private/ToolsetRegistry/ToolsetRegistrySubsystem.cpp:49
 *  — UAgentSkillToolset registered in subsystem Initialize the same way]
 *
 * Registration must wait until GEditor + UToolsetRegistrySubsystem exist.
 * StartupModule alone is too early (AIToolsetRegistrySubsystem unavailable).
 * [VERIFIED-RUNTIME: UnrealEditor-Cmd automation log 2026-07-30]
 */
class FUeremcpCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogUeremcp, Log, TEXT("UEREMCP core module started; deferring toolset registration."));
		// UE 5.8: OnPostEngineInit deprecated in favour of GetOnPostEngineInit()
		// [VERIFIED-RUNTIME: UeremcpCoreModule.cpp C4996 on UE 5.8.0-55116800]
		OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
			this, &FUeremcpCoreModule::RegisterReferenceToolset);
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
			UToolsetRegistry::UnregisterToolsetClass(UUeremcpReferenceToolset::StaticClass());
		}
		UE_LOG(LogUeremcp, Log, TEXT("UEREMCP core module shut down."));
	}

private:
	void RegisterReferenceToolset()
	{
		UToolsetRegistry::RegisterToolsetClass(UUeremcpReferenceToolset::StaticClass());

		if (UToolsetRegistry::IsToolsetClassRegistered(UUeremcpReferenceToolset::StaticClass()))
		{
			UE_LOG(LogUeremcp, Log, TEXT("UUeremcpReferenceToolset registered (PostEngineInit)."));
		}
		else
		{
			UE_LOG(LogUeremcp, Warning,
				TEXT("UUeremcpReferenceToolset registration failed at PostEngineInit. "
				     "Automation test UeremcpCore.ReferenceToolset.RegisterAndCaptureSchema "
				     "can re-check."));
		}
	}

	FDelegateHandle OnPostEngineInitHandle;
};

IMPLEMENT_MODULE(FUeremcpCoreModule, UeremcpCore)
