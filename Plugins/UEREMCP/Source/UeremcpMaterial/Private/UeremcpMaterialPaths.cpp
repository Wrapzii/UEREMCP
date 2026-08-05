// UEREMCP — scratch-path helpers (WS-08).

#include "UeremcpMaterialPaths.h"

namespace
{
	static FString NormalizePackagePath(const FString& SoftPackagePath)
	{
		return SoftPackagePath.StartsWith(TEXT("/")) ? SoftPackagePath : FString(TEXT("/")) + SoftPackagePath;
	}

	static bool IsUnderRoot(const FString& Normalized, const TCHAR* Root)
	{
		const int32 RootLen = FCString::Strlen(Root);
		return Normalized.StartsWith(Root) &&
			(Normalized.Len() == RootLen || Normalized[RootLen] == TEXT('/'));
	}
}

bool UeremcpMaterialPaths::IsAllowedInspectPath(const FString& SoftPackagePath)
{
	return IsUnderRoot(NormalizePackagePath(SoftPackagePath), GameContentRoot);
}

bool UeremcpMaterialPaths::IsAllowedMutateCreatePath(const FString& SoftPackagePath)
{
	return IsUnderAllowedScratchRoot(SoftPackagePath);
}

bool UeremcpMaterialPaths::IsUnderTestsRoot(const FString& SoftPackagePath)
{
	return IsUnderRoot(NormalizePackagePath(SoftPackagePath), TestsContentRoot);
}

bool UeremcpMaterialPaths::IsUnderPocRoot(const FString& SoftPackagePath)
{
	return IsUnderRoot(NormalizePackagePath(SoftPackagePath), PocContentRoot);
}

bool UeremcpMaterialPaths::IsUnderAllowedScratchRoot(const FString& SoftPackagePath)
{
	return !ResolveScratchContentRoot(SoftPackagePath).IsEmpty();
}

FString UeremcpMaterialPaths::ResolveScratchContentRoot(const FString& SoftPackagePath)
{
	const FString Normalized = NormalizePackagePath(SoftPackagePath);
	if (IsUnderRoot(Normalized, TestsContentRoot))
	{
		return TestsContentRoot;
	}
	if (IsUnderRoot(Normalized, PocContentRoot))
	{
		return PocContentRoot;
	}
	return FString();
}

FString UeremcpMaterialPaths::MaterialsFolderForContentRoot(const FString& ScratchContentRoot)
{
	return JoinPackagePath(ScratchContentRoot, TEXT("Materials"));
}

FString UeremcpMaterialPaths::MastersFolderForContentRoot(const FString& ScratchContentRoot)
{
	return JoinPackagePath(MaterialsFolderForContentRoot(ScratchContentRoot), TEXT("Masters"));
}

FString UeremcpMaterialPaths::TexturesFolderForContentRoot(const FString& ScratchContentRoot)
{
	return JoinPackagePath(ScratchContentRoot, TEXT("Textures"));
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
