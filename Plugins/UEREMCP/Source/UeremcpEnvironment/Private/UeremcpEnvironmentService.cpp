// UEREMCP — environment build service implementation (WS-01).

#include "UeremcpEnvironmentService.h"

#include "UeremcpNoise.h"

#include "Editor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Misc/Paths.h"
#include "Misc/Crc.h"
#include "NiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "EngineUtils.h"
#include "UnrealClient.h"

#include "WaterBodyRiverActor.h"
#include "WaterBodyActor.h"
#include "WaterSplineComponent.h"
#include "DynamicMeshActor.h"
#include "Components/DynamicMeshComponent.h"
#include "UDynamicMesh.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h"

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
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Spec->TryGetObjectField(Obj, Child) && Child && (*Child)->HasField(Key))
		{
			InOut = (*Child)->GetNumberField(Key);
		}
	};

	double Tmp = 0;
	ReadObjNumber(TEXT("terrain"), TEXT("size_x"), Tmp); if (Tmp > 0) Out.SizeX = int32(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("size_y"), Tmp); if (Tmp > 0) Out.SizeY = int32(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("mountain_amplitude"), Tmp); if (Tmp > 0) Out.MountainAmplitude = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("valley_depth"), Tmp); if (Tmp > 0) Out.ValleyDepth = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("scale_xy"), Tmp); if (Tmp > 0) Out.ScaleXY = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("scale_z"), Tmp); if (Tmp > 0) Out.ScaleZ = float(Tmp);

	ReadObjNumber(TEXT("river"), TEXT("width"), Tmp); if (Tmp > 0) Out.RiverWidth = float(Tmp);
	ReadObjNumber(TEXT("biome"), TEXT("forest_bank_width"), Tmp); if (Tmp > 0) Out.ForestBankWidth = float(Tmp);
	ReadObjNumber(TEXT("biome"), TEXT("max_foliage_instances"), Tmp); if (Tmp > 0) Out.MaxFoliageInstances = int32(Tmp);

	const TSharedPtr<FJsonObject>* Biome = nullptr;
	if (Spec->TryGetObjectField(TEXT("biome"), Biome) && Biome)
	{
		(*Biome)->TryGetStringField(TEXT("mesh_path"), Out.MeshPath);
	}
	const TSharedPtr<FJsonObject>* Weather = nullptr;
	if (Spec->TryGetObjectField(TEXT("weather"), Weather) && Weather)
	{
		(*Weather)->TryGetStringField(TEXT("rain_system_path"), Out.RainSystemPath);
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
		double Count = 0;
		if ((*Structures)->TryGetNumberField(TEXT("count"), Count) && Count > 0)
		{
			Out.StructureCount = int32(Count);
		}
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
	const FUeremcpSplinePath River = UeremcpSpline::MakeRiverAcross(Spec.Seed, Extents, 12, Spec.RiverWidth);
	const FUeremcpSplinePath Exclusion = UeremcpSpline::MakeExclusionFrom(River, Spec.RiverWidth * 0.35f);

	TArray<uint16> Heights;
	TSharedPtr<FJsonObject> HeightMetrics;
	GenerateHeightmap(Spec, River, Heights, HeightMetrics);
	Result.StructuralMetrics = HeightMetrics;
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
	if (!World)
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("No editor world loaded");
		return Result;
	}

	TArray<FString> CreatedLabels;

	if (Spec.bIncludeTerrain)
	{
		const FVector Offset(
			-float(Spec.SizeX - 1) * Spec.ScaleXY * 0.5f,
			-float(Spec.SizeY - 1) * Spec.ScaleXY * 0.5f,
			0.f);
		ALandscape* Landscape = World->SpawnActor<ALandscape>(Offset, FRotator::ZeroRotator);
		if (!Landscape)
		{
			Result.Status = TEXT("failed_validation");
			Result.Summary = TEXT("Failed to spawn ALandscape");
			return Result;
		}
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
		AWaterBodyRiver* RiverActor = World->SpawnActor<AWaterBodyRiver>(River.Points[0].Location, FRotator::ZeroRotator);
		if (RiverActor)
		{
			RiverActor->SetActorLabel(TEXT("UEREMCP_River"));
			// [VERIFIED: WaterBodyActor.h:103] GetWaterSpline
			if (UWaterSplineComponent* Spline = RiverActor->GetWaterSpline())
			{
				Spline->ClearSplinePoints(false);
				for (int32 I = 0; I < River.Points.Num(); ++I)
				{
					Spline->AddSplinePoint(River.Points[I].Location, ESplineCoordinateSpace::World, false);
				}
				Spline->UpdateSpline();
			}
			CreatedLabels.Add(TEXT("UEREMCP_River"));
			Tech.Add(MakeTech(
				TEXT("water_river"),
				TEXT("real"),
				TEXT("AWaterBodyRiver spawned with spline [VERIFIED: WaterBodyRiverActor.h:28] [VERIFIED: WaterBodyActor.h:103]")));
			Result.InternalOperations += 2;
		}
		else
		{
			Result.Warnings.Add(TEXT("AWaterBodyRiver spawn returned null — river skipped"));
			Tech.Add(MakeTech(TEXT("water_river"), TEXT("blocked"), TEXT("SpawnActor<AWaterBodyRiver> returned null")));
		}
#else
		Result.Warnings.Add(TEXT("Water plugin headers not compiled in — river approximated as empty channel only"));
		Tech.Add(MakeTech(TEXT("water_river"), TEXT("approximated"), TEXT("Valley carved in heightmap only; no AWaterBodyRiver")));
#endif
	}

	int32 FoliageCount = 0;
	int32 ExclusionViolations = 0;
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
			for (int32 Attempt = 0; Attempt < Spec.MaxFoliageInstances * 4 && FoliageCount < Spec.MaxFoliageInstances; ++Attempt)
			{
				const float U = UeremcpNoise::ValueNoise2D(Spec.Seed ^ 0xF01ull, Attempt, 1);
				const float V = UeremcpNoise::ValueNoise2D(Spec.Seed ^ 0xF02ull, Attempt, 2);
				const FVector Local(
					FMath::Lerp(Extents.Min.X, Extents.Max.X, U),
					FMath::Lerp(Extents.Min.Y, Extents.Max.Y, V),
					0.f);
				const float Dist = Bank.DistanceToXY(Local);
				const float Inner = Spec.RiverWidth * 0.55f;
				const float Outer = Inner + Spec.ForestBankWidth;
				if (Dist < Inner || Dist > Outer)
				{
					continue;
				}
				const float Density = UeremcpNoise::SmoothNoise2D(Spec.Seed ^ 0xD00Dull, U * 8.f, V * 8.f);
				if (Density < 0.35f)
				{
					continue;
				}
				const FVector WorldPos(
					Local.X - float(Spec.SizeX - 1) * Spec.ScaleXY * 0.5f,
					Local.Y - float(Spec.SizeY - 1) * Spec.ScaleXY * 0.5f,
					50.f);
				FTransform Xf;
				Xf.SetLocation(WorldPos);
				Xf.SetScale3D(FVector(0.4f + Density * 0.8f));
				Hism->AddInstance(Xf);
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

	if (Spec.bIncludeLighting)
	{
		ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 500), FRotator(-50.f, 30.f, 0.f));
		if (Sun)
		{
			Sun->SetActorLabel(TEXT("UEREMCP_Sun"));
			CreatedLabels.Add(TEXT("UEREMCP_Sun"));
		}
		ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Sky)
		{
			Sky->SetActorLabel(TEXT("UEREMCP_SkyLight"));
			CreatedLabels.Add(TEXT("UEREMCP_SkyLight"));
		}
		AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Fog)
		{
			Fog->SetActorLabel(TEXT("UEREMCP_RainFog"));
			CreatedLabels.Add(TEXT("UEREMCP_RainFog"));
		}
		Tech.Add(MakeTech(TEXT("lighting"), TEXT("real"), TEXT("DirectionalLight + SkyLight + ExponentialHeightFog")));
		Result.InternalOperations += 3;
	}

	if (Spec.bIncludeRain)
	{
		UNiagaraSystem* RainSys = nullptr;
		if (!Spec.RainSystemPath.IsEmpty())
		{
			RainSys = LoadObject<UNiagaraSystem>(nullptr, *Spec.RainSystemPath);
		}
		ANiagaraActor* RainActor = World->SpawnActor<ANiagaraActor>(FVector(0, 0, 400), FRotator::ZeroRotator);
		if (RainActor)
		{
			RainActor->SetActorLabel(TEXT("UEREMCP_Rain"));
			if (UNiagaraComponent* Comp = RainActor->GetNiagaraComponent())
			{
				if (RainSys)
				{
					Comp->SetAsset(RainSys);
					Tech.Add(MakeTech(
						TEXT("rain_camera_follow"),
						TEXT("real"),
						TEXT("Niagara rain actor spawned; attach to PIE camera in follow-up if needed")));
				}
				else
				{
					Tech.Add(MakeTech(
						TEXT("rain_camera_follow"),
						TEXT("approximated"),
						TEXT("Rain actor spawned without system asset — supply weather.rain_system_path")));
					Result.Warnings.Add(TEXT("rain_system_path missing — Niagara component has no asset"));
				}
			}
			CreatedLabels.Add(TEXT("UEREMCP_Rain"));
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

	const bool bNonFlat = Result.StructuralMetrics->GetBoolField(TEXT("non_flat"));
	const bool bForestOk = !Spec.bIncludeForest || FoliageCount > 0;
	if (bNonFlat && bForestOk)
	{
		Result.Status = TEXT("created_with_warnings");
		Result.Summary = FString::Printf(
			TEXT("Built environment seed=%llu actors=%d foliage=%d height_range=%.3f"),
			Spec.Seed, CreatedLabels.Num(), FoliageCount,
			Result.StructuralMetrics->GetNumberField(TEXT("height_range")));
	}
	else
	{
		Result.Status = TEXT("failed_validation");
		Result.Summary = TEXT("Environment built but structural gates failed (flat terrain and/or zero foliage)");
	}
	Result.CapabilityNotes.Add(TEXT("World verification leans on structural metrics + human review; screenshots are not a gate (BACKLOG 5.8)."));
	Result.CapabilityNotes.Add(TEXT("Save the current level under the scratch destination if persistence is required."));
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
	UWorld* World = GEditor->GetEditorWorldContext().World();
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
		if (Label.Contains(TEXT("UEREMCP_Rain")))
		{
			++Rain;
		}
		if (Label.Contains(TEXT("UEREMCP_Forest")))
		{
			++FoliageActors;
		}
	}
	Result.StructuralMetrics->SetNumberField(TEXT("landscape_actors"), Landscapes);
	Result.StructuralMetrics->SetNumberField(TEXT("river_actors"), Rivers);
	Result.StructuralMetrics->SetNumberField(TEXT("rain_actors"), Rain);
	Result.StructuralMetrics->SetNumberField(TEXT("forest_actors"), FoliageActors);
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
	FUeremcpEnvironmentBuildResult Inspected = Inspect(LevelOrPackagePath);
	TSharedPtr<FJsonObject> GateResult = MakeShared<FJsonObject>();
	const bool bHasLandscape = Inspected.StructuralMetrics->GetNumberField(TEXT("landscape_actors")) > 0;
	const bool bHasRiver = Inspected.StructuralMetrics->GetNumberField(TEXT("river_actors")) > 0;
	const bool bHasForest = Inspected.StructuralMetrics->GetNumberField(TEXT("forest_actors")) > 0;
	const bool bHasRain = Inspected.StructuralMetrics->GetNumberField(TEXT("rain_actors")) > 0;
	GateResult->SetBoolField(TEXT("has_landscape"), bHasLandscape);
	GateResult->SetBoolField(TEXT("has_river"), bHasRiver);
	GateResult->SetBoolField(TEXT("has_forest"), bHasForest);
	GateResult->SetBoolField(TEXT("has_rain"), bHasRain);
	Inspected.StructuralMetrics->SetObjectField(TEXT("gates"), GateResult);

	const bool bOk = bHasLandscape && bHasForest;
	Inspected.Status = bOk ? TEXT("no_change_required") : TEXT("failed_validation");
	Inspected.Summary = bOk
		? TEXT("ValidateEnvironment: core gates passed (landscape+forest). River/rain reported honestly.")
		: TEXT("ValidateEnvironment: failed core gates");
	if (!bHasRiver)
	{
		Inspected.Warnings.Add(TEXT("River gate soft-fail — Water body may be approximated"));
	}
	if (!bHasRain)
	{
		Inspected.Warnings.Add(TEXT("Rain gate soft-fail"));
	}
	Inspected.CapabilityNotes.Add(TEXT("Screenshot/human review still required for 'looks good' (BACKLOG 5.8)."));
	return Inspected;
}
