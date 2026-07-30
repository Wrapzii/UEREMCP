// UEREMCP — template search + instantiate service (ADR-0008). Owner: WS-15.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpTemplateStore.h"
#include "UeremcpTemplateTypes.h"

/**
 * Domain service for search_templates and instantiate_template.
 * Materialisation produces exactly the WS-05 execute_plan specification shape;
 * the AICallable boundary delegates that envelope through UeremcpTemplatesModule.
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
	static bool ValidateInputs(
		const FUeremcpTemplateRecord& Record,
		const TSharedPtr<FJsonObject>& Inputs,
		FString& OutError);
	static TSharedPtr<FJsonObject> MaterializePlan(
		const FUeremcpTemplateRecord& Record,
		const TSharedPtr<FJsonObject>& Inputs,
		const FString& TargetAssetPath,
		const FString& Mode,
		FString& OutError);

	FUeremcpTemplateStore& Store;
};
