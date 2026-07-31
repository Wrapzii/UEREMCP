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

	/**
	 * Search reusable construction patterns before creating assets from scratch.
	 * Use when: find a known projectile, emitter, material, or gameplay template.
	 * Inputs: action=search_templates; specification has no required keys; query/domain optional.
	 * Example: {"protocol_version":"1.0","action":"search_templates","specification":{"query":"fire projectile","domain":"niagara"}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Templates")
	static FString SearchTemplates(const FString& RequestJson);

	/**
	 * Build assets from one reusable template and validated inputs/modifiers.
	 * Use when: a template_id is known and should produce a complete asset or asset set.
	 * Inputs: action=instantiate_template; specification.template_id is required.
	 * Example: {"protocol_version":"1.0","action":"instantiate_template","target":{"asset_path":"/Game/__UeremcpTests/NS_FromTemplate"},"options":{"dry_run":true},"specification":{"template_id":"niagara.projectile.elemental.v1","inputs":{"element":"fire"}}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Templates")
	static FString InstantiateTemplate(const FString& RequestJson);

	/**
	 * Preview promotion of a successful scratch asset into a reusable template.
	 * Use when: capture a validated result as a repeatable pattern; currently preview-only.
	 * Inputs: action=promote_to_template; specification.source_asset is required.
	 * Example: {"protocol_version":"1.0","action":"promote_to_template","options":{"dry_run":true},"specification":{"source_asset":"/Game/__UeremcpTests/NS_Good","proposed_template_id":"niagara.projectile.good.v1"}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Templates")
	static FString PromoteToTemplate(const FString& RequestJson);
};
