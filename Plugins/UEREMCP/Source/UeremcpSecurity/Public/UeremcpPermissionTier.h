// UEREMCP — permission tier enum (ADR-0010 §2).
#pragma once

#include "CoreMinimal.h"
#include "UeremcpPermissionTier.generated.h"

UENUM(BlueprintType)
enum class EUeremcpPermissionTier : uint8
{
	Read        UMETA(DisplayName = "Read"),
	Write       UMETA(DisplayName = "Write"),
	Destructive UMETA(DisplayName = "Destructive"),
	Unsafe      UMETA(DisplayName = "Unsafe"),
};
