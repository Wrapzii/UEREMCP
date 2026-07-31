// UEREMCP — template module bootstrap. Owner: WS-15.

#include "UeremcpTemplatesModule.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UeremcpPlanExecutor.h"
#include "UeremcpTemplateService.h"
#include "UeremcpTemplateStore.h"
#include "UeremcpTemplatesToolset.h"

DEFINE_LOG_CATEGORY_STATIC(LogUeremcpTemplates, Log, All);

class FUeremcpTemplatesModule;

namespace
{
	FUeremcpTemplatesModule* GTemplatesModule = nullptr;
}

class FUeremcpTemplatesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		GTemplatesModule = this;
		Store = MakeUnique<FUeremcpTemplateStore>();
		Service = MakeUnique<FUeremcpTemplateService>(*Store);
		UeremcpTemplates::SetExecutePlanDelegate(&FUeremcpPlanExecutor::ExecuteRequest);

		TArray<FString> Errors;
		Store->LoadFromDirectory(UeremcpTemplates::ResolveTemplatesDirectory(), Errors);
		TArray<FString> AgentErrors;
		const FString AgentDir = UeremcpTemplates::ResolveAgentTemplatesDirectory();
		IFileManager::Get().MakeDirectory(*AgentDir, true);
		Store->MergeFromDirectory(AgentDir, AgentErrors);
		Errors.Append(AgentErrors);
		for (const FString& Err : Errors)
		{
			UE_LOG(LogUeremcpTemplates, Warning, TEXT("template load: %s"), *Err);
		}

		OnPostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(
			this, &FUeremcpTemplatesModule::RegisterTemplatesToolset);
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
			UToolsetRegistry::UnregisterToolsetClass(UUeremcpTemplatesToolset::StaticClass());
		}

		UeremcpTemplates::ClearExecutePlanDelegate();
		Service.Reset();
		Store.Reset();
		GTemplatesModule = nullptr;
	}

	FUeremcpTemplateStore& GetStore() { return *Store; }
	FUeremcpTemplateService& GetService() { return *Service; }
	void SetExecutePlanDelegate(FUeremcpExecutePlanDelegate InDelegate)
	{
		ExecutePlanDelegate = MoveTemp(InDelegate);
	}
	void ClearExecutePlanDelegate()
	{
		ExecutePlanDelegate = FUeremcpExecutePlanDelegate();
	}
	bool ExecutePlan(const FString& RequestJson, FString& OutResponseJson, FString& OutError)
	{
		if (!ExecutePlanDelegate)
		{
			OutError = TEXT("execute_plan executor is not registered by UeremcpProtocol.");
			return false;
		}
		return ExecutePlanDelegate(RequestJson, OutResponseJson, OutError);
	}

private:
	void RegisterTemplatesToolset()
	{
		// Domain toolsets register after engine initialization.
		// [VERIFIED: Plugins/UEREMCP/Source/UeremcpCore/Private/UeremcpCoreModule.cpp:31-53]
		UToolsetRegistry::RegisterToolsetClass(UUeremcpTemplatesToolset::StaticClass());

		if (UToolsetRegistry::IsToolsetClassRegistered(UUeremcpTemplatesToolset::StaticClass()))
		{
			UE_LOG(LogUeremcpTemplates, Log, TEXT("UUeremcpTemplatesToolset registered (PostEngineInit)."));
		}
		else
		{
			UE_LOG(LogUeremcpTemplates, Warning,
				TEXT("UUeremcpTemplatesToolset registration failed at PostEngineInit."));
		}
	}

	FDelegateHandle OnPostEngineInitHandle;
	TUniquePtr<FUeremcpTemplateStore> Store;
	TUniquePtr<FUeremcpTemplateService> Service;
	FUeremcpExecutePlanDelegate ExecutePlanDelegate;
};

namespace UeremcpTemplates
{
	FString ResolveTemplatesDirectory()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UEREMCP"));
		if (Plugin.IsValid())
		{
			const TArray<FString> Candidates = {
				FPaths::Combine(
					Plugin->GetBaseDir(),
					TEXT("Source/UeremcpTemplates/Resources/Templates")),
				FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Templates")),
				FPaths::Combine(Plugin->GetBaseDir(), TEXT("../../templates")),
			};
			for (const FString& Candidate : Candidates)
			{
				const FString Normalized = FPaths::ConvertRelativePathToFull(Candidate);
				if (FPaths::DirectoryExists(Normalized))
				{
					return Normalized;
				}
			}
		}

		return FPaths::Combine(FPaths::ProjectDir(), TEXT("templates"));
	}

	FString ResolveAgentTemplatesDirectory()
	{
		return FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UEREMCP/Templates/agent")));
	}

	FUeremcpTemplateStore& GetStore()
	{
		check(GTemplatesModule);
		return GTemplatesModule->GetStore();
	}

	FUeremcpTemplateService& GetService()
	{
		check(GTemplatesModule);
		return GTemplatesModule->GetService();
	}

	void SetExecutePlanDelegate(FUeremcpExecutePlanDelegate InDelegate)
	{
		check(GTemplatesModule);
		GTemplatesModule->SetExecutePlanDelegate(MoveTemp(InDelegate));
	}

	void ClearExecutePlanDelegate()
	{
		if (GTemplatesModule)
		{
			GTemplatesModule->ClearExecutePlanDelegate();
		}
	}

	bool ExecutePlan(
		const FString& RequestJson,
		FString& OutResponseJson,
		FString& OutError)
	{
		if (!GTemplatesModule)
		{
			OutError = TEXT("UeremcpTemplates module is not initialized.");
			return false;
		}
		return GTemplatesModule->ExecutePlan(RequestJson, OutResponseJson, OutError);
	}
}

IMPLEMENT_MODULE(FUeremcpTemplatesModule, UeremcpTemplates)
