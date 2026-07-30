#include "UeremcpSecuritySettings.h"

#include "UObject/UnrealType.h"

UUeremcpSecuritySettings::UUeremcpSecuritySettings()
{
	CategoryName = TEXT("Plugins");
}

const UUeremcpSecuritySettings* UUeremcpSecuritySettings::Get()
{
	return GetDefault<UUeremcpSecuritySettings>();
}

FName UUeremcpSecuritySettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}
