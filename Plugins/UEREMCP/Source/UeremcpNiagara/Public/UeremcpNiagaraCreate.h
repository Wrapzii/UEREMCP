// UEREMCP — goal-level Niagara effect creation (WS-07 / POC B slice).
//
// Composes UNiagaraExternalEditUtilities (public NiagaraEditor API) — same substrate
// as Epic NiagaraToolsets.NiagaraToolset_System.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"

/** Parsed create_niagara_effect specification (schemas/domains/niagara/create_niagara_effect.schema.json). */
struct FUeremcpNiagaraCreateSpec
{
	FString Name;
	FString EffectType;
	FString Element;

	/** Emitter roles to instantiate (e.g. sparks, impact_burst). */
	TArray<FString> ComponentRoles;

	/** Optional template system soft path (package/object). */
	FString TemplateSystemPath;

	/** User-parameter knobs from specification.parameters. */
	TSharedPtr<FJsonObject> Parameters;
};

/** Result of a create/modify Niagara compose pass. */
struct FUeremcpNiagaraCreateResult
{
	bool bSuccess = false;
	FString Error;

	FString Summary;
	FString CreatedAssetPath;

	TArray<FString> EmittersAdded;
	TArray<FString> UserVariablesAdded;
	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;

	TOptional<bool> bCompiled;
	TOptional<bool> bSaved;

	int32 InternalOperations = 0;
};

/** Duplicate-and-modify composer for POC B under /Game/__UeremcpTests/ only. */
class FUeremcpNiagaraCreate
{
public:
	static bool ParseSpecification(
		const FUeremcpRequest& Request,
		FUeremcpNiagaraCreateSpec& OutSpec,
		FString& OutError);

	static bool Run(
		const FUeremcpRequest& Request,
		const FUeremcpNiagaraCreateSpec& Spec,
		FUeremcpNiagaraCreateResult& OutResult);
};
