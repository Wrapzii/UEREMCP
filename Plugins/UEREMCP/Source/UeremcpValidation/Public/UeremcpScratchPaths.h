// Scratch-path conventions for UEREMCP editor tests (RB-14 q4).
//
// Rules (also in tests/README.md):
//   - All scratch assets live under /Game/__UeremcpTests/
//   - POC assets live under /Game/__UeremcpPoc/ (POC_ACCEPTANCE.md) — tests must
//     not write there unless the test IS a POC gate.
//   - Cleanup is mandatory even on failure; use FUeremcpScratchGuard.
//   - Never touch real project content outside these roots.
//
#pragma once

#include "CoreMinimal.h"

/** Soft path root for WS-11 integration tests. Trailing slash omitted. */
inline const TCHAR* UeremcpTestsContentRoot = TEXT("/Game/__UeremcpTests");

/** Soft path root for POC scratch (owned by POC runners; listed for exclusion). */
inline const TCHAR* UeremcpPocContentRoot = TEXT("/Game/__UeremcpPoc");

/** Absolute filesystem path under the project's Content/ for __UeremcpTests. */
FString UeremcpGetTestsFilesystemRoot();

/**
 * Build a unique soft package path under /Game/__UeremcpTests/<Suite>/<Name>.
 * Name should be alphanumeric; Suite groups related tests for cleanup.
 */
FString UeremcpMakeScratchPackagePath(const FString& Suite, const FString& Name);

/**
 * Delete every asset under /Game/__UeremcpTests/<Suite>/ (or the whole tests root
 * if Suite is empty). Returns the number of packages deleted. Safe no-op if the
 * folder does not exist. Does NOT touch /Game/__UeremcpPoc/ or any other content.
 */
int32 UeremcpCleanupScratchSuite(const FString& Suite);

/**
 * RAII guard: on destruction, deletes the named suite (or all tests content if
 * Suite is empty). Use at the top of every integration test.
 */
struct FUeremcpScratchGuard
{
	explicit FUeremcpScratchGuard(FString InSuite)
		: Suite(MoveTemp(InSuite))
	{
	}

	~FUeremcpScratchGuard()
	{
		UeremcpCleanupScratchSuite(Suite);
	}

	FUeremcpScratchGuard(const FUeremcpScratchGuard&) = delete;
	FUeremcpScratchGuard& operator=(const FUeremcpScratchGuard&) = delete;

	FString Suite;
};
