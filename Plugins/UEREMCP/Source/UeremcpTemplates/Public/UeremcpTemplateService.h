// UEREMCP — template search + instantiate service (ADR-0008). Owner: WS-15.

#pragma once

#include "CoreMinimal.h"
#include "UeremcpTemplateStore.h"
#include "UeremcpTemplateTypes.h"

/**
 * Domain service for search_templates and instantiate_template.
 * Execution delegates to execute_plan (WS-05) once batch executor lands; v1
 * materialises construction_plan JSON only.
 */
class UEREMCPTEMPLATES_API FUeremcpTemplateService
{
public:
	explicit FUeremcpTemplateService(FUeremcpTemplateStore& InStore);

	TArray<FUeremcpTemplateSearchHit> Search(const FUeremcpTemplateSearchQuery& Query) const;

	FUeremcpTemplateInstantiateResult Instantiate(const FUeremcpTemplateInstantiateRequest& Request) const;

private:
	static float ScoreRecord(const FUeremcpTemplateRecord& Record, const FUeremcpTemplateSearchQuery& Query);
	static bool PassesElementFilter(const FUeremcpTemplateRecord& Record, const FString& ElementFilter);
	static TSharedPtr<FJsonValue> ApplyInputsToJsonValue(
		const TSharedPtr<FJsonValue>& Value,
		const TSharedPtr<FJsonObject>& Inputs);
	static TSharedPtr<FJsonObject> MaterializePlan(
		const FUeremcpTemplateRecord& Record,
		const TSharedPtr<FJsonObject>& Inputs,
		const TSharedPtr<FJsonObject>& Modifiers,
		FString& OutError);

	FUeremcpTemplateStore& Store;
};
