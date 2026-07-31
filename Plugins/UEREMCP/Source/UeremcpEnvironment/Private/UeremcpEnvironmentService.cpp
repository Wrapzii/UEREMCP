// UEREMCP — environment build service implementation (WS-01).

#include "UeremcpEnvironmentService.h"

#include "UeremcpNoise.h"

#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Camera/CameraActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerStart.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "EngineUtils.h"
#include "UnrealClient.h"

#include "WaterBodyRiverActor.h"
#include "WaterBodyActor.h"
#include "WaterBodyComponent.h"
#include "WaterSplineComponent.h"
#include "DynamicMeshActor.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "Kismet/GameplayStatics.h"
#include "FileHelpers.h"
#include "ScopedTransaction.h"
#include "UeremcpWeatherFollower.h"

#define UEREMCP_HAS_WATER 1

namespace
{
	bool PathIsScratch(const FString& Path)
	{
		return Path.StartsWith(TEXT("/Game/__UeremcpPoc/"))
			|| Path.StartsWith(TEXT("/Game/__UeremcpTests/"));
	}

	TSharedPtr<FJsonObject> MakeTech(const FString& Name, const FString& Kind, const FString& Evidence)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("name"), Name);
		O->SetStringField(TEXT("kind"), Kind); // real | approximated | blocked
		O->SetStringField(TEXT("evidence"), Evidence);
		return O;
	}

	void AddTechArray(TSharedPtr<FJsonObject>& Root, const FString& Field, const TArray<TSharedPtr<FJsonObject>>& Items)
	{
		TArray<TSharedPtr<FJsonValue>> Arr;
		for (const TSharedPtr<FJsonObject>& Item : Items)
		{
			Arr.Add(MakeShared<FJsonValueObject>(Item));
		}
		Root->SetArrayField(Field, Arr);
	}

	uint16 HeightToUint16(float Normalized01)
	{
		const float Clamped = FMath::Clamp(Normalized01, 0.f, 1.f);
		return uint16(FMath::RoundToInt(Clamped * 65535.f));
	}

	FString NormalizeMapPath(const FString& RequestedPath)
	{
		FString Result = RequestedPath;
		if (Result.EndsWith(TEXT("/")))
		{
			Result.LeftChopInline(1);
			const FString Leaf = FPaths::GetCleanFilename(Result);
			Result += TEXT("/") + Leaf;
		}
		return Result;
	}

	FString ReadEnvironmentTag(UWorld* World, const FString& Prefix)
	{
		if (!World)
		{
			return FString();
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetActorLabel() != TEXT("UEREMCP_Metadata"))
			{
				continue;
			}
			for (const FName& Tag : It->Tags)
			{
				const FString Value = Tag.ToString();
				if (Value.StartsWith(Prefix))
				{
					return Value.RightChop(Prefix.Len());
				}
			}
		}
		return FString();
	}

	FString ReadEnvironmentRevision(UWorld* World)
	{
		return ReadEnvironmentTag(World, TEXT("UEREMCP_REV="));
	}

	int32 DestroyOwnedEnvironmentActors(UWorld* World)
	{
		TArray<AActor*> ToDestroy;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetActorLabel().StartsWith(TEXT("UEREMCP_")))
			{
				ToDestroy.Add(*It);
			}
		}
		for (AActor* Actor : ToDestroy)
		{
			World->DestroyActor(Actor);
		}
		return ToDestroy.Num();
	}

	float SampleNormalizedHeight(
		const TArray<uint16>& Heights,
		const FUeremcpEnvironmentBuildSpec& Spec,
		float LocalX,
		float LocalY)
	{
		const float GridX = FMath::Clamp(LocalX / Spec.ScaleXY, 0.f, float(Spec.SizeX - 1));
		const float GridY = FMath::Clamp(LocalY / Spec.ScaleXY, 0.f, float(Spec.SizeY - 1));
		const int32 X0 = FMath::FloorToInt(GridX);
		const int32 Y0 = FMath::FloorToInt(GridY);
		const int32 X1 = FMath::Min(X0 + 1, Spec.SizeX - 1);
		const int32 Y1 = FMath::Min(Y0 + 1, Spec.SizeY - 1);
		const float Fx = GridX - X0;
		const float Fy = GridY - Y0;
		auto H = [&](int32 X, int32 Y)
		{
			return float(Heights[Y * Spec.SizeX + X]) / 65535.f;
		};
		return FMath::Lerp(
			FMath::Lerp(H(X0, Y0), H(X1, Y0), Fx),
			FMath::Lerp(H(X0, Y1), H(X1, Y1), Fx),
			Fy);
	}

	float NormalizedToWorldHeight(float Normalized, float ScaleZ)
	{
		// [VERIFIED: LandscapeDataAccess.h:26-32] local=(height-32768)*LANDSCAPE_ZSCALE.
		return ((Normalized * 65535.f) - 32768.f) * (1.f / 128.f) * ScaleZ;
	}

	float SampleSlopeDegrees(
		const TArray<uint16>& Heights,
		const FUeremcpEnvironmentBuildSpec& Spec,
		float LocalX,
		float LocalY)
	{
		const float Step = Spec.ScaleXY;
		const float Hx0 = NormalizedToWorldHeight(
			SampleNormalizedHeight(Heights, Spec, LocalX - Step, LocalY), Spec.ScaleZ);
		const float Hx1 = NormalizedToWorldHeight(
			SampleNormalizedHeight(Heights, Spec, LocalX + Step, LocalY), Spec.ScaleZ);
		const float Hy0 = NormalizedToWorldHeight(
			SampleNormalizedHeight(Heights, Spec, LocalX, LocalY - Step), Spec.ScaleZ);
		const float Hy1 = NormalizedToWorldHeight(
			SampleNormalizedHeight(Heights, Spec, LocalX, LocalY + Step), Spec.ScaleZ);
		const float Gradient = FVector2D(
			(Hx1 - Hx0) / (2.f * Step),
			(Hy1 - Hy0) / (2.f * Step)).Size();
		return FMath::RadiansToDegrees(FMath::Atan(Gradient));
	}
}

bool FUeremcpEnvironmentService::ParseBuildSpec(
	const TSharedPtr<FJsonObject>& Spec,
	FUeremcpEnvironmentBuildSpec& Out,
	FString& OutError)
{
	if (!Spec.IsValid())
	{
		OutError = TEXT("specification object is required");
		return false;
	}

	if (Spec->HasField(TEXT("seed")))
	{
		Out.Seed = uint64(Spec->GetNumberField(TEXT("seed")));
	}
	else
	{
		OutError = TEXT("specification.seed is required for deterministic builds (BACKLOG 5.4)");
		return false;
	}

	auto ReadObjNumber = [&](const FString& Obj, const FString& Key, double& InOut)
	{
		InOut = -1.0;
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Spec->TryGetObjectField(Obj, Child) && Child && (*Child)->HasField(Key))
		{
			InOut = (*Child)->GetNumberField(Key);
		}
	};

	double Tmp = 0;
	ReadObjNumber(TEXT("terrain"), TEXT("size_x"), Tmp); if (Tmp > 0) Out.SizeX = int32(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("size_y"), Tmp); if (Tmp > 0) Out.SizeY = int32(Tmp);
	// Alias: terrain.size → square SizeX/SizeY (COVERAGE_PLAN / agent examples).
	ReadObjNumber(TEXT("terrain"), TEXT("size"), Tmp);
	if (Tmp > 0)
	{
		Out.SizeX = int32(Tmp);
		Out.SizeY = int32(Tmp);
	}
	ReadObjNumber(TEXT("terrain"), TEXT("mountain_amplitude"), Tmp); if (Tmp > 0) Out.MountainAmplitude = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("mountain_weight"), Tmp); if (Tmp > 0) Out.MountainAmplitude = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("valley_depth"), Tmp); if (Tmp > 0) Out.ValleyDepth = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("scale_xy"), Tmp); if (Tmp > 0) Out.ScaleXY = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("scale_z"), Tmp); if (Tmp > 0) Out.ScaleZ = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("z_scale"), Tmp); if (Tmp > 0) Out.ScaleZ = float(Tmp);

	ReadObjNumber(TEXT("river"), TEXT("width"), Tmp); if (Tmp > 0) Out.RiverWidth = float(Tmp);
	ReadObjNumber(TEXT("biome"), TEXT("forest_bank_width"), Tmp); if (Tmp > 0) Out.ForestBankWidth = float(Tmp);
	ReadObjNumber(TEXT("biome"), TEXT("max_foliage_instances"), Tmp); if (Tmp > 0) Out.MaxFoliageInstances = int32(Tmp);
	ReadObjNumber(TEXT("biome"), TEXT("slope_limit_deg"), Tmp); if (Tmp > 0) Out.FoliageSlopeLimitDegrees = float(Tmp);
	ReadObjNumber(TEXT("biome"), TEXT("min_normalized_height"), Tmp); if (Tmp >= 0) Out.FoliageMinNormalizedHeight = float(Tmp);
	ReadObjNumber(TEXT("biome"), TEXT("max_normalized_height"), Tmp); if (Tmp > 0) Out.FoliageMaxNormalizedHeight = float(Tmp);

	const TSharedPtr<FJsonObject>* Biome = nullptr;
	if (Spec->TryGetObjectField(TEXT("biome"), Biome) && Biome)
	{
		(*Biome)->TryGetStringField(TEXT("mesh_path"), Out.MeshPath);
	}
	const TSharedPtr<FJsonObject>* Weather = nullptr;
	if (Spec->TryGetObjectField(TEXT("weather"), Weather) && Weather)
	{
		(*Weather)->TryGetStringField(TEXT("rain_system_path"), Out.RainSystemPath);
		(*Weather)->TryGetStringField(TEXT("follow"), Out.WeatherFollow);
		double StreakCount = 0;
		if ((*Weather)->TryGetNumberField(TEXT("streak_count"), StreakCount) && StreakCount > 0)
		{
			Out.RainStreakCount = int32(StreakCount);
		}
	}

	Spec->TryGetStringField(TEXT("fallback_policy"), Out.FallbackPolicy);
	Spec->TryGetStringField(TEXT("destination_level_path"), Out.DestinationLevelPath);

	if (Spec->HasField(TEXT("include")))
	{
		const TSharedPtr<FJsonObject>* Inc = nullptr;
		if (Spec->TryGetObjectField(TEXT("include"), Inc) && Inc)
		{
			(*Inc)->TryGetBoolField(TEXT("terrain"), Out.bIncludeTerrain);
			(*Inc)->TryGetBoolField(TEXT("river"), Out.bIncludeRiver);
			(*Inc)->TryGetBoolField(TEXT("forest"), Out.bIncludeForest);
			(*Inc)->TryGetBoolField(TEXT("rain"), Out.bIncludeRain);
			(*Inc)->TryGetBoolField(TEXT("lighting"), Out.bIncludeLighting);
			(*Inc)->TryGetBoolField(TEXT("capture"), Out.bCaptureScreenshot);
			(*Inc)->TryGetBoolField(TEXT("structures"), Out.bIncludeStructures);
		}
	}

	const TSharedPtr<FJsonObject>* Structures = nullptr;
	if (Spec->TryGetObjectField(TEXT("structures"), Structures) && Structures)
	{
		Out.bIncludeStructures = true;
		double Count = 0;
		if ((*Structures)->TryGetNumberField(TEXT("count"), Count) && Count > 0)
		{
			Out.StructureCount = int32(Count);
		}
	}

	const TSharedPtr<FJsonObject>* Capture = nullptr;
	if (Spec->TryGetObjectField(TEXT("capture"), Capture) && Capture)
	{
		bool bEnabled = false;
		if ((*Capture)->TryGetBoolField(TEXT("enabled"), bEnabled))
		{
			Out.bCaptureScreenshot = bEnabled;
		}
	}

	if (Out.FoliageMinNormalizedHeight > Out.FoliageMaxNormalizedHeight)
	{
		OutError = TEXT("biome.min_normalized_height must be <= max_normalized_height");
		return false;
	}
	if (Out.WeatherFollow != TEXT("player_camera") && Out.WeatherFollow != TEXT("player_pawn"))
	{
		OutError = TEXT("weather.follow must be player_camera or player_pawn");
		return false;
	}
	return true;
}

void FUeremcpEnvironmentService::GenerateHeightmap(
	const FUeremcpEnvironmentBuildSpec& Spec,
	const FUeremcpSplinePath& River,
	TArray<uint16>& OutHeights,
	TSharedPtr<FJsonObject>& OutMetrics)
{
	OutHeights.SetNumUninitialized(Spec.SizeX * Spec.SizeY);
	double MinH = 1.0;
	double MaxH = 0.0;
	double Sum = 0.0;
	int32 ValleySamples = 0;

	const FBox Extents(
		FVector(0, 0, 0),
		FVector(float(Spec.SizeX - 1) * Spec.ScaleXY, float(Spec.SizeY - 1) * Spec.ScaleXY, 0));

	for (int32 Y = 0; Y < Spec.SizeY; ++Y)
	{
		for (int32 X = 0; X < Spec.SizeX; ++X)
		{
			const float Nx = float(X) / float(FMath::Max(1, Spec.SizeX - 1));
			const float Ny = float(Y) / float(FMath::Max(1, Spec.SizeY - 1));
			float H = UeremcpNoise::FBm2D(Spec.Seed, Nx * 6.f, Ny * 6.f, 5, 2.1f, 0.5f);
			H = 0.45f + (H - 0.5f) * 2.f * Spec.MountainAmplitude;

			const FVector World(float(X) * Spec.ScaleXY, float(Y) * Spec.ScaleXY, 0);
			const float Dist = River.DistanceToXY(World);
			const float HalfW = River.WidthAtClosest(World) * 0.5f;
			if (Dist < HalfW)
			{
				const float T = Dist / FMath::Max(1.f, HalfW);
				H -= Spec.ValleyDepth * (1.f - T * T);
				++ValleySamples;
			}

			H = FMath::Clamp(H, 0.02f, 0.98f);
			OutHeights[Y * Spec.SizeX + X] = HeightToUint16(H);
			MinH = FMath::Min(MinH, double(H));
			MaxH = FMath::Max(MaxH, double(H));
			Sum += H;
		}
	}

	OutMetrics = MakeShared<FJsonObject>();
	OutMetrics->SetNumberField(TEXT("verts_x"), Spec.SizeX);
	OutMetrics->SetNumberField(TEXT("verts_y"), Spec.SizeY);
	OutMetrics->SetNumberField(TEXT("height_min"), MinH);
	OutMetrics->SetNumberField(TEXT("height_max"), MaxH);
	OutMetrics->SetNumberField(TEXT("height_mean"), Sum / double(Spec.SizeX * Spec.SizeY));
	OutMetrics->SetNumberField(TEXT("height_range"), MaxH - MinH);
	OutMetrics->SetNumberField(TEXT("valley_samples"), ValleySamples);
	OutMetrics->SetBoolField(TEXT("non_flat"), (MaxH - MinH) > 0.08);
	OutMetrics->SetNumberField(TEXT("river_length"), River.ApproximateLength());
	OutMetrics->SetNumberField(TEXT("river_points"), River.Points.Num());

	// Determinism gate (COVERAGE_PLAN III.4 / III.6 / III.11.2).
	const uint32 Hash = FCrc::MemCrc32(OutHeights.GetData(), OutHeights.Num() * sizeof(uint16));
	OutMetrics->SetStringField(
		TEXT("heightmap_hash"),
		FString::Printf(TEXT("%08x"), Hash));
}

FUeremcpEnvironmentBuildResult FUeremcpEnvironmentService::Build(
	const FUeremcpRequest& Request,
	const FUeremcpEnvironmentBuildSpec& Spec,
	bool bDryRun)
{
	FUeremcpEnvironmentBuildResult Result;
	Result.RealVsApproximated = MakeShared<FJsonObject>();
	Result.ChangeManifest = MakeShared<FJsonObject>();
	Result.StructuralMetrics = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonObject>> Tech;

	FString Dest = Spec.DestinationLevelPath;
	if (Dest.IsEmpty())
	{
		Dest = Request.TargetAssetPath;
	}
	Dest = NormalizeMapPath(Dest);
	if (Dest.IsEmpty())
	{
		Result.Status = TEXT("rejected");
		Result.Summary = TEXT("target.asset_path or specification.destination_level_path is required");
		return Result;
	}
	if (!PathIsScratch(Dest))
	{
		Result.Status = TEXT("rejected");
		Result.Summary = TEXT("Environment builds are restricted to /Game/__UeremcpPoc/ or /Game/__UeremcpTests/");
		Result.CapabilityNotes.Add(TEXT("Never destroy user content (AGENTS.md rule 8)."));
		return Result;
	}

	const int32 QuadsPerComponent = Spec.SectionsPerComponent * Spec.QuadsPerSection;
	if (((Spec.SizeX - 1) % QuadsPerComponent) != 0 || ((Spec.SizeY - 1) % QuadsPerComponent) != 0)
	{
		Result.Status = TEXT("rejected");
		Result.Summary = FString::Printf(
			TEXT("terrain size_x/size_y must be (N*QuadsPerComponent)+1; got %dx%d with quads/component=%d"),
			Spec.SizeX, Spec.SizeY, QuadsPerComponent);
		return Result;
	}

	const FBox Extents(
		FVector(0, 0, 0),
		FVector(float(Spec.SizeX - 1) * Spec.ScaleXY, float(Spec.SizeY - 1) * Spec.ScaleXY, 0));
	const FVector LandscapeOffset(
		-float(Spec.SizeX - 1) * Spec.ScaleXY * 0.5f,
		-float(Spec.SizeY - 1) * Spec.ScaleXY * 0.5f,
		0.f);
	const FUeremcpSplinePath River = UeremcpSpline::MakeRiverAcross(Spec.Seed, Extents, 12, Spec.RiverWidth);
	const FUeremcpSplinePath Exclusion = UeremcpSpline::MakeExclusionFrom(River, Spec.RiverWidth * 0.35f);

	TArray<uint16> Heights;
	TSharedPtr<FJsonObject> HeightMetrics;
	GenerateHeightmap(Spec, River, Heights, HeightMetrics);
	Result.StructuralMetrics = HeightMetrics;
	const FString HeightmapHash = HeightMetrics->GetStringField(TEXT("heightmap_hash"));
	const uint32 RevisionCrc = FCrc::StrCrc32(*FString::Printf(
		TEXT("env-impl-3|%s|%llu|%dx%d|%.4f|%.4f|%.2f|%.2f|%d"),
		*HeightmapHash,
		Spec.Seed,
		Spec.SizeX,
		Spec.SizeY,
		Spec.MountainAmplitude,
		Spec.ValleyDepth,
		Spec.RiverWidth,
		Spec.ForestBankWidth,
		Spec.MaxFoliageInstances));
	Result.Revision = FString::Printf(TEXT("env:%08x"), RevisionCrc);
	Result.InternalOperations += 1;

	if (bDryRun)
	{
		Tech.Add(MakeTech(
			TEXT("landscape_heightmap"),
			TEXT("real"),
			TEXT("ALandscape::Import heightmap path planned [VERIFIED: LandscapeProxy.h:1418-1420]")));
#if UEREMCP_HAS_WATER
		Tech.Add(MakeTech(
			TEXT("water_river"),
			TEXT("real"),
			TEXT("AWaterBodyRiver planned [VERIFIED: WaterBodyRiverActor.h:28]")));
#else
		Tech.Add(MakeTech(
			TEXT("water_river"),
			TEXT("blocked"),
			TEXT("Water headers unavailable at compile time")));
#endif
		Tech.Add(MakeTech(
			TEXT("foliage_scatter"),
			Spec.MeshPath.IsEmpty() ? FString(TEXT("approximated")) : FString(TEXT("real")),
			TEXT("Seeded HISMC / InstancedFoliageActor scatter with exclusion corridor")));
		Tech.Add(MakeTech(
			TEXT("rain_camera_follow"),
			Spec.RainSystemPath.IsEmpty() ? FString(TEXT("approximated")) : FString(TEXT("real")),
			TEXT("Niagara component attach to viewport/player camera when system path provided")));
		Tech.Add(MakeTech(
			TEXT("structures_geometryscript"),
			TEXT("real"),
			TEXT("GeometryScript AppendBox available [VERIFIED: MeshPrimitiveFunctions.h:168] [VERIFIED-RUNTIME: GeometryScripting enabled]")));
		AddTechArray(Result.RealVsApproximated, TEXT("technologies"), Tech);
		Result.ChangeManifest->SetStringField(TEXT("destination"), Dest);
		Result.ChangeManifest->SetNumberField(TEXT("seed"), double(Spec.Seed));
		Result.ChangeManifest->SetBoolField(TEXT("dry_run"), true);
		Result.ChangeManifest->SetStringField(TEXT("revision"), Result.Revision);
		Result.Status = TEXT("no_change_required");
		Result.Summary = FString::Printf(
			TEXT("Dry-run build_environment seed=%llu destination=%s — height_range=%.3f non_flat=%s river_len=%.0f"),
			Spec.Seed,
			*Dest,
			HeightMetrics->GetNumberField(TEXT("height_range")),
			HeightMetrics->GetBoolField(TEXT("non_flat")) ? TEXT("true") : TEXT("false"),
			HeightMetrics->GetNumberField(TEXT("river_length")));
		Result.CapabilityNotes.Add(TEXT("dry_run: no actors spawned; re-call with options.dry_run=false to mutate."));
		Result.CapabilityNotes.Add(TEXT("Batching: internal plan terrain→river→foliage→weather→capture; no second batch layer."));
		return Result;
	}

	if (!GEditor)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("GEditor unavailable — Environment builds require the editor");
		return Result;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	const bool bOwnsMapLifecycle = Request.Action == TEXT("build_environment");
	if (bOwnsMapLifecycle)
	{
		const FString CurrentPackage = World ? World->GetOutermost()->GetName() : FString();
		if (CurrentPackage != Dest)
		{
			if (World && World->GetOutermost()->IsDirty())
			{
				Result.Status = TEXT("rejected");
				Result.Summary = FString::Printf(
					TEXT("Current map %s has unsaved changes; refusing to switch maps and risk user content"),
					*CurrentPackage);
				Result.CapabilityNotes.Add(TEXT("Save or discard the current map explicitly, then retry BuildEnvironment."));
				return Result;
			}

			// [VERIFIED: PackageName.h:450] DoesPackageExist
			// [VERIFIED: FileHelpers.h:45,64] NewBlankMap / LoadMap
			if (FPackageName::DoesPackageExist(Dest))
			{
				World = UEditorLoadingAndSavingUtils::LoadMap(Dest);
			}
			else
			{
				World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
			}
		}
	}
	if (!World)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("No editor world loaded");
		return Result;
	}

	const FString ExistingRevision = ReadEnvironmentRevision(World);
	if (Request.bHasExpectedRevision && Request.ExpectedRevision != ExistingRevision)
	{
		Result.Status = TEXT("rejected");
		Result.Summary = FString::Printf(
			TEXT("expected_revision conflict: expected '%s', current '%s'"),
			*Request.ExpectedRevision,
			ExistingRevision.IsEmpty() ? TEXT("<none>") : *ExistingRevision);
		Result.CapabilityNotes.Add(TEXT("InspectEnvironment and retry with the returned revision."));
		return Result;
	}
	if (!ExistingRevision.IsEmpty() && ExistingRevision == Result.Revision)
	{
		Result.Status = TEXT("no_change_required");
		Result.Summary = FString::Printf(
			TEXT("Environment already matches revision %s at %s"),
			*Result.Revision,
			*Dest);
		Result.ChangeManifest->SetStringField(TEXT("destination"), Dest);
		Result.ChangeManifest->SetStringField(TEXT("revision"), Result.Revision);
		Result.ChangeManifest->SetBoolField(TEXT("dry_run"), false);
		Result.CapabilityNotes.Add(TEXT("Idempotency gate matched saved environment metadata; no actors were changed."));
		return Result;
	}

	const FScopedTransaction Transaction(
		NSLOCTEXT("UEREMCP", "BuildEnvironmentTransaction", "UEREMCP Build Environment"),
		Request.bAtomic);
	const int32 ReplacedActorCount = DestroyOwnedEnvironmentActors(World);
	Result.ChangeManifest->SetNumberField(TEXT("replaced_owned_actors"), ReplacedActorCount);

	TArray<FString> CreatedLabels;
	bool bRiverCreated = false;
	bool bRainCreated = false;
	bool bSaved = false;
	bool bReloaded = false;

	if (Spec.bIncludeTerrain)
	{
		ALandscape* Landscape = World->SpawnActor<ALandscape>(
			LandscapeOffset, FRotator::ZeroRotator);
		if (!Landscape)
		{
			DestroyOwnedEnvironmentActors(World);
			Result.Status = TEXT("rolled_back");
			Result.Summary = TEXT("Failed to spawn ALandscape; removed UEREMCP-owned actors");
			return Result;
		}
		Landscape->SetFlags(RF_Transactional);
		Landscape->SetActorLabel(TEXT("UEREMCP_Landscape"));
		Landscape->SetActorRelativeScale3D(FVector(Spec.ScaleXY, Spec.ScaleXY, Spec.ScaleZ));

		TMap<FGuid, TArray<uint16>> HeightDataPerLayers;
		HeightDataPerLayers.Add(FGuid(), Heights);
		TMap<FGuid, TArray<FLandscapeImportLayerInfo>> MaterialLayerDataPerLayers;
		MaterialLayerDataPerLayers.Add(FGuid(), TArray<FLandscapeImportLayerInfo>());

		// [VERIFIED: LandscapeProxy.h:1418-1420]
		Landscape->Import(
			FGuid::NewGuid(),
			0,
			0,
			Spec.SizeX - 1,
			Spec.SizeY - 1,
			Spec.SectionsPerComponent,
			Spec.QuadsPerSection,
			HeightDataPerLayers,
			TEXT(""),
			MaterialLayerDataPerLayers,
			ELandscapeImportAlphamapType::Additive,
			TArrayView<const FLandscapeLayer>());

		CreatedLabels.Add(TEXT("UEREMCP_Landscape"));
		Tech.Add(MakeTech(
			TEXT("landscape_heightmap"),
			TEXT("real"),
			TEXT("ALandscape::Import applied [VERIFIED: LandscapeProxy.h:1418-1420]")));
		Result.InternalOperations += 2;
	}

	if (Spec.bIncludeRiver)
	{
#if UEREMCP_HAS_WATER
		// Clear bAffectsLandscape in CustomPreSpawnInitialization, before
		// OnLevelActorAddedToWorld sees the actor. Deferred spawn alone is too late:
		// the actor-added delegate fires from SpawnActor before FinishSpawningActor.
		// SetupDefaultMaterials + brush rebuild deadlocks the game thread when invoked
		// under a synchronous MCP tools/call.
		// [VERIFIED-RUNTIME: BuildEnvironment hung at WaterBrushManager spawn 2026-07-30]
		// [VERIFIED: WaterBodyComponent.h:630] bAffectsLandscape
		// [VERIFIED: WaterEditorModule.cpp:190] AffectsLandscape() gates brush spawn
		// [VERIFIED: World.h:517] CustomPreSpawnInitialization
		const FTransform RiverXform(River.Points[0].Location + LandscapeOffset);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.CustomPreSpawnInitialization = [](AActor* SpawnedActor)
		{
			if (AWaterBodyRiver* SpawnedRiver = Cast<AWaterBodyRiver>(SpawnedActor))
			{
				if (UWaterBodyComponent* WaterComp = SpawnedRiver->GetWaterBodyComponent())
				{
					WaterComp->bAffectsLandscape = false;
				}
			}
		};
		AWaterBodyRiver* RiverActor = World->SpawnActor<AWaterBodyRiver>(
			AWaterBodyRiver::StaticClass(),
			RiverXform,
			SpawnParameters);
		if (RiverActor)
		{
			RiverActor->SetActorLabel(TEXT("UEREMCP_River"));
			// [VERIFIED: WaterBodyActor.h:103] GetWaterSpline
			if (UWaterSplineComponent* Spline = RiverActor->GetWaterSpline())
			{
				Spline->ClearSplinePoints(false);
				for (int32 I = 0; I < River.Points.Num(); ++I)
				{
					Spline->AddSplinePoint(
						River.Points[I].Location + LandscapeOffset,
						ESplineCoordinateSpace::World,
						false);
				}
				Spline->UpdateSpline();
			}
			CreatedLabels.Add(TEXT("UEREMCP_River"));
			bRiverCreated = true;
			Tech.Add(MakeTech(
				TEXT("water_river"),
				TEXT("real"),
				TEXT("AWaterBodyRiver pre-spawn bAffectsLandscape=false, spline set [VERIFIED: WaterBodyRiverActor.h:28] [VERIFIED: WaterBodyComponent.h:630]")));
			Result.CapabilityNotes.Add(
				TEXT("River uses visible water mesh without landscape water brush (heightmap valley is authoritative)."));
			Result.InternalOperations += 2;
		}
		else
		{
			Result.Warnings.Add(TEXT("AWaterBodyRiver deferred spawn returned null — river skipped"));
			Tech.Add(MakeTech(TEXT("water_river"), TEXT("blocked"), TEXT("SpawnActorDeferred<AWaterBodyRiver> returned null")));
		}
#else
		Result.Warnings.Add(TEXT("Water plugin headers not compiled in — river approximated as empty channel only"));
		Tech.Add(MakeTech(TEXT("water_river"), TEXT("approximated"), TEXT("Valley carved in heightmap only; no AWaterBodyRiver")));
#endif
	}

	int32 FoliageCount = 0;
	int32 ExclusionViolations = 0;
	int32 LeftBankCount = 0;
	int32 RightBankCount = 0;
	int32 SlopeRejected = 0;
	int32 HeightRejected = 0;
	if (Spec.bIncludeForest)
	{
		UStaticMesh* Mesh = nullptr;
		if (!Spec.MeshPath.IsEmpty())
		{
			Mesh = LoadObject<UStaticMesh>(nullptr, *Spec.MeshPath);
		}
		if (!Mesh)
		{
			// Engine default cube as last-resort visible marker when no tree mesh supplied.
			Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
			Result.Warnings.Add(TEXT("biome.mesh_path missing/unloadable — using /Engine/BasicShapes/Cube as foliage placeholder"));
			Tech.Add(MakeTech(
				TEXT("foliage_scatter"),
				TEXT("approximated"),
				TEXT("HISMC cubes stand in for trees; supply biome.mesh_path for real foliage")));
		}
		else
		{
			Tech.Add(MakeTech(
				TEXT("foliage_scatter"),
				TEXT("real"),
				TEXT("Seeded HierarchicalInstancedStaticMesh scatter with exclusion corridor")));
		}

		if (Mesh)
		{
			AActor* Holder = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
			Holder->SetActorLabel(TEXT("UEREMCP_Forest"));
			USceneComponent* Root = NewObject<USceneComponent>(Holder);
			Root->SetMobility(EComponentMobility::Static);
			Holder->SetRootComponent(Root);
			Root->RegisterComponent();

			UHierarchicalInstancedStaticMeshComponent* Hism =
				NewObject<UHierarchicalInstancedStaticMeshComponent>(Holder);
			Hism->SetStaticMesh(Mesh);
			Hism->SetupAttachment(Root);
			Hism->RegisterComponent();
			Holder->AddInstanceComponent(Hism);

			const FUeremcpSplinePath& Bank = Exclusion;
			const float Inner = Spec.RiverWidth * 0.55f;
			const float Outer = Inner + Spec.ForestBankWidth;
			// Corridor-biased samples (both banks) — random XY rarely hits plantable
			// shoulders when valley carve creates steep near-channel slopes
			// [VERIFIED-RUNTIME: seed=42 yielded slope_rejected=168 foliage=0 at 32°].
			const int32 MaxAttempts = Spec.MaxFoliageInstances * 12;
			for (int32 Attempt = 0; Attempt < MaxAttempts && FoliageCount < Spec.MaxFoliageInstances; ++Attempt)
			{
				const float Along = UeremcpNoise::ValueNoise2D(Spec.Seed ^ 0xF01ull, Attempt, 1);
				const float BandT = 0.40f + 0.50f * UeremcpNoise::ValueNoise2D(Spec.Seed ^ 0xF02ull, Attempt, 2);
				const float Side = (Attempt % 2 == 0) ? 1.f : -1.f;
				const float DistTarget = Inner + Spec.ForestBankWidth * BandT;
				FVector2D Center;
				FVector2D Perp;
				if (!River.SampleAlongXY(Along, Center, Perp))
				{
					continue;
				}
				const FVector Local(Center.X + Perp.X * DistTarget * Side, Center.Y + Perp.Y * DistTarget * Side, 0.f);
				const float Dist = Bank.DistanceToXY(Local);
				if (Dist < Inner || Dist > Outer)
				{
					continue;
				}
				const float Density = UeremcpNoise::SmoothNoise2D(
					Spec.Seed ^ 0xD00Dull, Local.X * 0.001f, Local.Y * 0.001f);
				if (Density < 0.28f)
				{
					continue;
				}
				const float NormalizedHeight = SampleNormalizedHeight(
					Heights, Spec, Local.X, Local.Y);
				if (NormalizedHeight < Spec.FoliageMinNormalizedHeight
					|| NormalizedHeight > Spec.FoliageMaxNormalizedHeight)
				{
					++HeightRejected;
					continue;
				}
				const float SlopeDegrees = SampleSlopeDegrees(
					Heights, Spec, Local.X, Local.Y);
				if (SlopeDegrees > Spec.FoliageSlopeLimitDegrees)
				{
					++SlopeRejected;
					continue;
				}
				const FVector WorldPos(
					Local.X - float(Spec.SizeX - 1) * Spec.ScaleXY * 0.5f,
					Local.Y - float(Spec.SizeY - 1) * Spec.ScaleXY * 0.5f,
					NormalizedToWorldHeight(NormalizedHeight, Spec.ScaleZ) + 50.f);
				FTransform Xf;
				Xf.SetLocation(WorldPos);
				Xf.SetScale3D(FVector(0.4f + Density * 0.8f));
				Hism->AddInstance(Xf);
				if (Side >= 0.f)
				{
					++LeftBankCount;
				}
				else
				{
					++RightBankCount;
				}
				++FoliageCount;
			}
			// Re-measure exclusion corridor (COVERAGE_PLAN III.11.3) — not by eye.
			const float InnerGate = Spec.RiverWidth * 0.55f;
			for (int32 Ii = 0; Ii < Hism->GetInstanceCount(); ++Ii)
			{
				FTransform InstXf;
				Hism->GetInstanceTransform(Ii, InstXf, true);
				const FVector LocalCheck(
					InstXf.GetLocation().X + float(Spec.SizeX - 1) * Spec.ScaleXY * 0.5f,
					InstXf.GetLocation().Y + float(Spec.SizeY - 1) * Spec.ScaleXY * 0.5f,
					0.f);
				if (Bank.DistanceToXY(LocalCheck) < InnerGate)
				{
					++ExclusionViolations;
				}
			}
			CreatedLabels.Add(TEXT("UEREMCP_Forest"));
			Result.InternalOperations += 1;
		}
	}
	Result.StructuralMetrics->SetNumberField(TEXT("foliage_instances"), FoliageCount);
	Result.StructuralMetrics->SetBoolField(TEXT("forest_bounded"), FoliageCount <= Spec.MaxFoliageInstances);
	Result.StructuralMetrics->SetNumberField(TEXT("exclusion_violations"), ExclusionViolations);
	Result.StructuralMetrics->SetBoolField(TEXT("exclusion_respected"), ExclusionViolations == 0);
	Result.StructuralMetrics->SetNumberField(TEXT("left_bank_instances"), LeftBankCount);
	Result.StructuralMetrics->SetNumberField(TEXT("right_bank_instances"), RightBankCount);
	Result.StructuralMetrics->SetBoolField(
		TEXT("both_banks_populated"), LeftBankCount > 0 && RightBankCount > 0);
	Result.StructuralMetrics->SetNumberField(TEXT("slope_rejected_candidates"), SlopeRejected);
	Result.StructuralMetrics->SetNumberField(TEXT("height_rejected_candidates"), HeightRejected);
	Result.StructuralMetrics->SetNumberField(
		TEXT("slope_limit_degrees"), Spec.FoliageSlopeLimitDegrees);

	if (Spec.bIncludeLighting)
	{
		ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 500), FRotator(-50.f, 30.f, 0.f));
		if (Sun)
		{
			Sun->SetActorLabel(TEXT("UEREMCP_Sun"));
			// [VERIFIED: LightComponent.h:286,296]
			Sun->GetLightComponent()->SetIntensity(4.0f);
			Sun->GetLightComponent()->SetLightColor(FLinearColor(0.56f, 0.64f, 0.78f));
			CreatedLabels.Add(TEXT("UEREMCP_Sun"));
		}
		ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Sky)
		{
			Sky->SetActorLabel(TEXT("UEREMCP_SkyLight"));
			// [VERIFIED: SkyLightComponent.h:235,265]
			Sky->GetLightComponent()->SetIntensity(0.65f);
			Sky->GetLightComponent()->SetLowerHemisphereColor(
				FLinearColor(0.04f, 0.06f, 0.09f));
			CreatedLabels.Add(TEXT("UEREMCP_SkyLight"));
		}
		AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Fog)
		{
			Fog->SetActorLabel(TEXT("UEREMCP_RainFog"));
			// [VERIFIED: ExponentialHeightFogComponent.h:251,257,287]
			Fog->GetComponent()->SetFogDensity(0.025f);
			Fog->GetComponent()->SetFogHeightFalloff(0.12f);
			Fog->GetComponent()->SetFogInscatteringColor(
				FLinearColor(0.19f, 0.24f, 0.31f));
			CreatedLabels.Add(TEXT("UEREMCP_RainFog"));
		}
		const FVector ViewLocation(
			-float(Spec.SizeX - 1) * Spec.ScaleXY * 0.36f,
			-float(Spec.SizeY - 1) * Spec.ScaleXY * 0.36f,
			6500.f);
		const FRotator ViewRotation = (FVector::ZeroVector - ViewLocation).Rotation();
		if (ACameraActor* Camera = World->SpawnActor<ACameraActor>(ViewLocation, ViewRotation))
		{
			Camera->SetActorLabel(TEXT("UEREMCP_RainyViewpoint"));
			CreatedLabels.Add(TEXT("UEREMCP_RainyViewpoint"));
		}
		if (APlayerStart* Start = World->SpawnActor<APlayerStart>(
			ViewLocation + FVector(0.f, 0.f, -500.f), ViewRotation))
		{
			Start->SetActorLabel(TEXT("UEREMCP_PlayerStart"));
			CreatedLabels.Add(TEXT("UEREMCP_PlayerStart"));
		}
		Tech.Add(MakeTech(TEXT("lighting"), TEXT("real"), TEXT("DirectionalLight + SkyLight + ExponentialHeightFog")));
		Result.StructuralMetrics->SetStringField(TEXT("lighting_preset"), TEXT("rainy_overcast"));
		Result.StructuralMetrics->SetStringField(TEXT("viewpoint"), TEXT("UEREMCP_RainyViewpoint"));
		Result.InternalOperations += 5;
	}

	if (Spec.bIncludeRain)
	{
		UNiagaraSystem* RainSys = nullptr;
		if (!Spec.RainSystemPath.IsEmpty())
		{
			RainSys = LoadObject<UNiagaraSystem>(nullptr, *Spec.RainSystemPath);
		}
		AUeremcpWeatherFollower* RainActor = World->SpawnActor<AUeremcpWeatherFollower>(
			FVector(0, 0, 400), FRotator::ZeroRotator);
		if (RainActor)
		{
			RainActor->SetActorLabel(TEXT("UEREMCP_Rain"));
			if (RainSys)
			{
				RainActor->NiagaraRain->SetAsset(RainSys);
				RainActor->FallbackRain->SetVisibility(false);
				Tech.Add(MakeTech(
					TEXT("rain_camera_follow"),
					TEXT("real"),
					TEXT("AUeremcpWeatherFollower ticks to player camera with supplied Niagara rain")));
			}
			else
			{
				RainActor->NiagaraRain->SetVisibility(false);
				RainActor->ConfigureFallbackRain(int32(Spec.Seed), Spec.RainStreakCount);
				Tech.Add(MakeTech(
					TEXT("rain_camera_follow"),
					TEXT("approximated"),
					TEXT("Camera-follow transform is real; visible rain uses bounded instanced streak fallback")));
				Result.Warnings.Add(
					TEXT("rain_system_path missing — using visible instanced-streak rain fallback"));
			}
			CreatedLabels.Add(TEXT("UEREMCP_Rain"));
			bRainCreated = true;
			Result.StructuralMetrics->SetStringField(TEXT("weather_follow"), Spec.WeatherFollow);
			Result.StructuralMetrics->SetBoolField(TEXT("weather_follower_tick_enabled"), true);
			Result.StructuralMetrics->SetNumberField(
				TEXT("fallback_rain_streaks"),
				RainSys ? 0 : RainActor->FallbackRain->GetInstanceCount());
			Result.InternalOperations += 1;
		}
	}

	if (Spec.bIncludeStructures)
	{
		// GeometryScript AppendBox along river spline [VERIFIED: MeshPrimitiveFunctions.h:168]
		// [VERIFIED: ADynamicMeshActor DynamicMeshActor.h:16]
		int32 Placed = 0;
		const int32 Count = FMath::Clamp(Spec.StructureCount, 1, 32);
		for (int32 I = 0; I < Count && I < River.Points.Num(); ++I)
		{
			const FVector Loc = River.Points[I].Location
				+ LandscapeOffset
				+ FVector(River.Points[I].Width * 0.75f, 0.f, 100.f);
			ADynamicMeshActor* Structure = World->SpawnActor<ADynamicMeshActor>(Loc, FRotator::ZeroRotator);
			if (!Structure)
			{
				continue;
			}
			Structure->SetActorLabel(FString::Printf(TEXT("UEREMCP_Structure_%d"), I));
			if (UDynamicMeshComponent* DMC = Structure->GetDynamicMeshComponent())
			{
				if (UDynamicMesh* Mesh = DMC->GetDynamicMesh())
				{
					FGeometryScriptPrimitiveOptions Opts;
					UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
						Mesh,
						Opts,
						FTransform::Identity,
						200.f, 200.f, 400.f,
						0, 0, 0,
						EGeometryScriptPrimitiveOriginMode::Base,
						nullptr);
					DMC->NotifyMeshUpdated();
					++Placed;
				}
			}
			CreatedLabels.Add(Structure->GetActorLabel());
		}
		Result.StructuralMetrics->SetNumberField(TEXT("structures_placed"), Placed);
		Tech.Add(MakeTech(
			TEXT("structures_geometryscript"),
			Placed > 0 ? TEXT("real") : TEXT("blocked"),
			TEXT("ADynamicMeshActor + GeometryScript AppendBox [VERIFIED: MeshPrimitiveFunctions.h:168]")));
		Result.InternalOperations += Placed;
	}

	if (Spec.bCaptureScreenshot)
	{
		const FString ShotDir = FPaths::ProjectSavedDir() / TEXT("UEREMCP") / TEXT("EnvironmentCapture");
		IFileManager::Get().MakeDirectory(*ShotDir, true);
		const FString ShotPath = ShotDir / FString::Printf(TEXT("env_%llu.png"), Spec.Seed);
		FScreenshotRequest::RequestScreenshot(ShotPath, false, false);
		Result.ScreenshotPaths.Add(ShotPath);
		Result.Warnings.Add(TEXT("Screenshot requested asynchronously — confirm file exists before treating as pixel gate"));
		Tech.Add(MakeTech(
			TEXT("capture"),
			TEXT("approximated"),
			TEXT("FScreenshotRequest — not VisualCapture warm-up frames; human review still required (BACKLOG 5.8)")));
		Result.InternalOperations += 1;
	}

	if (AActor* Metadata = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		Metadata->SetActorLabel(TEXT("UEREMCP_Metadata"));
		Metadata->Tags.Add(FName(*FString::Printf(TEXT("UEREMCP_REV=%s"), *Result.Revision)));
		Metadata->Tags.Add(FName(*FString::Printf(TEXT("UEREMCP_SEED=%llu"), Spec.Seed)));
		Metadata->Tags.Add(FName(*FString::Printf(TEXT("UEREMCP_RIVER_WIDTH=%.3f"), Spec.RiverWidth)));
		Metadata->Tags.Add(FName(*FString::Printf(TEXT("UEREMCP_HEIGHTMAP_HASH=%s"), *HeightmapHash)));
		CreatedLabels.Add(TEXT("UEREMCP_Metadata"));
		++Result.InternalOperations;
	}

	AddTechArray(Result.RealVsApproximated, TEXT("technologies"), Tech);
	TArray<TSharedPtr<FJsonValue>> Labels;
	for (const FString& L : CreatedLabels)
	{
		Labels.Add(MakeShared<FJsonValueString>(L));
	}
	Result.ChangeManifest->SetArrayField(TEXT("created_actors"), Labels);
	Result.ChangeManifest->SetStringField(TEXT("destination"), Dest);
	Result.ChangeManifest->SetNumberField(TEXT("seed"), double(Spec.Seed));
	Result.ChangeManifest->SetBoolField(TEXT("dry_run"), false);
	Result.ChangeManifest->SetStringField(TEXT("revision"), Result.Revision);

	const bool bNonFlat = Result.StructuralMetrics->GetBoolField(TEXT("non_flat"));
	const bool bForestOk = !Spec.bIncludeForest || FoliageCount > 0;
	const bool bBothBanks = !Spec.bIncludeForest || (LeftBankCount > 0 && RightBankCount > 0);
	const bool bOpenChannel = !Spec.bIncludeForest || ExclusionViolations == 0;
	const bool bRiverOk = !Spec.bIncludeRiver || bRiverCreated;
	const bool bWeatherOk = !Spec.bIncludeRain || bRainCreated;

	if (bOwnsMapLifecycle && Request.bSave)
	{
		// [VERIFIED: FileHelpers.h:67-75] SaveMap
		bSaved = UEditorLoadingAndSavingUtils::SaveMap(World, Dest);
		Result.InternalOperations += 1;
		if (!bSaved)
		{
			DestroyOwnedEnvironmentActors(World);
			Result.Status = TEXT("rolled_back");
			Result.Summary = FString::Printf(
				TEXT("Failed to save generated map %s; removed UEREMCP-owned actors"),
				*Dest);
			Result.StructuralMetrics->SetBoolField(TEXT("saved"), false);
			return Result;
		}
		if (Request.bValidate)
		{
			UWorld* ReloadedWorld = UEditorLoadingAndSavingUtils::LoadMap(Dest);
			bReloaded = ReloadedWorld != nullptr
				&& ReadEnvironmentRevision(ReloadedWorld) == Result.Revision;
			Result.InternalOperations += 1;
		}
	}
	Result.StructuralMetrics->SetBoolField(TEXT("saved"), bSaved);
	Result.StructuralMetrics->SetBoolField(TEXT("reloaded"), bReloaded);
	Result.StructuralMetrics->SetBoolField(TEXT("compile_not_applicable"), true);
	Result.StructuralMetrics->SetBoolField(TEXT("river_continuous"), bRiverOk);
	Result.ChangeManifest->SetBoolField(TEXT("saved"), bSaved);
	Result.ChangeManifest->SetBoolField(TEXT("reloaded"), bReloaded);

	const bool bPersistenceOk = !bOwnsMapLifecycle
		|| !Request.bSave
		|| (bSaved && (!Request.bValidate || bReloaded));
	if (bNonFlat && bForestOk && bBothBanks && bOpenChannel
		&& bRiverOk && bWeatherOk && bPersistenceOk)
	{
		Result.Status = Result.Warnings.IsEmpty()
			? TEXT("created_and_validated")
			: TEXT("created_with_warnings");
		Result.Summary = FString::Printf(
			TEXT("Built environment seed=%llu actors=%d foliage=%d banks=%d/%d height_range=%.3f saved=%s reloaded=%s"),
			Spec.Seed, CreatedLabels.Num(), FoliageCount,
			LeftBankCount, RightBankCount,
			Result.StructuralMetrics->GetNumberField(TEXT("height_range")),
			bSaved ? TEXT("true") : TEXT("false"),
			bReloaded ? TEXT("true") : TEXT("false"));
	}
	else
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = FString::Printf(
			TEXT("Environment structural gates failed: non_flat=%s forest=%s both_banks=%s open_channel=%s river=%s weather=%s persistence=%s"),
			bNonFlat ? TEXT("true") : TEXT("false"),
			bForestOk ? TEXT("true") : TEXT("false"),
			bBothBanks ? TEXT("true") : TEXT("false"),
			bOpenChannel ? TEXT("true") : TEXT("false"),
			bRiverOk ? TEXT("true") : TEXT("false"),
			bWeatherOk ? TEXT("true") : TEXT("false"),
			bPersistenceOk ? TEXT("true") : TEXT("false"));
	}
	Result.CapabilityNotes.Add(TEXT("World verification leans on structural metrics + human review; screenshots are not a gate (BACKLOG 5.8)."));
	Result.CapabilityNotes.Add(TEXT("CaptureWorldFrames is the general world capture hook; BuildEnvironment capture is supplementary only."));
	return Result;
}

FUeremcpEnvironmentBuildResult FUeremcpEnvironmentService::Inspect(const FString& LevelOrPackagePath)
{
	FUeremcpEnvironmentBuildResult Result;
	Result.StructuralMetrics = MakeShared<FJsonObject>();
	if (!GEditor)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("GEditor unavailable");
		return Result;
	}
	UWorld* World = GEditor->PlayWorld
		? GEditor->PlayWorld.Get()
		: GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("No editor world");
		return Result;
	}

	int32 Landscapes = 0;
	int32 Rivers = 0;
	int32 Rain = 0;
	int32 FoliageActors = 0;
	int32 FoliageInstances = 0;
	int32 WeatherFollowSamples = 0;
	float WeatherFollowDistance = 0.f;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const FString Label = It->GetActorLabel();
		if (Label.Contains(TEXT("UEREMCP_Landscape")) || It->IsA(ALandscape::StaticClass()))
		{
			++Landscapes;
		}
		if (Label.Contains(TEXT("UEREMCP_River")))
		{
			++Rivers;
		}
		if (Label == TEXT("UEREMCP_Rain"))
		{
			++Rain;
			if (const AUeremcpWeatherFollower* Follower =
				Cast<AUeremcpWeatherFollower>(*It))
			{
				WeatherFollowSamples = Follower->FollowSamples;
				WeatherFollowDistance = FVector::Distance(
					Follower->FirstTrackedLocation,
					Follower->LastTrackedLocation);
			}
		}
		if (Label.Contains(TEXT("UEREMCP_Forest")))
		{
			++FoliageActors;
			if (const UHierarchicalInstancedStaticMeshComponent* Hism =
				It->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>())
			{
				FoliageInstances += Hism->GetInstanceCount();
			}
		}
	}
	Result.StructuralMetrics->SetNumberField(TEXT("landscape_actors"), Landscapes);
	Result.StructuralMetrics->SetNumberField(TEXT("river_actors"), Rivers);
	Result.StructuralMetrics->SetNumberField(TEXT("rain_actors"), Rain);
	Result.StructuralMetrics->SetNumberField(TEXT("forest_actors"), FoliageActors);
	Result.StructuralMetrics->SetNumberField(TEXT("foliage_instances"), FoliageInstances);
	Result.StructuralMetrics->SetNumberField(TEXT("weather_follow_samples"), WeatherFollowSamples);
	Result.StructuralMetrics->SetNumberField(TEXT("weather_follow_distance_cm"), WeatherFollowDistance);
	Result.StructuralMetrics->SetBoolField(TEXT("weather_followed_10m"), WeatherFollowDistance >= 1000.f);
	Result.StructuralMetrics->SetStringField(
		TEXT("world_type"),
		GEditor->PlayWorld ? TEXT("PIE") : TEXT("Editor"));
	Result.Revision = ReadEnvironmentRevision(World);
	Result.StructuralMetrics->SetStringField(TEXT("revision"), Result.Revision);
	Result.StructuralMetrics->SetStringField(TEXT("loaded_package"), World->GetOutermost()->GetName());
	Result.StructuralMetrics->SetStringField(TEXT("query_path"), LevelOrPackagePath);
	Result.Status = TEXT("no_change_required");
	Result.Summary = FString::Printf(
		TEXT("InspectEnvironment: landscapes=%d rivers=%d rain=%d forest=%d"),
		Landscapes, Rivers, Rain, FoliageActors);
	return Result;
}

FUeremcpEnvironmentBuildResult FUeremcpEnvironmentService::Validate(
	const FString& LevelOrPackagePath,
	const TSharedPtr<FJsonObject>& Gates)
{
	const double StartSeconds = FPlatformTime::Seconds();
	FUeremcpEnvironmentBuildResult Inspected = Inspect(LevelOrPackagePath);
	TSharedPtr<FJsonObject> GateResult = MakeShared<FJsonObject>();
	const bool bHasLandscape = Inspected.StructuralMetrics->GetNumberField(TEXT("landscape_actors")) > 0;
	const bool bHasRiver = Inspected.StructuralMetrics->GetNumberField(TEXT("river_actors")) > 0;
	const bool bHasForest = Inspected.StructuralMetrics->GetNumberField(TEXT("forest_actors")) > 0;
	const bool bHasRain = Inspected.StructuralMetrics->GetNumberField(TEXT("rain_actors")) > 0;
	const int32 FoliageInstances = int32(
		Inspected.StructuralMetrics->GetNumberField(TEXT("foliage_instances")));

	UWorld* World = GEditor && GEditor->PlayWorld
		? GEditor->PlayWorld.Get()
		: (GEditor ? GEditor->GetEditorWorldContext().World() : nullptr);
	ALandscape* Landscape = nullptr;
	UWaterSplineComponent* RiverSpline = nullptr;
	UHierarchicalInstancedStaticMeshComponent* ForestHism = nullptr;
	if (World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!Landscape)
			{
				Landscape = Cast<ALandscape>(*It);
			}
			if (It->GetActorLabel() == TEXT("UEREMCP_River"))
			{
				if (AWaterBodyRiver* River = Cast<AWaterBodyRiver>(*It))
				{
					RiverSpline = River->GetWaterSpline();
				}
			}
			if (It->GetActorLabel() == TEXT("UEREMCP_Forest"))
			{
				ForestHism = It->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>();
			}
		}
	}

	bool bNonFlat = false;
	float ReloadedHeightRange = 0.f;
	if (Landscape)
	{
		int32 SizeX = 0;
		int32 SizeY = 0;
		TArray<float> Values;
		// [VERIFIED: LandscapeProxy.h:1105] GetHeightValues
		Landscape->GetHeightValues(SizeX, SizeY, Values);
		if (!Values.IsEmpty())
		{
			float MinHeight = TNumericLimits<float>::Max();
			float MaxHeight = TNumericLimits<float>::Lowest();
			for (float Height : Values)
			{
				MinHeight = FMath::Min(MinHeight, Height);
				MaxHeight = FMath::Max(MaxHeight, Height);
			}
			ReloadedHeightRange = MaxHeight - MinHeight;
			bNonFlat = ReloadedHeightRange > 100.f;
		}
	}

	int32 LeftBank = 0;
	int32 RightBank = 0;
	int32 ExclusionViolations = 0;
	const float RiverWidth = World
		? FCString::Atof(*ReadEnvironmentTag(World, TEXT("UEREMCP_RIVER_WIDTH=")))
		: 0.f;
	const float ExclusionDistance = FMath::Max(1.f, RiverWidth * 0.55f);
	if (RiverSpline && ForestHism)
	{
		for (int32 Index = 0; Index < ForestHism->GetInstanceCount(); ++Index)
		{
			FTransform Transform;
			if (!ForestHism->GetInstanceTransform(Index, Transform, true))
			{
				continue;
			}
			const FVector Position = Transform.GetLocation();
			// [VERIFIED: SplineComponent.h:841,849]
			const FVector Closest = RiverSpline->FindLocationClosestToWorldLocation(
				Position, ESplineCoordinateSpace::World);
			const FVector Tangent = RiverSpline->FindTangentClosestToWorldLocation(
				Position, ESplineCoordinateSpace::World);
			if (FVector2D::Distance(FVector2D(Position), FVector2D(Closest)) < ExclusionDistance)
			{
				++ExclusionViolations;
			}
			const FVector2D Delta(Position.X - Closest.X, Position.Y - Closest.Y);
			const float Side = Tangent.X * Delta.Y - Tangent.Y * Delta.X;
			Side >= 0.f ? ++LeftBank : ++RightBank;
		}
	}

	const bool bRiverContinuous = RiverSpline
		&& RiverSpline->GetNumberOfSplinePoints() >= 4
		&& RiverSpline->GetSplineLength() > 1000.f;
	const bool bBothBanks = LeftBank > 0 && RightBank > 0;
	const bool bOpenChannel = ExclusionViolations == 0;
	bool bRequireWeatherFollow = false;
	if (Gates.IsValid())
	{
		Gates->TryGetBoolField(TEXT("require_weather_follow_10m"), bRequireWeatherFollow);
	}
	const bool bWeatherFollowed = Inspected.StructuralMetrics->GetBoolField(
		TEXT("weather_followed_10m"));
	GateResult->SetBoolField(TEXT("has_landscape"), bHasLandscape);
	GateResult->SetBoolField(TEXT("has_river"), bHasRiver);
	GateResult->SetBoolField(TEXT("has_forest"), bHasForest);
	GateResult->SetBoolField(TEXT("has_rain"), bHasRain);
	GateResult->SetBoolField(TEXT("non_flat"), bNonFlat);
	GateResult->SetNumberField(TEXT("reloaded_height_range_cm"), ReloadedHeightRange);
	GateResult->SetBoolField(TEXT("river_continuous"), bRiverContinuous);
	GateResult->SetNumberField(TEXT("left_bank_instances"), LeftBank);
	GateResult->SetNumberField(TEXT("right_bank_instances"), RightBank);
	GateResult->SetBoolField(TEXT("both_banks_populated"), bBothBanks);
	GateResult->SetNumberField(TEXT("exclusion_violations"), ExclusionViolations);
	GateResult->SetBoolField(TEXT("open_channel"), bOpenChannel);
	GateResult->SetBoolField(TEXT("weather_followed_10m"), bWeatherFollowed);
	GateResult->SetNumberField(TEXT("foliage_instances"), FoliageInstances);
	Inspected.StructuralMetrics->SetObjectField(TEXT("gates"), GateResult);
	Inspected.StructuralMetrics->SetNumberField(
		TEXT("validation_elapsed_ms"),
		(FPlatformTime::Seconds() - StartSeconds) * 1000.0);

	const bool bOk = bHasLandscape && bHasRiver && bHasForest && bHasRain
		&& bNonFlat && bRiverContinuous && bBothBanks && bOpenChannel
		&& FoliageInstances > 0
		&& (!bRequireWeatherFollow || bWeatherFollowed);
	Inspected.Status = bOk ? TEXT("no_change_required") : TEXT("failed_validation");
	Inspected.Summary = bOk
		? TEXT("ValidateEnvironment: non-flat landscape, continuous river, both-bank forest, open channel, rain gates passed.")
		: TEXT("ValidateEnvironment: one or more structural/PIE gates failed; inspect structural_metrics.gates.");
	Inspected.CapabilityNotes.Add(TEXT("Screenshot/human review still required for 'looks good' (BACKLOG 5.8)."));
	return Inspected;
}
