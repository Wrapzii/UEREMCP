// UEREMCP — resolve emitter renderers without non-exported stack helpers (WS-07).

#include "UeremcpNiagaraRendererResolve.h"

#include "NiagaraEmitter.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystem.h"

UNiagaraRendererProperties* UeremcpNiagaraRendererResolve::GetRendererAtIndex(
	UNiagaraSystem* System,
	FName EmitterName,
	int32 RendererIndex)
{
	if (!System || EmitterName.IsNone() || RendererIndex == INDEX_NONE)
	{
		return nullptr;
	}

	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		if (Handle.GetName() != EmitterName)
		{
			continue;
		}

		if (FVersionedNiagaraEmitterData* EmitterData = Handle.GetEmitterData())
		{
			return EmitterData->GetRenderer(RendererIndex);
		}

		return nullptr;
	}

	return nullptr;
}

UNiagaraMeshRendererProperties* UeremcpNiagaraRendererResolve::GetMeshRendererAtIndex(
	UNiagaraSystem* System,
	FName EmitterName,
	int32 RendererIndex)
{
	return Cast<UNiagaraMeshRendererProperties>(GetRendererAtIndex(System, EmitterName, RendererIndex));
}
