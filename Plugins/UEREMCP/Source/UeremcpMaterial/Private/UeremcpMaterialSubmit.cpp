// UEREMCP — submit edited MaterialGraph / parameters (WS-08).
//
// Prefer in-place production edits. Never silent-delete user masters.
// [VERIFIED: MaterialEditingLibrary.h:168 CreateMaterialExpression]
// [VERIFIED: MaterialEditingLibrary.h:232 ConnectMaterialProperty]
// [VERIFIED: MaterialEditingLibrary.h:242 ConnectMaterialExpressions]
// [VERIFIED: MaterialEditingLibrary.h:251 DisconnectMaterialExpressions]
// [VERIFIED: MaterialEditingLibrary.h:154 DeleteMaterialExpression]

#include "UeremcpMaterialSubmit.h"

#include "Editor.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UeremcpMaterialAssetLoad.h"
#include "UeremcpMaterialCapabilityNotes.h"
#include "UeremcpMaterialPaths.h"

namespace
{
	void AddChange(
		TArray<TSharedPtr<FJsonValue>>& Changes,
		const FString& Kind,
		const FString& Detail,
		bool bApplied)
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("kind"), Kind);
		Row->SetStringField(TEXT("detail"), Detail);
		Row->SetBoolField(TEXT("applied"), bApplied);
		Changes.Add(MakeShared<FJsonValueObject>(Row));
	}

	bool IsReplaceMode(const FString& Mode)
	{
		return Mode.Equals(TEXT("replace"), ESearchCase::IgnoreCase)
			|| Mode.Equals(TEXT("rebuild_from_specification"), ESearchCase::IgnoreCase);
	}

	UMaterialInterface* LoadMaterialInterface(const FString& Path)
	{
		if (UObject* Obj = UeremcpMaterialAssetLoad::TryLoadRegisteredAsset(Path))
		{
			return Cast<UMaterialInterface>(Obj);
		}
		return LoadObject<UMaterialInterface>(nullptr, *Path);
	}

	bool ParseColorArray(const TArray<TSharedPtr<FJsonValue>>& Arr, FLinearColor& OutColor)
	{
		if (Arr.Num() < 3)
		{
			return false;
		}
		OutColor.R = Arr[0]->AsNumber();
		OutColor.G = Arr[1]->AsNumber();
		OutColor.B = Arr[2]->AsNumber();
		OutColor.A = Arr.Num() > 3 ? Arr[3]->AsNumber() : 1.0;
		return true;
	}

	bool ApplyMicParameters(
		UMaterialInstanceConstant* MIC,
		const TSharedPtr<FJsonObject>& ParamsObjIn,
		bool bDryRun,
		TArray<TSharedPtr<FJsonValue>>& Planned,
		TArray<TSharedPtr<FJsonValue>>& Applied,
		TArray<FString>& Errors,
		int32& Ops)
	{
		if (!MIC || !ParamsObjIn.IsValid())
		{
			return true;
		}

		// Normalize inspect array inventory → map form.
		TSharedPtr<FJsonObject> ParamsObj = ParamsObjIn;
		{
			TSharedPtr<FJsonObject> Converted = MakeShared<FJsonObject>();
			bool bConverted = false;
			auto MaybeConvert = [&](const TCHAR* Field)
			{
				const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
				if (!ParamsObjIn->TryGetArrayField(Field, Arr) || !Arr)
				{
					const TSharedPtr<FJsonObject>* ExistingMap = nullptr;
					if (ParamsObjIn->TryGetObjectField(Field, ExistingMap) && ExistingMap)
					{
						Converted->SetObjectField(Field, *ExistingMap);
					}
					return;
				}
				bConverted = true;
				TSharedPtr<FJsonObject> Map = MakeShared<FJsonObject>();
				for (const TSharedPtr<FJsonValue>& Item : *Arr)
				{
					const TSharedPtr<FJsonObject> Row = Item->AsObject();
					if (!Row.IsValid())
					{
						continue;
					}
					FString Name;
					if (!Row->TryGetStringField(TEXT("name"), Name))
					{
						continue;
					}
					if (FCString::Stricmp(Field, TEXT("scalar")) == 0)
					{
						double Number = 0.0;
						if (Row->TryGetNumberField(TEXT("value"), Number))
						{
							Map->SetNumberField(Name, Number);
						}
					}
					else if (FCString::Stricmp(Field, TEXT("vector")) == 0)
					{
						const TArray<TSharedPtr<FJsonValue>>* Vec = nullptr;
						if (Row->TryGetArrayField(TEXT("value"), Vec) && Vec)
						{
							Map->SetArrayField(Name, *Vec);
						}
					}
					else if (FCString::Stricmp(Field, TEXT("texture")) == 0)
					{
						FString Tex;
						if (Row->TryGetStringField(TEXT("value"), Tex))
						{
							Map->SetStringField(Name, Tex);
						}
					}
					else if (FCString::Stricmp(Field, TEXT("static_switch")) == 0)
					{
						bool bVal = false;
						if (Row->TryGetBoolField(TEXT("value"), bVal))
						{
							Map->SetBoolField(Name, bVal);
						}
					}
				}
				if (Map->Values.Num() > 0)
				{
					Converted->SetObjectField(Field, Map);
				}
			};
			MaybeConvert(TEXT("scalar"));
			MaybeConvert(TEXT("vector"));
			MaybeConvert(TEXT("texture"));
			MaybeConvert(TEXT("static_switch"));
			if (bConverted || Converted->Values.Num() > 0)
			{
				ParamsObj = Converted;
			}
		}

		bool bAny = false;

		const TSharedPtr<FJsonObject>* ScalarMap = nullptr;
		if (ParamsObj->TryGetObjectField(TEXT("scalar"), ScalarMap) && ScalarMap && ScalarMap->IsValid())
		{
			for (const auto& Pair : (*ScalarMap)->Values)
			{
				const FName Name(*Pair.Key);
				const float Value = static_cast<float>(Pair.Value->AsNumber());
				const float Before = UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(MIC, Name);
				++Ops;
				const FString Detail = FString::Printf(TEXT("%s: %f -> %f"), *Pair.Key, Before, Value);
				AddChange(Planned, TEXT("scalar"), Detail, false);
				if (!bDryRun)
				{
					UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MIC, Name, Value);
					++Ops;
					AddChange(Applied, TEXT("scalar"), Detail, true);
				}
				bAny = true;
			}
		}

		const TSharedPtr<FJsonObject>* VectorMap = nullptr;
		if (ParamsObj->TryGetObjectField(TEXT("vector"), VectorMap) && VectorMap && VectorMap->IsValid())
		{
			for (const auto& Pair : (*VectorMap)->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
				if (!Pair.Value->TryGetArray(Arr) || !Arr)
				{
					Errors.Add(TEXT("vector override must be [r,g,b] or [r,g,b,a]"));
					continue;
				}
				FLinearColor Value;
				if (!ParseColorArray(*Arr, Value))
				{
					Errors.Add(FString::Printf(TEXT("invalid vector for %s"), *Pair.Key));
					continue;
				}
				const FName Name(*Pair.Key);
				const FLinearColor Before =
					UMaterialEditingLibrary::GetMaterialInstanceVectorParameterValue(MIC, Name);
				++Ops;
				const FString Detail = FString::Printf(
					TEXT("%s: (%f,%f,%f,%f) -> (%f,%f,%f,%f)"),
					*Pair.Key,
					Before.R, Before.G, Before.B, Before.A,
					Value.R, Value.G, Value.B, Value.A);
				AddChange(Planned, TEXT("vector"), Detail, false);
				if (!bDryRun)
				{
					UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MIC, Name, Value);
					++Ops;
					AddChange(Applied, TEXT("vector"), Detail, true);
				}
				bAny = true;
			}
		}

		const TSharedPtr<FJsonObject>* TextureMap = nullptr;
		if (ParamsObj->TryGetObjectField(TEXT("texture"), TextureMap) && TextureMap && TextureMap->IsValid())
		{
			for (const auto& Pair : (*TextureMap)->Values)
			{
				const FString TexPath = Pair.Value->AsString();
				UTexture* Texture = LoadObject<UTexture>(nullptr, *TexPath);
				if (!Texture)
				{
					Errors.Add(FString::Printf(TEXT("texture not found: %s"), *TexPath));
					continue;
				}
				const FName Name(*Pair.Key);
				UTexture* Before =
					UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(MIC, Name);
				++Ops;
				const FString Detail = FString::Printf(
					TEXT("%s: %s -> %s"),
					*Pair.Key,
					Before ? *Before->GetPathName() : TEXT("null"),
					*TexPath);
				AddChange(Planned, TEXT("texture"), Detail, false);
				if (!bDryRun)
				{
					UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MIC, Name, Texture);
					++Ops;
					AddChange(Applied, TEXT("texture"), Detail, true);
				}
				bAny = true;
			}
		}

		return bAny || Errors.Num() == 0;
	}

	TSharedPtr<FJsonObject> ParametersFromGraphs(const TArray<TSharedPtr<FJsonValue>>& Graphs)
	{
		for (const TSharedPtr<FJsonValue>& GraphValue : Graphs)
		{
			const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
			if (!Graph.IsValid())
			{
				continue;
			}
			const TSharedPtr<FJsonObject>* Extensions = nullptr;
			if (!Graph->TryGetObjectField(TEXT("extensions"), Extensions) || !Extensions)
			{
				continue;
			}
			const TSharedPtr<FJsonObject>* Material = nullptr;
			if (!(*Extensions)->TryGetObjectField(TEXT("material"), Material) || !Material)
			{
				continue;
			}
			const TSharedPtr<FJsonObject>* Params = nullptr;
			if ((*Material)->TryGetObjectField(TEXT("parameters"), Params) && Params)
			{
				// Convert array inventory → map form for ApplyMicParameters map path,
				// and keep arrays for the array path.
				TSharedPtr<FJsonObject> Converted = MakeShared<FJsonObject>();
				auto ConvertArrayToMap = [&](const TCHAR* Field)
				{
					const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
					if (!(*Params)->TryGetArrayField(Field, Arr) || !Arr)
					{
						return;
					}
					TSharedPtr<FJsonObject> Map = MakeShared<FJsonObject>();
					for (const TSharedPtr<FJsonValue>& Item : *Arr)
					{
						const TSharedPtr<FJsonObject> Row = Item->AsObject();
						if (!Row.IsValid())
						{
							continue;
						}
						FString Name;
						if (!Row->TryGetStringField(TEXT("name"), Name))
						{
							continue;
						}
						if (FCString::Stricmp(Field, TEXT("scalar")) == 0)
						{
							double Number = 0.0;
							if (Row->TryGetNumberField(TEXT("value"), Number))
							{
								Map->SetNumberField(Name, Number);
							}
						}
						else if (FCString::Stricmp(Field, TEXT("vector")) == 0)
						{
							const TArray<TSharedPtr<FJsonValue>>* Vec = nullptr;
							if (Row->TryGetArrayField(TEXT("value"), Vec) && Vec)
							{
								Map->SetArrayField(Name, *Vec);
							}
						}
						else if (FCString::Stricmp(Field, TEXT("texture")) == 0)
						{
							FString Tex;
							if (Row->TryGetStringField(TEXT("value"), Tex))
							{
								Map->SetStringField(Name, Tex);
							}
						}
						else if (FCString::Stricmp(Field, TEXT("static_switch")) == 0)
						{
							bool bVal = false;
							if (Row->TryGetBoolField(TEXT("value"), bVal))
							{
								Map->SetBoolField(Name, bVal);
							}
						}
					}
					if (Map->Values.Num() > 0)
					{
						Converted->SetObjectField(Field, Map);
					}
				};
				ConvertArrayToMap(TEXT("scalar"));
				ConvertArrayToMap(TEXT("vector"));
				ConvertArrayToMap(TEXT("texture"));
				ConvertArrayToMap(TEXT("static_switch"));
				if (Converted->Values.Num() > 0)
				{
					return Converted;
				}
			}
		}
		return nullptr;
	}

	bool ApplyMasterLinks(
		UMaterial* Material,
		const TSharedPtr<FJsonObject>& Graph,
		bool bDryRun,
		bool bApplyLinks,
		bool bApplyPropertyInputs,
		TArray<TSharedPtr<FJsonValue>>& Planned,
		TArray<TSharedPtr<FJsonValue>>& Applied,
		TArray<FString>& Errors,
		int32& Ops)
	{
		if (!Material || !Graph.IsValid())
		{
			return true;
		}

		TMap<FString, UMaterialExpression*> IdToExpr;
		const TArray<UMaterialExpression*> Expressions =
			UMaterialEditingLibrary::GetMaterialExpressions(Material);
		++Ops;
		for (int32 Index = 0; Index < Expressions.Num(); ++Index)
		{
			UMaterialExpression* Expr = Expressions[Index];
			if (!Expr)
			{
				continue;
			}
			FString NodeId = Expr->MaterialExpressionGuid.IsValid()
				? Expr->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower)
				: FString::Printf(TEXT("%s_%d"), *Expr->GetClass()->GetName(), Index);
			IdToExpr.Add(NodeId, Expr);
		}

		if (bApplyLinks)
		{
			const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
			if (Graph->TryGetArrayField(TEXT("links"), Links) && Links)
			{
				for (const TSharedPtr<FJsonValue>& LinkValue : *Links)
				{
					const TSharedPtr<FJsonObject> Link = LinkValue->AsObject();
					if (!Link.IsValid())
					{
						continue;
					}
					FString FromNode, FromPin, ToNode, ToPin;
					Link->TryGetStringField(TEXT("from_node"), FromNode);
					Link->TryGetStringField(TEXT("from_pin"), FromPin);
					Link->TryGetStringField(TEXT("to_node"), ToNode);
					Link->TryGetStringField(TEXT("to_pin"), ToPin);
					UMaterialExpression** FromExpr = IdToExpr.Find(FromNode);
					UMaterialExpression** ToExpr = IdToExpr.Find(ToNode);
					if (!FromExpr || !ToExpr)
					{
						Errors.Add(FString::Printf(
							TEXT("link skipped — missing node_id (%s -> %s); create_missing_expressions not enabled or nodes not found"),
							*FromNode,
							*ToNode));
						continue;
					}
					const FString Detail = FString::Printf(
						TEXT("%s.%s -> %s.%s"), *FromNode, *FromPin, *ToNode, *ToPin);
					AddChange(Planned, TEXT("link"), Detail, false);
					if (!bDryRun)
					{
						UMaterialEditingLibrary::DisconnectMaterialExpressions(*ToExpr, ToPin);
						const bool bOk = UMaterialEditingLibrary::ConnectMaterialExpressions(
							*FromExpr, FromPin, *ToExpr, ToPin);
						Ops += 2;
						if (!bOk)
						{
							Errors.Add(TEXT("ConnectMaterialExpressions failed: ") + Detail);
						}
						else
						{
							AddChange(Applied, TEXT("link"), Detail, true);
						}
					}
				}
			}
		}

		if (bApplyPropertyInputs)
		{
			const TSharedPtr<FJsonObject>* Extensions = nullptr;
			if (Graph->TryGetObjectField(TEXT("extensions"), Extensions) && Extensions)
			{
				const TSharedPtr<FJsonObject>* MaterialExt = nullptr;
				if ((*Extensions)->TryGetObjectField(TEXT("material"), MaterialExt) && MaterialExt)
				{
					const TSharedPtr<FJsonObject>* PropertyInputs = nullptr;
					if ((*MaterialExt)->TryGetObjectField(TEXT("property_inputs"), PropertyInputs)
						&& PropertyInputs)
					{
						for (const auto& Pair : (*PropertyInputs)->Values)
						{
							const TSharedPtr<FJsonObject> Row = Pair.Value->AsObject();
							if (!Row.IsValid())
							{
								continue;
							}
							FString NodeId;
							FString OutputName;
							Row->TryGetStringField(TEXT("node_id"), NodeId);
							Row->TryGetStringField(TEXT("output_name"), OutputName);
							UMaterialExpression** Expr = IdToExpr.Find(NodeId);
							const FString PropertyKey(Pair.Key);
							if (!Expr)
							{
								Errors.Add(TEXT("property_input missing node: ") + PropertyKey);
								continue;
							}
							EMaterialProperty Property = MP_EmissiveColor;
							if (PropertyKey.Contains(TEXT("OpacityMask"))) Property = MP_OpacityMask;
							else if (PropertyKey.Contains(TEXT("Opacity"))) Property = MP_Opacity;
							else if (PropertyKey.Contains(TEXT("BaseColor"))) Property = MP_BaseColor;
							else if (PropertyKey.Contains(TEXT("Metallic"))) Property = MP_Metallic;
							else if (PropertyKey.Contains(TEXT("Specular"))) Property = MP_Specular;
							else if (PropertyKey.Contains(TEXT("Roughness"))) Property = MP_Roughness;
							else if (PropertyKey.Contains(TEXT("Normal"))) Property = MP_Normal;
							else if (PropertyKey.Contains(TEXT("WorldPositionOffset"))) Property = MP_WorldPositionOffset;
							else if (PropertyKey.Contains(TEXT("SubsurfaceColor"))) Property = MP_SubsurfaceColor;
							else if (PropertyKey.Contains(TEXT("AmbientOcclusion"))) Property = MP_AmbientOcclusion;
							else if (PropertyKey.Contains(TEXT("Refraction"))) Property = MP_Refraction;
							else Property = MP_EmissiveColor;

							const FString Detail = PropertyKey + TEXT(" <- ") + NodeId;
							AddChange(Planned, TEXT("property_input"), Detail, false);
							if (!bDryRun)
							{
								UMaterialEditingLibrary::DisconnectMaterialProperty(Material, Property);
								const bool bOk = UMaterialEditingLibrary::ConnectMaterialProperty(
									*Expr, OutputName, Property);
								Ops += 2;
								if (!bOk)
								{
									Errors.Add(TEXT("ConnectMaterialProperty failed: ") + Detail);
								}
								else
								{
									AddChange(Applied, TEXT("property_input"), Detail, true);
								}
							}
						}
					}
				}
			}
		}

		return true;
	}
}

bool FUeremcpMaterialSubmit::ParseSpecification(
	const TSharedPtr<FJsonObject>& Spec,
	FUeremcpMaterialSubmitSpec& OutSpec,
	FString& OutError)
{
	OutSpec = FUeremcpMaterialSubmitSpec();
	if (!Spec.IsValid())
	{
		OutError = TEXT("submit_material_graph requires a specification object.");
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Graphs = nullptr;
	if (Spec->TryGetArrayField(TEXT("graphs"), Graphs) && Graphs)
	{
		OutSpec.Graphs = *Graphs;
	}
	const TSharedPtr<FJsonObject>* Params = nullptr;
	if (Spec->TryGetObjectField(TEXT("parameters"), Params) && Params)
	{
		OutSpec.Parameters = *Params;
	}
	Spec->TryGetStringField(TEXT("parent_material"), OutSpec.ParentMaterial);

	const TSharedPtr<FJsonObject>* Apply = nullptr;
	if (Spec->TryGetObjectField(TEXT("apply"), Apply) && Apply)
	{
		if ((*Apply)->HasField(TEXT("parameters")))
		{
			OutSpec.Apply.bParameters = (*Apply)->GetBoolField(TEXT("parameters"));
		}
		if ((*Apply)->HasField(TEXT("links")))
		{
			OutSpec.Apply.bLinks = (*Apply)->GetBoolField(TEXT("links"));
		}
		if ((*Apply)->HasField(TEXT("property_inputs")))
		{
			OutSpec.Apply.bPropertyInputs = (*Apply)->GetBoolField(TEXT("property_inputs"));
		}
		if ((*Apply)->HasField(TEXT("create_missing_expressions")))
		{
			OutSpec.Apply.bCreateMissingExpressions =
				(*Apply)->GetBoolField(TEXT("create_missing_expressions"));
		}
		if ((*Apply)->HasField(TEXT("delete_missing_expressions")))
		{
			OutSpec.Apply.bDeleteMissingExpressions =
				(*Apply)->GetBoolField(TEXT("delete_missing_expressions"));
		}
	}

	if (OutSpec.Graphs.Num() == 0 && !OutSpec.Parameters.IsValid())
	{
		OutError = TEXT("submit_material_graph requires specification.graphs and/or specification.parameters.");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FUeremcpMaterialSubmit::Run(
	const FUeremcpRequest& Request,
	const FUeremcpMaterialSubmitSpec& Spec,
	FUeremcpMaterialSubmitResult& OutResult)
{
	OutResult = FUeremcpMaterialSubmitResult();
	OutResult.CapabilityNotes = UeremcpMaterialCapability::DefaultSubmitCapabilityNotes();

	if (Request.TargetAssetPath.IsEmpty())
	{
		OutResult.Error = TEXT("submit_material_graph requires target.asset_path.");
		OutResult.Status = TEXT("rejected");
		return false;
	}

	const bool bScratch = UeremcpMaterialPaths::IsUnderAllowedScratchRoot(Request.TargetAssetPath);
	const bool bInspectOk = UeremcpMaterialPaths::IsAllowedInspectPath(Request.TargetAssetPath);
	if (!bInspectOk)
	{
		OutResult.Error = TEXT("submit_material_graph target must be under /Game.");
		OutResult.Status = TEXT("rejected");
		return false;
	}

	if (Spec.Apply.bDeleteMissingExpressions)
	{
		if (!bScratch || !IsReplaceMode(Request.Mode))
		{
			OutResult.Error = TEXT(
				"delete_missing_expressions requires envelope mode=replace AND scratch roots "
				"(/Game/__UeremcpTests or /Game/__UeremcpPoc). Production masters are never wiped.");
			OutResult.Status = TEXT("rejected");
			return false;
		}
	}

	if (Spec.Apply.bCreateMissingExpressions && !bScratch)
	{
		OutResult.InterpretationNotes.Add(TEXT(
			"create_missing_expressions ignored on production path — in-place links/params only."));
	}

	UMaterialInterface* Existing = LoadMaterialInterface(Request.TargetAssetPath);
	const bool bDryRun = Request.bDryRun;

	if (!Existing)
	{
		if (!bScratch)
		{
			OutResult.Error = TEXT(
				"Asset does not exist and create is only allowed under /Game/__UeremcpTests or /Game/__UeremcpPoc.");
			OutResult.Status = TEXT("rejected");
			return false;
		}
		if (bDryRun)
		{
			AddChange(
				OutResult.PlannedChanges,
				TEXT("create"),
				TEXT("would create material/MIC under scratch root"),
				false);
			OutResult.Status = TEXT("no_change_required");
			OutResult.Summary = TEXT("dry_run: would create material under scratch root.");
			OutResult.bSuccess = true;
			OutResult.ResultPayload = MakeShared<FJsonObject>();
			OutResult.ResultPayload->SetStringField(TEXT("primary_asset"), Request.TargetAssetPath);
			OutResult.ResultPayload->SetStringField(TEXT("asset_path"), Request.TargetAssetPath);
			OutResult.ResultPayload->SetBoolField(TEXT("dry_run"), true);
			OutResult.ResultPayload->SetArrayField(TEXT("planned_changes"), OutResult.PlannedChanges);
			TSharedPtr<FJsonObject> Fidelity = MakeShared<FJsonObject>();
			Fidelity->SetBoolField(TEXT("round_trip_supported"), false);
			OutResult.ResultPayload->SetObjectField(TEXT("fidelity"), Fidelity);
			return true;
		}

		OutResult.Error = TEXT(
			"Creating a brand-new master from graphs[] is not implemented in this slice; "
			"use CreateMasterMaterial / CreateVfxMaterial for new assets, or submit against an existing path.");
		OutResult.Status = TEXT("rejected");
		OutResult.InterpretationNotes.Add(TEXT(
			"submit_material_graph v1 applies in-place to existing assets; create-from-graph is deferred."));
		return false;
	}

	TSharedPtr<FJsonObject> Params = Spec.Parameters;
	if (!Params.IsValid())
	{
		Params = ParametersFromGraphs(Spec.Graphs);
	}

	{
		TUniquePtr<FScopedTransaction> Transaction;
		if (!bDryRun)
		{
			Transaction = MakeUnique<FScopedTransaction>(
				NSLOCTEXT("UeremcpMaterial", "SubmitMaterialGraph", "UEREMCP SubmitMaterialGraph"));
		}

		if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Existing))
		{
			if (Spec.Apply.bParameters && Params.IsValid())
			{
				ApplyMicParameters(
					MIC,
					Params,
					bDryRun,
					OutResult.PlannedChanges,
					OutResult.AppliedChanges,
					OutResult.Errors,
					OutResult.InternalOperations);
			}

			if (!bDryRun && OutResult.AppliedChanges.Num() > 0)
			{
				UMaterialEditingLibrary::UpdateMaterialInstance(MIC);
				++OutResult.InternalOperations;
				if (UEditorAssetSubsystem* Assets = GEditor
						? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
						: nullptr)
				{
					Assets->SaveLoadedAsset(MIC);
					++OutResult.InternalOperations;
				}
				FUeremcpAssetRef Ref;
				Ref.AssetPath = Request.TargetAssetPath;
				Ref.AssetClass = TEXT("MaterialInstanceConstant");
				OutResult.ModifiedAssets.Add(Ref);
			}
		}
		else if (UMaterial* Master = Cast<UMaterial>(Existing))
		{
			if (Spec.Apply.bDeleteMissingExpressions && bScratch && IsReplaceMode(Request.Mode))
			{
				AddChange(
					OutResult.PlannedChanges,
					TEXT("delete_missing"),
					TEXT("requested — not auto-executed in v1 without explicit node reconcile proof"),
					false);
				OutResult.InterpretationNotes.Add(TEXT(
					"delete_missing_expressions acknowledged but not auto-executed; "
					"refuse silent master wipe even on scratch until round-trip proven."));
			}

			for (const TSharedPtr<FJsonValue>& GraphValue : Spec.Graphs)
			{
				const TSharedPtr<FJsonObject> Graph = GraphValue->AsObject();
				if (!Graph.IsValid())
				{
					continue;
				}
				FString GraphType;
				Graph->TryGetStringField(TEXT("graph_type"), GraphType);
				if (!GraphType.Equals(TEXT("MaterialGraph")))
				{
					continue;
				}
				ApplyMasterLinks(
					Master,
					Graph,
					bDryRun,
					Spec.Apply.bLinks,
					Spec.Apply.bPropertyInputs,
					OutResult.PlannedChanges,
					OutResult.AppliedChanges,
					OutResult.Errors,
					OutResult.InternalOperations);
			}

			if (!bDryRun && OutResult.AppliedChanges.Num() > 0)
			{
				const TArray<FString> CompileErrors = UMaterialEditingLibrary::RecompileMaterial(Master);
				++OutResult.InternalOperations;
				for (const FString& CompileError : CompileErrors)
				{
					OutResult.Errors.Add(CompileError);
				}
				if (UEditorAssetSubsystem* Assets = GEditor
						? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
						: nullptr)
				{
					Assets->SaveLoadedAsset(Master);
					++OutResult.InternalOperations;
				}
				FUeremcpAssetRef Ref;
				Ref.AssetPath = Request.TargetAssetPath;
				Ref.AssetClass = TEXT("Material");
				OutResult.ModifiedAssets.Add(Ref);
			}
		}
		else
		{
			OutResult.Error = TEXT("Target is not UMaterial or UMaterialInstanceConstant.");
			OutResult.Status = TEXT("rejected");
			return false;
		}
	}

	OutResult.ResultPayload = MakeShared<FJsonObject>();
	OutResult.ResultPayload->SetStringField(TEXT("primary_asset"), Request.TargetAssetPath);
	OutResult.ResultPayload->SetStringField(TEXT("asset_path"), Request.TargetAssetPath);
	OutResult.ResultPayload->SetStringField(
		TEXT("asset_class"),
		Existing->IsA<UMaterialInstanceConstant>()
			? TEXT("MaterialInstanceConstant")
			: TEXT("Material"));
	OutResult.ResultPayload->SetBoolField(TEXT("dry_run"), bDryRun);
	OutResult.ResultPayload->SetArrayField(TEXT("planned_changes"), OutResult.PlannedChanges);
	OutResult.ResultPayload->SetArrayField(TEXT("applied_changes"), OutResult.AppliedChanges);
	TSharedPtr<FJsonObject> Fidelity = MakeShared<FJsonObject>();
	Fidelity->SetBoolField(TEXT("round_trip_supported"), false);
	TArray<TSharedPtr<FJsonValue>> Lossy;
	for (const FString& Area : UeremcpMaterialCapability::DefaultFidelityLossyAreas())
	{
		Lossy.Add(MakeShared<FJsonValueString>(Area));
	}
	Fidelity->SetArrayField(TEXT("lossy_areas"), Lossy);
	OutResult.ResultPayload->SetObjectField(TEXT("fidelity"), Fidelity);

	if (OutResult.Errors.Num() > 0 && OutResult.AppliedChanges.Num() == 0 && !bDryRun)
	{
		OutResult.Status = TEXT("failed_validation");
		OutResult.Summary = TEXT("submit_material_graph applied nothing; see errors.");
		OutResult.bSuccess = false;
		return true;
	}

	if (bDryRun || OutResult.AppliedChanges.Num() == 0)
	{
		OutResult.Status = TEXT("no_change_required");
		OutResult.Summary = bDryRun
			? TEXT("dry_run: planned material graph/parameter changes; nothing written.")
			: TEXT("No material changes required.");
	}
	else
	{
		// Honest: never *_validated until retrieve→replace→retrieve hash proof.
		OutResult.Status = TEXT("partially_completed");
		OutResult.Summary = FString::Printf(
			TEXT("Applied %d material change(s); round_trip_supported remains false."),
			OutResult.AppliedChanges.Num());
	}

	OutResult.bSuccess = true;
	return true;
}
