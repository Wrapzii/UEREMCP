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
	 * Promote a successful scratch asset into a reusable, versioned template JSON.
	 * Use when: bank a validated result for later InstantiateTemplate.
	 * Inputs: action=promote_to_template; specification.source_asset required.
	 * Writes under Saved/UEREMCP/Templates/agent/ when options.dry_run=false (default dry_run=true).
	 * Example: {"protocol_version":"1.0","action":"promote_to_template","options":{"dry_run":false},"specification":{"source_asset":"/Game/__UeremcpTests/NS_Good","proposed_template_id":"niagara.projectile.good.v1"}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Templates")
	static FString PromoteToTemplate(const FString& RequestJson);

	/**
	 * Author a new template from a complete template document (not from an existing asset).
	 * Use when: define emitter roles, materials, meshes, timing, inputs from scratch for this game.
	 * Inputs: action=create_template; specification.template object required (template_id/domain/category/description).
	 * Example: {"protocol_version":"1.0","action":"create_template","options":{"dry_run":false},"specification":{"template":{"template_id":"niagara.cast.helix.v1","domain":"niagara","category":"cast","version":1,"description":"Ice helix cast ring","construction_plan":[]}}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Templates")
	static FString CreateTemplate(const FString& RequestJson);

	/**
	 * Replace an existing agent-authored template document by template_id.
	 * Use when: refine construction_plan / inputs after CreateTemplate or PromoteToTemplate.
	 * Inputs: action=update_template; specification.template object required; template_id must already exist.
	 * Example: {"protocol_version":"1.0","action":"update_template","options":{"dry_run":false},"specification":{"template":{"template_id":"niagara.cast.helix.v1","domain":"niagara","category":"cast","version":2,"description":"Refined helix","construction_plan":[]}}}
	 */
	UFUNCTION(meta = (AICallable), Category = "UEREMCP|Templates")
	static FString UpdateTemplate(const FString& RequestJson);
};
