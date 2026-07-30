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
	 * Roles where inline create_spec succeeded (PrimaryAsset saved) but renderer binding
	 * remains unresolved — honest partial-failure signal without claiming *_validated.
	 */
	static TArray<FString> FindOrphanedInlineCreates(
		const FUeremcpNiagaraMaterialBindingResult& Result);
};
