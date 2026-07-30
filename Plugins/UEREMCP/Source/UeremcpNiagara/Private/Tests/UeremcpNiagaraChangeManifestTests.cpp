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
	FUeremcpAssetRef CreatedMaster;
	CreatedMaster.AssetPath = TEXT("/Game/__UeremcpTests/Materials/M_Ueremcp_ElementalProjectileCore.M_Ueremcp_ElementalProjectileCore");
	CreatedMaster.AssetClass = TEXT("Material");
	CreatedMaster.Role = TEXT("master_template");
	Inline.CreatedAssets.Add(CreatedMaster);
	Inline.PrimaryAsset = TEXT("/Game/__UeremcpTests/Materials/MI_POCB_Core.MI_POCB_Core");
	FUeremcpAssetRef CreatedMi;
	CreatedMi.AssetPath = Inline.PrimaryAsset;
	CreatedMi.AssetClass = TEXT("MaterialInstanceConstant");
	CreatedMi.Role = TEXT("core");
	Inline.CreatedAssets.Add(CreatedMi);

	FUeremcpNiagaraInlineMaterialCreate ReusedInline;
	ReusedInline.bSuccess = true;
	ReusedInline.bShortCircuitedReuse = true;
	ReusedInline.Role = TEXT("ribbon_trail");
	ReusedInline.PrimaryAsset = TEXT("/Game/__UeremcpTests/Materials/MI_POCB_Ribbon.MI_POCB_Ribbon");
	FUeremcpAssetRef ReusedMi;
	ReusedMi.AssetPath = ReusedInline.PrimaryAsset;
	ReusedMi.AssetClass = TEXT("MaterialInstanceConstant");
	ReusedMi.Role = TEXT("ribbon_trail");
	ReusedInline.ReusedAssets.Add(ReusedMi);

	CreateResult.MaterialBindings.InlineMaterialCreates = { Inline, ReusedInline };
	CreateResult.MaterialBindings.ResolvedMaterialPaths.Add(
		TEXT("sparks"),
		TEXT("/Game/__UeremcpTests/Materials/MI_POCB_Sparks.MI_POCB_Sparks"));

	const FUeremcpNiagaraChangeManifestResult Manifest =
		FUeremcpNiagaraChangeManifest::BuildFromCreateResult(CreateResult, /*bDryRun=*/false);

	TestTrue(TEXT("manifest populated"), Manifest.bPopulated);
	TestEqual(TEXT("created system + MI + master"), Manifest.CreatedAssets.Num(), 3);
	TestEqual(TEXT("one short-circuit reused MI"), Manifest.ReusedAssets.Num(), 2);
	TestTrue(TEXT("changes non-empty"), Manifest.Changes.Num() >= 4);
	TestEqual(TEXT("assets affected"), Manifest.AssetsAffected, 1);

	const FUeremcpNiagaraChangeManifestResult DryRun =
		FUeremcpNiagaraChangeManifest::BuildFromCreateResult(CreateResult, /*bDryRun=*/true);
	TestFalse(TEXT("dry run not populated"), DryRun.bPopulated);
	TestEqual(TEXT("dry run no changes"), DryRun.Changes.Num(), 0);

	CreateResult.bReplacedExisting = true;
	const FUeremcpNiagaraChangeManifestResult ReplaceManifest =
		FUeremcpNiagaraChangeManifest::BuildFromCreateResult(CreateResult, /*bDryRun=*/false);
	TestEqual(TEXT("replace uses modified_assets for system"), ReplaceManifest.ModifiedAssets.Num(), 1);
	TestTrue(TEXT("replace still lists created materials"), ReplaceManifest.CreatedAssets.Num() >= 2);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
