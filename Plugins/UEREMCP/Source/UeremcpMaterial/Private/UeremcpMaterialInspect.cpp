// UEREMCP — Material inspect → MaterialGraph JSON (WS-08).
//
// [VERIFIED: MaterialEditingLibrary.h:141 GetMaterialExpressions]
// [VERIFIED: MaterialEditingLibrary.h:317-349 property / pin / input APIs]
// [VERIFIED: MaterialEditingLibrary.h:502-514 Get*ParameterNames]
// [VERIFIED: MaterialExpression.h:159,177,195 EditorX/Guid/Desc]

#include "UeremcpMaterialInspect.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/PackageName.h"
#include "SceneTypes.h"
#include "UeremcpMaterialCapabilityNotes.h"
#include "UeremcpMaterialPaths.h"

namespace
{
	const TCHAR* GGraphSchemaVersion = TEXT("1.0");

	void AddTrace(
		TArray<TSharedPtr<FJsonValue>>& Trace,
		const FString& Op,
		bool bOk,
		const FString& Detail)
	{
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("op"), Op);
		Row->SetBoolField(TEXT("ok"), bOk);
		Row->SetStringField(TEXT("detail"), Detail);
		Trace.Add(MakeShared<FJsonValueObject>(Row));
	}

	TSharedPtr<FJsonObject> MakeFidelityObject()
	{
		TSharedPtr<FJsonObject> Fidelity = MakeShared<FJsonObject>();
		Fidelity->SetBoolField(TEXT("round_trip_supported"), false);
		TArray<TSharedPtr<FJsonValue>> Lossy;
		for (const FString& Area : UeremcpMaterialCapability::DefaultFidelityLossyAreas())
		{
			Lossy.Add(MakeShared<FJsonValueString>(Area));
		}
		Fidelity->SetArrayField(TEXT("lossy_areas"), Lossy);
		return Fidelity;
	}

	FString SoftPathFromAssetData(const FAssetData& Asset)
	{
		const FString PackageName = Asset.PackageName.ToString();
		const FString AssetName = Asset.AssetName.ToString();
		if (PackageName.IsEmpty() || AssetName.IsEmpty())
		{
			return FString();
		}
		return PackageName + TEXT(".") + AssetName;
	}

	FString PackagePathFromSoft(const FString& SoftOrPackage)
	{
		FString Package;
		FString Remaining;
		if (SoftOrPackage.Split(TEXT("."), &Package, &Remaining))
		{
			return Package;
		}
		return SoftOrPackage;
	}

	FString NormalizeSearchRoot(const FString& Root)
	{
		FString Normalized = Root.IsEmpty() ? FString(TEXT("/Game")) : Root;
		if (!Normalized.StartsWith(TEXT("/")))
		{
			Normalized = TEXT("/") + Normalized;
		}
		while (Normalized.Len() > 1 && Normalized.EndsWith(TEXT("/")))
		{
			Normalized.LeftChopInline(1);
		}
		return Normalized;
	}

	bool SoftPathUnderRoot(const FString& SoftPath, const FString& Root)
	{
		const FString Package = PackagePathFromSoft(SoftPath);
		return Package.Equals(Root, ESearchCase::IgnoreCase)
			|| Package.StartsWith(Root + TEXT("/"), ESearchCase::IgnoreCase);
	}

	FString MaterialPropertyName(EMaterialProperty Property)
	{
		switch (Property)
		{
		case MP_EmissiveColor: return TEXT("MP_EmissiveColor");
		case MP_Opacity: return TEXT("MP_Opacity");
		case MP_OpacityMask: return TEXT("MP_OpacityMask");
		case MP_BaseColor: return TEXT("MP_BaseColor");
		case MP_Metallic: return TEXT("MP_Metallic");
		case MP_Specular: return TEXT("MP_Specular");
		case MP_Roughness: return TEXT("MP_Roughness");
		case MP_Normal: return TEXT("MP_Normal");
		case MP_WorldPositionOffset: return TEXT("MP_WorldPositionOffset");
		case MP_SubsurfaceColor: return TEXT("MP_SubsurfaceColor");
		case MP_AmbientOcclusion: return TEXT("MP_AmbientOcclusion");
		case MP_Refraction: return TEXT("MP_Refraction");
		default: return FString::Printf(TEXT("MP_%d"), static_cast<int32>(Property));
		}
	}

	TArray<EMaterialProperty> CommonMaterialProperties()
	{
		return {
			MP_EmissiveColor,
			MP_Opacity,
			MP_OpacityMask,
			MP_BaseColor,
			MP_Metallic,
			MP_Specular,
			MP_Roughness,
			MP_Normal,
			MP_WorldPositionOffset,
			MP_SubsurfaceColor,
			MP_AmbientOcclusion,
			MP_Refraction,
		};
	}

	FString ExpressionNodeId(const UMaterialExpression* Expr, int32 FallbackIndex)
	{
		if (!Expr)
		{
			return FString::Printf(TEXT("expr_%d"), FallbackIndex);
		}
		if (Expr->MaterialExpressionGuid.IsValid())
		{
			return Expr->MaterialExpressionGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
		}
		return FString::Printf(TEXT("%s_%d"), *Expr->GetClass()->GetName(), FallbackIndex);
	}

	TSharedPtr<FJsonObject> MakePin(
		const FString& NodeId,
		const FString& Name,
		const FString& Direction,
		int32 Index)
	{
		TSharedPtr<FJsonObject> Pin = MakeShared<FJsonObject>();
		Pin->SetStringField(TEXT("pin_id"), FString::Printf(TEXT("%s::%s::%s::%d"), *NodeId, *Direction, *Name, Index));
		Pin->SetStringField(TEXT("name"), Name.IsEmpty() ? FString::Printf(TEXT("%s%d"), *Direction, Index) : Name);
		Pin->SetStringField(TEXT("direction"), Direction);
		return Pin;
	}

	void AppendParameterInventory(
		UMaterialInterface* MaterialInterface,
		TSharedPtr<FJsonObject>& OutParams,
		int32& OutCount,
		int32& OutOps)
	{
		OutParams = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Scalars;
		TArray<TSharedPtr<FJsonValue>> Vectors;
		TArray<TSharedPtr<FJsonValue>> Textures;
		TArray<TSharedPtr<FJsonValue>> Switches;

		TArray<FName> ScalarNames;
		UMaterialEditingLibrary::GetScalarParameterNames(MaterialInterface, ScalarNames);
		++OutOps;
		for (const FName& Name : ScalarNames)
		{
			TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), Name.ToString());
			float Value = 0.f;
			if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MaterialInterface))
			{
				Value = UMaterialEditingLibrary::GetMaterialInstanceScalarParameterValue(MIC, Name);
			}
			else if (UMaterial* Master = Cast<UMaterial>(MaterialInterface))
			{
				Value = UMaterialEditingLibrary::GetMaterialDefaultScalarParameterValue(Master, Name);
			}
			Row->SetNumberField(TEXT("value"), Value);
			FSoftObjectPath Source;
			if (UMaterialEditingLibrary::GetScalarParameterSource(MaterialInterface, Name, Source))
			{
				Row->SetStringField(TEXT("source"), Source.ToString());
			}
			Scalars.Add(MakeShared<FJsonValueObject>(Row));
			++OutCount;
		}

		TArray<FName> VectorNames;
		UMaterialEditingLibrary::GetVectorParameterNames(MaterialInterface, VectorNames);
		++OutOps;
		for (const FName& Name : VectorNames)
		{
			TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), Name.ToString());
			FLinearColor Value = FLinearColor::White;
			if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MaterialInterface))
			{
				Value = UMaterialEditingLibrary::GetMaterialInstanceVectorParameterValue(MIC, Name);
			}
			else if (UMaterial* Master = Cast<UMaterial>(MaterialInterface))
			{
				Value = UMaterialEditingLibrary::GetMaterialDefaultVectorParameterValue(Master, Name);
			}
			TArray<TSharedPtr<FJsonValue>> Arr;
			Arr.Add(MakeShared<FJsonValueNumber>(Value.R));
			Arr.Add(MakeShared<FJsonValueNumber>(Value.G));
			Arr.Add(MakeShared<FJsonValueNumber>(Value.B));
			Arr.Add(MakeShared<FJsonValueNumber>(Value.A));
			Row->SetArrayField(TEXT("value"), Arr);
			FSoftObjectPath Source;
			if (UMaterialEditingLibrary::GetVectorParameterSource(MaterialInterface, Name, Source))
			{
				Row->SetStringField(TEXT("source"), Source.ToString());
			}
			Vectors.Add(MakeShared<FJsonValueObject>(Row));
			++OutCount;
		}

		TArray<FName> TextureNames;
		UMaterialEditingLibrary::GetTextureParameterNames(MaterialInterface, TextureNames);
		++OutOps;
		for (const FName& Name : TextureNames)
		{
			TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), Name.ToString());
			UTexture* Texture = nullptr;
			if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MaterialInterface))
			{
				Texture = UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(MIC, Name);
			}
			else if (UMaterial* Master = Cast<UMaterial>(MaterialInterface))
			{
				Texture = UMaterialEditingLibrary::GetMaterialDefaultTextureParameterValue(Master, Name);
			}
			if (Texture)
			{
				Row->SetStringField(TEXT("value"), Texture->GetPathName());
			}
			else
			{
				Row->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
			}
			FSoftObjectPath Source;
			if (UMaterialEditingLibrary::GetTextureParameterSource(MaterialInterface, Name, Source))
			{
				Row->SetStringField(TEXT("source"), Source.ToString());
			}
			Textures.Add(MakeShared<FJsonValueObject>(Row));
			++OutCount;
		}

		TArray<FName> SwitchNames;
		UMaterialEditingLibrary::GetStaticSwitchParameterNames(MaterialInterface, SwitchNames);
		++OutOps;
		for (const FName& Name : SwitchNames)
		{
			TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("name"), Name.ToString());
			bool bValue = false;
			if (UMaterial* Master = Cast<UMaterial>(MaterialInterface))
			{
				bValue = UMaterialEditingLibrary::GetMaterialDefaultStaticSwitchParameterValue(Master, Name);
			}
			Row->SetBoolField(TEXT("value"), bValue);
			Switches.Add(MakeShared<FJsonValueObject>(Row));
			++OutCount;
		}

		OutParams->SetArrayField(TEXT("scalar"), Scalars);
		OutParams->SetArrayField(TEXT("vector"), Vectors);
		OutParams->SetArrayField(TEXT("texture"), Textures);
		OutParams->SetArrayField(TEXT("static_switch"), Switches);
	}

	TSharedPtr<FJsonObject> BuildMasterGraph(
		UMaterial* Material,
		const FString& AssetPath,
		const FUeremcpMaterialInspectSpec& Spec,
		FUeremcpMaterialInspectResult& OutResult)
	{
		const FString GraphId = AssetPath + TEXT("::Material");
		TSharedPtr<FJsonObject> Graph = MakeShared<FJsonObject>();
		Graph->SetStringField(TEXT("asset_path"), AssetPath);
		Graph->SetStringField(TEXT("graph_id"), GraphId);
		Graph->SetStringField(TEXT("graph_name"), Material->GetName());
		Graph->SetStringField(TEXT("graph_type"), TEXT("MaterialGraph"));
		Graph->SetStringField(TEXT("schema_version"), GGraphSchemaVersion);
		Graph->SetObjectField(TEXT("fidelity"), MakeFidelityObject());

		TSharedPtr<FJsonObject> ExtRoot = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> MatExt = MakeShared<FJsonObject>();
		MatExt->SetStringField(TEXT("asset_class"), TEXT("Material"));
		FString BlendModeName = TEXT("Unknown");
		if (const UEnum* BlendEnum = StaticEnum<EBlendMode>())
		{
			BlendModeName = BlendEnum->GetNameStringByValue(static_cast<int64>(Material->BlendMode.GetValue()));
		}
		MatExt->SetStringField(TEXT("blend_mode"), BlendModeName);
		MatExt->SetBoolField(TEXT("two_sided"), Material->IsTwoSided());

		TMap<UMaterialExpression*, FString> ExprToId;
		TArray<TSharedPtr<FJsonValue>> Nodes;
		TArray<TSharedPtr<FJsonValue>> Links;

		const bool bOmitNodes = Spec.ResponseDetail.Equals(TEXT("summary"), ESearchCase::IgnoreCase)
			|| Spec.ResponseDetail.Equals(TEXT("minimal"), ESearchCase::IgnoreCase)
			|| !Spec.bIncludeExpressionGraph;

		if (!bOmitNodes)
		{
			const TArray<UMaterialExpression*> Expressions =
				UMaterialEditingLibrary::GetMaterialExpressions(Material);
			++OutResult.InternalOperations;
			OutResult.ExpressionCount = Expressions.Num();
			OutResult.ChecksPerformed.Add(TEXT("GetMaterialExpressions"));

			for (int32 Index = 0; Index < Expressions.Num(); ++Index)
			{
				UMaterialExpression* Expr = Expressions[Index];
				if (!Expr)
				{
					continue;
				}
				const FString NodeId = ExpressionNodeId(Expr, Index);
				ExprToId.Add(Expr, NodeId);

				TSharedPtr<FJsonObject> Node = MakeShared<FJsonObject>();
				Node->SetStringField(TEXT("node_id"), NodeId);
				Node->SetStringField(TEXT("node_class"), Expr->GetClass()->GetName());
				Node->SetStringField(TEXT("semantic_type"), TEXT("material_expression"));
				Node->SetStringField(TEXT("title"), Expr->GetDescription());
				if (!Expr->Desc.IsEmpty())
				{
					Node->SetStringField(TEXT("comment"), Expr->Desc);
				}

				int32 PosX = 0;
				int32 PosY = 0;
				UMaterialEditingLibrary::GetMaterialExpressionNodePosition(Expr, PosX, PosY);
				++OutResult.InternalOperations;
				TArray<TSharedPtr<FJsonValue>> Position;
				Position.Add(MakeShared<FJsonValueNumber>(PosX));
				Position.Add(MakeShared<FJsonValueNumber>(PosY));
				Node->SetArrayField(TEXT("position"), Position);

				TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
				Props->SetNumberField(TEXT("MaterialExpressionEditorX"), PosX);
				Props->SetNumberField(TEXT("MaterialExpressionEditorY"), PosY);
				if (const UMaterialExpressionScalarParameter* Scalar =
						Cast<UMaterialExpressionScalarParameter>(Expr))
				{
					Props->SetStringField(TEXT("ParameterName"), Scalar->ParameterName.ToString());
					Node->SetStringField(TEXT("semantic_id"), TEXT("param.scalar.") + Scalar->ParameterName.ToString());
				}
				else if (const UMaterialExpressionVectorParameter* Vector =
							 Cast<UMaterialExpressionVectorParameter>(Expr))
				{
					Props->SetStringField(TEXT("ParameterName"), Vector->ParameterName.ToString());
					Node->SetStringField(TEXT("semantic_id"), TEXT("param.vector.") + Vector->ParameterName.ToString());
				}
				else if (const UMaterialExpressionTextureSampleParameter2D* Tex =
							 Cast<UMaterialExpressionTextureSampleParameter2D>(Expr))
				{
					Props->SetStringField(TEXT("ParameterName"), Tex->ParameterName.ToString());
					Node->SetStringField(TEXT("semantic_id"), TEXT("param.texture.") + Tex->ParameterName.ToString());
				}
				else
				{
					Node->SetStringField(TEXT("semantic_id"), NodeId);
				}
				Node->SetObjectField(TEXT("properties"), Props);

				const TArray<FString> InputNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(Expr);
				const TArray<FString> OutputNames = UMaterialEditingLibrary::GetMaterialExpressionOutputNames(Expr);
				OutResult.InternalOperations += 2;

				TArray<TSharedPtr<FJsonValue>> InputPins;
				for (int32 PinIndex = 0; PinIndex < InputNames.Num(); ++PinIndex)
				{
					InputPins.Add(MakeShared<FJsonValueObject>(
						MakePin(NodeId, InputNames[PinIndex], TEXT("input"), PinIndex)));
				}
				TArray<TSharedPtr<FJsonValue>> OutputPins;
				for (int32 PinIndex = 0; PinIndex < OutputNames.Num(); ++PinIndex)
				{
					OutputPins.Add(MakeShared<FJsonValueObject>(
						MakePin(NodeId, OutputNames[PinIndex], TEXT("output"), PinIndex)));
				}
				Node->SetArrayField(TEXT("input_pins"), InputPins);
				Node->SetArrayField(TEXT("output_pins"), OutputPins);
				Nodes.Add(MakeShared<FJsonValueObject>(Node));
			}

			for (int32 Index = 0; Index < Expressions.Num(); ++Index)
			{
				UMaterialExpression* Expr = Expressions[Index];
				if (!Expr || !ExprToId.Contains(Expr))
				{
					continue;
				}
				const FString ToNodeId = ExprToId.FindChecked(Expr);
				const TArray<FString> InputNames = UMaterialEditingLibrary::GetMaterialExpressionInputNames(Expr);
				const TArray<UMaterialExpression*> Inputs =
					UMaterialEditingLibrary::GetInputsForMaterialExpression(Material, Expr);
				++OutResult.InternalOperations;

				const int32 Count = FMath::Min(InputNames.Num(), Inputs.Num());
				for (int32 PinIndex = 0; PinIndex < Count; ++PinIndex)
				{
					UMaterialExpression* FromExpr = Inputs[PinIndex];
					if (!FromExpr || !ExprToId.Contains(FromExpr))
					{
						continue;
					}
					const FString FromNodeId = ExprToId.FindChecked(FromExpr);
					FString FromOutputName;
					UMaterialEditingLibrary::GetInputNodeOutputNameForMaterialExpression(
						Expr, FromExpr, FromOutputName);
					++OutResult.InternalOperations;

					TSharedPtr<FJsonObject> Link = MakeShared<FJsonObject>();
					Link->SetStringField(TEXT("from_node"), FromNodeId);
					Link->SetStringField(TEXT("from_pin"), FromOutputName);
					Link->SetStringField(TEXT("to_node"), ToNodeId);
					Link->SetStringField(TEXT("to_pin"), InputNames[PinIndex]);
					Link->SetStringField(TEXT("kind"), TEXT("data"));
					Link->SetBoolField(TEXT("valid"), true);
					Links.Add(MakeShared<FJsonValueObject>(Link));
				}
			}
			OutResult.ChecksPerformed.Add(TEXT("expression_links"));
		}
		else
		{
			OutResult.ChecksSkipped.Add(TEXT("expression_graph_omitted_for_detail"));
		}

		if (Spec.bIncludePropertyInputs)
		{
			TSharedPtr<FJsonObject> PropertyInputs = MakeShared<FJsonObject>();
			for (const EMaterialProperty Property : CommonMaterialProperties())
			{
				UMaterialExpression* InputNode =
					UMaterialEditingLibrary::GetMaterialPropertyInputNode(Material, Property);
				++OutResult.InternalOperations;
				if (!InputNode)
				{
					continue;
				}
				TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
				const FString* FoundId = ExprToId.Find(InputNode);
				Row->SetStringField(
					TEXT("node_id"),
					FoundId ? *FoundId : ExpressionNodeId(InputNode, 0));
				Row->SetStringField(
					TEXT("output_name"),
					UMaterialEditingLibrary::GetMaterialPropertyInputNodeOutputName(Material, Property));
				Row->SetStringField(TEXT("expression_class"), InputNode->GetClass()->GetName());
				PropertyInputs->SetObjectField(MaterialPropertyName(Property), Row);
			}
			MatExt->SetObjectField(TEXT("property_inputs"), PropertyInputs);
			OutResult.ChecksPerformed.Add(TEXT("property_inputs"));
		}
		else
		{
			OutResult.ChecksSkipped.Add(TEXT("property_inputs"));
		}

		if (Spec.bIncludeParameters && OutResult.Parameters.IsValid())
		{
			MatExt->SetObjectField(TEXT("parameters"), OutResult.Parameters);
		}

		ExtRoot->SetObjectField(TEXT("material"), MatExt);
		Graph->SetObjectField(TEXT("extensions"), ExtRoot);
		Graph->SetArrayField(TEXT("nodes"), Nodes);
		Graph->SetArrayField(TEXT("links"), Links);

		TSharedPtr<FJsonObject> Summary = MakeShared<FJsonObject>();
		Summary->SetNumberField(TEXT("node_count"), Nodes.Num());
		Summary->SetStringField(TEXT("purpose"), TEXT("material_expression_graph"));
		Graph->SetObjectField(TEXT("semantic_summary"), Summary);
		return Graph;
	}
}

bool FUeremcpMaterialInspect::ParseSpecification(
	const TSharedPtr<FJsonObject>& Spec,
	FUeremcpMaterialInspectSpec& OutSpec,
	FString& OutError)
{
	OutSpec = FUeremcpMaterialInspectSpec();
	if (!Spec.IsValid())
	{
		OutError.Reset();
		return true;
	}

	Spec->TryGetStringField(TEXT("query"), OutSpec.Query);
	Spec->TryGetStringField(TEXT("asset_name"), OutSpec.AssetName);
	FString SearchRoot;
	if (Spec->TryGetStringField(TEXT("search_root"), SearchRoot) && !SearchRoot.IsEmpty())
	{
		OutSpec.SearchRoot = SearchRoot;
	}
	if (Spec->HasField(TEXT("include_expression_graph")))
	{
		OutSpec.bIncludeExpressionGraph = Spec->GetBoolField(TEXT("include_expression_graph"));
	}
	if (Spec->HasField(TEXT("include_parameters")))
	{
		OutSpec.bIncludeParameters = Spec->GetBoolField(TEXT("include_parameters"));
	}
	if (Spec->HasField(TEXT("include_property_inputs")))
	{
		OutSpec.bIncludePropertyInputs = Spec->GetBoolField(TEXT("include_property_inputs"));
	}
	Spec->TryGetStringField(TEXT("response_detail"), OutSpec.ResponseDetail);
	OutError.Reset();
	return true;
}

bool FUeremcpMaterialInspect::IsAllowedInspectPath(const FString& AssetPath)
{
	return UeremcpMaterialPaths::IsAllowedInspectPath(AssetPath);
}

bool FUeremcpMaterialInspect::ResolveTargetPath(
	const FUeremcpRequest& Request,
	const FUeremcpMaterialInspectSpec& Spec,
	FString& OutAssetPath,
	FString& OutError,
	TArray<FString>& OutCandidates)
{
	OutCandidates.Reset();
	OutAssetPath.Reset();

	if (!Request.TargetAssetPath.IsEmpty())
	{
		if (!IsAllowedInspectPath(Request.TargetAssetPath))
		{
			OutError = FString::Printf(
				TEXT("inspect_material path '%s' is outside /Game."),
				*Request.TargetAssetPath);
			return false;
		}
		OutAssetPath = Request.TargetAssetPath;
		OutError.Reset();
		return true;
	}

	const FString Query = !Spec.Query.IsEmpty() ? Spec.Query : Spec.AssetName;
	if (Query.IsEmpty())
	{
		OutError = TEXT("inspect_material requires target.asset_path or specification.query / asset_name.");
		return false;
	}

	const FString SearchRoot = NormalizeSearchRoot(Spec.SearchRoot);
	FAssetRegistryModule& ARM =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = ARM.Get();

	auto CollectClass = [&](UClass* Class)
	{
		FARFilter Filter;
		Filter.PackagePaths.Add(*SearchRoot);
		Filter.bRecursivePaths = true;
		Filter.ClassPaths.Add(Class->GetClassPathName());
		Filter.bRecursiveClasses = true;
		TArray<FAssetData> Assets;
		Registry.GetAssets(Filter, Assets);
		for (const FAssetData& Asset : Assets)
		{
			const FString Soft = SoftPathFromAssetData(Asset);
			if (Soft.IsEmpty() || !SoftPathUnderRoot(Soft, SearchRoot))
			{
				continue;
			}
			OutCandidates.AddUnique(PackagePathFromSoft(Soft));
		}
	};

	CollectClass(UMaterial::StaticClass());
	CollectClass(UMaterialInstanceConstant::StaticClass());

	const FString QueryLower = Query.ToLower();
	TArray<FString> Exact;
	TArray<FString> Substring;
	for (const FString& Candidate : OutCandidates)
	{
		const FString Name = FPackageName::GetShortName(Candidate).ToLower();
		if (Name.Equals(QueryLower))
		{
			Exact.Add(Candidate);
		}
		else if (Name.Contains(QueryLower) || Candidate.ToLower().Contains(QueryLower))
		{
			Substring.Add(Candidate);
		}
	}

	const TArray<FString>& Matches = Exact.Num() > 0 ? Exact : Substring;
	OutCandidates = Matches;
	if (Matches.Num() == 0)
	{
		OutError = FString::Printf(
			TEXT("No UMaterial/UMaterialInstanceConstant matching '%s' under %s."),
			*Query,
			*SearchRoot);
		return false;
	}
	if (Matches.Num() > 1)
	{
		OutError = FString::Printf(
			TEXT("Ambiguous material query '%s' matched %d assets; pass target.asset_path or a more specific name."),
			*Query,
			Matches.Num());
		return false;
	}

	OutAssetPath = Matches[0];
	OutError.Reset();
	return true;
}

bool FUeremcpMaterialInspect::Run(
	const FUeremcpRequest& Request,
	const FUeremcpMaterialInspectSpec& Spec,
	FUeremcpMaterialInspectResult& OutResult)
{
	OutResult = FUeremcpMaterialInspectResult();
	OutResult.Fidelity = MakeFidelityObject();

	FUeremcpMaterialInspectSpec EffectiveSpec = Spec;
	if (EffectiveSpec.ResponseDetail.IsEmpty())
	{
		EffectiveSpec.ResponseDetail = Request.ResponseDetail.IsEmpty()
			? FString(TEXT("complete"))
			: Request.ResponseDetail;
	}

	FString ResolveError;
	if (!ResolveTargetPath(Request, EffectiveSpec, OutResult.ResolvedAssetPath, ResolveError, OutResult.Candidates))
	{
		OutResult.Error = ResolveError;
		AddTrace(OutResult.ExecutionTrace, TEXT("resolve_target"), false, ResolveError);
		return false;
	}
	AddTrace(OutResult.ExecutionTrace, TEXT("resolve_target"), true, OutResult.ResolvedAssetPath);

	if (!IsAllowedInspectPath(OutResult.ResolvedAssetPath))
	{
		OutResult.Error = FString::Printf(
			TEXT("inspect_material path '%s' is outside /Game."),
			*OutResult.ResolvedAssetPath);
		return false;
	}

	const FSoftObjectPath SoftPath(OutResult.ResolvedAssetPath);
	UObject* Loaded = SoftPath.TryLoad();
	if (!Loaded)
	{
		// Accept package path without .Asset suffix.
		Loaded = LoadObject<UObject>(nullptr, *OutResult.ResolvedAssetPath);
	}
	UMaterialInterface* MaterialInterface = Cast<UMaterialInterface>(Loaded);
	if (!MaterialInterface)
	{
		OutResult.Error = FString::Printf(
			TEXT("Failed to load UMaterialInterface at '%s'."),
			*OutResult.ResolvedAssetPath);
		AddTrace(OutResult.ExecutionTrace, TEXT("load_asset"), false, OutResult.Error);
		return false;
	}
	AddTrace(OutResult.ExecutionTrace, TEXT("load_asset"), true, MaterialInterface->GetClass()->GetName());
	++OutResult.InternalOperations;

	if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MaterialInterface))
	{
		OutResult.AssetClass = TEXT("MaterialInstanceConstant");
		if (UMaterialInterface* Parent = MIC->Parent)
		{
			OutResult.ParentMaterialPath = Parent->GetPathName();
		}
	}
	else if (Cast<UMaterial>(MaterialInterface))
	{
		OutResult.AssetClass = TEXT("Material");
	}
	else
	{
		OutResult.AssetClass = MaterialInterface->GetClass()->GetName();
	}

	if (EffectiveSpec.bIncludeParameters)
	{
		AppendParameterInventory(
			MaterialInterface,
			OutResult.Parameters,
			OutResult.ParameterCount,
			OutResult.InternalOperations);
		OutResult.ChecksPerformed.Add(TEXT("parameter_inventory"));
	}
	else
	{
		OutResult.ChecksSkipped.Add(TEXT("parameters"));
	}

	if (UMaterial* Master = Cast<UMaterial>(MaterialInterface))
	{
		TSharedPtr<FJsonObject> Graph =
			BuildMasterGraph(Master, OutResult.ResolvedAssetPath, EffectiveSpec, OutResult);
		OutResult.Graphs.Add(MakeShared<FJsonValueObject>(Graph));
	}
	else if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(MaterialInterface))
	{
		// MIC: parameter inventory is the primary payload; still emit a MaterialGraph shell
		// so agents have one graphs[] shape to edit before SubmitMaterialGraph.
		TSharedPtr<FJsonObject> Graph = MakeShared<FJsonObject>();
		Graph->SetStringField(TEXT("asset_path"), OutResult.ResolvedAssetPath);
		Graph->SetStringField(TEXT("graph_id"), OutResult.ResolvedAssetPath + TEXT("::MaterialInstance"));
		Graph->SetStringField(TEXT("graph_name"), MIC->GetName());
		Graph->SetStringField(TEXT("graph_type"), TEXT("MaterialGraph"));
		Graph->SetStringField(TEXT("schema_version"), GGraphSchemaVersion);
		Graph->SetObjectField(TEXT("fidelity"), MakeFidelityObject());
		TSharedPtr<FJsonObject> ExtRoot = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> MatExt = MakeShared<FJsonObject>();
		MatExt->SetStringField(TEXT("asset_class"), TEXT("MaterialInstanceConstant"));
		if (!OutResult.ParentMaterialPath.IsEmpty())
		{
			MatExt->SetStringField(TEXT("parent_material"), OutResult.ParentMaterialPath);
		}
		if (OutResult.Parameters.IsValid())
		{
			MatExt->SetObjectField(TEXT("parameters"), OutResult.Parameters);
		}
		ExtRoot->SetObjectField(TEXT("material"), MatExt);
		Graph->SetObjectField(TEXT("extensions"), ExtRoot);
		Graph->SetArrayField(TEXT("nodes"), TArray<TSharedPtr<FJsonValue>>());
		Graph->SetArrayField(TEXT("links"), TArray<TSharedPtr<FJsonValue>>());
		OutResult.Graphs.Add(MakeShared<FJsonValueObject>(Graph));
		OutResult.ChecksPerformed.Add(TEXT("mic_parameter_shell_graph"));
		OutResult.ChecksSkipped.Add(TEXT("master_expression_graph"));
	}

	OutResult.Summary = FString::Printf(
		TEXT("Inspected %s (%s): %d expression(s), %d parameter(s). round_trip_supported=false."),
		*OutResult.ResolvedAssetPath,
		*OutResult.AssetClass,
		OutResult.ExpressionCount,
		OutResult.ParameterCount);
	OutResult.bSuccess = true;
	return true;
}
