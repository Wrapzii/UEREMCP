// Destructive ops default dry_run=true (ADR-0010 / AGENTS.md rule 8) — POC E cross-cut.
#include "Misc/AutomationTest.h"
#include "UeremcpPermissionPolicy.h"
#include "UeremcpSecurityTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Locks the security policy that forces dry_run for destructive contexts when the
 * caller omitted options.dry_run. Domains must call FUeremcpPermissionPolicy; this
 * Validation gate prevents silent weakening of the default.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpHonestyDestructiveDryRunDefault,
	"UEREMCP.Validation.Honesty.DestructiveDryRunDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpHonestyDestructiveDryRunDefault::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FUeremcpPermissionOptions Options;
	Options.bDryRun = false;
	Options.bDryRunWasExplicit = false;

	const auto DeleteDefault = FUeremcpPermissionPolicy::Evaluate(
		TEXT("delete_asset"), TEXT("delete"), Options, true);
	TestTrue(TEXT("delete forces dry_run when omitted"), DeleteDefault.bDryRunForced);
	TestTrue(TEXT("delete effective dry_run true"), DeleteDefault.bEffectiveDryRun);

	const auto ReplaceExists = FUeremcpPermissionPolicy::Evaluate(
		TEXT("submit_graph"), TEXT("replace"), Options, true);
	TestTrue(TEXT("replace-on-existing forces dry_run when omitted"), ReplaceExists.bDryRunForced);
	TestTrue(TEXT("replace effective dry_run true"), ReplaceExists.bEffectiveDryRun);

	Options.bDryRunWasExplicit = true;
	Options.bDryRun = false;
	const auto DeleteExplicit = FUeremcpPermissionPolicy::Evaluate(
		TEXT("delete_asset"), TEXT("delete"), Options, true);
	TestFalse(TEXT("explicit dry_run=false is not forced"), DeleteExplicit.bDryRunForced);
	TestFalse(TEXT("explicit dry_run=false honoured"), DeleteExplicit.bEffectiveDryRun);

	AddInfo(TEXT("POC_E dry_run default: destructive contexts force dry_run unless explicitly set"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
