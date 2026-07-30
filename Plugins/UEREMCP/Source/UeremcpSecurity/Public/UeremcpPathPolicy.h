// UEREMCP — allowed roots and path traversal rejection (ADR-0010 §3.3).
//
// Runs before FileSandbox Enter. FileSandbox is a transaction boundary, not an ACL
// (ADR-0005, RB-13 B4).

#pragma once

#include "CoreMinimal.h"
#include "UeremcpSecurityTypes.h"

class UEREMCPSECURITY_API FUeremcpPathPolicy
{
public:
	/** Roots from the currently open project (editor). */
	static FUeremcpPathPolicyRoots RootsFromProject();

	/**
	 * Validate a soft object / package path (e.g. /Game/Foo/Bar).
	 * Rejects /Engine/ writes, /Temp/ persistence, traversal, and Windows absolutes.
	 */
	static FUeremcpPathValidationResult ValidateSoftPath(
		const FString& SoftPath,
		bool bForWrite,
		const FUeremcpPathPolicyRoots* Roots = nullptr);

	/**
	 * Validate a filesystem path after normalisation.
	 * Accepts project dir, Content, and Saved/UEREMCP/** plugin-owned subtrees.
	 */
	static FUeremcpPathValidationResult ValidateFilesystemPath(
		const FString& FilesystemPath,
		bool bForWrite,
		const FUeremcpPathPolicyRoots* Roots = nullptr);

	/** request.project.path must equal the open .uproject (normalised). */
	static FUeremcpPathValidationResult ValidateProjectPathMatch(
		const FString& RequestProjectPath,
		const FString& OpenProjectPath);

	static bool ContainsTraversalSegment(const FString& Path);
	static bool IsEngineSoftPath(const FString& SoftPath);
	static bool IsTempSoftPath(const FString& SoftPath);

	/** Relative path under Saved for plugin-owned durable stores. */
	static FString SavedUeremcpRoot(const FUeremcpPathPolicyRoots& Roots);
};
