// UEREMCP — template library types (ADR-0008). Owner: WS-15.

#pragma once

#include "CoreMinimal.h"

/** Loaded template metadata + raw JSON document. */
struct UEREMCPTEMPLATES_API FUeremcpTemplateRecord
{
	FString TemplateId;
	FString Domain;
	FString Category;
	int32 Version = 0;
	FString Description;
	TArray<FString> SearchTerms;
	TArray<FString> Composes;
	FString InheritsFrom;
	TArray<FString> SupportedModifiers;

	/** Declared element input when the template exposes inputs.element. */
	FString DeclaredElement;

	TSharedPtr<FJsonObject> Document;
	FString SourcePath;
};

/** search_templates specification (schemas/domains/templates/search_templates.schema.json). */
struct UEREMCPTEMPLATES_API FUeremcpTemplateSearchQuery
{
	FString Query;
	FString Domain;
	FString Element;
	int32 Limit = 20;
};

/** A single search hit returned to the agent. */
struct UEREMCPTEMPLATES_API FUeremcpTemplateSearchHit
{
	FString TemplateId;
	FString Domain;
	FString Category;
	FString Description;
	float Score = 0.f;
};

/** instantiate_template specification (schemas/domains/templates/instantiate_template.schema.json). */
struct UEREMCPTEMPLATES_API FUeremcpTemplateInstantiateRequest
{
	FString TemplateId;
	TSharedPtr<FJsonObject> Inputs;
	TSharedPtr<FJsonObject> Modifiers;
	FString TargetAssetPath;
	FString TargetName;
	FString Mode = TEXT("create_or_update");
};

/** Materialised plan returned by instantiate (delegates to execute_plan when wired). */
struct UEREMCPTEMPLATES_API FUeremcpTemplateInstantiateResult
{
	bool bSuccess = false;
	FString Summary;
	FString Status;
	TArray<FString> CapabilityNotes;
	TSharedPtr<FJsonObject> MaterializedPlan;
};
