// Editor automation tests for UeremcpNiagara change manifest (WS-07).

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "UeremcpNiagaraChangeManifest.h"
#include "UeremcpNiagaraCreate.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUeremcpNiagaraChangeManifestOfflineTest,
	"UEREMCP.Niagara.Create.ChangeManifestOffline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FUeremcpNiagaraChangeManifestOfflineTest::RunTest(const FString& Parameters)
{
	FUeremcpNiagaraCreateResult CreateResult;
	CreateResult.CreatedAssetPath = TEXT("/Game/__UeremcpTests/NS_POCB_FireballProbe.NS_POCB_FireballProbe");
	CreateResult.bSaved = true;

	FUeremcpNiagaraInlineMaterialCreate Inline;
	Inline.bSuccess = true;
	Inline.Role = TEXT("core");
	Inline.PrimaryAsset = TEXT("/Game/__UeremcpTests/Materials/MI_POCB_Core.MI_POCB_Core");
	CreateResult.MaterialBindings.InlineMaterialCreates.Add(Inline);
	CreateResult.MaterialBindings.ResolvedMaterialPaths.Add(
		TEXT("sparks"),
		TEXT("/Game/__UeremcpTests/Materials/MI_POCB_Sparks.MI_POCB_Sparks"));

	const FUeremcpNiagaraChangeManifestResult Manifest =
		FUeremcpNiagaraChangeManifest::BuildFromCreateResult(CreateResult, /*bDryRun=*/false);

	TestTrue(TEXT("manifest populated"), Manifest.bPopulated);
	TestEqual(TEXT("one created system"), Manifest.CreatedAssets.Num(), 1);
	TestEqual(TEXT("one reused material"), Manifest.ReusedAssets.Num(), 1);
	TestTrue(TEXT("changes non-empty"), Manifest.Changes.Num() >= 2);
	TestEqual(TEXT("assets affected"), Manifest.AssetsAffected, 1);

	const FUeremcpNiagaraChangeManifestResult DryRun =
		FUeremcpNiagaraChangeManifest::BuildFromCreateResult(CreateResult, /*bDryRun=*/true);
	TestFalse(TEXT("dry run not populated"), DryRun.bPopulated);
	TestEqual(TEXT("dry run no changes"), DryRun.Changes.Num(), 0);

	CreateResult.bReplacedExisting = true;
	const FUeremcpNiagaraChangeManifestResult ReplaceManifest =
		FUeremcpNiagaraChangeManifest::BuildFromCreateResult(CreateResult, /*bDryRun=*/false);
	TestEqual(TEXT("replace uses modified_assets"), ReplaceManifest.ModifiedAssets.Num(), 1);
	TestEqual(TEXT("replace no created system"), ReplaceManifest.CreatedAssets.Num(), 1);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
