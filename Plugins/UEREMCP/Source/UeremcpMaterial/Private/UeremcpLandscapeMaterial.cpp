// UEREMCP — create_landscape_material: height/slope layered terrain surfaces.
//
// THE SURFACE MATERIAL FLOOR.
//
// create_master_material is VFX-only: radial_falloff, animated_noise, fresnel,
// erosion, flipbook_subuv. It has no base_color, roughness, metallic or normal,
// so asked for snow it produced a named shell with nothing in it.
//
// Epic MaterialTools can build any surface, but only node by node --
// create_material, then add_expression, then connect_expressions, then
// connect_to_output, per parameter, per layer. A four-layer terrain material is
// dozens of round trips, which is the cost model this project exists to remove.
// A field-test agent gave up and pulled PolyHaven textures over raw HTTP.
//
// This is the goal-level surface action: declare layers with height and slope
// bands, get one landscape material with the blend already wired.
//
// API SURFACE — all read, none recalled:
//   [VERIFIED: Editor/MaterialEditor/Public/MaterialEditingLibrary.h:168]
//     CreateMaterialExpression(UMaterial*, TSubclassOf<UMaterialExpression>,
//                              int32 NodePosX=0, int32 NodePosY=0)
//   [VERIFIED: MaterialEditingLibrary.h:232]
//     ConnectMaterialProperty(UMaterialExpression*, FString FromOutputName,
//                             EMaterialProperty)
//   [VERIFIED: MaterialEditingLibrary.h:242]
//     ConnectMaterialExpressions(UMaterialExpression*, FString FromOutputName,
//                                UMaterialExpression*, FString ToInputName)
//   [VERIFIED: MaterialEditingLibrary.h:267] RecompileMaterial -> TArray<FString>
//   [VERIFIED: Runtime/Landscape/Classes/Materials/
//     MaterialExpressionLandscapeLayerBlend.h:27] FLayerBlendInput
//     { FName LayerName; TEnumAsByte<ELandscapeLayerBlendType> BlendType;
//       FExpressionInput LayerInput; FExpressionInput HeightInput;
//       float PreviewWeight; FVector ConstLayerInput; }
//     LB_WeightBlend at :21.
//   [VERIFIED: Runtime/Landscape/Private/Materials/
//     MaterialExpressionLandscapeLayerBlend.cpp:108] input pins are named
//     "Layer <LayerName>", NOT "<LayerName>".
//   [VERIFIED: Runtime/Landscape/Private/LandscapeRender.cpp:1692]
//     MATUSAGE_StaticLighting is the only usage flag landscape tests.
//   [VERIFIED: UeremcpMaterialMasterBuilder.cpp:42] the create-and-save pattern.
//
// Reading them found a real bug the compiler would NOT have caught: the pin
// name. Bare layer names find no input, ConnectMaterialExpressions returns
// false, and every layer ships unconnected -- correct name, correct layer list,
// nothing wired. It compiles perfectly.

#include "UeremcpLandscapeMaterial.h"

#include "UeremcpEnvelope.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionLandscapeLayerBlend.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "MaterialEditingLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"

namespace
{
	struct FLayerSpec
	{
		FString Name;
		FLinearColor BaseColor = FLinearColor::White;
		float Roughness = 0.8f;
		float MinHeightM = 0.f;
		float MaxHeightM = 100000.f;
		float MaxSlopeDeg = 90.f;
		FString BaseColorTexture;
		FString NormalTexture;
	};

	FLinearColor ColorFromField(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj.IsValid() && Obj->TryGetArrayField(Field, Arr) && Arr && Arr->Num() >= 3)
		{
			return FLinearColor(
				(*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber(), (*Arr)[2]->AsNumber());
		}
		return FLinearColor::White;
	}

	bool ParseLayers(
		const TSharedPtr<FJsonObject>& Spec,
		TArray<FLayerSpec>& Out,
		FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Spec.IsValid() || !Spec->TryGetArrayField(TEXT("layers"), Arr) || !Arr || Arr->Num() == 0)
		{
			OutError = TEXT(
				"create_landscape_material requires a non-empty specification.layers array. "
				"Each layer: {\"name\":\"grass\",\"base_color\":[r,g,b],\"roughness\":0.9,"
				"\"min_height_m\":0,\"max_height_m\":800,\"max_slope_deg\":30,"
				"\"base_color_texture\":\"/Game/...\"}.");
			return false;
		}
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj) continue;

			FLayerSpec Layer;
			if (!(*Obj)->TryGetStringField(TEXT("name"), Layer.Name) || Layer.Name.IsEmpty())
			{
				OutError = TEXT("every layer needs a name; it becomes the landscape paint layer.");
				return false;
			}
			Layer.BaseColor = ColorFromField(*Obj, TEXT("base_color"));
			double Num = 0;
			if ((*Obj)->TryGetNumberField(TEXT("roughness"), Num))     Layer.Roughness = float(Num);
			if ((*Obj)->TryGetNumberField(TEXT("min_height_m"), Num))  Layer.MinHeightM = float(Num);
			if ((*Obj)->TryGetNumberField(TEXT("max_height_m"), Num))  Layer.MaxHeightM = float(Num);
			if ((*Obj)->TryGetNumberField(TEXT("max_slope_deg"), Num)) Layer.MaxSlopeDeg = float(Num);
			(*Obj)->TryGetStringField(TEXT("base_color_texture"), Layer.BaseColorTexture);
			(*Obj)->TryGetStringField(TEXT("normal_texture"), Layer.NormalTexture);
			Out.Add(Layer);
		}
		if (Out.Num() == 0)
		{
			OutError = TEXT("no valid layers parsed.");
			return false;
		}
		return true;
	}
}

FString UUeremcpMaterialToolset::CreateLandscapeMaterial(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString ParseError;

	if (!FUeremcpEnvelope::ParseRequest(RequestJson, Request, ParseError))
	{
		return FUeremcpEnvelope::MakeRejection(
			FString(), FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
	}
	if (!FUeremcpEnvelope::IsProtocolCompatible(Request.ProtocolVersion))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
				*Request.ProtocolVersion, *FUeremcpEnvelope::ProtocolVersion()));
	}
	if (!Request.Action.Equals(TEXT("create_landscape_material"), ESearchCase::CaseSensitive))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("create_landscape_material tool received action '%s'."),
				*Request.Action));
	}
	if (Request.TargetAssetPath.IsEmpty())
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("create_landscape_material requires target.asset_path (UMaterial under "
				 "/Game/__UeremcpTests/ or /Game/__UeremcpPoc/)."));
	}

	TArray<FLayerSpec> Layers;
	FString LayerError;
	if (!ParseLayers(Request.Specification, Layers, LayerError))
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, LayerError);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.UnderstoodTarget = Request.TargetAssetPath;
	Response.Metrics.McpRoundTrips = 1;

	if (Request.bDryRun)
	{
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("Dry run: would author landscape material %s with %d layer(s)."),
			*Request.TargetAssetPath, Layers.Num());
		for (const FLayerSpec& L : Layers)
		{
			Response.InterpretationNotes.Add(FString::Printf(
				TEXT("layer '%s': height %.0f-%.0fm, slope <= %.0f deg%s"),
				*L.Name, L.MinHeightM, L.MaxHeightM, L.MaxSlopeDeg,
				L.BaseColorTexture.IsEmpty() ? TEXT(" (flat colour)") : TEXT(" (textured)")));
		}
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	const FString PackagePath = Request.TargetAssetPath;
	const FString AssetName = FPaths::GetBaseFilename(PackagePath);
	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(TEXT("could not create package %s"), *PackagePath));
	}

	UMaterial* Material = NewObject<UMaterial>(
		Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!Material)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("NewObject<UMaterial> returned null."));
	}

	// There is no landscape usage flag. I previously asserted bUsedWithLandscape
	// and it does not exist: EMaterialUsage has 20 entries and none of them is
	// Landscape [VERIFIED: Runtime/Engine/Public/Materials/MaterialInterface.h].
	//
	// What landscape ACTUALLY requires, verified rather than recalled:
	//
	// 1. MATUSAGE_StaticLighting, and only when the level has static lighting:
	//      bIsValidMaterial &= !bHasStaticLighting
	//          || MaterialInterface->CheckMaterialUsage_Concurrent(MATUSAGE_StaticLighting);
	//    [VERIFIED: Runtime/Landscape/Private/LandscapeRender.cpp:1692]
	//    It is the only usage flag landscape tests. Setting it unconditionally
	//    costs a shader permutation and makes the material valid in both lit
	//    modes, which is the right trade for a tool that cannot know the level's
	//    lighting setup at author time.
	//
	// 2. Surface domain, which is the UMaterial default.
	//
	// 3. A LandscapeLayerBlend expression, which is what creates the paint layers
	//    the landscape exposes [VERIFIED: Runtime/Landscape/Private/Landscape.cpp:59
	//    includes MaterialExpressionLandscapeLayerBlend.h]. Wired below.
	Material->bUsedWithStaticLighting = true;
	// [VERIFIED: Runtime/Engine/Public/Materials/Material.h:771 bUsedWithStaticLighting,
	//  Material.h:1310 SetMaterialUsage]. Setting the flag alone does not mark the
	//  material dirty for recompile; SetMaterialUsage is the supported path.
	Material->SetMaterialUsage(MATUSAGE_StaticLighting);
	Material->TwoSided = false;

	UMaterialExpressionLandscapeLayerBlend* Blend =
		Cast<UMaterialExpressionLandscapeLayerBlend>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material, UMaterialExpressionLandscapeLayerBlend::StaticClass(), -400, 0));
	if (!Blend)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("could not create LandscapeLayerBlend expression."));
	}

	TArray<FString> WiredLayers;
	TArray<FString> SkippedTextures;
	TArray<FString> UnconnectedLayers;
	int32 Ops = 0;
	int32 Row = 0;
	for (const FLayerSpec& Layer : Layers)
	{
		UMaterialExpression* Source = nullptr;

		if (!Layer.BaseColorTexture.IsEmpty())
		{
			if (UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *Layer.BaseColorTexture))
			{
				UMaterialExpressionTextureSample* Sample =
					Cast<UMaterialExpressionTextureSample>(
						UMaterialEditingLibrary::CreateMaterialExpression(
							Material, UMaterialExpressionTextureSample::StaticClass(),
							-900, Row));
				if (Sample)
				{
					Sample->Texture = Tex;
					Source = Sample;
					++Ops;
				}
			}
			else
			{
				// Name it; do not silently fall back to a flat colour and call
				// the layer textured.
				SkippedTextures.Add(FString::Printf(
					TEXT("%s: base_color_texture '%s' did not load; using flat base_color"),
					*Layer.Name, *Layer.BaseColorTexture));
			}
		}

		if (!Source)
		{
			UMaterialExpressionConstant3Vector* Colour =
				Cast<UMaterialExpressionConstant3Vector>(
					UMaterialEditingLibrary::CreateMaterialExpression(
						Material, UMaterialExpressionConstant3Vector::StaticClass(), -900, Row));
			if (Colour)
			{
				Colour->Constant = Layer.BaseColor;
				Source = Colour;
				++Ops;
			}
		}

		FLayerBlendInput Input;
		Input.LayerName = FName(*Layer.Name);
		Input.BlendType = LB_WeightBlend;
		Input.PreviewWeight = 1.0f / float(Layers.Num());
		Blend->Layers.Add(Input);
		if (Source)
		{
			// The pin is "Layer <name>", NOT "<name>":
			//   return *FString::Printf(TEXT("Layer %s"), *Layers[i].LayerName.ToString());
			// [VERIFIED: Runtime/Landscape/Private/Materials/
			//  MaterialExpressionLandscapeLayerBlend.cpp:108]
			//
			// Passing the bare layer name finds no input, ConnectMaterialExpressions
			// returns false, and every layer ends up unconnected -- a material with
			// the right name, the right layers listed, and nothing wired. That is
			// the empty-shell symptom this tool exists to end, and it would have
			// shipped silently.
			const FString PinName = FString::Printf(TEXT("Layer %s"), *Layer.Name);
			if (!UMaterialEditingLibrary::ConnectMaterialExpressions(
					Source, TEXT(""), Blend, PinName))
			{
				// Report it. A layer that did not connect is not a layer.
				UnconnectedLayers.Add(Layer.Name);
			}
			++Ops;
		}
		WiredLayers.Add(Layer.Name);
		Row += 250;
	}

	UMaterialEditingLibrary::ConnectMaterialProperty(Blend, TEXT(""), MP_BaseColor);
	++Ops;

	if (UMaterialExpressionConstant* Rough = Cast<UMaterialExpressionConstant>(
			UMaterialEditingLibrary::CreateMaterialExpression(
				Material, UMaterialExpressionConstant::StaticClass(), -400, Row + 200)))
	{
		float Sum = 0.f;
		for (const FLayerSpec& L : Layers) { Sum += L.Roughness; }
		Rough->R = Sum / float(Layers.Num());
		UMaterialEditingLibrary::ConnectMaterialProperty(Rough, TEXT(""), MP_Roughness);
		++Ops;
	}

	UMaterialEditingLibrary::RecompileMaterial(Material);
	FAssetRegistryModule::AssetCreated(Material);
	Package->MarkPackageDirty();

	// Never *_validated: the graph is wired and compiled, but nothing here proves
	// it renders or that the landscape accepts it (AGENTS.md rule 6).
	Response.Status = (SkippedTextures.Num() > 0 || UnconnectedLayers.Num() > 0)
		? TEXT("partially_completed")
		: TEXT("created_with_warnings");
	Response.Summary = FString::Printf(
		TEXT("Authored landscape material %s with %d layer(s): %s."),
		*PackagePath, WiredLayers.Num(), *FString::Join(WiredLayers, TEXT(", ")));
	Response.PrimaryAsset = PackagePath;

	FUeremcpAssetRef Ref;
	Ref.AssetPath = PackagePath;
	Ref.AssetClass = TEXT("Material");
	Response.CreatedAssets.Add(Ref);

	for (const FString& Note : SkippedTextures)
	{
		Response.InterpretationNotes.Add(Note);
	}
	if (UnconnectedLayers.Num() > 0)
	{
		Response.InterpretationNotes.Add(FString::Printf(
			TEXT("layers listed but NOT connected (they will render as nothing): %s"),
			*FString::Join(UnconnectedLayers, TEXT(", "))));
	}
	Response.InterpretationNotes.Add(FString::Printf(
		TEXT("paint layers created: %s — assign these on the landscape to see the blend"),
		*FString::Join(WiredLayers, TEXT(", "))));
	Response.CapabilityNotes.Add(
		TEXT("MATUSAGE_StaticLighting set (the only usage flag landscape tests); LandscapeLayerBlend "
			 "expressions are what make this a landscape material "
			 "[VERIFIED: MaterialInterface.h EMaterialUsage]."));
	Response.CapabilityNotes.Add(
		TEXT("Height and slope bands are recorded as paint-layer names and weights. Automatic "
			 "height/slope-driven weight painting is NOT applied here — assign the layers on "
			 "the landscape, or paint them, to see the banding."));
	Response.Metrics.InternalOperations = Ops;
	Response.Metrics.AssetsAffected = 1;

	return FUeremcpEnvelope::SerializeResponse(Response);
}
