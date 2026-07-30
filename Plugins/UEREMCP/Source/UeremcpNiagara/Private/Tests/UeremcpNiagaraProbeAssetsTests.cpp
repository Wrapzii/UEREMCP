// Editor automation tests for UeremcpNiagara probe asset helpers (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraProbeAssets.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraReplaceModeTest,
	"UEREMCP.Niagara.Create.ReplaceModeOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraReplaceModeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("replace mode recognised"),
		UeremcpNiagaraProbeAssets::IsReplaceMode(TEXT("replace")));
	TestTrue(TEXT("replace mode case-insensitive"),
		UeremcpNiagaraProbeAssets::IsReplaceMode(TEXT("Replace")));
	TestFalse(TEXT("create_or_update is not replace"),
		UeremcpNiagaraProbeAssets::IsReplaceMode(TEXT("create_or_update")));

	FString Error;
	TestFalse(
		TEXT("delete outside probe root refused"),
		UeremcpNiagaraProbeAssets::DeleteProbeAssetAtPath(TEXT("/Game/VFX/NS_Fireball"), Error));
	TestFalse(TEXT("error message set"), Error.IsEmpty());

	Error.Reset();
	TestTrue(
		TEXT("delete on missing probe asset is no-op"),
		UeremcpNiagaraProbeAssets::DeleteProbeAssetAtPath(
			TEXT("/Game/__UeremcpTests/NS_WS07_DoesNotExist"),
			Error));
	TestTrue(TEXT("no error on missing asset"), Error.IsEmpty());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
