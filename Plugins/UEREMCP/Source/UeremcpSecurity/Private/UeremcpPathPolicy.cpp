#include "UeremcpPathPolicy.h"

#include "Misc/Paths.h"

namespace UeremcpPathPolicyPrivate
{
	static FString NormaliseDir(const FString& Dir)
	{
		FString Normalised = FPaths::ConvertRelativePathToFull(Dir);
		FPaths::NormalizeDirectoryName(Normalised);
		if (!Normalised.EndsWith(TEXT("/")) && !Normalised.EndsWith(TEXT("\\")))
		{
			Normalised.AppendChar(TEXT('/'));
		}
		return Normalised;
	}

	static bool IsUnderRoot(const FString& Candidate, const FString& Root)
	{
		if (Root.IsEmpty())
		{
			return false;
		}
		const FString NormalCandidate = NormaliseDir(Candidate);
		const FString NormalRoot = NormaliseDir(Root);
		return NormalCandidate.StartsWith(NormalRoot, ESearchCase::IgnoreCase);
	}

	static FUeremcpPathPolicyRoots ResolveRoots(const FUeremcpPathPolicyRoots* Roots)
	{
		if (Roots && Roots->IsConfigured())
		{
			return *Roots;
		}
		return FUeremcpPathPolicy::RootsFromProject();
	}
}

FUeremcpPathPolicyRoots FUeremcpPathPolicy::RootsFromProject()
{
	FUeremcpPathPolicyRoots Roots;
	Roots.ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	Roots.ProjectContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
	Roots.ProjectSavedDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	return Roots;
}

FString FUeremcpPathPolicy::SavedUeremcpRoot(const FUeremcpPathPolicyRoots& Roots)
{
	return FPaths::Combine(Roots.ProjectSavedDir, TEXT("UEREMCP"));
}

bool FUeremcpPathPolicy::ContainsTraversalSegment(const FString& Path)
{
	auto HasDotDot = [](const FString& SegmentPath, const TCHAR* Delim) -> bool
	{
		TArray<FString> Segments;
		SegmentPath.ParseIntoArray(Segments, Delim, true);
		for (const FString& Segment : Segments)
		{
			if (Segment == TEXT(".."))
			{
				return true;
			}
		}
		return false;
	};

	return HasDotDot(Path, TEXT("/")) || HasDotDot(Path, TEXT("\\"));
}

bool FUeremcpPathPolicy::IsEngineSoftPath(const FString& SoftPath)
{
	return SoftPath.StartsWith(TEXT("/Engine/"), ESearchCase::IgnoreCase)
		|| SoftPath.Equals(TEXT("/Engine"), ESearchCase::IgnoreCase);
}

bool FUeremcpPathPolicy::IsTempSoftPath(const FString& SoftPath)
{
	return SoftPath.StartsWith(TEXT("/Temp/"), ESearchCase::IgnoreCase)
		|| SoftPath.Equals(TEXT("/Temp"), ESearchCase::IgnoreCase);
}

FUeremcpPathValidationResult FUeremcpPathPolicy::ValidateSoftPath(
	const FString& SoftPath,
	bool bForWrite,
	const FUeremcpPathPolicyRoots* Roots)
{
	(void)Roots;

	if (SoftPath.IsEmpty())
	{
		return FUeremcpPathValidationResult::Denied(TEXT("soft path is empty"));
	}

	if (SoftPath.Contains(TEXT("\\")) || SoftPath.Contains(TEXT(":")))
	{
		return FUeremcpPathValidationResult::Denied(TEXT("soft path must not be a filesystem path"));
	}

	if (!SoftPath.StartsWith(TEXT("/")))
	{
		return FUeremcpPathValidationResult::Denied(TEXT("soft path must start with '/'"));
	}

	if (ContainsTraversalSegment(SoftPath))
	{
		return FUeremcpPathValidationResult::Denied(TEXT("soft path contains '..' traversal"));
	}

	if (bForWrite && IsEngineSoftPath(SoftPath))
	{
		return FUeremcpPathValidationResult::Denied(TEXT("/Engine/ writes are forbidden"));
	}

	if (bForWrite && IsTempSoftPath(SoftPath))
	{
		return FUeremcpPathValidationResult::Denied(TEXT("/Temp/ persistence is forbidden"));
	}

	if (SoftPath.StartsWith(TEXT("/Game/"), ESearchCase::IgnoreCase)
		|| SoftPath.Equals(TEXT("/Game"), ESearchCase::IgnoreCase))
	{
		return FUeremcpPathValidationResult::Allowed();
	}

	if (bForWrite)
	{
		// Project plugin soft paths (/MyPlugin/...) — tightened to registered plugins later.
		if (SoftPath.StartsWith(TEXT("/")) && !IsEngineSoftPath(SoftPath) && !IsTempSoftPath(SoftPath))
		{
			return FUeremcpPathValidationResult::Allowed();
		}
		return FUeremcpPathValidationResult::Denied(TEXT("unsupported soft path root for write"));
	}

	// Reads may inspect engine content.
	return FUeremcpPathValidationResult::Allowed();
}

FUeremcpPathValidationResult FUeremcpPathPolicy::ValidateFilesystemPath(
	const FString& FilesystemPath,
	bool bForWrite,
	const FUeremcpPathPolicyRoots* Roots)
{
	const FUeremcpPathPolicyRoots Resolved = UeremcpPathPolicyPrivate::ResolveRoots(Roots);
	if (!Resolved.IsConfigured())
	{
		return FUeremcpPathValidationResult::Denied(TEXT("project roots are not configured"));
	}

	if (FilesystemPath.IsEmpty())
	{
		return FUeremcpPathValidationResult::Denied(TEXT("filesystem path is empty"));
	}

	if (ContainsTraversalSegment(FilesystemPath))
	{
		return FUeremcpPathValidationResult::Denied(TEXT("filesystem path contains traversal"));
	}

	const FString FullPath = FPaths::ConvertRelativePathToFull(FilesystemPath);
	const FString SavedRoot = SavedUeremcpRoot(Resolved);

	const bool bUnderProject = UeremcpPathPolicyPrivate::IsUnderRoot(FullPath, Resolved.ProjectDir);
	const bool bUnderContent = UeremcpPathPolicyPrivate::IsUnderRoot(FullPath, Resolved.ProjectContentDir);
	const bool bUnderSavedUeremcp = UeremcpPathPolicyPrivate::IsUnderRoot(FullPath, SavedRoot);

	if (!bUnderProject)
	{
		return FUeremcpPathValidationResult::Denied(TEXT("path is outside the project directory"));
	}

	if (bForWrite)
	{
		if (bUnderContent || bUnderSavedUeremcp || UeremcpPathPolicyPrivate::IsUnderRoot(FullPath, Resolved.ProjectDir))
		{
			// Saved outside UEREMCP is read-only for mutators (audit/idempotency only under UEREMCP).
			const FString SavedDir = UeremcpPathPolicyPrivate::NormaliseDir(Resolved.ProjectSavedDir);
			const FString NormalFull = UeremcpPathPolicyPrivate::NormaliseDir(FullPath);
			if (NormalFull.StartsWith(SavedDir, ESearchCase::IgnoreCase) && !bUnderSavedUeremcp)
			{
				return FUeremcpPathValidationResult::Denied(
					TEXT("writes under Saved/ are limited to Saved/UEREMCP/**"));
			}
			return FUeremcpPathValidationResult::Allowed();
		}
		return FUeremcpPathValidationResult::Denied(TEXT("write path is not under an allowed root"));
	}

	return FUeremcpPathValidationResult::Allowed();
}

FUeremcpPathValidationResult FUeremcpPathPolicy::ValidateProjectPathMatch(
	const FString& RequestProjectPath,
	const FString& OpenProjectPath)
{
	if (RequestProjectPath.IsEmpty())
	{
		return FUeremcpPathValidationResult::Denied(TEXT("request.project.path is empty"));
	}
	if (OpenProjectPath.IsEmpty())
	{
		return FUeremcpPathValidationResult::Denied(TEXT("no project is loaded"));
	}

	const FString NormalRequest = FPaths::ConvertRelativePathToFull(RequestProjectPath);
	const FString NormalOpen = FPaths::ConvertRelativePathToFull(OpenProjectPath);
	if (!NormalRequest.Equals(NormalOpen, ESearchCase::IgnoreCase))
	{
		return FUeremcpPathValidationResult::Denied(TEXT("request.project.path does not match the open project"));
	}
	return FUeremcpPathValidationResult::Allowed();
}
