// UEREMCP — resolve emitter renderers without non-exported stack helpers (WS-07).

#pragma once

#include "CoreMinimal.h"

class UNiagaraMeshRendererProperties;
class UNiagaraRendererProperties;
class UNiagaraSystem;

namespace UeremcpNiagaraRendererResolve
{
	/**
	 * Resolve UNiagaraRendererProperties by system/emitter/index.
	 *
	 * FNiagaraExt_StackItemReference::GetRenderer is implemented in NiagaraEditor but
	 * is not UE_API exported (NiagaraExternalSystemEditorUtilities.h:1007), so calling
	 * it from UeremcpNiagara fails at link time. This uses exported Niagara APIs instead:
	 * UNiagaraSystem::GetEmitterHandles [VERIFIED: NiagaraSystem.h:314]
	 * FNiagaraEmitterHandle::GetEmitterData [VERIFIED: NiagaraEmitterHandle.h:86]
	 * FVersionedNiagaraEmitterData::GetRenderer [VERIFIED: NiagaraEmitter.h:422]
	 */
	UNiagaraRendererProperties* GetRendererAtIndex(
		UNiagaraSystem* System,
		FName EmitterName,
		int32 RendererIndex);

	UNiagaraMeshRendererProperties* GetMeshRendererAtIndex(
		UNiagaraSystem* System,
		FName EmitterName,
		int32 RendererIndex);
}
