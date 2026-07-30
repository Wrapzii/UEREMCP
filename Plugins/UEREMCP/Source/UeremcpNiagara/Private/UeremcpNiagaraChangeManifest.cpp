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

	void AppendAssetRefChange(
		const FString& Kind,
		const FUeremcpAssetRef& Ref,
		const FString& Detail,
		TArray<TSharedPtr<FJsonValue>>& InOutChanges)
	{
		if (Ref.AssetPath.IsEmpty())
		{
			return;
		}
		InOutChanges.Add(MakeShared<FJsonValueObject>(
			MakeChangeEntry(Kind, Ref.AssetPath, Ref.AssetClass, Detail)));
	}

	bool IsMaterialAssetClass(const FString& AssetClass)
	{
		return AssetClass.Contains(TEXT("Material"));
	}
}

void FUeremcpNiagaraChangeManifest::MergeInlineMaterialSubManifest(
	const FUeremcpNiagaraInlineMaterialCreate& Inline,
	FUeremcpNiagaraChangeManifestResult& InOutManifest)
{
	if (!Inline.bSuccess)
	{
		return;
	}

	const FString RoleDetail = FString::Printf(TEXT("material role %s"), *Inline.Role);

	for (const FUeremcpAssetRef& Ref : Inline.CreatedAssets)
	{
		FUeremcpAssetRef AssetRef = Ref;
		if (AssetRef.Role.IsEmpty())
		{
			AssetRef.Role = Inline.Role;
		}
		AppendUniqueAssetRef(InOutManifest.CreatedAssets, AssetRef);
		AppendAssetRefChange(TEXT("created"), AssetRef, RoleDetail, InOutManifest.Changes);
	}

	for (const FUeremcpAssetRef& Ref : Inline.ModifiedAssets)
	{
		FUeremcpAssetRef AssetRef = Ref;
		if (AssetRef.Role.IsEmpty())
		{
			AssetRef.Role = Inline.Role;
		}
		AppendUniqueAssetRef(InOutManifest.ModifiedAssets, AssetRef);
		AppendAssetRefChange(TEXT("modified"), AssetRef, RoleDetail, InOutManifest.Changes);
	}

	for (const FUeremcpAssetRef& Ref : Inline.ReusedAssets)
	{
		FUeremcpAssetRef AssetRef = Ref;
		if (AssetRef.Role.IsEmpty())
		{
			AssetRef.Role = Inline.Role;
		}
		AppendUniqueAssetRef(InOutManifest.ReusedAssets, AssetRef);
		AppendAssetRefChange(TEXT("reused"), AssetRef, RoleDetail, InOutManifest.Changes);
	}

	if (Inline.CreatedAssets.Num() == 0
		&& Inline.ModifiedAssets.Num() == 0
		&& Inline.ReusedAssets.Num() == 0
		&& !Inline.PrimaryAsset.IsEmpty())
	{
		const FString AssetClass = Inline.bShortCircuitedReuse
			? TEXT("MaterialInterface")
			: TEXT("MaterialInstanceConstant");
		const FString Kind = Inline.bShortCircuitedReuse ? TEXT("reused") : TEXT("created");
		const FUeremcpAssetRef PrimaryRef = MakeAssetRef(Inline.PrimaryAsset, AssetClass, Inline.Role);
		if (Inline.bShortCircuitedReuse)
		{
			AppendUniqueAssetRef(InOutManifest.ReusedAssets, PrimaryRef);
		}
		else
		{
			AppendUniqueAssetRef(InOutManifest.CreatedAssets, PrimaryRef);
		}
		AppendAssetRefChange(Kind, PrimaryRef, RoleDetail, InOutManifest.Changes);
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
		MergeInlineMaterialSubManifest(Inline, Out);
	}

	if (!CreateResult.InheritedAssetPath.IsEmpty())
	{
		const FUeremcpAssetRef SourceRef = MakeAssetRef(
			CreateResult.InheritedAssetPath,
			TEXT("NiagaraSystem"),
			TEXT("variation_source"));
		AppendUniqueAssetRef(Out.ReusedAssets, SourceRef);
		Out.Changes.Add(MakeShared<FJsonValueObject>(MakeChangeEntry(
			TEXT("reused"),
			CreateResult.InheritedAssetPath,
			TEXT("NiagaraSystem"),
			FString::Printf(
				TEXT("inherited %d source emitter(s) without reconstructing them"),
				CreateResult.EmittersInherited.Num()))));
	}

	for (const FString& EmitterName : CreateResult.EmittersAdded)
	{
		Out.Changes.Add(MakeShared<FJsonValueObject>(MakeChangeEntry(
			TEXT("emitter_added"),
			CreateResult.CreatedAssetPath,
			SystemClass,
			FString::Printf(TEXT("emitter %s"), *EmitterName))));
	}

	TSet<FString> MaterialManifestPaths;
	for (const FUeremcpAssetRef& Ref : Out.CreatedAssets)
	{
		if (IsMaterialAssetClass(Ref.AssetClass))
		{
			MaterialManifestPaths.Add(Ref.AssetPath);
		}
	}
	for (const FUeremcpAssetRef& Ref : Out.ReusedAssets)
	{
		if (IsMaterialAssetClass(Ref.AssetClass))
		{
			MaterialManifestPaths.Add(Ref.AssetPath);
		}
	}

	for (const TPair<FString, FString>& Pair : CreateResult.MaterialBindings.ResolvedMaterialPaths)
	{
		if (Pair.Value.IsEmpty() || MaterialManifestPaths.Contains(Pair.Value))
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
