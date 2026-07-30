// UEREMCP — create_niagara_effect change manifest builder (WS-07 / POC B B9).

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "UeremcpEnvelope.h"
#include "UeremcpNiagaraCreate.h"

/** Asset refs + schema change entries derived from a create pass. */
struct FUeremcpNiagaraChangeManifestResult
{
	TArray<FUeremcpAssetRef> CreatedAssets;
	TArray<FUeremcpAssetRef> ModifiedAssets;
	TArray<FUeremcpAssetRef> ReusedAssets;
	TArray<TSharedPtr<FJsonValue>> Changes;
	int32 AssetsAffected = 0;

	bool bPopulated = false;
};

class FUeremcpNiagaraChangeManifest
{
public:
	static FUeremcpNiagaraChangeManifestResult BuildFromCreateResult(
		const FUeremcpNiagaraCreateResult& CreateResult,
		bool bDryRun);

	/** Merge one inline material sub-result into a Niagara response manifest. */
	static void MergeInlineMaterialSubManifest(
		const FUeremcpNiagaraInlineMaterialCreate& Inline,
		FUeremcpNiagaraChangeManifestResult& InOutManifest);
};
