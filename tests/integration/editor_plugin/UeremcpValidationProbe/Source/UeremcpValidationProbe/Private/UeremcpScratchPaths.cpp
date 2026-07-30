#include "UeremcpScratchPaths.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

FString UeremcpGetTestsFilesystemRoot()
{
	FString Filename;
	if (!FPackageName::TryConvertLongPackageNameToFilename(
			FString(UeremcpTestsContentRoot) + TEXT("/"), Filename, FString()))
	{
		return FString();
	}
	return FPaths::ConvertRelativePathToFull(Filename);
}

FString UeremcpMakeScratchPackagePath(const FString& Suite, const FString& Name)
{
	check(!Name.IsEmpty());
	if (Suite.IsEmpty())
	{
		return FString::Printf(TEXT("%s/%s"), UeremcpTestsContentRoot, *Name);
	}
	return FString::Printf(TEXT("%s/%s/%s"), UeremcpTestsContentRoot, *Suite, *Name);
}

int32 UeremcpCleanupScratchSuite(const FString& Suite)
{
	const FString Folder = Suite.IsEmpty()
		? FString(UeremcpTestsContentRoot)
		: FString::Printf(TEXT("%s/%s"), UeremcpTestsContentRoot, *Suite);

	// Hard safety: never allow cleanup outside the tests root.
	if (!Folder.StartsWith(UeremcpTestsContentRoot))
	{
		UE_LOG(LogTemp, Error, TEXT("UeremcpCleanupScratchSuite refused non-tests path: %s"), *Folder);
		return 0;
	}

	TArray<FString> AssetPaths = UEditorAssetLibrary::ListAssets(Folder, /*bRecursive=*/true, /*bIncludeFolder=*/false);
	int32 Deleted = 0;
	for (const FString& Path : AssetPaths)
	{
		if (!Path.StartsWith(UeremcpTestsContentRoot))
		{
			UE_LOG(LogTemp, Error, TEXT("UeremcpCleanupScratchSuite skipped out-of-root asset: %s"), *Path);
			continue;
		}
		if (UEditorAssetLibrary::DoesAssetExist(Path) && UEditorAssetLibrary::DeleteAsset(Path))
		{
			++Deleted;
		}
	}

	// Also remove empty on-disk directories left behind under Content/__UeremcpTests.
	const FString FsRoot = UeremcpGetTestsFilesystemRoot();
	if (!FsRoot.IsEmpty())
	{
		const FString FsSuite = Suite.IsEmpty() ? FsRoot : FPaths::Combine(FsRoot, Suite);
		IFileManager::Get().DeleteDirectory(*FsSuite, /*RequireExists=*/false, /*Tree=*/true);
	}

	return Deleted;
}
