// UEREMCP — in-place Niagara adapt (User.* params + material bindings) for Magecraft.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"
#include "UeremcpNiagaraMaterialBinding.h"

struct FUeremcpNiagaraAdaptSpec
{
	TSharedPtr<FJsonObject> Parameters;
	TArray<FUeremcpNiagaraMaterialRequest> MaterialRequests;
	/**
	 * Optional emitters[{name, sim_target, life_cycle, loop_duration, ...}] —
	 * first-class Emitter Properties / Emitter State Life Cycle patches.
	 */
	TArray<TSharedPtr<FJsonObject>> EmitterPropertyPatches;
};

struct FUeremcpNiagaraAdaptResult
{
	bool bSuccess = false;
	FString Error;
	FString Summary;
	FString AssetPath;
	TArray<FString> UserVariablesTouched;
	TArray<FString> EmitterNames;
	TArray<FString> EmitterPropertiesApplied;
	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;
	FUeremcpNiagaraMaterialBindingResult MaterialBindings;
	TOptional<bool> bCompiled;
	TOptional<bool> bSaved;
	int32 InternalOperations = 0;
};

/** In-place adapt of an existing Niagara system (sandbox or Magecraft). Never deletes. */
class FUeremcpNiagaraAdapt
{
public:
	static bool ParseSpecification(
		const TSharedPtr<FJsonObject>& Specification,
		FUeremcpNiagaraAdaptSpec& OutSpec,
		FString& OutError);

	static bool Run(
		const FUeremcpRequest& Request,
		const FUeremcpNiagaraAdaptSpec& Spec,
		FUeremcpNiagaraAdaptResult& OutResult);
};
