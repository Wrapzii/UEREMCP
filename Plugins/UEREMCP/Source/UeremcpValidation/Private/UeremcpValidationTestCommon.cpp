#include "UeremcpValidationTestCommon.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Curves/CurveFloat.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ToolsetRegistry/SandboxLibrary.h"
#include "UeremcpScratchPaths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace UeremcpValidationTests
{
	using namespace UE::ToolsetRegistry;
	using namespace UE::FileSandboxCore;

	void EnsureSandboxInactive(FAutomationTestBase& Test)
	{
		if (FGlobalSandbox::IsActive())
		{
			Test.AddWarning(FString::Printf(
				TEXT("Sandbox already active (%s); Discard+Leave before test"),
				*FGlobalSandbox::GetActiveName()));
			FGlobalSandbox::Discard();
			FGlobalSandbox::Leave();
		}
	}

	FString PackageToFilesystemPath(const FString& LongPackageName)
	{
		FString Filename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(
				LongPackageName, Filename, FPackageName::GetAssetPackageExtension()))
		{
			return FString();
		}
		return FPaths::ConvertRelativePathToFull(Filename);
	}

	bool RealDiskFileExists(const FString& LongPackageName)
	{
		const FString Path = PackageToFilesystemPath(LongPackageName);
		return !Path.IsEmpty() && IFileManager::Get().FileExists(*Path);
	}

	FString CreateAndSaveScratchCurve(const FString& Suite, const FString& AssetName, FAutomationTestBase& Test)
	{
		const FString PackagePath = UeremcpMakeScratchPackagePath(Suite, AssetName);
		const FString AssetSoftPath = PackagePath + TEXT(".") + AssetName;

		UPackage* Package = CreatePackage(*PackagePath);
		if (!Test.TestNotNull(TEXT("CreatePackage"), Package))
		{
			return FString();
		}
		Package->FullyLoad();

		UCurveFloat* Asset = NewObject<UCurveFloat>(Package, *AssetName, RF_Public | RF_Standalone);
		if (!Test.TestNotNull(TEXT("NewObject<UCurveFloat>"), Asset))
		{
			return FString();
		}

		FAssetRegistryModule::AssetCreated(Asset);
		Package->MarkPackageDirty();

		const FString Filename = PackageToFilesystemPath(PackagePath);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GWarn;
		const FSavePackageResultStruct Result = UPackage::Save(Package, Asset, *Filename, SaveArgs);
		if (!Test.TestTrue(TEXT("UPackage::Save succeeded"), Result.Result == ESavePackageResult::Success))
		{
			return FString();
		}
		return AssetSoftPath;
	}

	bool AssetRegistryHasPackage(const FString& LongPackageName)
	{
		IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		AR.GetAssetsByPackageName(FName(*LongPackageName), Assets, /*bIncludeOnlyOnDiskAssets=*/true);
		if (Assets.Num() > 0)
		{
			return true;
		}
		AR.GetAssetsByPackageName(FName(*LongPackageName), Assets, /*bIncludeOnlyOnDiskAssets=*/false);
		return Assets.Num() > 0;
	}

	bool DiscardAndLeaveSandbox(FAutomationTestBase& Test)
	{
		if (!Test.TestTrue(TEXT("FGlobalSandbox::Discard"), FGlobalSandbox::Discard()))
		{
			FGlobalSandbox::Leave();
			return false;
		}
		return Test.TestTrue(TEXT("FGlobalSandbox::Leave"), FGlobalSandbox::Leave());
	}

	bool AssertPackagesFullyGone(
		const TArray<FString>& PackagePaths,
		const TArray<FString>& SoftPaths,
		FAutomationTestBase& Test,
		const TCHAR* ContextLabel)
	{
		bool bClean = true;
		for (int32 i = 0; i < PackagePaths.Num(); ++i)
		{
			const FString& PackagePath = PackagePaths[i];
			const FString& Soft = SoftPaths[i];
			const FString Fs = PackageToFilesystemPath(PackagePath);

			if (!Fs.IsEmpty() && IFileManager::Get().FileExists(*Fs))
			{
				Test.AddError(FString::Printf(TEXT("%s: real disk file still exists: %s"), ContextLabel, *Fs));
				bClean = false;
			}
			if (UEditorAssetLibrary::DoesAssetExist(Soft))
			{
				Test.AddError(FString::Printf(TEXT("%s: EditorAssetLibrary still reports asset: %s"), ContextLabel, *Soft));
				bClean = false;
			}
			if (AssetRegistryHasPackage(PackagePath))
			{
				Test.AddError(FString::Printf(TEXT("%s: AssetRegistry still has package: %s"), ContextLabel, *PackagePath));
				bClean = false;
			}
			if (FindPackage(nullptr, *PackagePath) != nullptr)
			{
				Test.AddError(FString::Printf(TEXT("%s: FindPackage still returns UPackage: %s"), ContextLabel, *PackagePath));
				bClean = false;
			}
		}
		return bClean;
	}

	bool AssertAssetFullyPresent(const FString& PackagePath, const FString& SoftPath, FAutomationTestBase& Test)
	{
		bool bOk = true;
		if (!RealDiskFileExists(PackagePath))
		{
			Test.AddError(FString::Printf(TEXT("disk file missing: %s"), *PackagePath));
			bOk = false;
		}
		if (!UEditorAssetLibrary::DoesAssetExist(SoftPath))
		{
			Test.AddError(FString::Printf(TEXT("EditorAssetLibrary missing: %s"), *SoftPath));
			bOk = false;
		}
		if (!AssetRegistryHasPackage(PackagePath))
		{
			Test.AddError(FString::Printf(TEXT("AssetRegistry missing: %s"), *PackagePath));
			bOk = false;
		}
		return bOk;
	}

	bool ReadFileBytes(const FString& Path, TArray<uint8>& OutBytes, FAutomationTestBase& Test)
	{
		OutBytes.Reset();
		if (!IFileManager::Get().FileExists(*Path))
		{
			Test.AddError(FString::Printf(TEXT("file does not exist: %s"), *Path));
			return false;
		}
		if (!FFileHelper::LoadFileToArray(OutBytes, *Path))
		{
			Test.AddError(FString::Printf(TEXT("failed to read file: %s"), *Path));
			return false;
		}
		return true;
	}
}
