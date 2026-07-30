// UEREMCP — template module accessors. Owner: WS-15.

#pragma once

#include "CoreMinimal.h"

class FUeremcpTemplateService;
class FUeremcpTemplateStore;

namespace UeremcpTemplates
{
	UEREMCPTEMPLATES_API FUeremcpTemplateStore& GetStore();
	UEREMCPTEMPLATES_API FUeremcpTemplateService& GetService();
	FString ResolveTemplatesDirectory();
}
