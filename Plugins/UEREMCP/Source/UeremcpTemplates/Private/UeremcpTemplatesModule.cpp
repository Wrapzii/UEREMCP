// UEREMCP — template module bootstrap. Owner: WS-15.

#include "Modules/ModuleManager.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"

#include "UeremcpTemplatesModule.h"
#include "UeremcpTemplateService.h"
#include "UeremcpTemplateStore.h"

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

private:
	TUniquePtr<FUeremcpTemplateStore> Store;
	TUniquePtr<FUeremcpTemplateService> Service;
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
}

IMPLEMENT_MODULE(FUeremcpTemplatesModule, UeremcpTemplates)
