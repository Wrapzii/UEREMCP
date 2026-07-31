// UEREMCP — paint_landscape_layers (MCP-004).
//
// CreateLandscapeMaterial authors the blend; nothing assigned weights, so the
// terrain rendered as the first layer (the white-landscape complaint). This
// evaluates height/slope from the LIVE landscape and writes weightmaps.
//
// API NOTES — read, not recalled:
//   [VERIFIED: LandscapeEdit.h:128] FLandscapeEditDataInterface(ULandscapeInfo*)
//   [VERIFIED: LandscapeEdit.h:165] GetHeightData
//   [VERIFIED: LandscapeEdit.h:197] SetAlphaData(LayerInfo, X1,Y1,X2,Y2, Data, Stride, ...)
//   [VERIFIED: LandscapeInfo.h:283] GetLayerInfoByName
//   [VERIFIED: LandscapeDataAccess.h] local Z = (height-32768)*LANDSCAPE_ZSCALE
//   Never recompute a heightmap — that is the MCP-006 bug.

#include "UeremcpEnvironmentToolset.h"

#include "UeremcpEnvelope.h"

#include "EngineUtils.h"
#include "Editor.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeProxy.h"
#include "LandscapeUtils.h"
#include "Materials/MaterialInterface.h"
#include "Dom/JsonObject.h"

namespace
{
	bool CommonPreamble(
		const FString& RequestJson,
		const TCHAR* ExpectedAction,
		FUeremcpRequest& OutRequest,
		FString& OutRejection)
	{
		FString ParseError;
		if (!FUeremcpEnvelope::ParseRequest(RequestJson, OutRequest, ParseError))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				FString(), FString::Printf(TEXT("Malformed request envelope: %s"), *ParseError));
			return false;
		}
		if (!FUeremcpEnvelope::IsProtocolCompatible(OutRequest.ProtocolVersion))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(TEXT("Unsupported protocol_version '%s'; this server speaks %s."),
					*OutRequest.ProtocolVersion, *FUeremcpEnvelope::ProtocolVersion()));
			return false;
		}
		if (!OutRequest.Action.Equals(ExpectedAction, ESearchCase::CaseSensitive))
		{
			OutRejection = FUeremcpEnvelope::MakeRejection(
				OutRequest.RequestId,
				FString::Printf(TEXT("%s tool received action '%s'."),
					ExpectedAction, *OutRequest.Action));
			return false;
		}
		return true;
	}

	struct FPaintRule
	{
		FString Layer;
		bool bFallback = false;
		bool bHasMinHeight = false;
		bool bHasMinSlope = false;
		float MinHeightM = 0.f;
		float BlendM = 50.f;
		float MinSlopeDeg = 0.f;
		float BlendDeg = 5.f;
	};

	float SoftStep(float Value, float Edge, float Blend)
	{
		if (Blend <= KINDA_SMALL_NUMBER)
		{
			return Value >= Edge ? 1.f : 0.f;
		}
		const float T = (Value - (Edge - Blend)) / Blend;
		return FMath::Clamp(T, 0.f, 1.f);
	}

	ALandscape* FindLandscape(UWorld* World, const FString& LabelHint)
	{
		ALandscape* Fallback = nullptr;
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			if (!Fallback) Fallback = *It;
			if (!LabelHint.IsEmpty() && It->GetActorLabel().Contains(LabelHint))
			{
				return *It;
			}
			if (It->GetActorLabel().StartsWith(TEXT("UEREMCP_")))
			{
				return *It;
			}
		}
		return Fallback;
	}

	// MCP-004 close-the-loop: CreateLandscapeMaterial names layers but never
	// creates/assigns ULandscapeLayerInfoObject assets, so paint hit LAYER_NOT_FOUND.
	// [VERIFIED: LandscapeUtils.h CreateTargetLayerInfo]
	// [VERIFIED: LandscapeInfo.h CreateTargetLayerSettingsFor / UpdateLayerInfoMap]
	// [VERIFIED: LandscapeProxy.h AddTargetLayer / UpdateTargetLayer]
	ULandscapeLayerInfoObject* EnsureLandscapeLayerInfo(
		ALandscape* Landscape,
		ULandscapeInfo* Info,
		const FName LayerName,
		FString& OutError)
	{
		if (!Landscape || !Info || LayerName.IsNone())
		{
			OutError = TEXT("EnsureLandscapeLayerInfo: missing landscape/info/name.");
			return nullptr;
		}

		if (ULandscapeLayerInfoObject* Existing = Info->GetLayerInfoByName(LayerName, Landscape))
		{
			return Existing;
		}
		if (ULandscapeLayerInfoObject* Existing = Info->GetLayerInfoByName(LayerName))
		{
			Info->CreateTargetLayerSettingsFor(Existing);
			Info->UpdateLayerInfoMap(Landscape);
			return Existing;
		}

		if (Landscape->HasTargetLayer(LayerName))
		{
			const FLandscapeTargetLayerSettings* Settings = Landscape->GetTargetLayers().Find(LayerName);
			if (Settings && Settings->LayerInfoObj)
			{
				Info->UpdateLayerInfoMap(Landscape);
				if (ULandscapeLayerInfoObject* Resolved = Info->GetLayerInfoByName(LayerName, Landscape))
				{
					return Resolved;
				}
				return Settings->LayerInfoObj;
			}
		}

		const FString PackageDir = TEXT("/Game/__UeremcpPoc/LandscapeLayers");
		ULandscapeLayerInfoObject* Created =
			UE::Landscape::CreateTargetLayerInfo(LayerName, PackageDir);
		if (!Created)
		{
			OutError = FString::Printf(
				TEXT("CreateTargetLayerInfo failed for layer '%s' under %s."),
				*LayerName.ToString(), *PackageDir);
			return nullptr;
		}

		Info->CreateTargetLayerSettingsFor(Created);
		Info->UpdateLayerInfoMap(Landscape);

		ULandscapeLayerInfoObject* Resolved = Info->GetLayerInfoByName(LayerName, Landscape);
		if (!Resolved)
		{
			Resolved = Info->GetLayerInfoByName(LayerName);
		}
		if (!Resolved)
		{
			OutError = FString::Printf(
				TEXT("Created LayerInfo for '%s' but GetLayerInfoByName still failed after "
					 "CreateTargetLayerSettingsFor/UpdateLayerInfoMap."),
				*LayerName.ToString());
			return nullptr;
		}
		return Resolved;
	}
}

FString UUeremcpEnvironmentToolset::PaintLandscapeLayers(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("paint_landscape_layers"), Request, Rejection))
	{
		return Rejection;
	}

	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TEXT("No editor world."));
	}

	TArray<FPaintRule> Rules;
	int32 FallbackCount = 0;
	if (Request.Specification.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Request.Specification->TryGetArrayField(TEXT("rules"), Arr) && Arr)
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				const TSharedPtr<FJsonObject>* Obj = nullptr;
				if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj) continue;
				FPaintRule Rule;
				(*Obj)->TryGetStringField(TEXT("layer"), Rule.Layer);
				(*Obj)->TryGetBoolField(TEXT("fallback"), Rule.bFallback);
				double N = 0.0;
				if ((*Obj)->TryGetNumberField(TEXT("min_height_m"), N))
				{
					Rule.bHasMinHeight = true;
					Rule.MinHeightM = float(N);
				}
				if ((*Obj)->TryGetNumberField(TEXT("blend_m"), N)) Rule.BlendM = float(N);
				if ((*Obj)->TryGetNumberField(TEXT("min_slope_deg"), N))
				{
					Rule.bHasMinSlope = true;
					Rule.MinSlopeDeg = float(N);
				}
				if ((*Obj)->TryGetNumberField(TEXT("blend_deg"), N)) Rule.BlendDeg = float(N);
				if (Rule.Layer.IsEmpty()) continue;
				if (Rule.bFallback) ++FallbackCount;
				Rules.Add(Rule);
			}
		}
	}
	if (Rules.Num() == 0)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("paint_landscape_layers requires specification.rules: non-empty array with "
				 "layer names and height/slope gates (exactly one fallback=true)."));
	}
	if (FallbackCount != 1)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("Exactly one rule must set fallback=true so weights sum to 1 per vertex."));
	}

	FString LabelHint;
	if (!Request.TargetActorLabel.IsEmpty()) LabelHint = Request.TargetActorLabel;
	ALandscape* Landscape = FindLandscape(World, LabelHint);
	if (!Landscape)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("No ALandscape in the editor world."));
	}

	ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
	if (!Info)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("Landscape has no ULandscapeInfo."));
	}

	int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
	if (!Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("Could not read landscape extent."));
	}
	const int32 SizeX = MaxX - MinX + 1;
	const int32 SizeY = MaxY - MinY + 1;
	const int32 Count = SizeX * SizeY;
	if (Count <= 0 || Count > 4096 * 4096)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId, TEXT("Landscape extent is empty or unreasonably large."));
	}

	FString MaterialPath;
	if (Request.Specification.IsValid())
	{
		Request.Specification->TryGetStringField(TEXT("material_path"), MaterialPath);
	}

	if (Request.bDryRun)
	{
		FUeremcpResponse Response;
		Response.RequestId = Request.RequestId;
		Response.UnderstoodAction = Request.Action;
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("Dry run: would ensure LayerInfo + paint %d layer rule(s) across %dx%d "
				 "landscape verts from LIVE height data%s."),
			Rules.Num(), SizeX, SizeY,
			MaterialPath.IsEmpty() ? TEXT("") : TEXT(" (and assign material_path)"));
		Response.Metrics.McpRoundTrips = 1;
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	// Optional: assign landscape material so layer blend names match paint rules.
	FString MaterialAssignNote;
	if (!MaterialPath.IsEmpty())
	{
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MaterialPath))
		{
			Landscape->Modify();
			Landscape->LandscapeMaterial = Mat;
			Landscape->PostEditChange();
			MaterialAssignNote = FString::Printf(TEXT("Assigned landscape material %s."), *MaterialPath);
		}
		else
		{
			return FUeremcpEnvelope::MakeRejection(
				Request.RequestId,
				FString::Printf(
					TEXT("specification.material_path '%s' did not load as a MaterialInterface."),
					*MaterialPath));
		}
	}

	TArray<ULandscapeLayerInfoObject*> LayerInfos;
	TArray<FString> CreatedLayerInfos;
	TArray<FString> Failed;
	for (const FPaintRule& Rule : Rules)
	{
		FString EnsureError;
		const bool bHad =
			Info->GetLayerInfoByName(FName(*Rule.Layer), Landscape) != nullptr
			|| Info->GetLayerInfoByName(FName(*Rule.Layer)) != nullptr;
		ULandscapeLayerInfoObject* LayerInfo =
			EnsureLandscapeLayerInfo(Landscape, Info, FName(*Rule.Layer), EnsureError);
		if (!LayerInfo)
		{
			Failed.Add(Rule.Layer);
			LayerInfos.Add(nullptr);
			continue;
		}
		if (!bHad)
		{
			CreatedLayerInfos.Add(Rule.Layer);
		}
		LayerInfos.Add(LayerInfo);
	}
	if (Failed.Num() > 0)
	{
		TSharedPtr<FJsonObject> NextArgs = MakeShared<FJsonObject>();
		NextArgs->SetStringField(
			TEXT("recovery"),
			TEXT("paint_landscape_layers auto-creates LayerInfo under "
				 "/Game/__UeremcpPoc/LandscapeLayers; retry after create_landscape exists. "
				 "Optional specification.material_path assigns the blend material."));
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			FString::Printf(
				TEXT("Could not ensure landscape LayerInfo(s): %s."),
				*FString::Join(Failed, TEXT(", "))),
			TEXT("LAYER_NOT_FOUND"),
			NextArgs);
	}

	// [VERIFIED: LandscapeEdit.h] read live heights — never a recomputed surface.
	FLandscapeEditDataInterface Edit(Info);
	TArray<uint16> Heights;
	Heights.SetNumUninitialized(Count);
	{
		int32 X1 = MinX, Y1 = MinY, X2 = MaxX, Y2 = MaxY;
		Edit.GetHeightData(X1, Y1, X2, Y2, Heights.GetData(), 0);
	}

	const FVector Scale = Landscape->GetActorScale3D();
	const float ZScale = Scale.Z; // applied after LANDSCAPE_ZSCALE

	auto HeightMAt = [&](int32 X, int32 Y) -> float
	{
		const int32 IX = FMath::Clamp(X - MinX, 0, SizeX - 1);
		const int32 IY = FMath::Clamp(Y - MinY, 0, SizeY - 1);
		const uint16 H = Heights[IY * SizeX + IX];
		// [VERIFIED: LandscapeDataAccess.h] local = (height-32768)*LANDSCAPE_ZSCALE
		const float LocalZ = LandscapeDataAccess::GetLocalHeight(H);
		return (LocalZ * ZScale) / 100.f; // cm → m
	};

	auto SlopeDegAt = [&](int32 X, int32 Y) -> float
	{
		const float Hx0 = HeightMAt(X - 1, Y);
		const float Hx1 = HeightMAt(X + 1, Y);
		const float Hy0 = HeightMAt(X, Y - 1);
		const float Hy1 = HeightMAt(X, Y + 1);
		const float DxM = (2.f * Scale.X) / 100.f;
		const float DyM = (2.f * Scale.Y) / 100.f;
		const float Dzx = (Hx1 - Hx0) / FMath::Max(DxM, KINDA_SMALL_NUMBER);
		const float Dzy = (Hy1 - Hy0) / FMath::Max(DyM, KINDA_SMALL_NUMBER);
		const float SlopeRad = FMath::Atan(FMath::Sqrt(Dzx * Dzx + Dzy * Dzy));
		return FMath::RadiansToDegrees(SlopeRad);
	};

	TArray<TArray<uint8>> WeightMaps;
	WeightMaps.SetNum(Rules.Num());
	for (TArray<uint8>& W : WeightMaps)
	{
		W.SetNumZeroed(Count);
	}
	TArray<double> Coverage;
	Coverage.Init(0.0, Rules.Num());

	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const int32 Idx = (Y - MinY) * SizeX + (X - MinX);
			const float HeightM = HeightMAt(X, Y);
			const float SlopeDeg = SlopeDegAt(X, Y);

			TArray<float> Weights;
			Weights.SetNum(Rules.Num());
			float NonFallback = 0.f;
			int32 FallbackIdx = INDEX_NONE;
			for (int32 i = 0; i < Rules.Num(); ++i)
			{
				const FPaintRule& Rule = Rules[i];
				if (Rule.bFallback)
				{
					FallbackIdx = i;
					Weights[i] = 0.f;
					continue;
				}
				float W = 1.f;
				if (Rule.bHasMinHeight)
				{
					W *= SoftStep(HeightM, Rule.MinHeightM, Rule.BlendM);
				}
				if (Rule.bHasMinSlope)
				{
					W *= SoftStep(SlopeDeg, Rule.MinSlopeDeg, Rule.BlendDeg);
				}
				Weights[i] = W;
				NonFallback += W;
			}
			if (FallbackIdx != INDEX_NONE)
			{
				Weights[FallbackIdx] = FMath::Max(0.f, 1.f - NonFallback);
			}
			float Sum = 0.f;
			for (float W : Weights) Sum += W;
			if (Sum > KINDA_SMALL_NUMBER)
			{
				for (float& W : Weights) W /= Sum;
			}
			else if (FallbackIdx != INDEX_NONE)
			{
				Weights[FallbackIdx] = 1.f;
			}

			for (int32 i = 0; i < Rules.Num(); ++i)
			{
				const uint8 Byte = static_cast<uint8>(FMath::Clamp(Weights[i], 0.f, 1.f) * 255.f + 0.5f);
				WeightMaps[i][Idx] = Byte;
				Coverage[i] += Weights[i];
			}
		}
	}

	for (int32 i = 0; i < Rules.Num(); ++i)
	{
		Edit.SetAlphaData(
			LayerInfos[i],
			MinX, MinY, MaxX, MaxY,
			WeightMaps[i].GetData(),
			0,
			ELandscapeLayerPaintingRestriction::None,
			/*bWeightAdjust=*/true,
			/*bTotalWeightAdjust=*/true);
		Coverage[i] = (Coverage[i] / double(Count)) * 100.0;
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Status = TEXT("created_with_warnings");
	Response.Summary = FString::Printf(
		TEXT("Painted %d landscape layer(s) from live height/slope across %dx%d verts."),
		Rules.Num(), SizeX, SizeY);
	Response.Metrics.McpRoundTrips = 1;
	Response.Metrics.AssetsAffected = 1;
	Response.CapabilityNotes.Add(
		TEXT("Weights read the live landscape heightmap — never a recomputed surface "
			 "(MCP-006). Exactly one fallback layer took the remainder so weights sum to 1."));
	if (CreatedLayerInfos.Num() > 0)
	{
		Response.CapabilityNotes.Add(FString::Printf(
			TEXT("Auto-created/assigned LayerInfo for: %s (under /Game/__UeremcpPoc/LandscapeLayers)."),
			*FString::Join(CreatedLayerInfos, TEXT(", "))));
	}
	if (!MaterialAssignNote.IsEmpty())
	{
		Response.CapabilityNotes.Add(MaterialAssignNote);
	}

	TArray<TSharedPtr<FJsonValue>> Painted;
	for (int32 i = 0; i < Rules.Num(); ++i)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("layer"), Rules[i].Layer);
		Entry->SetNumberField(TEXT("coverage_pct"), Coverage[i]);
		Entry->SetBoolField(TEXT("fallback"), Rules[i].bFallback);
		Painted.Add(MakeShared<FJsonValueObject>(Entry));
	}
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("painted_layers"), Painted);
	Result->SetStringField(TEXT("landscape"), Landscape->GetActorLabel());
	if (CreatedLayerInfos.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Ensured;
		for (const FString& Name : CreatedLayerInfos)
		{
			Ensured.Add(MakeShared<FJsonValueString>(Name));
		}
		Result->SetArrayField(TEXT("ensured_layer_infos"), Ensured);
	}
	Response.ExtraFields = MakeShared<FJsonObject>();
	Response.ExtraFields->SetObjectField(TEXT("result"), Result);
	return FUeremcpEnvelope::SerializeResponse(Response);
}
