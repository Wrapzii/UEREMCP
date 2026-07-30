// UEREMCP — AICallable template library tools (ADR-0008). Owner: WS-15.

#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "UeremcpTemplatesToolset.generated.h"

/**
 * Thin toolset over FUeremcpTemplateService. One envelope in, one envelope out
 * per ADR-0002 / ADR-0003.
 */
UCLASS()
class UEREMCPTEMPLATES_API UUeremcpTemplatesToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	virtual FString GetToolsetVersion() const override { return TEXT("0.1.0"); }

	/** action=search_templates */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Templates")
	static FString SearchTemplates(const FString& RequestJson);

	/** action=instantiate_template */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Templates")
	static FString InstantiateTemplate(const FString& RequestJson);

	/** action=promote_to_template; preview-only until documented cross-domain gates are bound. */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Templates")
	static FString PromoteToTemplate(const FString& RequestJson);
};
