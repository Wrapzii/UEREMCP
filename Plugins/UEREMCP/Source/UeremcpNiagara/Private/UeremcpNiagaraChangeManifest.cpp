// UEREMCP — create_niagara_effect change manifest builder (WS-07 / POC B B9).

#include "UeremcpNiagaraChangeManifest.h"

namespace
{
	FUeremcpAssetRef MakeAssetRef(
		const FString& AssetPath,
		const FString& AssetClass,
		const FString& Role = FString())
	{
		FUeremcpAssetRef Ref;
		Ref.AssetPath = AssetPath;
		Ref.AssetClass = AssetClass;
		Ref.Role = Role;
		return Ref;
	}

	TSharedPtr<FJsonObject> MakeChangeEntry(
		const FString& Kind,
		const FString& AssetPath,
		const FString& AssetClass,
		const FString& Detail = FString())
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("kind"), Kind);
		Entry->SetStringField(TEXT("asset_path"), AssetPath);
		if (!AssetClass.IsEmpty())
		{
			Entry->SetStringField(TEXT("asset_class"), AssetClass);
		}
		if (!Detail.IsEmpty())
		{
			Entry->SetStringField(TEXT("detail"), Detail);
		}
		return Entry;
	}

	void AppendUniqueAssetRef(TArray<FUeremcpAssetRef>& InOut, const FUeremcpAssetRef& Ref)
	{
		if (Ref.AssetPath.IsEmpty())
		{
			return;
		}
		for (const FUeremcpAssetRef& Existing : InOut)
		{
			if (Existing.AssetPath == Ref.AssetPath)
			{
				return;
			}
		}
		InOut.Add(Ref);
	}
}

FUeremcpNiagaraChangeManifestResult FUeremcpNiagaraChangeManifest::BuildFromCreateResult(
	const FUeremcpNiagaraCreateResult& CreateResult,
	bool bDryRun)
{
	FUeremcpNiagaraChangeManifestResult Out;
	if (bDryRun || CreateResult.CreatedAssetPath.IsEmpty())
	{
		return Out;
	}

	const FString SystemClass = TEXT("NiagaraSystem");
	if (CreateResult.bReplacedExisting)
	{
		const FUeremcpAssetRef SystemRef = MakeAssetRef(CreateResult.CreatedAssetPath, SystemClass, TEXT("primary"));
		AppendUniqueAssetRef(Out.ModifiedAssets, SystemRef);
		Out.Changes.Add(MakeShared<FJsonValueObject>(
			MakeChangeEntry(TEXT("modified"), CreateResult.CreatedAssetPath, SystemClass, TEXT("replace mode"))));
		Out.Changes.Add(MakeShared<FJsonValueObject>(
			MakeChangeEntry(TEXT("saved"), CreateResult.CreatedAssetPath, SystemClass)));
	}
	else
	{
		const FUeremcpAssetRef SystemRef = MakeAssetRef(CreateResult.CreatedAssetPath, SystemClass, TEXT("primary"));
		AppendUniqueAssetRef(Out.CreatedAssets, SystemRef);
		Out.Changes.Add(MakeShared<FJsonValueObject>(
			MakeChangeEntry(TEXT("created"), CreateResult.CreatedAssetPath, SystemClass)));
		if (CreateResult.bSaved.Get(false))
		{
			Out.Changes.Add(MakeShared<FJsonValueObject>(
				MakeChangeEntry(TEXT("saved"), CreateResult.CreatedAssetPath, SystemClass)));
		}
	}

	for (const FUeremcpNiagaraInlineMaterialCreate& Inline : CreateResult.MaterialBindings.InlineMaterialCreates)
	{
		if (!Inline.bSuccess || Inline.PrimaryAsset.IsEmpty())
		{
			continue;
		}

		const FUeremcpAssetRef InlineRef = MakeAssetRef(
			Inline.PrimaryAsset,
			TEXT("MaterialInstanceConstant"),
			Inline.Role);
		AppendUniqueAssetRef(Out.CreatedAssets, InlineRef);
		Out.Changes.Add(MakeShared<FJsonValueObject>(MakeChangeEntry(
			TEXT("created"),
			Inline.PrimaryAsset,
			TEXT("MaterialInstanceConstant"),
			FString::Printf(TEXT("inline create_spec for role %s"), *Inline.Role))));

		for (const FUeremcpAssetRef& Nested : Inline.CreatedAssets)
		{
			if (Nested.AssetPath.IsEmpty())
			{
				continue;
			}
			AppendUniqueAssetRef(Out.CreatedAssets, Nested);
			Out.Changes.Add(MakeShared<FJsonValueObject>(MakeChangeEntry(
				TEXT("created"),
				Nested.AssetPath,
				Nested.AssetClass,
				FString::Printf(TEXT("dependency of inline material role %s"), *Inline.Role))));
		}
	}

	TSet<FString> InlinePrimaryPaths;
	for (const FUeremcpNiagaraInlineMaterialCreate& Inline : CreateResult.MaterialBindings.InlineMaterialCreates)
	{
		if (Inline.bSuccess && !Inline.PrimaryAsset.IsEmpty())
		{
			InlinePrimaryPaths.Add(Inline.PrimaryAsset);
		}
	}

	for (const TPair<FString, FString>& Pair : CreateResult.MaterialBindings.ResolvedMaterialPaths)
	{
		if (Pair.Value.IsEmpty() || InlinePrimaryPaths.Contains(Pair.Value))
		{
			continue;
		}

		const FUeremcpAssetRef ReusedRef = MakeAssetRef(
			Pair.Value,
			TEXT("MaterialInterface"),
			Pair.Key);
		AppendUniqueAssetRef(Out.ReusedAssets, ReusedRef);
		Out.Changes.Add(MakeShared<FJsonValueObject>(MakeChangeEntry(
			TEXT("reused"),
			Pair.Value,
			TEXT("MaterialInterface"),
			FString::Printf(TEXT("renderer material role %s"), *Pair.Key))));
	}

	Out.AssetsAffected = Out.CreatedAssets.Num() + Out.ModifiedAssets.Num();
	Out.bPopulated = Out.CreatedAssets.Num() > 0 || Out.ModifiedAssets.Num() > 0;
	return Out;
}
