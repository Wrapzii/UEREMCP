// Interim launch-smoke probe (WS-11 / C-3).
//
// Proves this plugin can load under UnrealEditor-Cmd when UEREMCP cannot.
// Does NOT duplicate Rollback.MultiAssetDiscard — that source of truth is:
//   Plugins/UEREMCP/Source/UeremcpValidation/Private/Tests/RollbackMultiAssetDiscard.spec.cpp
// Probe FileSandbox evidence (earlier green run) remains valid as *engine* semantics
// only; it does not prove the shipping UEREMCP plugin gate.
//
#include "Modules/ModuleManager.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, UeremcpValidationProbe)

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpValidationProbeLaunchSmoke,
	"UEREMCP.ValidationProbe.Launch.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUeremcpValidationProbeLaunchSmoke::RunTest(const FString& Parameters)
{
	IModuleInterface* Module = FModuleManager::Get().GetModule(TEXT("UeremcpValidationProbe"));
	if (!Module)
	{
		Module = FModuleManager::Get().LoadModule(TEXT("UeremcpValidationProbe"));
	}
	TestNotNull(TEXT("UeremcpValidationProbe module loaded"), Module);
	AddInfo(TEXT("Interim probe launch OK. Shipping gate requires UeremcpValidation "
		"registered in UEREMCP.uplugin (WS-03) and green UEREMCP.Validation.Rollback.MultiAssetDiscard "
		"with UEREMCP enabled — see docs/proposals/ws-11-register-validation-module.md"));
	return Module != nullptr;
}

#endif // WITH_DEV_AUTOMATION_TESTS
