#include "Modules/ModuleManager.h"
#include "Modules/ModuleInterface.h"
#include "Misc/CoreDelegates.h"
#include "Containers/Ticker.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpPlanTransactionCoordinator.h"
#include "UeremcpReferenceToolset.h"
#include "UeremcpSchemaPublishing.h"

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
		FUeremcpPlanTransactionCoordinator::UnregisterFromExecutor();

		if (SchemaPublishTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(SchemaPublishTickerHandle);
			SchemaPublishTickerHandle.Reset();
		}

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

		// Publish nested schemas after sibling Ueremcp* modules finish their
		// PostEngineInit RegisterToolsetClass calls (next tick).
		SchemaPublishTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FUeremcpCoreModule::PublishSchemasNextTick),
			0.0f);

		FString PlanTransactionError;
		if (FUeremcpPlanTransactionCoordinator::RegisterWithExecutor(PlanTransactionError))
		{
			UE_LOG(LogUeremcp, Log, TEXT("execute_plan transaction callbacks registered."));
		}
		else
		{
			UE_LOG(LogUeremcp, Warning,
				TEXT("execute_plan transaction callback registration failed: %s"),
				*PlanTransactionError);
		}
	}

	bool PublishSchemasNextTick(float)
	{
		SchemaPublishTickerHandle.Reset();
		const int32 Wrapped = UeremcpSchemaPublishing::PublishNestedSchemasForAllUeremcpToolsets();
		UE_LOG(LogUeremcp, Log,
			TEXT("Nested envelope schema publishing wrapped %d Ueremcp toolset(s) (BACKLOG 1.2a)."),
			Wrapped);
		return false; // one-shot
	}

	FDelegateHandle OnPostEngineInitHandle;
	FTSTicker::FDelegateHandle SchemaPublishTickerHandle;
};

IMPLEMENT_MODULE(FUeremcpCoreModule, UeremcpCore)
