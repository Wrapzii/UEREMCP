#include "UeremcpVoxelService.h"

#include "VoxelWorld.h"
#include "VoxelIntBox.h"
#include "VoxelTools/Gen/VoxelBoxTools.h"
#include "VoxelTools/Gen/VoxelLevelTools.h"
#include "VoxelTools/Gen/VoxelSphereTools.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelTools/VoxelPaintMaterial.h"
#include "VoxelEnums.h"

#include "Editor.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"

#if WITH_EDITOR
#include "WaterBodyRiverActor.h"
#include "WaterBodyLakeActor.h"
#include "WaterSplineComponent.h"
#endif

namespace UeremcpVoxelNoise
{
	inline uint64 MixSeed(uint64 Seed)
	{
		Seed += 0x9E3779B97F4A7C15ull;
		Seed = (Seed ^ (Seed >> 30)) * 0xBF58476D1CE4E5B9ull;
		Seed = (Seed ^ (Seed >> 27)) * 0x94D049BB133111EBull;
		return Seed ^ (Seed >> 31);
	}

	inline float ValueNoise2D(uint64 Seed, int32 X, int32 Y)
	{
		const uint64 H = MixSeed(Seed ^ (uint64(uint32(X)) * 0xD1B54A32Dull) ^ (uint64(uint32(Y)) * 0xABC98388ull));
		return float(H & 0xFFFFFFull) / float(0xFFFFFFull);
	}

	inline float Fade(float T) { return T * T * (3.f - 2.f * T); }

	inline float SmoothNoise2D(uint64 Seed, float X, float Y)
	{
		const int32 X0 = FMath::FloorToInt(X);
		const int32 Y0 = FMath::FloorToInt(Y);
		const float Fx = Fade(X - float(X0));
		const float Fy = Fade(Y - float(Y0));
		const float V00 = ValueNoise2D(Seed, X0, Y0);
		const float V10 = ValueNoise2D(Seed, X0 + 1, Y0);
		const float V01 = ValueNoise2D(Seed, X0, Y0 + 1);
		const float V11 = ValueNoise2D(Seed, X0 + 1, Y0 + 1);
		return FMath::Lerp(FMath::Lerp(V00, V10, Fx), FMath::Lerp(V01, V11, Fx), Fy);
	}

	inline float FBm2D(uint64 Seed, float X, float Y, int32 Octaves)
	{
		float Sum = 0.f;
		float Amp = 1.f;
		float Norm = 0.f;
		float Fx = X;
		float Fy = Y;
		for (int32 O = 0; O < Octaves; ++O)
		{
			Sum += SmoothNoise2D(Seed + uint64(O) * 1013ull, Fx, Fy) * Amp;
			Norm += Amp;
			Amp *= 0.5f;
			Fx *= 2.f;
			Fy *= 2.f;
		}
		return Norm > 0.f ? Sum / Norm : 0.f;
	}
}

namespace
{
	constexpr int32 MaxHeightfieldCells = 2500;

	UWorld* EditorWorld()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		return GEditor->GetEditorWorldContext().World();
	}

	float VoxelSizeCm(AVoxelWorld* World)
	{
		return World ? World->VoxelSize : 100.f;
	}

	FVoxelIntBox WorldAabbToVoxelBox(AVoxelWorld* World, const FVector& MinWorld, const FVector& MaxWorld)
	{
		const float Vs = VoxelSizeCm(World);
		const FVector Origin = World->GetActorLocation();
		const FVector MinLocal = MinWorld - Origin;
		const FVector MaxLocal = MaxWorld - Origin;
		FIntVector MinV(
			FMath::FloorToInt(MinLocal.X / Vs) - 1,
			FMath::FloorToInt(MinLocal.Y / Vs) - 1,
			FMath::FloorToInt(MinLocal.Z / Vs) - 1);
		FIntVector MaxV(
			FMath::CeilToInt(MaxLocal.X / Vs) + 1,
			FMath::CeilToInt(MaxLocal.Y / Vs) + 1,
			FMath::CeilToInt(MaxLocal.Z / Vs) + 1);
		if (MaxV.X <= MinV.X) MaxV.X = MinV.X + 1;
		if (MaxV.Y <= MinV.Y) MaxV.Y = MinV.Y + 1;
		if (MaxV.Z <= MinV.Z) MaxV.Z = MinV.Z + 1;
		return FVoxelIntBox(MinV, MaxV);
	}

	void RemoveBoxWorld(AVoxelWorld* World, const FVector& MinW, const FVector& MaxW, bool bUpdateRender)
	{
		const FVoxelIntBox Box = WorldAabbToVoxelBox(World, MinW, MaxW);
		UVoxelBoxTools::RemoveBox(World, Box, nullptr, nullptr, true, bUpdateRender);
	}

	void AddBoxWorld(AVoxelWorld* World, const FVector& MinW, const FVector& MaxW, bool bUpdateRender)
	{
		const FVoxelIntBox Box = WorldAabbToVoxelBox(World, MinW, MaxW);
		UVoxelBoxTools::AddBox(World, Box, nullptr, nullptr, true, bUpdateRender);
	}

	void UpdateRender(AVoxelWorld* World)
	{
		if (World)
		{
			UVoxelBlueprintLibrary::UpdateAll(World);
		}
	}

	TArray<FVector> DensifyPolyline(const TArray<FVector>& Points, float StepCm)
	{
		TArray<FVector> Out;
		if (Points.Num() == 0)
		{
			return Out;
		}
		Out.Add(Points[0]);
		for (int32 I = 1; I < Points.Num(); ++I)
		{
			const FVector A = Points[I - 1];
			const FVector B = Points[I];
			const float Dist = FVector::Distance(A, B);
			if (Dist < 1e-3f)
			{
				continue;
			}
			const int32 N = FMath::Max(1, FMath::CeilToInt(Dist / FMath::Max(1.f, StepCm)));
			for (int32 K = 1; K <= N; ++K)
			{
				Out.Add(FMath::Lerp(A, B, float(K) / float(N)));
			}
		}
		return Out;
	}

	float SpecNumber(const TSharedPtr<FJsonObject>& Spec, const TCHAR* Key, float Default)
	{
		if (!Spec.IsValid() || !Spec->HasField(Key))
		{
			return Default;
		}
		return static_cast<float>(Spec->GetNumberField(Key));
	}

	int32 SpecInt(const TSharedPtr<FJsonObject>& Spec, const TCHAR* Key, int32 Default)
	{
		if (!Spec.IsValid() || !Spec->HasField(Key))
		{
			return Default;
		}
		return static_cast<int32>(Spec->GetNumberField(Key));
	}

	bool SpecBool(const TSharedPtr<FJsonObject>& Spec, const TCHAR* Key, bool Default)
	{
		if (!Spec.IsValid() || !Spec->HasField(Key))
		{
			return Default;
		}
		return Spec->GetBoolField(Key);
	}

	FString SpecString(const TSharedPtr<FJsonObject>& Spec, const TCHAR* Key, const FString& Default)
	{
		if (!Spec.IsValid() || !Spec->HasField(Key))
		{
			return Default;
		}
		return Spec->GetStringField(Key);
	}

	FUeremcpVoxelOpResult OkResult(const FString& Summary, int32 Ops, TSharedPtr<FJsonObject> Extra = nullptr)
	{
		FUeremcpVoxelOpResult R;
		R.bOk = true;
		R.Status = TEXT("modified_and_validated");
		R.Summary = Summary;
		R.InternalOperations = Ops;
		R.Extra = Extra.IsValid() ? Extra : MakeShared<FJsonObject>();
		R.CapabilityNotes.Add(TEXT("Uses VoxelBoxTools / VoxelLevelTools / VoxelSphereTools — not sphere-spam tunnels."));
		return R;
	}

	FUeremcpVoxelOpResult FailResult(const FString& Message, const FString& Code = TEXT("VOXEL_OP_FAILED"))
	{
		FUeremcpVoxelOpResult R;
		R.bOk = false;
		R.Status = TEXT("failed");
		R.Summary = Message;
		R.ErrorCode = Code;
		R.Extra = MakeShared<FJsonObject>();
		return R;
	}

	void DestroyActorsWithLabel(UWorld* World, const FString& Label)
	{
		if (!World || Label.IsEmpty())
		{
			return;
		}
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetActorLabel() == Label)
			{
				It->Destroy();
			}
		}
	}
}

AVoxelWorld* FUeremcpVoxelService::FindVoxelWorld(const FString& ActorLabel, FString& OutError)
{
	UWorld* World = EditorWorld();
	if (!World)
	{
		OutError = TEXT("No editor world");
		return nullptr;
	}
	for (TActorIterator<AVoxelWorld> It(World); It; ++It)
	{
		if (ActorLabel.IsEmpty() || It->GetActorLabel() == ActorLabel)
		{
			return *It;
		}
	}
	OutError = ActorLabel.IsEmpty()
		? FString(TEXT("No VoxelWorld in editor world"))
		: FString::Printf(TEXT("No VoxelWorld with label '%s'"), *ActorLabel);
	return nullptr;
}

bool FUeremcpVoxelService::ParseVector3FromArray(const TArray<TSharedPtr<FJsonValue>>& Arr, FVector& Out, FString& OutError)
{
	if (Arr.Num() < 3)
	{
		OutError = TEXT("vector needs 3 numbers");
		return false;
	}
	Out.X = static_cast<float>(Arr[0]->AsNumber());
	Out.Y = static_cast<float>(Arr[1]->AsNumber());
	Out.Z = static_cast<float>(Arr[2]->AsNumber());
	return true;
}

bool FUeremcpVoxelService::ParseVector3(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, FVector& Out, FString& OutError)
{
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!Obj.IsValid() || !Obj->TryGetArrayField(Key, Arr) || !Arr)
	{
		OutError = FString::Printf(TEXT("missing %s"), Key);
		return false;
	}
	return ParseVector3FromArray(*Arr, Out, OutError);
}

bool FUeremcpVoxelService::ParseVectorArray(const TArray<TSharedPtr<FJsonValue>>* Arr, TArray<FVector>& Out, FString& OutError)
{
	Out.Reset();
	if (!Arr)
	{
		OutError = TEXT("points array missing");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& V : *Arr)
	{
		const TArray<TSharedPtr<FJsonValue>>* Pt = nullptr;
		if (!V.IsValid() || !V->TryGetArray(Pt) || !Pt)
		{
			OutError = TEXT("point is not an array");
			return false;
		}
		FVector P;
		if (!ParseVector3FromArray(*Pt, P, OutError))
		{
			return false;
		}
		Out.Add(P);
	}
	return true;
}

FUeremcpVoxelOpResult FUeremcpVoxelService::CarveSpline(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun)
{
	const TArray<TSharedPtr<FJsonValue>>* PointsJson = nullptr;
	if (!Spec.IsValid() || !Spec->TryGetArrayField(TEXT("points"), PointsJson))
	{
		return FailResult(TEXT("carve_spline requires specification.points"), TEXT("MISSING_POINTS"));
	}
	TArray<FVector> Points;
	FString Err;
	if (!ParseVectorArray(PointsJson, Points, Err) || Points.Num() < 2)
	{
		return FailResult(TEXT("carve_spline needs ≥2 points"), TEXT("BAD_POINTS"));
	}

	const float Radius = FMath::Max(SpecNumber(Spec, TEXT("radius_cm"), 180.f), VoxelSizeCm(World) * 1.6f);
	const float StepFactor = SpecNumber(Spec, TEXT("step_factor"), 0.95f);
	const float FloorBias = SpecNumber(Spec, TEXT("floor_bias_cm"), 0.f);
	const FString Cross = SpecString(Spec, TEXT("cross_section"), TEXT("box"));
	const float Step = FMath::Max(VoxelSizeCm(World), Radius * StepFactor);
	const TArray<FVector> Samples = DensifyPolyline(Points, Step);

	float Hx = Radius, Hy = Radius, Hz = Radius;
	if (Cross.Equals(TEXT("capsule"), ESearchCase::IgnoreCase))
	{
		Hx = Radius * 1.05f;
		Hy = Radius * 1.05f;
		Hz = Radius * 1.25f;
	}

	if (bDryRun)
	{
		FUeremcpVoxelOpResult R = OkResult(
			FString::Printf(TEXT("Dry-run carve_spline: %d box stamps, radius=%.0f"), Samples.Num(), Radius),
			Samples.Num());
		R.Status = TEXT("partially_completed");
		R.Extra->SetNumberField(TEXT("planned_edits"), Samples.Num());
		return R;
	}

	for (const FVector& P : Samples)
	{
		const FVector C(P.X, P.Y, P.Z - FloorBias);
		RemoveBoxWorld(World, C - FVector(Hx, Hy, Hz), C + FVector(Hx, Hy, Hz), false);
	}
	UpdateRender(World);

	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetNumberField(TEXT("edits"), Samples.Num());
	Extra->SetNumberField(TEXT("radius_cm"), Radius);
	Extra->SetStringField(TEXT("cross_section"), Cross);
	return OkResult(
		FString::Printf(TEXT("Carved spline with %d box stamps (radius %.0f cm)."), Samples.Num(), Radius),
		Samples.Num(),
		Extra);
}

FUeremcpVoxelOpResult FUeremcpVoxelService::FlattenArea(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun)
{
	FVector Center;
	FString Err;
	if (!ParseVector3(Spec, TEXT("center_cm"), Center, Err))
	{
		return FailResult(TEXT("flatten_area requires center_cm"), TEXT("MISSING_CENTER"));
	}
	const float Radius = SpecNumber(Spec, TEXT("radius_cm"), 1000.f);
	const float Height = SpecNumber(Spec, TEXT("height_cm"), 200.f);
	const float Falloff = SpecNumber(Spec, TEXT("falloff"), 0.35f);
	const bool bAdditive = SpecBool(Spec, TEXT("additive"), false);

	if (bDryRun)
	{
		FUeremcpVoxelOpResult R = OkResult(TEXT("Dry-run flatten_area"), 1);
		R.Status = TEXT("partially_completed");
		return R;
	}

	UVoxelLevelTools::Level(World, Center, Radius, Falloff, Height, bAdditive, nullptr, nullptr, true, true, false);
	UpdateRender(World);
	return OkResult(
		FString::Printf(TEXT("Flattened pad r=%.0f h=%.0f at (%.0f,%.0f,%.0f)."), Radius, Height, Center.X, Center.Y, Center.Z),
		1);
}

FUeremcpVoxelOpResult FUeremcpVoxelService::SmoothRegion(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun)
{
	FVector Center;
	FString Err;
	if (!ParseVector3(Spec, TEXT("center_cm"), Center, Err))
	{
		return FailResult(TEXT("smooth_region requires center_cm"), TEXT("MISSING_CENTER"));
	}
	const float RegionR = SpecNumber(Spec, TEXT("radius_cm"), 2000.f);
	const float Strength = SpecNumber(Spec, TEXT("strength"), 0.55f);
	const int32 Iters = SpecInt(Spec, TEXT("iterations"), 2);
	const float BrushR = SpecNumber(Spec, TEXT("brush_radius_cm"), FMath::Max(RegionR * 0.35f, VoxelSizeCm(World) * 4.f));
	const float Spacing = BrushR * 0.85f;

	int32 Edits = 0;
	for (float Y = -RegionR; Y <= RegionR + 1e-3f; Y += Spacing)
	{
		for (float X = -RegionR; X <= RegionR + 1e-3f; X += Spacing)
		{
			if (X * X + Y * Y <= RegionR * RegionR)
			{
				++Edits;
			}
		}
	}

	if (bDryRun)
	{
		FUeremcpVoxelOpResult R = OkResult(FString::Printf(TEXT("Dry-run smooth_region: %d brushes"), Edits), Edits);
		R.Status = TEXT("partially_completed");
		return R;
	}

	int32 Applied = 0;
	for (float Y = -RegionR; Y <= RegionR + 1e-3f; Y += Spacing)
	{
		for (float X = -RegionR; X <= RegionR + 1e-3f; X += Spacing)
		{
			if (X * X + Y * Y > RegionR * RegionR)
			{
				continue;
			}
			UVoxelSphereTools::SmoothSphere(
				World,
				Center + FVector(X, Y, 0.f),
				BrushR,
				Strength,
				Iters,
				EVoxelFalloff::Linear,
				0.5f,
				nullptr,
				nullptr,
				true,
				true,
				false);
			++Applied;
		}
	}
	UpdateRender(World);
	return OkResult(FString::Printf(TEXT("Smoothed region with %d brushes."), Applied), Applied);
}

FUeremcpVoxelOpResult FUeremcpVoxelService::TerrainStamp(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun)
{
	FVector Origin;
	FString Err;
	if (!ParseVector3(Spec, TEXT("origin_cm"), Origin, Err))
	{
		Origin = World->GetActorLocation();
	}
	FVector SizeXY(12000.f, 12000.f, 0.f);
	const TArray<TSharedPtr<FJsonValue>>* SizeArr = nullptr;
	if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("size_xy_cm"), SizeArr) && SizeArr && SizeArr->Num() >= 2)
	{
		SizeXY.X = static_cast<float>((*SizeArr)[0]->AsNumber());
		SizeXY.Y = static_cast<float>((*SizeArr)[1]->AsNumber());
	}

	const float Amplitude = SpecNumber(Spec, TEXT("amplitude_cm"), 800.f);
	const int32 Seed = SpecInt(Spec, TEXT("seed"), 1);
	const float Frequency = SpecNumber(Spec, TEXT("frequency"), 0.004f);
	const int32 Octaves = SpecInt(Spec, TEXT("octaves"), 4);
	const FString Mode = SpecString(Spec, TEXT("mode"), TEXT("add"));
	float Cell = SpecNumber(Spec, TEXT("cell_cm"), FMath::Max(VoxelSizeCm(World) * 4.f, FMath::Min(SizeXY.X, SizeXY.Y) / 40.f));

	int32 NX = FMath::Max(1, FMath::CeilToInt(SizeXY.X / Cell));
	int32 NY = FMath::Max(1, FMath::CeilToInt(SizeXY.Y / Cell));
	while (NX * NY > MaxHeightfieldCells)
	{
		Cell *= 1.25f;
		NX = FMath::Max(1, FMath::CeilToInt(SizeXY.X / Cell));
		NY = FMath::Max(1, FMath::CeilToInt(SizeXY.Y / Cell));
	}

	if (bDryRun)
	{
		FUeremcpVoxelOpResult R = OkResult(
			FString::Printf(TEXT("Dry-run terrain_stamp: %d cells"), NX * NY), NX * NY);
		R.Status = TEXT("partially_completed");
		return R;
	}

	const float BaseZ = SpecNumber(Spec, TEXT("base_z_cm"), Origin.Z);
	const float Vs = VoxelSizeCm(World);
	int32 Edits = 0;
	for (int32 J = 0; J < NY; ++J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const float Lx = (float(I) + 0.5f) * Cell - SizeXY.X * 0.5f;
			const float Ly = (float(J) + 0.5f) * Cell - SizeXY.Y * 0.5f;
			const float N = UeremcpVoxelNoise::FBm2D(uint64(Seed), Lx * Frequency, Ly * Frequency, Octaves);
			const float H = FMath::Max(0.f, FMath::Pow(N, 1.35f) * Amplitude);
			if (Mode.Equals(TEXT("add"), ESearchCase::IgnoreCase) && H < Vs * 0.5f)
			{
				continue;
			}
			const float Cx = Origin.X + Lx;
			const float Cy = Origin.Y + Ly;
			const float Half = Cell * 0.5f;
			if (Mode.Equals(TEXT("carve"), ESearchCase::IgnoreCase))
			{
				const float Headroom = FMath::Max(H + Vs * 8.f, Vs * 20.f);
				RemoveBoxWorld(
					World,
					FVector(Cx - Half, Cy - Half, BaseZ + H),
					FVector(Cx + Half, Cy + Half, BaseZ + Headroom),
					false);
			}
			else
			{
				AddBoxWorld(
					World,
					FVector(Cx - Half, Cy - Half, BaseZ),
					FVector(Cx + Half, Cy + Half, BaseZ + H),
					false);
			}
			++Edits;
		}
	}
	UpdateRender(World);
	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetNumberField(TEXT("edits"), Edits);
	Extra->SetNumberField(TEXT("cells"), NX * NY);
	Extra->SetNumberField(TEXT("cell_cm"), Cell);
	return OkResult(FString::Printf(TEXT("Terrain stamp applied (%d column boxes)."), Edits), Edits, Extra);
}

FUeremcpVoxelOpResult FUeremcpVoxelService::NoiseSculpt(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun)
{
	// Reuse TerrainStamp path but with valley groove baked into height via a local stamp.
	FVector Origin;
	FString Err;
	if (!ParseVector3(Spec, TEXT("origin_cm"), Origin, Err))
	{
		Origin = World->GetActorLocation();
	}
	FVector SizeXY(12000.f, 12000.f, 0.f);
	const TArray<TSharedPtr<FJsonValue>>* SizeArr = nullptr;
	if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("size_xy_cm"), SizeArr) && SizeArr && SizeArr->Num() >= 2)
	{
		SizeXY.X = static_cast<float>((*SizeArr)[0]->AsNumber());
		SizeXY.Y = static_cast<float>((*SizeArr)[1]->AsNumber());
	}

	const float Amplitude = SpecNumber(Spec, TEXT("amplitude_cm"), 900.f);
	const int32 Seed = SpecInt(Spec, TEXT("seed"), 1);
	const float Frequency = SpecNumber(Spec, TEXT("frequency"), 0.0035f);
	const int32 Octaves = SpecInt(Spec, TEXT("octaves"), 4);
	const FString ValleyAxis = SpecString(Spec, TEXT("valley_axis"), TEXT("x"));
	const float ValleyWidth = SpecNumber(Spec, TEXT("valley_width_cm"), 2800.f);
	const float ValleyDepth = SpecNumber(Spec, TEXT("valley_depth_cm"), 550.f);
	float Cell = SpecNumber(Spec, TEXT("cell_cm"), FMath::Max(VoxelSizeCm(World) * 4.f, FMath::Min(SizeXY.X, SizeXY.Y) / 40.f));

	int32 NX = FMath::Max(1, FMath::CeilToInt(SizeXY.X / Cell));
	int32 NY = FMath::Max(1, FMath::CeilToInt(SizeXY.Y / Cell));
	while (NX * NY > MaxHeightfieldCells)
	{
		Cell *= 1.25f;
		NX = FMath::Max(1, FMath::CeilToInt(SizeXY.X / Cell));
		NY = FMath::Max(1, FMath::CeilToInt(SizeXY.Y / Cell));
	}

	if (bDryRun)
	{
		FUeremcpVoxelOpResult R = OkResult(
			FString::Printf(TEXT("Dry-run noise_sculpt: %d cells, valley=%s"), NX * NY, *ValleyAxis),
			NX * NY);
		R.Status = TEXT("partially_completed");
		return R;
	}

	const float BaseZ = SpecNumber(Spec, TEXT("base_z_cm"), Origin.Z);
	const float Vs = VoxelSizeCm(World);
	int32 Edits = 0;
	for (int32 J = 0; J < NY; ++J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const float Lx = (float(I) + 0.5f) * Cell - SizeXY.X * 0.5f;
			const float Ly = (float(J) + 0.5f) * Cell - SizeXY.Y * 0.5f;
			float N = UeremcpVoxelNoise::FBm2D(uint64(Seed), Lx * Frequency, Ly * Frequency, Octaves);
			float H = FMath::Pow(N, 1.35f) * Amplitude;
			if (!ValleyAxis.Equals(TEXT("none"), ESearchCase::IgnoreCase) && ValleyWidth > 1.f)
			{
				const float Dist = ValleyAxis.Equals(TEXT("x"), ESearchCase::IgnoreCase) ? FMath::Abs(Ly) : FMath::Abs(Lx);
				const float T = FMath::Max(0.f, 1.f - Dist / (ValleyWidth * 0.5f));
				H -= ValleyDepth * (T * T);
			}
			H = FMath::Max(0.f, H);
			if (H < Vs * 0.5f)
			{
				continue;
			}
			const float Cx = Origin.X + Lx;
			const float Cy = Origin.Y + Ly;
			const float Half = Cell * 0.5f;
			AddBoxWorld(
				World,
				FVector(Cx - Half, Cy - Half, BaseZ),
				FVector(Cx + Half, Cy + Half, BaseZ + H),
				false);
			++Edits;
		}
	}
	UpdateRender(World);
	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetNumberField(TEXT("edits"), Edits);
	Extra->SetStringField(TEXT("valley_axis"), ValleyAxis);
	Extra->SetNumberField(TEXT("seed"), Seed);
	return OkResult(FString::Printf(TEXT("Noise sculpt applied (%d columns, valley=%s)."), Edits, *ValleyAxis), Edits, Extra);
}

FUeremcpVoxelOpResult FUeremcpVoxelService::PaintMaterial(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun)
{
	const FString Shape = SpecString(Spec, TEXT("shape"), TEXT("sphere"));
	const int32 Channel = SpecInt(Spec, TEXT("channel"), 0);
	const float Strength = SpecNumber(Spec, TEXT("strength"), 1.f);

	FVoxelPaintMaterialFiveWayBlend Five;
	Five.Channel = Channel;
	Five.TargetValue = 1.f;
	const FVoxelPaintMaterial Paint = UVoxelBlueprintLibrary::CreateFiveWayBlendPaintMaterial(Five);

	if (bDryRun)
	{
		FUeremcpVoxelOpResult R = OkResult(TEXT("Dry-run paint_material"), 1);
		R.Status = TEXT("partially_completed");
		return R;
	}

	if (Shape.Equals(TEXT("box"), ESearchCase::IgnoreCase))
	{
		FVector MinW, MaxW;
		FString Err;
		if (!ParseVector3(Spec, TEXT("min_cm"), MinW, Err) || !ParseVector3(Spec, TEXT("max_cm"), MaxW, Err))
		{
			return FailResult(TEXT("paint_material box requires min_cm and max_cm"), TEXT("MISSING_BOUNDS"));
		}
		const FVoxelIntBox Box = WorldAabbToVoxelBox(World, MinW, MaxW);
		UVoxelBoxTools::SetMaterialBox(World, Box, Paint, nullptr, nullptr, true, false);
	}
	else
	{
		FVector Center;
		FString Err;
		if (!ParseVector3(Spec, TEXT("center_cm"), Center, Err))
		{
			return FailResult(TEXT("paint_material sphere requires center_cm"), TEXT("MISSING_CENTER"));
		}
		const float Radius = SpecNumber(Spec, TEXT("radius_cm"), 500.f);
		UVoxelSphereTools::SetMaterialSphere(
			World, Center, Radius, Paint, Strength, EVoxelFalloff::Linear, 0.4f, nullptr, nullptr, true, true, false);
	}
	UpdateRender(World);
	return OkResult(FString::Printf(TEXT("Painted material channel %d (%s)."), Channel, *Shape), 1);
}

FUeremcpVoxelOpResult FUeremcpVoxelService::GenerateWaterBody(const TSharedPtr<FJsonObject>& Spec, bool bDryRun)
{
	const TArray<TSharedPtr<FJsonValue>>* PointsJson = nullptr;
	if (!Spec.IsValid() || !Spec->TryGetArrayField(TEXT("points"), PointsJson))
	{
		return FailResult(TEXT("generate_water_body requires points"), TEXT("MISSING_POINTS"));
	}
	TArray<FVector> Points;
	FString Err;
	if (!ParseVectorArray(PointsJson, Points, Err) || Points.Num() < 1)
	{
		return FailResult(TEXT("bad water points"), TEXT("BAD_POINTS"));
	}

	const FString BodyType = SpecString(Spec, TEXT("body_type"), TEXT("river"));
	const FString Label = SpecString(Spec, TEXT("label"), TEXT("RE_VoxelRiver"));
	if (Spec->HasField(TEXT("water_z_cm")))
	{
		const float Z = SpecNumber(Spec, TEXT("water_z_cm"), Points[0].Z);
		for (FVector& P : Points)
		{
			P.Z = Z;
		}
	}

	if (bDryRun)
	{
		FUeremcpVoxelOpResult R = OkResult(
			FString::Printf(TEXT("Dry-run generate_water_body (%s, %d pts)"), *BodyType, Points.Num()), 1);
		R.Status = TEXT("partially_completed");
		return R;
	}

	UWorld* World = EditorWorld();
	if (!World)
	{
		return FailResult(TEXT("No editor world"), TEXT("NO_WORLD"));
	}
	DestroyActorsWithLabel(World, Label);

#if WITH_EDITOR
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* WaterActor = nullptr;
	if (BodyType.Equals(TEXT("lake"), ESearchCase::IgnoreCase))
	{
		WaterActor = World->SpawnActor<AWaterBodyLake>(AWaterBodyLake::StaticClass(), Points[0], FRotator::ZeroRotator, SpawnParams);
	}
	else
	{
		AWaterBodyRiver* River = World->SpawnActor<AWaterBodyRiver>(
			AWaterBodyRiver::StaticClass(), Points[0], FRotator::ZeroRotator, SpawnParams);
		if (River)
		{
			if (UWaterSplineComponent* Spline = River->GetWaterSpline())
			{
				Spline->ClearSplinePoints(false);
				for (const FVector& P : Points)
				{
					Spline->AddSplinePoint(P, ESplineCoordinateSpace::World, false);
				}
				Spline->UpdateSpline();
			}
			WaterActor = River;
		}
	}
	if (WaterActor)
	{
		WaterActor->SetActorLabel(Label);
		TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
		Extra->SetStringField(TEXT("label"), Label);
		Extra->SetStringField(TEXT("body_type"), BodyType);
		Extra->SetNumberField(TEXT("points"), Points.Num());
		return OkResult(FString::Printf(TEXT("Spawned %s '%s'."), *BodyType, *Label), 1, Extra);
	}
#endif

	// Fallback plane
	AStaticMeshActor* Plane = World->SpawnActor<AStaticMeshActor>(Points[0], FRotator::ZeroRotator);
	if (!Plane)
	{
		return FailResult(TEXT("Water spawn failed"), TEXT("WATER_SPAWN_FAILED"));
	}
	Plane->SetActorLabel(Label);
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Mesh && Plane->GetStaticMeshComponent())
	{
		Plane->GetStaticMeshComponent()->SetStaticMesh(Mesh);
	}
	FUeremcpVoxelOpResult R = OkResult(TEXT("Spawned plane fallback water (WaterBody unavailable)."), 1);
	R.Warnings.Add(TEXT("Approximated water as StaticMesh Plane"));
	R.Extra->SetBoolField(TEXT("approximated"), true);
	return R;
}

FUeremcpVoxelOpResult FUeremcpVoxelService::ProceduralScatter(const TSharedPtr<FJsonObject>& Spec, bool bDryRun)
{
	FVector Origin(0.f), SizeXY(8000.f, 8000.f, 0.f);
	FString Err;
	ParseVector3(Spec, TEXT("origin_cm"), Origin, Err);
	const TArray<TSharedPtr<FJsonValue>>* SizeArr = nullptr;
	if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("size_xy_cm"), SizeArr) && SizeArr && SizeArr->Num() >= 2)
	{
		SizeXY.X = static_cast<float>((*SizeArr)[0]->AsNumber());
		SizeXY.Y = static_cast<float>((*SizeArr)[1]->AsNumber());
	}
	const int32 Count = SpecInt(Spec, TEXT("count"), 40);
	const int32 Seed = SpecInt(Spec, TEXT("seed"), 1);
	const FString Prefix = SpecString(Spec, TEXT("label_prefix"), TEXT("RE_VoxelScatter"));
	const bool bSnap = SpecBool(Spec, TEXT("snap_trace"), true);

	TArray<FString> MeshPaths;
	const TArray<TSharedPtr<FJsonValue>>* MeshesJson = nullptr;
	if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("mesh_paths"), MeshesJson) && MeshesJson)
	{
		for (const TSharedPtr<FJsonValue>& V : *MeshesJson)
		{
			MeshPaths.Add(V->AsString());
		}
	}
	if (MeshPaths.Num() == 0)
	{
		return FailResult(TEXT("procedural_scatter requires mesh_paths"), TEXT("MISSING_MESHES"));
	}

	if (bDryRun)
	{
		FUeremcpVoxelOpResult R = OkResult(FString::Printf(TEXT("Dry-run procedural_scatter x%d"), Count), Count);
		R.Status = TEXT("partially_completed");
		return R;
	}

	UWorld* World = EditorWorld();
	if (!World)
	{
		return FailResult(TEXT("No editor world"), TEXT("NO_WORLD"));
	}

	TArray<UStaticMesh*> Meshes;
	for (const FString& Path : MeshPaths)
	{
		if (UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *Path))
		{
			Meshes.Add(M);
		}
	}
	if (Meshes.Num() == 0)
	{
		return FailResult(TEXT("No meshes loaded"), TEXT("MESH_LOAD_FAILED"));
	}

	FRandomStream Rng(Seed);
	int32 Placed = 0;
	TArray<TSharedPtr<FJsonValue>> Labels;
	for (int32 I = 0; I < Count; ++I)
	{
		const float X = Origin.X + Rng.FRandRange(-SizeXY.X * 0.5f, SizeXY.X * 0.5f);
		const float Y = Origin.Y + Rng.FRandRange(-SizeXY.Y * 0.5f, SizeXY.Y * 0.5f);
		float Z = Origin.Z;
		if (bSnap)
		{
			FHitResult Hit;
			const FVector Start(X, Y, Origin.Z + 5000.f);
			const FVector End(X, Y, Origin.Z - 5000.f);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(UeremcpVoxelScatter), true);
			if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
			{
				Z = Hit.ImpactPoint.Z;
			}
		}
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(FVector(X, Y, Z), FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f));
		if (!Actor)
		{
			continue;
		}
		const FString Label = FString::Printf(TEXT("%s_%03d"), *Prefix, I);
		Actor->SetActorLabel(Label);
		if (UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent())
		{
			Comp->SetStaticMesh(Meshes[I % Meshes.Num()]);
			const float S = Rng.FRandRange(0.8f, 1.3f);
			Actor->SetActorScale3D(FVector(S));
		}
		++Placed;
		if (Labels.Num() < 20)
		{
			Labels.Add(MakeShared<FJsonValueString>(Label));
		}
	}
	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetNumberField(TEXT("placed"), Placed);
	Extra->SetArrayField(TEXT("labels"), Labels);
	return OkResult(FString::Printf(TEXT("Scattered %d meshes."), Placed), Placed, Extra);
}

FUeremcpVoxelOpResult FUeremcpVoxelService::GeneratePois(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun)
{
	const TArray<TSharedPtr<FJsonValue>>* PoisJson = nullptr;
	if (!Spec.IsValid() || !Spec->TryGetArrayField(TEXT("pois"), PoisJson) || !PoisJson)
	{
		return FailResult(TEXT("generate_pois requires pois array"), TEXT("MISSING_POIS"));
	}

	if (bDryRun)
	{
		FUeremcpVoxelOpResult R = OkResult(
			FString::Printf(TEXT("Dry-run generate_pois x%d"), PoisJson->Num()), PoisJson->Num());
		R.Status = TEXT("partially_completed");
		return R;
	}

	UWorld* Editor = EditorWorld();
	int32 Ops = 0;
	TArray<TSharedPtr<FJsonValue>> OutPois;
	for (int32 I = 0; I < PoisJson->Num(); ++I)
	{
		const TSharedPtr<FJsonObject> Poi = (*PoisJson)[I]->AsObject();
		if (!Poi.IsValid())
		{
			continue;
		}
		const FString Kind = SpecString(Poi, TEXT("kind"), TEXT("marker"));
		FVector Center;
		FString Err;
		if (!ParseVector3(Poi, TEXT("center_cm"), Center, Err))
		{
			continue;
		}
		const FString Label = SpecString(Poi, TEXT("label"), FString::Printf(TEXT("RE_POI_%s_%02d"), *Kind, I));
		const float Radius = SpecNumber(Poi, TEXT("radius_cm"), 600.f);
		const float Height = SpecNumber(Poi, TEXT("height_cm"), 200.f);

		if (World && (Kind.Equals(TEXT("flatten_pad")) || Kind.Equals(TEXT("building")) || Kind.Equals(TEXT("village_pad"))))
		{
			UVoxelLevelTools::Level(
				World, Center, Radius, 0.35f, Height, SpecBool(Poi, TEXT("additive"), false),
				nullptr, nullptr, true, true, false);
			++Ops;
		}

		if (Editor && (Kind.Equals(TEXT("building")) || Kind.Equals(TEXT("marker")) || Kind.Equals(TEXT("village_pad"))))
		{
			const FString MeshPath = SpecString(Poi, TEXT("mesh_path"), TEXT("/Engine/BasicShapes/Cube.Cube"));
			AStaticMeshActor* Actor = Editor->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator);
			if (Actor)
			{
				Actor->SetActorLabel(Label);
				if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath))
				{
					if (UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent())
					{
						Comp->SetStaticMesh(Mesh);
					}
					const float Scale = SpecNumber(Poi, TEXT("scale"), 1.f);
					Actor->SetActorScale3D(FVector(Scale));
				}
				++Ops;
			}
		}

		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("kind"), Kind);
		Row->SetStringField(TEXT("label"), Label);
		OutPois.Add(MakeShared<FJsonValueObject>(Row));
	}
	if (World)
	{
		UpdateRender(World);
	}
	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	Extra->SetArrayField(TEXT("pois"), OutPois);
	Extra->SetNumberField(TEXT("count"), OutPois.Num());
	return OkResult(FString::Printf(TEXT("Generated %d POIs."), OutPois.Num()), Ops, Extra);
}

FUeremcpVoxelOpResult FUeremcpVoxelService::ComposeInteriorTerrain(AVoxelWorld* World, const TSharedPtr<FJsonObject>& Spec, bool bDryRun)
{
	FVector Origin;
	FString Err;
	if (!ParseVector3(Spec, TEXT("origin_cm"), Origin, Err))
	{
		Origin = World->GetActorLocation();
		Origin.Z += 200.f;
	}

	TSharedPtr<FJsonObject> NoiseSpec = MakeShared<FJsonObject>();
	if (Spec.IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Spec->Values)
		{
			NoiseSpec->SetField(Pair.Key, Pair.Value);
		}
	}
	NoiseSpec->SetArrayField(TEXT("origin_cm"), TArray<TSharedPtr<FJsonValue>>{
		MakeShared<FJsonValueNumber>(Origin.X),
		MakeShared<FJsonValueNumber>(Origin.Y),
		MakeShared<FJsonValueNumber>(Origin.Z)});

	TArray<FString> Stages;
	int32 Ops = 0;
	TArray<FString> Warnings;

	auto Run = [&](const FString& Name, const FUeremcpVoxelOpResult& R)
	{
		Stages.Add(Name);
		Ops += R.InternalOperations;
		Warnings.Append(R.Warnings);
		return R.bOk;
	};

	if (!Run(TEXT("noise_sculpt"), NoiseSculpt(World, NoiseSpec, bDryRun)))
	{
		return FailResult(TEXT("compose failed at noise_sculpt"));
	}

	// River
	TArray<FVector> RiverPts;
	const TArray<TSharedPtr<FJsonValue>>* RiverJson = nullptr;
	if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("river_points_cm"), RiverJson))
	{
		ParseVectorArray(RiverJson, RiverPts, Err);
	}
	else if (SpecBool(Spec, TEXT("auto_river"), true))
	{
		FVector SizeXY(12000.f, 12000.f, 0.f);
		const TArray<TSharedPtr<FJsonValue>>* SizeArr = nullptr;
		if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("size_xy_cm"), SizeArr) && SizeArr && SizeArr->Num() >= 2)
		{
			SizeXY.X = static_cast<float>((*SizeArr)[0]->AsNumber());
			SizeXY.Y = static_cast<float>((*SizeArr)[1]->AsNumber());
		}
		const FString Axis = SpecString(Spec, TEXT("valley_axis"), TEXT("x"));
		const float Half = (Axis.Equals(TEXT("x")) ? SizeXY.X : SizeXY.Y) * 0.45f;
		const float Z = Origin.Z + SpecNumber(Spec, TEXT("river_z_cm"), 80.f);
		if (Axis.Equals(TEXT("x")))
		{
			RiverPts = { FVector(Origin.X - Half, Origin.Y, Z), Origin + FVector(0, 0, Z - Origin.Z), FVector(Origin.X + Half, Origin.Y, Z) };
			RiverPts[1].Z = Z;
		}
		else
		{
			RiverPts = { FVector(Origin.X, Origin.Y - Half, Z), FVector(Origin.X, Origin.Y, Z), FVector(Origin.X, Origin.Y + Half, Z) };
		}
	}

	if (RiverPts.Num() >= 2)
	{
		TSharedPtr<FJsonObject> CarveSpec = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> Pts;
		for (const FVector& P : RiverPts)
		{
			TArray<TSharedPtr<FJsonValue>> V;
			V.Add(MakeShared<FJsonValueNumber>(P.X));
			V.Add(MakeShared<FJsonValueNumber>(P.Y));
			V.Add(MakeShared<FJsonValueNumber>(P.Z));
			Pts.Add(MakeShared<FJsonValueArray>(V));
		}
		CarveSpec->SetArrayField(TEXT("points"), Pts);
		const float RiverR = SpecNumber(Spec, TEXT("river_radius_cm"), 350.f);
		CarveSpec->SetNumberField(TEXT("radius_cm"), RiverR);
		CarveSpec->SetNumberField(TEXT("floor_bias_cm"), SpecNumber(Spec, TEXT("river_depth_cm"), 220.f) * 0.35f);
		Run(TEXT("carve_spline_river"), CarveSpline(World, CarveSpec, bDryRun));

		TSharedPtr<FJsonObject> WaterSpec = MakeShared<FJsonObject>();
		WaterSpec->SetArrayField(TEXT("points"), Pts);
		WaterSpec->SetStringField(TEXT("body_type"), TEXT("river"));
		WaterSpec->SetStringField(TEXT("label"), SpecString(Spec, TEXT("water_label"), TEXT("RE_VoxelRiver")));
		WaterSpec->SetNumberField(TEXT("water_z_cm"), RiverPts[0].Z + SpecNumber(Spec, TEXT("water_z_offset_cm"), -40.f));
		Run(TEXT("generate_water_body"), GenerateWaterBody(WaterSpec, bDryRun));
	}

	// Village
	FVector VillageCenter;
	bool bHaveVillage = ParseVector3(Spec, TEXT("village_center_cm"), VillageCenter, Err);
	if (!bHaveVillage && SpecBool(Spec, TEXT("auto_village"), true))
	{
		VillageCenter = FVector(
			Origin.X,
			Origin.Y + SpecNumber(Spec, TEXT("village_bank_offset_cm"), 1800.f),
			Origin.Z + SpecNumber(Spec, TEXT("amplitude_cm"), 900.f) * 0.15f);
		bHaveVillage = true;
	}
	if (bHaveVillage)
	{
		TSharedPtr<FJsonObject> PoiSpec = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> Poi = MakeShared<FJsonObject>();
		Poi->SetStringField(TEXT("kind"), TEXT("village_pad"));
		Poi->SetArrayField(TEXT("center_cm"), TArray<TSharedPtr<FJsonValue>>{
			MakeShared<FJsonValueNumber>(VillageCenter.X),
			MakeShared<FJsonValueNumber>(VillageCenter.Y),
			MakeShared<FJsonValueNumber>(VillageCenter.Z)});
		Poi->SetNumberField(TEXT("radius_cm"), SpecNumber(Spec, TEXT("village_radius_cm"), 1200.f));
		Poi->SetNumberField(TEXT("height_cm"), SpecNumber(Spec, TEXT("village_flatten_height_cm"), 250.f));
		Poi->SetStringField(TEXT("label"), SpecString(Spec, TEXT("village_label"), TEXT("RE_VillagePad")));
		TArray<TSharedPtr<FJsonValue>> Arr;
		Arr.Add(MakeShared<FJsonValueObject>(Poi));
		PoiSpec->SetArrayField(TEXT("pois"), Arr);
		Run(TEXT("generate_pois"), GeneratePois(World, PoiSpec, bDryRun));
	}

	// Scatter
	const TArray<TSharedPtr<FJsonValue>>* MeshesJson = nullptr;
	if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("scatter_meshes"), MeshesJson) && MeshesJson && MeshesJson->Num() > 0)
	{
		TSharedPtr<FJsonObject> ScatterSpec = MakeShared<FJsonObject>();
		ScatterSpec->SetArrayField(TEXT("origin_cm"), TArray<TSharedPtr<FJsonValue>>{
			MakeShared<FJsonValueNumber>(Origin.X),
			MakeShared<FJsonValueNumber>(Origin.Y),
			MakeShared<FJsonValueNumber>(Origin.Z)});
		if (Spec->HasField(TEXT("size_xy_cm")))
		{
			ScatterSpec->SetArrayField(TEXT("size_xy_cm"), Spec->GetArrayField(TEXT("size_xy_cm")));
		}
		ScatterSpec->SetArrayField(TEXT("mesh_paths"), *MeshesJson);
		ScatterSpec->SetNumberField(TEXT("count"), SpecNumber(Spec, TEXT("scatter_count"), 48.f));
		ScatterSpec->SetNumberField(TEXT("seed"), SpecInt(Spec, TEXT("seed"), 1) + 17);
		Run(TEXT("procedural_scatter"), ProceduralScatter(ScatterSpec, bDryRun));
	}

	if (SpecBool(Spec, TEXT("smooth"), true))
	{
		TSharedPtr<FJsonObject> SmoothSpec = MakeShared<FJsonObject>();
		SmoothSpec->SetArrayField(TEXT("center_cm"), TArray<TSharedPtr<FJsonValue>>{
			MakeShared<FJsonValueNumber>(Origin.X),
			MakeShared<FJsonValueNumber>(Origin.Y),
			MakeShared<FJsonValueNumber>(Origin.Z)});
		FVector SizeXY(12000.f, 12000.f, 0.f);
		const TArray<TSharedPtr<FJsonValue>>* SizeArr = nullptr;
		if (Spec.IsValid() && Spec->TryGetArrayField(TEXT("size_xy_cm"), SizeArr) && SizeArr && SizeArr->Num() >= 2)
		{
			SizeXY.X = static_cast<float>((*SizeArr)[0]->AsNumber());
			SizeXY.Y = static_cast<float>((*SizeArr)[1]->AsNumber());
		}
		SmoothSpec->SetNumberField(TEXT("radius_cm"), FMath::Max(SizeXY.X, SizeXY.Y) * 0.35f);
		SmoothSpec->SetNumberField(TEXT("strength"), SpecNumber(Spec, TEXT("smooth_strength"), 0.45f));
		Run(TEXT("smooth_region"), SmoothRegion(World, SmoothSpec, bDryRun));
	}

	TSharedPtr<FJsonObject> Extra = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> StageVals;
	for (const FString& S : Stages)
	{
		StageVals.Add(MakeShared<FJsonValueString>(S));
	}
	Extra->SetArrayField(TEXT("stages"), StageVals);
	FUeremcpVoxelOpResult R = OkResult(
		FString::Printf(TEXT("Composed interior terrain (%d stages, %d ops)."), Stages.Num(), Ops),
		Ops,
		Extra);
	R.Warnings = Warnings;
	if (bDryRun)
	{
		R.Status = TEXT("partially_completed");
	}
	return R;
}
