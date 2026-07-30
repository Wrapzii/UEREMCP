// UEREMCP — scratch-path helpers (WS-08).

#include "UeremcpMaterialPaths.h"

bool UeremcpMaterialPaths::IsUnderTestsRoot(const FString& SoftPackagePath)
{
	const FString Normalized = SoftPackagePath.StartsWith(TEXT("/")) ? SoftPackagePath : FString(TEXT("/")) + SoftPackagePath;
	return Normalized.StartsWith(TestsContentRoot) &&
		(Normalized.Len() == FCString::Strlen(TestsContentRoot) || Normalized[FCString::Strlen(TestsContentRoot)] == TEXT('/'));
}

bool UeremcpMaterialPaths::SplitPackagePath(const FString& SoftPackagePath, FString& OutFolder, FString& OutAssetName)
{
	if (SoftPackagePath.IsEmpty() || !SoftPackagePath.StartsWith(TEXT("/")))
	{
		return false;
	}

	FString Folder;
	FString AssetName;
	if (!SoftPackagePath.Split(
		TEXT("/"),
		&Folder,
		&AssetName,
		ESearchCase::CaseSensitive,
		ESearchDir::FromEnd))
	{
		return false;
	}

	if (Folder.IsEmpty() || AssetName.IsEmpty())
	{
		return false;
	}

	OutFolder = Folder;
	OutAssetName = AssetName;
	return true;
}

FString UeremcpMaterialPaths::JoinPackagePath(const FString& Folder, const FString& AssetName)
{
	if (Folder.EndsWith(TEXT("/")))
	{
		return Folder + AssetName;
	}
	return Folder + TEXT("/") + AssetName;
}
