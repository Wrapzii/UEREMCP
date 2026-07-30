#pragma once

#include "CoreMinimal.h"

struct FUeremcpAbilityTableWritePlan;
struct FUeremcpSpellPlan;

/** Verified outcome of one sandboxed FREAbilityDef row write. */
struct UEREMCPGAMEPLAY_API FUeremcpAbilityTableMutationResult
{
	bool bCreatedTable = false;
	bool bModifiedTable = false;
	bool bNoChange = false;
	bool bSaved = false;
	bool bRereadAfterWrite = false;
	bool bPersisted = false;
	bool bRolledBack = false;
	FString RevisionBefore;
	FString RevisionAfter;
	TArray<FString> SandboxedFiles;
};

/** Internal semantic executor; never exposed as an agent-facing DataTable primitive. */
class UEREMCPGAMEPLAY_API FUeremcpAbilityTableMutator
{
public:
	/**
	 * Create or upsert one planned FREAbilityDef row, save inside FileSandbox,
	 * re-read the normalized row, and persist only after equality is verified.
	 * [VERIFIED: ToolsetRegistry/SandboxLibrary.h:12-73]
	 * [VERIFIED: Engine/DataTable.h:253-316]
	 */
	static bool Execute(
		const FUeremcpAbilityTableWritePlan& WritePlan,
		const FUeremcpSpellPlan& SpellPlan,
		FUeremcpAbilityTableMutationResult& OutResult,
		FString& OutError);
};
