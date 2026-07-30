// UEREMCP — Niagara renderer material binding (WS-07).

#include "UeremcpNiagaraMaterialBinding.h"

#include "UeremcpNiagaraPaths.h"
#include "UeremcpNiagaraRoleNames.h"
#include "UeremcpMaterialNiagaraExport.h"

#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSystem.h"

#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	const TCHAR* GMaterialsProbeRoot = TEXT("/Game/__UeremcpTests/Materials");

	bool IsAllowedMaterialProbePath(const FString& AssetPath)
	{
		return AssetPath.StartsWith(GMaterialsProbeRoot)
			|| UeremcpNiagaraPaths::IsAllowedProbePath(AssetPath);
	}

	TSharedPtr<FJsonObject> ParsePropertyValuesJson(const FString& JsonText)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return nullptr;
		}
		return Root;
	}

	FString SerializePropertyValuesJson(const TSharedPtr<FJsonObject>& Root)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		return Out;
	}

	TSharedPtr<FJsonObject> MakeMaterialRefObject(const FString& CanonicalPath)
	{
		TSharedPtr<FJsonObject> Ref = MakeShared<FJsonObject>();
		Ref->SetStringField(TEXT("refPath"), CanonicalPath);
		return Ref;
	}

	FString ReadRefPathField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		if (!Object.IsValid())
		{
			return FString();
		}

		const TSharedPtr<FJsonObject>* RefObj = nullptr;
		if (Object->TryGetObjectField(FieldName, RefObj) && RefObj && RefObj->IsValid())
		{
			FString RefPath;
			if ((*RefObj)->TryGetStringField(TEXT("refPath"), RefPath))
			{
				return RefPath;
			}
		}

		FString Direct;
		if (Object->TryGetStringField(FieldName, Direct))
		{
			return Direct;
		}

		return FString();
	}

	UMaterialInterface* LoadMaterialInterface(const FString& AssetPath, FString& OutCanonicalPath)
	{
		OutCanonicalPath.Reset();
		if (AssetPath.IsEmpty())
		{
			return nullptr;
		}

		UObject* Loaded = FSoftObjectPath(AssetPath).TryLoad();
		if (!Loaded)
		{
			const FString AssetName = FPackageName::GetLongPackageAssetName(
				AssetPath.Contains(TEXT(".")) ? AssetPath.Left(AssetPath.Find(TEXT("."))) : AssetPath);
			const FString PackagePath = UeremcpNiagaraPaths::PackageFolderFromAssetPath(AssetPath);
			Loaded = FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName)).TryLoad();
		}

		UMaterialInterface* Material = Cast<UMaterialInterface>(Loaded);
		if (Material)
		{
			OutCanonicalPath = Material->GetPathName();
		}
		return Material;
	}

	bool HasValidUserMaterialBindingCpp(const FNiagaraUserParameterBinding& Binding)
	{
		return Binding.Parameter.IsValid() && !Binding.Parameter.GetName().IsNone();
	}

	bool TryEnableMeshMaterialOverridesDirect(UNiagaraMeshRendererProperties* MeshProps)
	{
		if (!MeshProps || MeshProps->OverrideMaterials.Num() == 0)
		{
			return false;
		}

		if (MeshProps->bOverrideMaterials)
		{
			return true;
		}

		MeshProps->Modify();
		MeshProps->bOverrideMaterials = true;
		return true;
	}

	bool TrySetMeshExplicitMaterialDirect(
		UNiagaraMeshRendererProperties* MeshProps,
		UMaterialInterface* Material,
		FString& OutConflict)
	{
		OutConflict.Reset();
		if (!MeshProps || !Material)
		{
			OutConflict = TEXT("missing mesh renderer or material");
			return false;
		}

		if (MeshProps->OverrideMaterials.Num() == 0)
		{
			OutConflict = TEXT("mesh renderer has no OverrideMaterials slots");
			return false;
		}

		if (!MeshProps->bOverrideMaterials)
		{
			OutConflict = TEXT("mesh renderer bOverrideMaterials must be enabled before patching ExplicitMat");
			return false;
		}

		for (const FNiagaraMeshMaterialOverride& Override : MeshProps->OverrideMaterials)
		{
			if (HasValidUserMaterialBindingCpp(Override.UserParamBinding))
			{
				OutConflict = TEXT("OverrideMaterials UserParamBinding wins over ExplicitMat");
				return false;
			}
		}

		MeshProps->Modify();
		int32 PatchedSlots = 0;
		for (FNiagaraMeshMaterialOverride& Override : MeshProps->OverrideMaterials)
		{
			Override.ExplicitMat = Material;
			++PatchedSlots;
		}

		if (PatchedSlots == 0)
		{
			OutConflict = TEXT("no mesh override slots patched");
			return false;
		}

		return true;
	}
}

bool FUeremcpNiagaraMaterialBinding::ParseMaterialRequests(
	const TSharedPtr<FJsonObject>& Specification,
	TArray<FUeremcpNiagaraMaterialRequest>& OutRequests,
	FString& OutError)
{
	OutRequests.Reset();
	OutError.Reset();

	if (!Specification.IsValid() || !Specification->HasTypedField<EJson::Object>(TEXT("materials")))
	{
		return true;
	}

	const TSharedPtr<FJsonObject> Materials = Specification->GetObjectField(TEXT("materials"));
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Materials->Values)
	{
		if (Entry.Key.IsEmpty())
		{
			OutError = TEXT("materials contains an empty role key.");
			return false;
		}

		FUeremcpNiagaraMaterialRequest Request;
		Request.Role = Entry.Key;

		if (Entry.Value->Type == EJson::String)
		{
			Request.ExistingAssetPath = Entry.Value->AsString();
		}
		else if (Entry.Value->Type == EJson::Object)
		{
			const TSharedPtr<FJsonObject> Obj = Entry.Value->AsObject();
			Obj->TryGetStringField(TEXT("asset_path"), Request.ExistingAssetPath);
			Obj->TryGetBoolField(TEXT("reuse_if_present"), Request.bReuseIfPresent);
			if (Obj->HasTypedField<EJson::Object>(TEXT("create_spec")))
			{
				Request.CreateSpec = Obj->GetObjectField(TEXT("create_spec"));
			}
			if (Request.ExistingAssetPath.IsEmpty() && !Request.CreateSpec.IsValid())
			{
				OutError = FString::Printf(
					TEXT("materials.%s must be an asset path string or an object with asset_path or create_spec."),
					*Request.Role);
				return false;
			}
		}
		else
		{
			OutError = FString::Printf(TEXT("materials.%s has unsupported JSON type."), *Request.Role);
			return false;
		}

		OutRequests.Add(Request);
	}

	return true;
}

bool FUeremcpNiagaraMaterialBinding::ResolveDirectMaterialPaths(
	const TArray<FUeremcpNiagaraMaterialRequest>& Requests,
	TMap<FString, FString>& OutRoleToCanonicalPath,
	TArray<FString>& OutUnresolved,
	FString& OutError)
{
	OutRoleToCanonicalPath.Reset();
	OutUnresolved.Reset();
	OutError.Reset();

		for (const FUeremcpNiagaraMaterialRequest& Request : Requests)
	{
		if (Request.CreateSpec.IsValid())
		{
			continue;
		}

		if (Request.ExistingAssetPath.IsEmpty())
		{
			OutUnresolved.Add(FString::Printf(TEXT("%s: empty material path"), *Request.Role));
			continue;
		}

		if (!IsAllowedMaterialProbePath(Request.ExistingAssetPath))
		{
			OutError = FString::Printf(
				TEXT("materials.%s path '%s' must be under %s or %s."),
				*Request.Role,
				*Request.ExistingAssetPath,
				GMaterialsProbeRoot,
				UeremcpNiagaraPaths::TestsContentRoot);
			return false;
		}

		FString CanonicalPath = Request.ExistingAssetPath;
		if (!CanonicalPath.Contains(TEXT(".")))
		{
			const FString AssetName = UeremcpNiagaraPaths::AssetNameFromAssetPath(CanonicalPath);
			const FString PackagePath = UeremcpNiagaraPaths::PackageFolderFromAssetPath(CanonicalPath);
			CanonicalPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
		}

		OutRoleToCanonicalPath.Add(Request.Role, CanonicalPath);
	}

	return true;
}

bool FUeremcpNiagaraMaterialBinding::ResolveMaterialPaths(
	const FString& NiagaraAssetName,
	const TArray<FUeremcpNiagaraMaterialRequest>& Requests,
	bool bCompile,
	bool bValidate,
	bool bSave,
	TMap<FString, FString>& OutRoleToCanonicalPath,
	TArray<FUeremcpNiagaraInlineMaterialCreate>& OutInlineCreates,
	TArray<FString>& OutUnresolved,
	int32& InOutInternalOperations,
	FString& OutError)
{
	OutRoleToCanonicalPath.Reset();
	OutInlineCreates.Reset();
	OutUnresolved.Reset();
	OutError.Reset();

	for (const FUeremcpNiagaraMaterialRequest& Request : Requests)
	{
		if (Request.CreateSpec.IsValid())
		{
			const FString TargetPath = UeremcpMaterialNiagaraExport::ResolveMaterialInstancePath(
				NiagaraAssetName,
				Request.Role);
			if (!IsAllowedMaterialProbePath(TargetPath))
			{
				OutError = FString::Printf(
					TEXT("materials.%s inline create target '%s' must be under %s or %s."),
					*Request.Role,
					*TargetPath,
					GMaterialsProbeRoot,
					UeremcpNiagaraPaths::TestsContentRoot);
				return false;
			}

			const FUeremcpMaterialCreateResult MatResult =
				UeremcpMaterialNiagaraExport::ExecuteCreateVfxMaterialForNiagaraRole(
					NiagaraAssetName,
					Request.Role,
					Request.CreateSpec,
					bCompile,
					bValidate,
					bSave);
			InOutInternalOperations += MatResult.InternalOperations;

			FUeremcpNiagaraInlineMaterialCreate InlineRecord;
			InlineRecord.Role = Request.Role;
			InlineRecord.bSuccess = MatResult.bSuccess;
			InlineRecord.Status = MatResult.Status;
			InlineRecord.Summary = MatResult.Summary;
			InlineRecord.PrimaryAsset = MatResult.PrimaryAsset;
			InlineRecord.CreatedAssets = MatResult.CreatedAssets;
			InlineRecord.CapabilityNotes = MatResult.CapabilityNotes;
			OutInlineCreates.Add(InlineRecord);

			if (!MatResult.bSuccess || MatResult.PrimaryAsset.IsEmpty())
			{
				OutUnresolved.Add(FString::Printf(
					TEXT("%s: inline create_spec failed (status=%s)"),
					*Request.Role,
					MatResult.Status.IsEmpty() ? TEXT("unknown") : *MatResult.Status));
				continue;
			}

			FString VerifyError;
			if (!UeremcpMaterialNiagaraExport::VerifyPrimaryAssetIsMaterialInterface(
				MatResult.PrimaryAsset,
				VerifyError))
			{
				OutUnresolved.Add(FString::Printf(
					TEXT("%s: PrimaryAsset verification failed — %s"),
					*Request.Role,
					*VerifyError));
				continue;
			}

			FString CanonicalPath;
			UMaterialInterface* Material = LoadMaterialInterface(MatResult.PrimaryAsset, CanonicalPath);
			if (!Material || CanonicalPath.IsEmpty())
			{
				OutUnresolved.Add(FString::Printf(
					TEXT("%s: could not load canonical UMaterialInterface from '%s'"),
					*Request.Role,
					*MatResult.PrimaryAsset));
				continue;
			}

			OutRoleToCanonicalPath.Add(Request.Role, CanonicalPath);
			continue;
		}

		if (Request.ExistingAssetPath.IsEmpty())
		{
			OutUnresolved.Add(FString::Printf(TEXT("%s: empty material path"), *Request.Role));
			continue;
		}

		if (!IsAllowedMaterialProbePath(Request.ExistingAssetPath))
		{
			OutError = FString::Printf(
				TEXT("materials.%s path '%s' must be under %s or %s."),
				*Request.Role,
				*Request.ExistingAssetPath,
				GMaterialsProbeRoot,
				UeremcpNiagaraPaths::TestsContentRoot);
			return false;
		}

		FString CanonicalPath;
		UMaterialInterface* Material = LoadMaterialInterface(Request.ExistingAssetPath, CanonicalPath);
		if (!Material || CanonicalPath.IsEmpty())
		{
			OutUnresolved.Add(FString::Printf(
				TEXT("%s: could not load UMaterialInterface at '%s'"),
				*Request.Role,
				*Request.ExistingAssetPath));
			continue;
		}

		OutRoleToCanonicalPath.Add(Request.Role, CanonicalPath);
	}

	return true;
}

EUeremcpNiagaraRendererMaterialKind FUeremcpNiagaraMaterialBinding::ClassifyRenderer(
	const FString& RendererClassPath)
{
	if (RendererClassPath.Contains(TEXT("SpriteRenderer")))
	{
		return EUeremcpNiagaraRendererMaterialKind::Sprite;
	}
	if (RendererClassPath.Contains(TEXT("RibbonRenderer")))
	{
		return EUeremcpNiagaraRendererMaterialKind::Ribbon;
	}
	if (RendererClassPath.Contains(TEXT("MeshRenderer")))
	{
		return EUeremcpNiagaraRendererMaterialKind::Mesh;
	}
	return EUeremcpNiagaraRendererMaterialKind::Unsupported;
}

bool FUeremcpNiagaraMaterialBinding::HasValidUserMaterialBinding(
	const TSharedPtr<FJsonObject>& PropertyValues,
	const FString& BindingFieldName)
{
	if (!PropertyValues.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Binding = nullptr;
	if (!PropertyValues->TryGetObjectField(BindingFieldName, Binding) || !Binding || !Binding->IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Parameter = nullptr;
	if ((*Binding)->TryGetObjectField(TEXT("Parameter"), Parameter) && Parameter && Parameter->IsValid())
	{
		FString Name;
		if ((*Parameter)->TryGetStringField(TEXT("Name"), Name) && !Name.IsEmpty())
		{
			return true;
		}
	}

	return false;
}

bool FUeremcpNiagaraMaterialBinding::PatchSpriteOrRibbonMaterial(
	TSharedPtr<FJsonObject>& PropertyValues,
	const FString& CanonicalMaterialPath,
	FString& OutConflictReason)
{
	OutConflictReason.Reset();
	if (!PropertyValues.IsValid())
	{
		OutConflictReason = TEXT("missing PropertyValues JSON");
		return false;
	}

	if (HasValidUserMaterialBinding(PropertyValues, TEXT("MaterialUserParamBinding")))
	{
		OutConflictReason = TEXT("MaterialUserParamBinding wins over direct Material assignment");
		return false;
	}

	PropertyValues->SetObjectField(TEXT("Material"), MakeMaterialRefObject(CanonicalMaterialPath));
	return true;
}

bool FUeremcpNiagaraMaterialBinding::EnableMeshMaterialOverrides(
	TSharedPtr<FJsonObject>& PropertyValues)
{
	if (!PropertyValues.IsValid())
	{
		return false;
	}

	bool bAlreadyEnabled = false;
	PropertyValues->TryGetBoolField(TEXT("bOverrideMaterials"), bAlreadyEnabled);
	if (bAlreadyEnabled)
	{
		return true;
	}

	const TArray<TSharedPtr<FJsonValue>>* Overrides = nullptr;
	if (!PropertyValues->TryGetArrayField(TEXT("OverrideMaterials"), Overrides)
		|| !Overrides
		|| Overrides->Num() == 0)
	{
		return false;
	}

	PropertyValues->SetBoolField(TEXT("bOverrideMaterials"), true);
	return true;
}

bool FUeremcpNiagaraMaterialBinding::PatchMeshRendererMaterial(
	TSharedPtr<FJsonObject>& PropertyValues,
	const FString& CanonicalMaterialPath,
	FString& OutConflictReason)
{
	OutConflictReason.Reset();
	if (!PropertyValues.IsValid())
	{
		OutConflictReason = TEXT("missing PropertyValues JSON");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Overrides = nullptr;
	if (!PropertyValues->TryGetArrayField(TEXT("OverrideMaterials"), Overrides)
		|| !Overrides
		|| Overrides->Num() == 0)
	{
		OutConflictReason = TEXT("mesh renderer has no OverrideMaterials slots");
		return false;
	}

	bool bOverrideEnabled = false;
	PropertyValues->TryGetBoolField(TEXT("bOverrideMaterials"), bOverrideEnabled);
	if (!bOverrideEnabled)
	{
		OutConflictReason = TEXT("mesh renderer bOverrideMaterials must be enabled before patching ExplicitMat");
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> NewOverrides;
	for (const TSharedPtr<FJsonValue>& Entry : *Overrides)
	{
		TSharedPtr<FJsonObject> Slot = Entry->AsObject();
		if (!Slot.IsValid())
		{
			continue;
		}

		if (HasValidUserMaterialBinding(Slot, TEXT("UserParamBinding")))
		{
			OutConflictReason = TEXT("OverrideMaterials UserParamBinding wins over ExplicitMat");
			return false;
		}

		Slot->SetObjectField(TEXT("ExplicitMat"), MakeMaterialRefObject(CanonicalMaterialPath));
		NewOverrides.Add(MakeShared<FJsonValueObject>(Slot));
	}

	if (NewOverrides.Num() == 0)
	{
		OutConflictReason = TEXT("no mesh override slots patched");
		return false;
	}

	PropertyValues->SetArrayField(TEXT("OverrideMaterials"), NewOverrides);
	return true;
}

bool FUeremcpNiagaraMaterialBinding::MaterialMatchesExpectedAfterReread(
	const FString& PropertyValuesJson,
	EUeremcpNiagaraRendererMaterialKind Kind,
	const FString& ExpectedCanonicalPath)
{
	const TSharedPtr<FJsonObject> Root = ParsePropertyValuesJson(PropertyValuesJson);
	if (!Root.IsValid())
	{
		return false;
	}

	if (Kind == EUeremcpNiagaraRendererMaterialKind::Sprite
		|| Kind == EUeremcpNiagaraRendererMaterialKind::Ribbon)
	{
		return ReadRefPathField(Root, TEXT("Material")) == ExpectedCanonicalPath;
	}

	if (Kind == EUeremcpNiagaraRendererMaterialKind::Mesh)
	{
		const TArray<TSharedPtr<FJsonValue>>* Overrides = nullptr;
		if (!Root->TryGetArrayField(TEXT("OverrideMaterials"), Overrides) || !Overrides)
		{
			return false;
		}

		for (const TSharedPtr<FJsonValue>& Entry : *Overrides)
		{
			const TSharedPtr<FJsonObject> Slot = Entry->AsObject();
			if (!Slot.IsValid())
			{
				continue;
			}
			if (ReadRefPathField(Slot, TEXT("ExplicitMat")) == ExpectedCanonicalPath)
			{
				return true;
			}
		}
		return false;
	}

	return false;
}

bool FUeremcpNiagaraMaterialBinding::ApplyRoleMaterialBindings(
	UNiagaraSystem* System,
	const TArray<FString>& EmittersAdded,
	const TMap<FString, FString>& RoleToCanonicalMaterialPath,
	const TArray<FUeremcpNiagaraMaterialRequest>& Requests,
	FNiagaraExternalEditContext& Context,
	FUeremcpNiagaraMaterialBindingResult& OutResult,
	int32& InOutInternalOperations)
{
	const TArray<FUeremcpNiagaraInlineMaterialCreate> PreservedInlineCreates = OutResult.InlineMaterialCreates;
	const TArray<FString> PreservedPreBindUnresolved = OutResult.UnresolvedMaterialBindings;

	OutResult = FUeremcpNiagaraMaterialBindingResult();
	OutResult.InlineMaterialCreates = PreservedInlineCreates;
	OutResult.UnresolvedMaterialBindings = PreservedPreBindUnresolved;
	OutResult.ResolvedMaterialPaths = RoleToCanonicalMaterialPath;
	OutResult.bAttempted = RoleToCanonicalMaterialPath.Num() > 0;

	if (!System || RoleToCanonicalMaterialPath.Num() == 0)
	{
		return true;
	}

	TSet<FString> VerifiedRoles;

	for (const TPair<FString, FString>& RoleMaterial : RoleToCanonicalMaterialPath)
	{
		const FString& Role = RoleMaterial.Key;
		const FString& CanonicalMaterialPath = RoleMaterial.Value;
		const FString EmitterName = UeremcpNiagaraRoles::RoleToEmitterName(Role);

		if (!EmittersAdded.Contains(EmitterName))
		{
			OutResult.UnresolvedMaterialBindings.Add(FString::Printf(
				TEXT("%s: emitter '%s' was not created"),
				*Role,
				*EmitterName));
			continue;
		}

		FNiagaraExt_StackItemReference EmitterRef(System, FName(*EmitterName));
		FNiagaraExt_EmitterTopology Topology;
		UNiagaraExternalEditUtilities::GetEmitterTopology(EmitterRef, Topology, Context);
		++InOutInternalOperations;

		if (Topology.Renderers.Num() == 0)
		{
			OutResult.UnresolvedMaterialBindings.Add(FString::Printf(
				TEXT("%s: emitter '%s' has no renderers"),
				*Role,
				*EmitterName));
			continue;
		}

		bool bRoleVerified = false;

		for (const FNiagaraExt_RendererRef& Renderer : Topology.Renderers)
		{
			const FString RendererClassPath = Renderer.RendererClass
				? Renderer.RendererClass->GetPathName()
				: FString();
			const EUeremcpNiagaraRendererMaterialKind Kind = ClassifyRenderer(RendererClassPath);

			if (Kind == EUeremcpNiagaraRendererMaterialKind::Unsupported)
			{
				OutResult.UnresolvedMaterialBindings.Add(FString::Printf(
					TEXT("%s: unsupported renderer class '%s'"),
					*Role,
					*RendererClassPath));
				continue;
			}

			FNiagaraExt_StackItemReference RendererRef(System, FName(*EmitterName));
			RendererRef.RendererIndex = Renderer.RendererIndex;

			FNiagaraExt_RendererData RendererData;
			UNiagaraExternalEditUtilities::GetRendererData(RendererRef, RendererData, Context);
			++InOutInternalOperations;

			TSharedPtr<FJsonObject> PropertyValues = ParsePropertyValuesJson(RendererData.PropertyValues);
			if (!PropertyValues.IsValid())
			{
				OutResult.UnresolvedMaterialBindings.Add(FString::Printf(
					TEXT("%s: renderer %d PropertyValues JSON unreadable"),
					*Role,
					Renderer.RendererIndex));
				continue;
			}

			FString Conflict;
			bool bPatched = false;
			if (Kind == EUeremcpNiagaraRendererMaterialKind::Sprite
				|| Kind == EUeremcpNiagaraRendererMaterialKind::Ribbon)
			{
				bPatched = PatchSpriteOrRibbonMaterial(PropertyValues, CanonicalMaterialPath, Conflict);
			}
			else
			{
				UNiagaraRendererProperties* RendererProps = RendererRef.GetRenderer(Context, false);
				UNiagaraMeshRendererProperties* MeshProps = Cast<UNiagaraMeshRendererProperties>(RendererProps);
				if (!MeshProps)
				{
					OutResult.UnresolvedMaterialBindings.Add(FString::Printf(
						TEXT("%s: renderer %d is not a mesh renderer"),
						*Role,
						Renderer.RendererIndex));
					continue;
				}

				if (!TryEnableMeshMaterialOverridesDirect(MeshProps))
				{
					OutResult.UnresolvedMaterialBindings.Add(FString::Printf(
						TEXT("%s: failed enabling mesh material overrides on renderer %d"),
						*Role,
						Renderer.RendererIndex));
					continue;
				}
				++InOutInternalOperations;

				UMaterialInterface* Material = Cast<UMaterialInterface>(
					FSoftObjectPath(CanonicalMaterialPath).TryLoad());
				if (!Material)
				{
					OutResult.UnresolvedMaterialBindings.Add(FString::Printf(
						TEXT("%s: could not load material '%s' for renderer %d"),
						*Role,
						*CanonicalMaterialPath,
						Renderer.RendererIndex));
					continue;
				}

				bPatched = TrySetMeshExplicitMaterialDirect(MeshProps, Material, Conflict);
			}

			if (!bPatched)
			{
				OutResult.UnresolvedMaterialBindings.Add(FString::Printf(
					TEXT("%s: renderer %d not patched — %s"),
					*Role,
					Renderer.RendererIndex,
					*Conflict));
				continue;
			}

			if (Kind == EUeremcpNiagaraRendererMaterialKind::Sprite
				|| Kind == EUeremcpNiagaraRendererMaterialKind::Ribbon)
			{
				FNiagaraExt_RendererData PatchedData;
				PatchedData.PropertyValues = SerializePropertyValuesJson(PropertyValues);
				UNiagaraExternalEditUtilities::SetRendererData(RendererRef, PatchedData, Context);
				++InOutInternalOperations;

				if (Context.HasErrors())
				{
					OutResult.bAnyBindingFailedReread = true;
					OutResult.UnresolvedMaterialBindings.Add(FString::Printf(
						TEXT("%s: SetRendererData failed for renderer %d"),
						*Role,
						Renderer.RendererIndex));
					continue;
				}
			}

			const FString BindingKey = FString::Printf(
				TEXT("%s/renderer_%d"),
				*Role,
				Renderer.RendererIndex);
			OutResult.RendererBindingsApplied.Add(BindingKey);

			bool bVerified = false;
			if (Kind == EUeremcpNiagaraRendererMaterialKind::Mesh)
			{
				UNiagaraMeshRendererProperties* MeshProps = Cast<UNiagaraMeshRendererProperties>(
					RendererRef.GetRenderer(Context, false));
				bVerified = MeshRendererMaterialMatchesExpected(MeshProps, CanonicalMaterialPath);
			}
			else
			{
				FNiagaraExt_RendererData RereadData;
				UNiagaraExternalEditUtilities::GetRendererData(RendererRef, RereadData, Context);
				++InOutInternalOperations;
				bVerified = MaterialMatchesExpectedAfterReread(
					RereadData.PropertyValues,
					Kind,
					CanonicalMaterialPath);
			}

			if (bVerified)
			{
				OutResult.RendererBindingsVerified.Add(BindingKey);
				bRoleVerified = true;
			}
			else
			{
				OutResult.bAnyBindingFailedReread = true;
				OutResult.UnresolvedMaterialBindings.Add(FString::Printf(
					TEXT("%s: renderer %d re-read material path mismatch"),
					*Role,
					Renderer.RendererIndex));
			}
		}

		if (bRoleVerified)
		{
			VerifiedRoles.Add(Role);
		}
	}

	for (const FUeremcpNiagaraMaterialRequest& Request : Requests)
	{
		if (Request.CreateSpec.IsValid())
		{
			continue;
		}
		if (RoleToCanonicalMaterialPath.Contains(Request.Role) && !VerifiedRoles.Contains(Request.Role))
		{
			// Already recorded unresolved per renderer.
		}
	}

	const int32 RolesRequested = RoleToCanonicalMaterialPath.Num();
	OutResult.bAllRequestedVerified = RolesRequested > 0
		&& VerifiedRoles.Num() == RolesRequested
		&& !OutResult.bAnyBindingFailedReread;

	return !OutResult.bAnyBindingFailedReread;
}

void FUeremcpNiagaraMaterialBinding::NormalizeMeshRendererOverrideFlags(
	UNiagaraSystem* System,
	FNiagaraExternalEditContext& Context,
	int32& InOutInternalOperations)
{
	if (!System)
	{
		return;
	}

	for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		const FName EmitterName = Handle.GetName();
		FNiagaraExt_StackItemReference EmitterRef(System, EmitterName);
		FNiagaraExt_EmitterTopology Topology;
		UNiagaraExternalEditUtilities::GetEmitterTopology(EmitterRef, Topology, Context);
		++InOutInternalOperations;

		for (const FNiagaraExt_RendererRef& Renderer : Topology.Renderers)
		{
			const FString RendererClassPath = Renderer.RendererClass
				? Renderer.RendererClass->GetPathName()
				: FString();
			if (ClassifyRenderer(RendererClassPath) != EUeremcpNiagaraRendererMaterialKind::Mesh)
			{
				continue;
			}

			FNiagaraExt_StackItemReference RendererRef(System, EmitterName);
			RendererRef.RendererIndex = Renderer.RendererIndex;

			UNiagaraMeshRendererProperties* MeshProps = Cast<UNiagaraMeshRendererProperties>(
				RendererRef.GetRenderer(Context, false));
			if (!MeshProps || MeshProps->bOverrideMaterials || MeshProps->OverrideMaterials.Num() == 0)
			{
				continue;
			}

			if (TryEnableMeshMaterialOverridesDirect(MeshProps))
			{
				++InOutInternalOperations;
			}
		}
	}
}

TSharedPtr<FJsonObject> FUeremcpNiagaraMaterialBinding::BuildMeshRendererObservabilityPropertyValues(
	UNiagaraMeshRendererProperties* MeshProps)
{
	if (!MeshProps)
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(TEXT("bOverrideMaterials"), MeshProps->bOverrideMaterials != 0);

	TArray<TSharedPtr<FJsonValue>> Overrides;
	for (const FNiagaraMeshMaterialOverride& Override : MeshProps->OverrideMaterials)
	{
		TSharedPtr<FJsonObject> Slot = MakeShared<FJsonObject>();
		if (Override.ExplicitMat)
		{
			Slot->SetObjectField(TEXT("ExplicitMat"), MakeMaterialRefObject(Override.ExplicitMat->GetPathName()));
		}

		if (HasValidUserMaterialBindingCpp(Override.UserParamBinding))
		{
			TSharedPtr<FJsonObject> Binding = MakeShared<FJsonObject>();
			TSharedPtr<FJsonObject> Parameter = MakeShared<FJsonObject>();
			Parameter->SetStringField(
				TEXT("Name"),
				Override.UserParamBinding.Parameter.GetName().ToString());
			Binding->SetObjectField(TEXT("Parameter"), Parameter);
			Slot->SetObjectField(TEXT("UserParamBinding"), Binding);
		}

		Overrides.Add(MakeShared<FJsonValueObject>(Slot));
	}

	Root->SetArrayField(TEXT("OverrideMaterials"), Overrides);
	return Root;
}

FString FUeremcpNiagaraMaterialBinding::ExtractMaterialPathFromMeshRenderer(
	UNiagaraMeshRendererProperties* MeshProps)
{
	if (!MeshProps)
	{
		return FString();
	}

	for (const FNiagaraMeshMaterialOverride& Override : MeshProps->OverrideMaterials)
	{
		if (Override.ExplicitMat)
		{
			return Override.ExplicitMat->GetPathName();
		}
	}

	return FString();
}

bool FUeremcpNiagaraMaterialBinding::MeshRendererMaterialMatchesExpected(
	UNiagaraMeshRendererProperties* MeshProps,
	const FString& ExpectedCanonicalPath)
{
	if (!MeshProps)
	{
		return false;
	}

	for (const FNiagaraMeshMaterialOverride& Override : MeshProps->OverrideMaterials)
	{
		if (Override.ExplicitMat && Override.ExplicitMat->GetPathName() == ExpectedCanonicalPath)
		{
			return true;
		}
	}

	return false;
}
