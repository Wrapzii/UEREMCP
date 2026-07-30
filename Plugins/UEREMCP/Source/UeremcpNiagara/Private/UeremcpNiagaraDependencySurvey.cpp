// UEREMCP — dependency counts without DI property serialization (WS-07).

#include "UeremcpNiagaraDependencySurvey.h"

#include "NiagaraScript.h"

namespace
{
	void AccumulateModuleStack(
		const FNiagaraExt_ScriptStackTopology& Stack,
		FUeremcpNiagaraDependencySurveyCounts& InOutCounts)
	{
		for (const FNiagaraExt_ModuleTopology& Module : Stack.Modules)
		{
			if (!Module.ModuleScript)
			{
				continue;
			}

			UClass* ModuleClass = Module.ModuleScript->GetClass();
			if (ModuleClass && !InOutCounts.SeenModules.Contains(ModuleClass))
			{
				InOutCounts.SeenModules.Add(ModuleClass);
				++InOutCounts.UsedModules;
			}

			// Default script DIs only — avoids InitFromStackInput → GetAllObjectProperties
			// on live stack inputs (e.g. MeshRendererInfo with bound UNiagaraMeshRendererProperties).
			// [VERIFIED: UNiagaraScript::GetCachedDefaultDataInterfaces — NiagaraScript.h:1294]
			for (const FNiagaraScriptDataInterfaceInfo& DIInfo : Module.ModuleScript->GetCachedDefaultDataInterfaces())
			{
				UClass* DIClass = nullptr;
				if (DIInfo.DataInterface)
				{
					DIClass = DIInfo.DataInterface->GetClass();
				}
				else if (DIInfo.Type.IsValid())
				{
					DIClass = DIInfo.Type.GetClass();
				}

				if (DIClass && !InOutCounts.SeenDataInterfaces.Contains(DIClass))
				{
					InOutCounts.SeenDataInterfaces.Add(DIClass);
					++InOutCounts.UsedDataInterfaces;
				}
			}
		}
	}
}

void UeremcpNiagaraDependencySurvey::AccumulateFromEmitterTopology(
	const FNiagaraExt_EmitterTopology& Topology,
	FUeremcpNiagaraDependencySurveyCounts& InOutCounts)
{
	for (const FNiagaraExt_RendererRef& Renderer : Topology.Renderers)
	{
		if (Renderer.RendererClass && !InOutCounts.SeenRenderers.Contains(Renderer.RendererClass))
		{
			InOutCounts.SeenRenderers.Add(Renderer.RendererClass);
			++InOutCounts.UsedRenderers;
		}
	}

	AccumulateModuleStack(Topology.EmitterSpawnScript, InOutCounts);
	AccumulateModuleStack(Topology.EmitterUpdateScript, InOutCounts);
	AccumulateModuleStack(Topology.ParticleSpawnScript, InOutCounts);
	AccumulateModuleStack(Topology.ParticleUpdateScript, InOutCounts);
}
