// UEREMCP — dependency counts without DI property serialization (WS-07).

#pragma once

#include "CoreMinimal.h"
#include "NiagaraExternalSystemEditorUtilities.h"

class UClass;

/**
 * Counts derived from GetEmitterTopology + script default DIs only.
 * Never calls GetSystemDependencies / InitFromStackInput (MeshRendererInfo DI
 * property export triggers bOverrideMaterials EditCondition LogError).
 */
struct FUeremcpNiagaraDependencySurveyCounts
{
	int32 UsedModules = 0;
	int32 UsedDataInterfaces = 0;
	int32 UsedDynamicInputs = 0;
	int32 UsedRenderers = 0;

	TSet<UClass*> SeenModules;
	TSet<UClass*> SeenDataInterfaces;
	TSet<UClass*> SeenRenderers;
};

namespace UeremcpNiagaraDependencySurvey
{
	void AccumulateFromEmitterTopology(
		const FNiagaraExt_EmitterTopology& Topology,
		FUeremcpNiagaraDependencySurveyCounts& InOutCounts);
}
