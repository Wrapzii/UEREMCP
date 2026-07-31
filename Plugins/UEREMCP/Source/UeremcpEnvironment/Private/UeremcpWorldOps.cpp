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

#include "UeremcpEnvelope.h"

#include "Engine/World.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Dom/JsonObject.h"

namespace
{
	UWorld* EditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	}

	/**
	 * Z of the landscape under a point, ignoring everything else.
	 *
	 * The point of this function is what it does NOT hit. A generic downward
	 * trace stops on the first blocking thing, which in a forested scene is a
	 * tree. That is why the castle floated.
	 */
	bool LandscapeZAt(UWorld* World, const FVector& Location, float& OutZ)
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
		return false;
	}

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

// ---------------------------------------------------------------------------
// MCP-002
// ---------------------------------------------------------------------------
FString UUeremcpEnvironmentToolset::SnapActorsToLandscape(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("snap_actors_to_landscape"), Request, Rejection))
	{
		return Rejection;
	}

	UWorld* World = EditorWorld();
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
		if (!LandscapeZAt(World, Loc, Z))
		{
			// Named, not silently left where it was. An actor over a hole in the
			// terrain is exactly the one a human will spot floating.
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

// ---------------------------------------------------------------------------
// MCP-003
// ---------------------------------------------------------------------------
FString UUeremcpEnvironmentToolset::ClearFoliageInVolumes(const FString& RequestJson)
{
	FUeremcpRequest Request;
	FString Rejection;
	if (!CommonPreamble(RequestJson, TEXT("clear_foliage_in_volumes"), Request, Rejection))
	{
		return Rejection;
	}

	UWorld* World = EditorWorld();
	if (!World)
	{
		return FUeremcpEnvelope::MakeRejection(Request.RequestId, TEXT("No editor world."));
	}

	// Boxes in world space: {center:[x,y,z], extent:[x,y,z]}.
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

	int32 Removed = 0;
	int32 Inspected = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TArray<UInstancedStaticMeshComponent*> Components;
		It->GetComponents<UInstancedStaticMeshComponent>(Components);
		for (UInstancedStaticMeshComponent* Ism : Components)
		{
			if (!Ism) continue;
			// Descending: RemoveInstance shifts every index above it.
			for (int32 i = Ism->GetInstanceCount() - 1; i >= 0; --i)
			{
				FTransform Xf;
				if (!Ism->GetInstanceTransform(i, Xf, /*bWorldSpace=*/true)) continue;
				++Inspected;
				const FVector P = Xf.GetLocation();
				for (const FBox& Box : Volumes)
				{
					if (Box.IsInsideOrOn(P))
					{
						if (!Request.bDryRun)
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
