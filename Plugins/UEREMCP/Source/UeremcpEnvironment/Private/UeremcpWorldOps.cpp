// UEREMCP — MCP-002 / MCP-003 / MCP-010: the world-assembly gaps.
//
// Every one of these was worked around by hand in the Northridge sessions, and
// each workaround produced the next bug:
//
//   SnapActorsToLandscape   agents traced per-actor with scripts; the traces hit
//                           the foliage canopy, so the castle and huts came to
//                           rest on treetops. Tracing against LANDSCAPE ONLY is
//                           the whole fix and was not expressible.
//   ClearFoliageInVolumes   agents placed WaterBodyExclusionVolume expecting it
//                           to cull HISM instances. It does not -- that is water
//                           exclusion. Trees stayed inside the castle walls.
//   PlacePrefabOnLandscape  place + snap + clear foundation was three fragile
//                           steps whose failure mode was a floating building.
//
// API NOTES — read, not recalled:
//   [VERIFIED: Engine/Public/WorldCollision.h + Engine/World.h]
//     UWorld::LineTraceSingleByObjectType(FHitResult&, Start, End,
//       FCollisionObjectQueryParams, FCollisionQueryParams)
//   [VERIFIED: Landscape/Classes/LandscapeProxy.h] ALandscapeProxy is the actor
//     class landscape components hang off; Cast on the hit actor identifies a
//     landscape hit without needing a custom trace channel.
//   [VERIFIED: Components/InstancedStaticMeshComponent.h] GetInstanceCount,
//     GetInstanceTransform(int32, FTransform&, bool bWorldSpace),
//     RemoveInstance(int32).

#include "UeremcpEnvironmentToolset.h"
#include "UeremcpWorldOpsHelpers.h"

#include "UeremcpEnvelope.h"

#include "Engine/World.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Dom/JsonObject.h"

namespace UeremcpWorldOps
{
	UWorld* EditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	bool LandscapeZAt(UWorld* World, const FVector& Location, float& OutZ)
	{
		if (!World)
		{
			return false;
		}

		// Prefer a Visibility multi-trace so foliage / props above the surface do
		// not win — we only accept ALandscapeProxy hits.
		// [VERIFIED: Engine/World.h LineTraceMultiByChannel]
		{
			const FVector Start(Location.X, Location.Y, Location.Z + 200000.f);
			const FVector End(Location.X, Location.Y, Location.Z - 200000.f);

			FCollisionQueryParams Params(SCENE_QUERY_STAT(UeremcpLandscapeSnap), true);
			Params.bTraceComplex = true;

			TArray<FHitResult> Hits;
			World->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, Params);
			for (const FHitResult& Hit : Hits)
			{
				if (Cast<ALandscapeProxy>(Hit.GetActor()))
				{
					OutZ = Hit.ImpactPoint.Z;
					return true;
				}
			}
		}

		// Fallback: sample the landscape heightfield directly. Freshly imported
		// landscapes (especially at small uniform scales) often have no Visibility
		// collision yet — GetHeightAtLocation(Editor) still returns a Z when the
		// XY is on the landscape. Without this, place_prefab_on_landscape after
		// NEEDLE/NONUNIFORM recovery to scale=3 floated at Z=0.
		// [VERIFIED: LandscapeProxy.h:1101] GetHeightAtLocation
		// [VERIFIED: LandscapeHeightfieldCollisionComponent.h:32] EHeightfieldSource::Editor
		for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
		{
			const TOptional<float> Height = It->GetHeightAtLocation(Location, EHeightfieldSource::Editor);
			if (Height.IsSet())
			{
				OutZ = Height.GetValue();
				return true;
			}
		}
		return false;
	}

	bool FlattenPadAt(
		UWorld* World,
		const FVector2D& LocationXY,
		float TargetWorldZ,
		float RadiusCm,
		float FalloffCm,
		FString& OutError,
		int32& OutVertsTouched)
	{
		OutError.Reset();
		OutVertsTouched = 0;
		if (!World)
		{
			OutError = TEXT("No editor world.");
			return false;
		}
		if (RadiusCm <= 0.f)
		{
			OutError = TEXT("flatten_pad.radius_cm must be > 0.");
			return false;
		}
		FalloffCm = FMath::Max(0.f, FalloffCm);

		ALandscape* Landscape = nullptr;
		for (TActorIterator<ALandscape> It(World); It; ++It)
		{
			if (It->GetActorLabel().StartsWith(TEXT("UEREMCP_Landscape")))
			{
				Landscape = *It;
				break;
			}
			if (!Landscape)
			{
				Landscape = *It;
			}
		}
		if (!Landscape)
		{
			OutError = TEXT("No ALandscape in the editor world to flatten.");
			return false;
		}

		ULandscapeInfo* Info = Landscape->GetLandscapeInfo();
		if (!Info)
		{
			Landscape->CreateLandscapeInfo();
			Info = Landscape->GetLandscapeInfo();
		}
		if (!Info)
		{
			OutError = TEXT("Landscape has no ULandscapeInfo.");
			return false;
		}

		int32 MinX = MAX_int32, MinY = MAX_int32, MaxX = MIN_int32, MaxY = MIN_int32;
		if (!Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
		{
			OutError = TEXT("Could not read landscape extent for flatten_pad.");
			return false;
		}

		const FTransform LandscapeToWorld = Landscape->LandscapeActorToWorld();
		const FVector CentreWorld(LocationXY.X, LocationXY.Y, TargetWorldZ);
		const FVector CentreLocal = LandscapeToWorld.InverseTransformPosition(CentreWorld);
		const int32 CentreQX = FMath::RoundToInt(CentreLocal.X);
		const int32 CentreQY = FMath::RoundToInt(CentreLocal.Y);
		const uint16 TargetHeight = LandscapeDataAccess::GetTexHeight(CentreLocal.Z);

		// Quad spacing in world cm ≈ actor scale XY (Import uses 1 unit per quad).
		const FVector Scale = Landscape->GetActorScale3D();
		const float QuadCm = FMath::Max(1.f, float(Scale.X));
		const int32 RadiusQuads = FMath::CeilToInt((RadiusCm + FalloffCm) / QuadCm) + 1;
		int32 X1 = FMath::Clamp(CentreQX - RadiusQuads, MinX, MaxX);
		int32 Y1 = FMath::Clamp(CentreQY - RadiusQuads, MinY, MaxY);
		int32 X2 = FMath::Clamp(CentreQX + RadiusQuads, MinX, MaxX);
		int32 Y2 = FMath::Clamp(CentreQY + RadiusQuads, MinY, MaxY);
		if (X2 < X1 || Y2 < Y1)
		{
			OutError = TEXT("flatten_pad centre is outside the landscape extent.");
			return false;
		}

		const int32 SizeX = X2 - X1 + 1;
		const int32 SizeY = Y2 - Y1 + 1;
		TArray<uint16> Heights;
		Heights.SetNumUninitialized(SizeX * SizeY);
		{
			// [VERIFIED: LandscapeEdit.h:128] FLandscapeEditDataInterface(ULandscapeInfo*)
			// [VERIFIED: LandscapeEdit.h:165] GetHeightData
			FLandscapeEditDataInterface Edit(Info);
			int32 GX1 = X1, GY1 = Y1, GX2 = X2, GY2 = Y2;
			Edit.GetHeightData(GX1, GY1, GX2, GY2, Heights.GetData(), 0);
		}

		const float Inner = RadiusCm;
		const float Outer = RadiusCm + FalloffCm;
		for (int32 Y = Y1; Y <= Y2; ++Y)
		{
			for (int32 X = X1; X <= X2; ++X)
			{
				const FVector VertWorld = LandscapeToWorld.TransformPosition(FVector(float(X), float(Y), 0.f));
				const float Dist = FVector2D::Distance(
					FVector2D(VertWorld.X, VertWorld.Y), LocationXY);
				float Weight = 0.f;
				if (Dist <= Inner)
				{
					Weight = 1.f;
				}
				else if (Dist < Outer && Outer > Inner)
				{
					Weight = 1.f - ((Dist - Inner) / (Outer - Inner));
				}
				if (Weight <= KINDA_SMALL_NUMBER)
				{
					continue;
				}
				const int32 Idx = (Y - Y1) * SizeX + (X - X1);
				const uint16 Old = Heights[Idx];
				const float Blended = FMath::Lerp(float(Old), float(TargetHeight), Weight);
				Heights[Idx] = uint16(FMath::Clamp(FMath::RoundToInt(Blended), 0, 65535));
				++OutVertsTouched;
			}
		}

		if (OutVertsTouched == 0)
		{
			OutError = TEXT("flatten_pad touched 0 verts — centre may miss the landscape.");
			return false;
		}

		{
			// [VERIFIED: LandscapeEdit.h:145] SetHeightData(..., InCalcNormals, ... InUpdateCollision)
			FLandscapeEditDataInterface Edit(Info);
			Edit.SetHeightData(
				X1, Y1, X2, Y2,
				Heights.GetData(),
				0,
				/*InCalcNormals=*/true);
		}
		Landscape->RecreateCollisionComponents();
		return true;
	}

	int32 ClearFoliageInBoxes(UWorld* World, const TArray<FBox>& Volumes, bool bDryRun, int32& OutInspected)
	{
		int32 Removed = 0;
		OutInspected = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TArray<UInstancedStaticMeshComponent*> Components;
			It->GetComponents<UInstancedStaticMeshComponent>(Components);
			for (UInstancedStaticMeshComponent* Ism : Components)
			{
				if (!Ism) continue;
				for (int32 i = Ism->GetInstanceCount() - 1; i >= 0; --i)
				{
					FTransform Xf;
					if (!Ism->GetInstanceTransform(i, Xf, /*bWorldSpace=*/true)) continue;
					++OutInspected;
					const FVector P = Xf.GetLocation();
					for (const FBox& Box : Volumes)
					{
						if (Box.IsInsideOrOn(P))
						{
							if (!bDryRun)
							{
								Ism->RemoveInstance(i);
							}
							++Removed;
							break;
						}
					}
				}
			}
		}
		return Removed;
	}
}

namespace
{
	TArray<AActor*> ActorsByLabelPrefixes(UWorld* World, const TArray<FString>& Prefixes)
	{
		TArray<AActor*> Out;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const FString Label = It->GetActorLabel();
			for (const FString& Prefix : Prefixes)
			{
				if (Label.StartsWith(Prefix))
				{
					Out.Add(*It);
					break;
				}
			}
		}
		return Out;
	}

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

	TArray<FString> StringArrayField(const TSharedPtr<FJsonObject>& Spec, const TCHAR* Field)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Spec.IsValid() && Spec->TryGetArrayField(Field, Arr) && Arr)
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S) && !S.IsEmpty()) Out.Add(S);
			}
		}
		return Out;
	}
}

FString UUeremcpEnvironmentToolset::SnapActorsToLandscape(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("snap_actors_to_landscape"), Request, Rejection))
	{
		return Rejection;
	}

	UWorld* World = UeremcpWorldOps::EditorWorld();
	if (!World)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TEXT("No editor world."));
	}

	TArray<FString> Prefixes = StringArrayField(Request.Specification, TEXT("label_prefixes"));
	if (Prefixes.Num() == 0)
	{
		Prefixes.Add(TEXT("UEREMCP_"));
	}
	double ZOffset = 0.0;
	if (Request.Specification.IsValid())
	{
		Request.Specification->TryGetNumberField(TEXT("z_offset_cm"), ZOffset);
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Metrics.McpRoundTrips = 1;

	TArray<AActor*> Targets = ActorsByLabelPrefixes(World, Prefixes);
	if (Request.bDryRun)
	{
		Response.Status = TEXT("no_change_required");
		Response.Summary = FString::Printf(
			TEXT("Dry run: would snap %d actor(s) matching %s to the landscape."),
			Targets.Num(), *FString::Join(Prefixes, TEXT(", ")));
		return FUeremcpEnvelope::SerializeResponse(Response);
	}

	int32 Snapped = 0;
	TArray<FString> NoLandscape;
	for (AActor* Actor : Targets)
	{
		if (!Actor || Cast<ALandscapeProxy>(Actor)) continue;
		const FVector Loc = Actor->GetActorLocation();
		float Z = 0.f;
		if (!UeremcpWorldOps::LandscapeZAt(World, Loc, Z))
		{
			NoLandscape.Add(Actor->GetActorLabel());
			continue;
		}
		Actor->SetActorLocation(FVector(Loc.X, Loc.Y, Z + float(ZOffset)));
		++Snapped;
	}

	Response.Status = NoLandscape.Num() > 0
		? TEXT("partially_completed") : TEXT("created_with_warnings");
	Response.Summary = FString::Printf(
		TEXT("Snapped %d of %d actor(s) to the landscape surface."), Snapped, Targets.Num());
	if (NoLandscape.Num() > 0)
	{
		Response.InterpretationNotes.Add(FString::Printf(
			TEXT("no landscape beneath (left untouched): %s"),
			*FString::Join(NoLandscape, TEXT(", "))));
	}
	Response.CapabilityNotes.Add(
		TEXT("Traces hit ALandscapeProxy only. A generic downward trace stops on the "
			 "first blocker, which in a forest is a tree -- that is what put buildings "
			 "on treetops."));
	Response.Metrics.AssetsAffected = Snapped;
	return FUeremcpEnvelope::SerializeResponse(Response);
}

FString UUeremcpEnvironmentToolset::ClearFoliageInVolumes(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("clear_foliage_in_volumes"), Request, Rejection))
	{
		return Rejection;
	}

	UWorld* World = UeremcpWorldOps::EditorWorld();
	if (!World)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TEXT("No editor world."));
	}

	TArray<FBox> Volumes;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Request.Specification.IsValid()
		&& Request.Specification->TryGetArrayField(TEXT("volumes"), Arr) && Arr)
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			const TSharedPtr<FJsonObject>* Obj = nullptr;
			if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj) continue;
			const TArray<TSharedPtr<FJsonValue>>* C = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* E = nullptr;
			if (!(*Obj)->TryGetArrayField(TEXT("center"), C) || !C || C->Num() != 3) continue;
			if (!(*Obj)->TryGetArrayField(TEXT("extent"), E) || !E || E->Num() != 3) continue;
			const FVector Centre((*C)[0]->AsNumber(), (*C)[1]->AsNumber(), (*C)[2]->AsNumber());
			const FVector Extent((*E)[0]->AsNumber(), (*E)[1]->AsNumber(), (*E)[2]->AsNumber());
			Volumes.Add(FBox(Centre - Extent, Centre + Extent));
		}
	}
	if (Volumes.Num() == 0)
	{
		return FUeremcpEnvelope::MakeRejection(
			Request.RequestId,
			TEXT("clear_foliage_in_volumes requires specification.volumes: a non-empty array "
				 "of {\"center\":[x,y,z],\"extent\":[x,y,z]} boxes in world space. "
				 "WaterBodyExclusionVolume does NOT cull foliage -- that is water exclusion, "
				 "and using it is why trees stayed inside the castle."));
	}

	FUeremcpResponse Response;
	Response.RequestId = Request.RequestId;
	Response.UnderstoodAction = Request.Action;
	Response.Metrics.McpRoundTrips = 1;

	int32 Inspected = 0;
	const int32 Removed = UeremcpWorldOps::ClearFoliageInBoxes(
		World, Volumes, Request.bDryRun, Inspected);

	Response.Status = Request.bDryRun ? TEXT("no_change_required") : TEXT("created_with_warnings");
	Response.Summary = FString::Printf(
		TEXT("%s %d instance(s) inside %d volume(s); %d inspected."),
		Request.bDryRun ? TEXT("Would remove") : TEXT("Removed"),
		Removed, Volumes.Num(), Inspected);
	Response.CapabilityNotes.Add(
		TEXT("Operates on instanced foliage components directly. WaterBodyExclusionVolume "
			 "excludes WATER, not foliage; it has no effect on HISM instances."));
	Response.Metrics.AssetsAffected = Removed;
	return FUeremcpEnvelope::SerializeResponse(Response);
}
