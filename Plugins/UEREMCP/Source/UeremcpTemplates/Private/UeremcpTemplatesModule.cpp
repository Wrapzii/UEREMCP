// UEREMCP — template module bootstrap. Owner: WS-15.

#include "UeremcpTemplatesModule.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

#include "UeremcpTemplateService.h"
#include "UeremcpTemplateStore.h"

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

		TArray<FString> Errors;
		Store->LoadFromDirectory(UeremcpTemplates::ResolveTemplatesDirectory(), Errors);
	}

	virtual void ShutdownModule() override
	{
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
			const FString RepoTemplates = FPaths::Combine(
				Plugin->GetBaseDir(),
				TEXT("../../../templates"));
			const FString Normalized = FPaths::ConvertRelativePathToFull(RepoTemplates);
			if (FPaths::DirectoryExists(Normalized))
			{
				return Normalized;
			}

			const FString Bundled = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/Templates"));
			if (FPaths::DirectoryExists(Bundled))
			{
				return Bundled;
			}
		}

		return FPaths::Combine(FPaths::ProjectDir(), TEXT("templates"));
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
