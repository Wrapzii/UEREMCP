// UEREMCP — template module accessors. Owner: WS-15.

#pragma once

#include "CoreMinimal.h"

class FUeremcpTemplateService;
class FUeremcpTemplateStore;

/**
 * WS-05 owns the execute_plan interpreter. This delegate is the narrow integration
 * seam that lets instantiate_template submit one complete execute_plan envelope
 * without implementing a second interpreter.
 */
using FUeremcpExecutePlanDelegate = TFunction<bool(
	const FString& RequestJson,
	FString& OutResponseJson,
	FString& OutError)>;

namespace UeremcpTemplates
{
	UEREMCPTEMPLATES_API FUeremcpTemplateStore& GetStore();
	UEREMCPTEMPLATES_API FUeremcpTemplateService& GetService();
	UEREMCPTEMPLATES_API FString ResolveTemplatesDirectory();

	UEREMCPTEMPLATES_API void SetExecutePlanDelegate(FUeremcpExecutePlanDelegate InDelegate);
	UEREMCPTEMPLATES_API void ClearExecutePlanDelegate();
	UEREMCPTEMPLATES_API bool ExecutePlan(
		const FString& RequestJson,
		FString& OutResponseJson,
		FString& OutError);
}
