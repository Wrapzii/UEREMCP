// UEREMCP — material_bindings response diagnostics (WS-07).

#include "UeremcpNiagaraMaterialBindingDiagnostics.h"

namespace
{
	TArray<TSharedPtr<FJsonValue>> StringArray(const TArray<FString>& Items)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FString& Item : Items)
		{
			Values.Add(MakeShared<FJsonValueString>(Item));
		}
		return Values;
	}

	TArray<TSharedPtr<FJsonValue>> BuildAssetRefArray(const TArray<FUeremcpAssetRef>& Assets)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FUeremcpAssetRef& Asset : Assets)
		{
			TSharedPtr<FJsonObject> AssetObj = MakeShared<FJsonObject>();
			if (!Asset.AssetPath.IsEmpty())
			{
				AssetObj->SetStringField(TEXT("asset_path"), Asset.AssetPath);
			}
			if (!Asset.AssetClass.IsEmpty())
			{
				AssetObj->SetStringField(TEXT("asset_class"), Asset.AssetClass);
			}
			if (!Asset.Role.IsEmpty())
			{
				AssetObj->SetStringField(TEXT("role"), Asset.Role);
			}
			Values.Add(MakeShared<FJsonValueObject>(AssetObj));
		}
		return Values;
	}

	TArray<TSharedPtr<FJsonValue>> BuildInlineMaterialCreatesArray(
		const TArray<FUeremcpNiagaraInlineMaterialCreate>& InlineCreates)
	{
		TArray<TSharedPtr<FJsonValue>> InlineValues;
		for (const FUeremcpNiagaraInlineMaterialCreate& Inline : InlineCreates)
		{
			TSharedPtr<FJsonObject> InlineObj = MakeShared<FJsonObject>();
			InlineObj->SetStringField(TEXT("role"), Inline.Role);
			InlineObj->SetBoolField(TEXT("success"), Inline.bSuccess);
			if (Inline.bShortCircuitedReuse)
			{
				InlineObj->SetBoolField(TEXT("short_circuited_reuse"), true);
			}
			if (!Inline.Status.IsEmpty())
			{
				InlineObj->SetStringField(TEXT("status"), Inline.Status);
			}
			if (!Inline.Summary.IsEmpty())
			{
				InlineObj->SetStringField(TEXT("summary"), Inline.Summary);
			}
			if (!Inline.PrimaryAsset.IsEmpty())
			{
				InlineObj->SetStringField(TEXT("primary_asset"), Inline.PrimaryAsset);
			}
			if (Inline.CapabilityNotes.Num() > 0)
			{
				InlineObj->SetArrayField(TEXT("capability_notes"), StringArray(Inline.CapabilityNotes));
			}
			if (Inline.CreatedAssets.Num() > 0)
			{
				InlineObj->SetArrayField(TEXT("created_assets"), BuildAssetRefArray(Inline.CreatedAssets));
			}
			if (Inline.ModifiedAssets.Num() > 0)
			{
				InlineObj->SetArrayField(TEXT("modified_assets"), BuildAssetRefArray(Inline.ModifiedAssets));
			}
			if (Inline.ReusedAssets.Num() > 0)
			{
				InlineObj->SetArrayField(TEXT("reused_assets"), BuildAssetRefArray(Inline.ReusedAssets));
			}
			InlineValues.Add(MakeShared<FJsonValueObject>(InlineObj));
		}
		return InlineValues;
	}
}

TArray<FString> FUeremcpNiagaraMaterialBindingDiagnostics::FindOrphanedInlineCreates(
	const FUeremcpNiagaraMaterialBindingResult& Result)
{
	TArray<FString> Orphans;
	for (const FUeremcpNiagaraInlineMaterialCreate& Inline : Result.InlineMaterialCreates)
	{
		if (!Inline.bSuccess || Inline.PrimaryAsset.IsEmpty())
		{
			continue;
		}

		const FString Prefix = Inline.Role + TEXT(":");
		for (const FString& Unresolved : Result.UnresolvedMaterialBindings)
		{
			if (Unresolved.StartsWith(Prefix))
			{
				Orphans.AddUnique(Inline.Role);
				break;
			}
		}
	}
	return Orphans;
}

bool FUeremcpNiagaraMaterialBindingDiagnostics::ShouldContinueAfterBindingFailure(
	const FUeremcpNiagaraMaterialBindingResult& Result)
{
	return FindOrphanedInlineCreates(Result).Num() > 0;
}

void FUeremcpNiagaraMaterialBindingDiagnostics::AppendOrphanPartialFailureChecksSkipped(
	TArray<FString>& OutChecksSkipped)
{
	OutChecksSkipped.Add(TEXT("niagara.material_bindings"));
	OutChecksSkipped.Add(TEXT("niagara.material_bindings_orphaned_inline_creates"));
}

FString FUeremcpNiagaraMaterialBindingDiagnostics::BuildOrphanPartialFailureSummarySuffix(
	int32 OrphanCount)
{
	if (OrphanCount <= 0)
	{
		return FString();
	}

	return FString::Printf(
		TEXT(" %d orphaned inline material(s) saved under probe root but renderer bind unverified."),
		OrphanCount);
}

TSharedPtr<FJsonObject> FUeremcpNiagaraMaterialBindingDiagnostics::BuildMaterialBindingsObject(
	const FUeremcpNiagaraMaterialBindingResult& Result)
{
	if (Result.ResolvedMaterialPaths.Num() == 0
		&& Result.UnresolvedMaterialBindings.Num() == 0
		&& Result.InlineMaterialCreates.Num() == 0)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Materials = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> Resolved = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Result.ResolvedMaterialPaths)
	{
		Resolved->SetStringField(Pair.Key, Pair.Value);
	}
	Materials->SetObjectField(TEXT("resolved_paths"), Resolved);

	if (Result.RendererBindingsApplied.Num() > 0)
	{
		Materials->SetArrayField(
			TEXT("renderer_bindings_applied"),
			StringArray(Result.RendererBindingsApplied));
	}
	if (Result.RendererBindingsVerified.Num() > 0)
	{
		Materials->SetArrayField(
			TEXT("renderer_bindings_verified"),
			StringArray(Result.RendererBindingsVerified));
	}
	if (Result.UnresolvedMaterialBindings.Num() > 0)
	{
		Materials->SetArrayField(
			TEXT("unresolved"),
			StringArray(Result.UnresolvedMaterialBindings));
	}
	if (Result.InlineMaterialCreates.Num() > 0)
	{
		Materials->SetArrayField(
			TEXT("inline_material_creates"),
			BuildInlineMaterialCreatesArray(Result.InlineMaterialCreates));
	}

	const TArray<FString> OrphanedRoles = FindOrphanedInlineCreates(Result);
	if (OrphanedRoles.Num() > 0)
	{
		Materials->SetArrayField(TEXT("orphaned_inline_creates"), StringArray(OrphanedRoles));
	}

	return Materials;
}
