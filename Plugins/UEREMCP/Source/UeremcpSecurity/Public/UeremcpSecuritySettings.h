// UEREMCP — project security settings (ADR-0010 §2, §6).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UeremcpSecurityTypes.h"
#include "UeremcpSecuritySettings.generated.h"

/**
 * Project-level security controls. Request envelopes cannot elevate to `unsafe`;
 * only this settings object can (ADR-0010 §2).
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "UEREMCP Security"))
class UEREMCPSECURITY_API UUeremcpSecuritySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UUeremcpSecuritySettings();

	static const UUeremcpSecuritySettings* Get();

	/** Highest tier any UEREMCP tool may use without per-request destructive opt-in. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	EUeremcpPermissionTier MaxProjectTier = EUeremcpPermissionTier::Write;

	/** When true, unsafe-tier tools may be registered (execute_tool_script, etc.). */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	bool bAllowUnsafe = false;

	/** Append-only audit JSONL retention under Saved/UEREMCP/audit/. */
	UPROPERTY(Config, EditAnywhere, Category = "Audit", meta = (ClampMin = "1", ClampMax = "365"))
	int32 AuditRetentionDays = 14;

	/** Best-effort: refuse mutators when MCP bind is not loopback (ADR-0010 §5). */
	UPROPERTY(Config, EditAnywhere, Category = "Transport")
	bool bRefuseMutatorsOnNonLoopbackBind = true;

	virtual FName GetCategoryName() const override;
};
