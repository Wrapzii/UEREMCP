// UEREMCP — material_bindings response diagnostics (WS-07).
//
// Pure JSON builders shared by the toolset and offline automation tests.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpNiagaraMaterialBinding.h"

class FUeremcpNiagaraMaterialBindingDiagnostics
{
public:
	/** Build extra.material_bindings; null when there is nothing to report. */
	static TSharedPtr<FJsonObject> BuildMaterialBindingsObject(
		const FUeremcpNiagaraMaterialBindingResult& Result);

	/**
	 * Roles where inline create_spec succeeded but renderer binding remains unresolved.
	 * When non-empty after ApplyRoleMaterialBindings, Create::Run continues as partially_completed.
	 */
	static TArray<FString> FindOrphanedInlineCreates(
		const FUeremcpNiagaraMaterialBindingResult& Result);

	/** True when bind failed but probe inline MIs were saved — continuable partial failure. */
	static bool ShouldContinueAfterBindingFailure(
		const FUeremcpNiagaraMaterialBindingResult& Result);

	/** Checks skipped when Create::Run continues after orphaned inline bind failure. */
	static void AppendOrphanPartialFailureChecksSkipped(TArray<FString>& OutChecksSkipped);

	/** Summary suffix appended when orphaned inline probe MIs remain on disk. */
	static FString BuildOrphanPartialFailureSummarySuffix(int32 OrphanCount);
};
