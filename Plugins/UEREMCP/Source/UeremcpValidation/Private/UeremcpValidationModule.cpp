// UEREMCP — validation module. Hosts tests and visual verification tools (WS-11).

#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpVisualCaptureToolset.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpValidation, Log, All);

class FUeremcpValidationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
			this, &FUeremcpValidationModule::RegisterToolsets);
		UE_LOG(LogUeremcpValidation, Log,
			TEXT("UEREMCP validation module started; registration deferred."));
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
			UToolsetRegistry::UnregisterToolsetClass(
				UUeremcpVisualCaptureToolset::StaticClass());
		}
		UE_LOG(LogUeremcpValidation, Log, TEXT("UEREMCP validation module shut down."));
	}

private:
	void RegisterToolsets()
	{
		// [VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Private/UeremcpCoreModule.cpp:31-53]
		UToolsetRegistry::RegisterToolsetClass(
			UUeremcpVisualCaptureToolset::StaticClass());
		const bool bRegistered = UToolsetRegistry::IsToolsetClassRegistered(
			UUeremcpVisualCaptureToolset::StaticClass());
		UE_LOG(LogUeremcpValidation, Log,
			TEXT("UUeremcpVisualCaptureToolset registration: %s"),
			bRegistered ? TEXT("registered") : TEXT("failed"));
	}

	FDelegateHandle OnPostEngineInitHandle;
};

IMPLEMENT_MODULE(FUeremcpValidationModule, UeremcpValidation)
