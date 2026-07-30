// UEREMCP — Niagara inspect → graph.schema.json mapper (WS-07).
//
// Composes UNiagaraExternalEditUtilities (public NiagaraEditor API) — the same surface
// Epic NiagaraToolsets.NiagaraToolset_System wraps internally.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"

/** Parsed inspect_system specification (schemas/domains/niagara/inspect_system.schema.json). */
struct FUeremcpNiagaraInspectSpec
{
	TArray<FString> EmitterFilter;
	TSet<FString> StackFilter;
	bool bIncludeInputValues = true;
	bool bIncludeRenderers = true;
	bool bIncludeDependencies = true;
	bool bIncludeCompileState = true;
	bool bIncludeStackIssues = true;
};

/** Result of a read-only Niagara inspection pass. */
struct FUeremcpNiagaraInspectResult
{
	bool bSuccess = false;
	FString Error;

	FString Summary;
	TArray<TSharedPtr<FJsonValue>> Graphs;
	TArray<TSharedPtr<FJsonValue>> ExecutionTrace;

	TArray<FString> ChecksPerformed;
	TArray<FString> ChecksSkipped;
	TOptional<bool> bCompiled;

	int32 InternalOperations = 0;
	int32 EmitterCount = 0;
	int32 ModuleCount = 0;
	int32 RendererCount = 0;
};

/** Maps Epic Niagara topology into ADR-0004 graph shapes + extensions.niagara. */
class FUeremcpNiagaraInspect
{
public:
	static bool ParseSpecification(
		const TSharedPtr<FJsonObject>& Spec,
		FUeremcpNiagaraInspectSpec& OutSpec,
		FString& OutError);

	/** Wave 2 probe guard — only /Game/__UeremcpTests/ or /Game/__UeremcpPoc/ assets. */
	static bool IsAllowedProbePath(const FString& AssetPath);

	/**
	 * Probe inspect must not call GetStackIssues: diagnostics VM builds renderer stack
	 * items that evaluate FNiagaraMeshMaterialOverride::ExplicitMat edit conditions
	 * (bOverrideMaterials on parent UNiagaraMeshRendererProperties) in struct scope.
	 * [VERIFIED: GetStackIssues → GetDiagnosticsSystemViewModel — NiagaraExternalSystemEditorUtilities.cpp:3367]
	 */
	static bool ShouldSkipStackIssuesForProbe(const FString& AssetPath);

	static bool Run(
		const FUeremcpRequest& Request,
		const FUeremcpNiagaraInspectSpec& Spec,
		FUeremcpNiagaraInspectResult& OutResult);
};
