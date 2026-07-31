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
#include "UeremcpNiagaraToolset.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define UEREMCP_HAS_WATER 1

namespace
{
	bool AllowApproximateFallback(const FUeremcpEnvironmentBuildSpec& Spec)
	{
		return Spec.FallbackPolicy.Equals(TEXT("allow_approximate"), ESearchCase::IgnoreCase);
	}

	FString DefaultNiagaraPathForDestination(const FString& Dest, const FString& Phenomenon)
	{
		FString Folder = Dest;
		while (Folder.EndsWith(TEXT("/")))
		{
			Folder.LeftChopInline(1);
		}
		const FString Key = Phenomenon.ToLower();
		if (Key == TEXT("snow"))
		{
			return Folder / TEXT("NS_EnvSnow");
		}
		if (Key == TEXT("hail"))
		{
			return Folder / TEXT("NS_EnvHail");
		}
		if (Key == TEXT("fog"))
		{
			return Folder / TEXT("NS_EnvFog");
		}
		return Folder / TEXT("NS_EnvRain");
	}

	TSharedPtr<FJsonObject> MakeElementCoreMaterialCreateSpec(const FString& Element)
	{
		TSharedPtr<FJsonObject> CreateSpec = MakeShared<FJsonObject>();
		CreateSpec->SetStringField(TEXT("purpose"), TEXT("elemental_projectile_core"));
		CreateSpec->SetStringField(TEXT("element"), Element);
		TArray<TSharedPtr<FJsonValue>> Features;
		Features.Add(MakeShared<FJsonValueString>(TEXT("radial_falloff")));
		Features.Add(MakeShared<FJsonValueString>(TEXT("animated_noise")));
		Features.Add(MakeShared<FJsonValueString>(TEXT("fresnel")));
		Features.Add(MakeShared<FJsonValueString>(TEXT("dynamic_color")));
		Features.Add(MakeShared<FJsonValueString>(TEXT("dynamic_intensity")));
		CreateSpec->SetArrayField(TEXT("features"), Features);
		return CreateSpec;
	}

	bool EnsurePrecipitationNiagaraSystem(
		const FUeremcpWeatherPhenomenonSpec& PhenomenonSpec,
		const FUeremcpEnvironmentBuildSpec& EnvSpec,
		const FString& Dest,
		const FString& RequestId,
		bool bDryRun,
		FString& OutSystemPath,
		FString& OutCreateStatus,
		FString& OutError,
		int32& InOutOps,
		TArray<FString>& OutNotes)
	{
		OutError.Reset();
		OutCreateStatus.Reset();
		const FString Key = PhenomenonSpec.Phenomenon.ToLower();
		OutSystemPath = PhenomenonSpec.AssetPathOverride;
		if (OutSystemPath.IsEmpty())
		{
			OutSystemPath = DefaultNiagaraPathForDestination(Dest, Key);
		}

		if (!OutSystemPath.StartsWith(TEXT("/Game/__UeremcpPoc/"))
			&& !OutSystemPath.StartsWith(TEXT("/Game/__UeremcpTests/")))
		{
			OutError = FString::Printf(
				TEXT("weather.%s asset_path '%s' must be under /Game/__UeremcpPoc/ or /Game/__UeremcpTests/."),
				*Key,
				*OutSystemPath);
			return false;
		}

		if (!PhenomenonSpec.AssetPathOverride.IsEmpty() && !bDryRun)
		{
			if (LoadObject<UNiagaraSystem>(nullptr, *OutSystemPath))
			{
				OutCreateStatus = TEXT("reused_existing");
				OutNotes.Add(FString::Printf(
					TEXT("Using caller-supplied %s Niagara at %s."), *Key, *OutSystemPath));
				return true;
			}
			OutError = FString::Printf(
				TEXT("weather asset_path '%s' could not be loaded as UNiagaraSystem."),
				*OutSystemPath);
			return false;
		}

		FString Element = TEXT("water");
		TArray<FString> ComponentRoles;
		TArray<TSharedPtr<FJsonValue>> PrimaryColor;
		float Scale = 1.25f;
		float Intensity = FMath::Clamp(PhenomenonSpec.Intensity, 0.f, 1.f) * 8.f + 1.f;
		if (Key == TEXT("snow"))
		{
			Element = TEXT("ice");
			ComponentRoles = { TEXT("rain"), TEXT("mist") };
			PrimaryColor = {
				MakeShared<FJsonValueNumber>(0.92),
				MakeShared<FJsonValueNumber>(0.95),
				MakeShared<FJsonValueNumber>(1.0),
				MakeShared<FJsonValueNumber>(0.75)
			};
			Scale = 1.4f;
		}
		else if (Key == TEXT("hail"))
		{
			Element = TEXT("ice");
			ComponentRoles = { TEXT("sparks") };
			PrimaryColor = {
				MakeShared<FJsonValueNumber>(0.85),
				MakeShared<FJsonValueNumber>(0.92),
				MakeShared<FJsonValueNumber>(1.0),
				MakeShared<FJsonValueNumber>(0.9)
			};
			Scale = 0.9f;
			Intensity *= 1.4f;
		}
		else if (Key == TEXT("fog"))
		{
			Element = TEXT("water");
			ComponentRoles = { TEXT("mist") };
			PrimaryColor = {
				MakeShared<FJsonValueNumber>(0.7),
				MakeShared<FJsonValueNumber>(0.75),
				MakeShared<FJsonValueNumber>(0.8),
				MakeShared<FJsonValueNumber>(0.5)
			};
			Scale = 2.0f;
			Intensity *= 0.6f;
		}
		else
		{
			ComponentRoles = { TEXT("rain"), TEXT("mist") };
			PrimaryColor = {
				MakeShared<FJsonValueNumber>(0.55),
				MakeShared<FJsonValueNumber>(0.70),
				MakeShared<FJsonValueNumber>(0.85),
				MakeShared<FJsonValueNumber>(0.65)
			};
		}

		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("protocol_version"), TEXT("1.0"));
		Root->SetStringField(TEXT("action"), TEXT("create_niagara_effect"));
		Root->SetStringField(
			TEXT("request_id"),
			FString::Printf(TEXT("%s-%s"), *RequestId, *Key));
		Root->SetStringField(TEXT("mode"), TEXT("replace"));
		Root->SetStringField(
			TEXT("idempotency_key"),
			FString::Printf(TEXT("env-%s-%s"), *Key, *OutSystemPath));

		TSharedPtr<FJsonObject> Target = MakeShared<FJsonObject>();
		Target->SetStringField(TEXT("asset_path"), OutSystemPath);
		Root->SetObjectField(TEXT("target"), Target);

		TSharedPtr<FJsonObject> Options = MakeShared<FJsonObject>();
		Options->SetBoolField(TEXT("dry_run"), bDryRun);
		Options->SetBoolField(TEXT("compile"), true);
		Options->SetBoolField(TEXT("validate"), true);
		Options->SetBoolField(TEXT("save"), true);
		Root->SetObjectField(TEXT("options"), Options);

		TSharedPtr<FJsonObject> Specification = MakeShared<FJsonObject>();
		Specification->SetStringField(TEXT("effect_type"), TEXT("precipitation"));
		Specification->SetStringField(TEXT("element"), Element);
		Specification->SetStringField(
			TEXT("name"),
			FString::Printf(
				TEXT("NS_Env%s"),
				*FString(Key.Left(1).ToUpper() + Key.Mid(1))));

		TArray<TSharedPtr<FJsonValue>> Components;
		for (const FString& Role : ComponentRoles)
		{
			Components.Add(MakeShared<FJsonValueString>(Role));
		}
		Specification->SetArrayField(TEXT("components"), Components);

		TSharedPtr<FJsonObject> Parameters = MakeShared<FJsonObject>();
		Parameters->SetArrayField(TEXT("primary_color"), PrimaryColor);
		Parameters->SetNumberField(TEXT("scale"), Scale);
		Parameters->SetNumberField(TEXT("intensity"), Intensity);
		Specification->SetObjectField(TEXT("parameters"), Parameters);

		TSharedPtr<FJsonObject> Materials = MakeShared<FJsonObject>();
		for (const FString& Role : ComponentRoles)
		{
			TSharedPtr<FJsonObject> RoleMat = MakeShared<FJsonObject>();
			RoleMat->SetObjectField(TEXT("create_spec"), MakeElementCoreMaterialCreateSpec(Element));
			RoleMat->SetBoolField(TEXT("reuse_if_present"), true);
			Materials->SetObjectField(Role, RoleMat);
		}
		Specification->SetObjectField(TEXT("materials"), Materials);
		Root->SetObjectField(TEXT("specification"), Specification);

		FString RequestJson;
		{
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestJson);
			FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		}

		const FString ResponseJson = UUeremcpNiagaraToolset::CreateNiagaraEffect(RequestJson);
		++InOutOps;

		TSharedPtr<FJsonObject> ResponseRoot;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);
		if (!FJsonSerializer::Deserialize(Reader, ResponseRoot) || !ResponseRoot.IsValid())
		{
			OutError = FString::Printf(
				TEXT("CreateNiagaraEffect returned unparseable JSON for %s."), *Key);
			return false;
		}

		OutCreateStatus = ResponseRoot->GetStringField(TEXT("status"));
		const FString Summary = ResponseRoot->GetStringField(TEXT("summary"));
		OutNotes.Add(FString::Printf(
			TEXT("CreateNiagaraEffect(%s) status=%s — %s"),
			*Key,
			*OutCreateStatus,
			*Summary));

		if (bDryRun)
		{
			return OutCreateStatus != TEXT("rejected")
				&& OutCreateStatus != TEXT("failed_validation");
		}

		const bool bStatusOk =
			OutCreateStatus == TEXT("created_and_validated")
			|| OutCreateStatus == TEXT("modified_and_validated")
			|| OutCreateStatus == TEXT("created_with_warnings")
			|| OutCreateStatus == TEXT("partially_completed")
			|| OutCreateStatus == TEXT("no_change_required");
		if (!bStatusOk)
		{
			OutError = FString::Printf(
				TEXT("CreateNiagaraEffect failed for %s (%s): %s"),
				*Key,
				*OutCreateStatus,
				*Summary);
			return false;
		}

		UNiagaraSystem* Created = LoadObject<UNiagaraSystem>(nullptr, *OutSystemPath);
		if (!Created)
		{
			const FString ObjectPath = FString::Printf(
				TEXT("%s.%s"),
				*OutSystemPath,
				*FPackageName::GetLongPackageAssetName(OutSystemPath));
			Created = LoadObject<UNiagaraSystem>(nullptr, *ObjectPath);
		}
		if (!Created)
		{
			OutError = FString::Printf(
				TEXT("CreateNiagaraEffect reported %s but UNiagaraSystem missing at '%s'."),
				*OutCreateStatus,
				*OutSystemPath);
			return false;
		}
		return true;
	}

	int32 PlaceIceWallRing(
		UWorld* World,
		const FUeremcpEnvironmentBuildSpec& Spec,
		const FUeremcpStructurePlacementSpec& Placement,
		const FVector& LandscapeOffset,
		const FBox& Extents,
		TArray<FString>& CreatedLabels)
	{
		const int32 Count = FMath::Clamp(Placement.Count, 4, 128);
		const float HalfX = Extents.GetExtent().X;
		const float HalfY = Extents.GetExtent().Y;
		const float Perimeter = 2.f * (HalfX + HalfY) * Spec.ScaleXY;
		const float Step = Perimeter / float(Count);
		int32 Placed = 0;

		for (int32 I = 0; I < Count; ++I)
		{
			const float Dist = Step * float(I);
			float LocalX = 0.f;
			float LocalY = 0.f;
			if (Dist < HalfX * 2.f * Spec.ScaleXY)
			{
				LocalX = Dist;
				LocalY = 0.f;
			}
			else if (Dist < (HalfX * 2.f + HalfY * 2.f) * Spec.ScaleXY)
			{
				LocalX = HalfX * 2.f * Spec.ScaleXY;
				LocalY = Dist - HalfX * 2.f * Spec.ScaleXY;
			}
			else if (Dist < (HalfX * 4.f + HalfY * 2.f) * Spec.ScaleXY)
			{
				LocalX = HalfX * 2.f * Spec.ScaleXY - (Dist - (HalfX * 2.f + HalfY * 2.f) * Spec.ScaleXY);
				LocalY = HalfY * 2.f * Spec.ScaleXY;
			}
			else
			{
				LocalX = 0.f;
				LocalY = HalfY * 2.f * Spec.ScaleXY - (Dist - (HalfX * 4.f + HalfY * 2.f) * Spec.ScaleXY);
			}

			const FVector Loc = LandscapeOffset + FVector(
				LocalX - HalfX * Spec.ScaleXY,
				LocalY - HalfY * Spec.ScaleXY,
				100.f);
			ADynamicMeshActor* Structure = World->SpawnActor<ADynamicMeshActor>(Loc, FRotator::ZeroRotator);
			if (!Structure)
			{
				continue;
			}
			Structure->SetActorLabel(FString::Printf(TEXT("UEREMCP_IceWall_%d"), I));
			if (UDynamicMeshComponent* DMC = Structure->GetDynamicMeshComponent())
			{
				if (UDynamicMesh* Mesh = DMC->GetDynamicMesh())
				{
					FGeometryScriptPrimitiveOptions Opts;
					UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
						Mesh,
						Opts,
						FTransform::Identity,
						Placement.Thickness,
						Placement.Width,
						Placement.Height,
						0, 0, 0,
						EGeometryScriptPrimitiveOriginMode::Base,
						nullptr);
					DMC->NotifyMeshUpdated();
					++Placed;
					CreatedLabels.Add(Structure->GetActorLabel());
				}
			}
		}
		return Placed;
	}

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

	bool AllowsApproximation(const FString& Policy)
	{
		return Policy.Equals(TEXT("allow_approximate"), ESearchCase::IgnoreCase);
	}

	bool WouldUseFoliageApproximation(const FUeremcpEnvironmentBuildSpec& Spec)
	{
		if (!Spec.WantsVegetation())
		{
			return false;
		}
		if (Spec.MeshPath.IsEmpty())
		{
			return true;
		}
		return LoadObject<UStaticMesh>(nullptr, *Spec.MeshPath) == nullptr;
	}

	bool WouldUseRainApproximation(const FUeremcpEnvironmentBuildSpec& Spec)
	{
		for (const FUeremcpWeatherPhenomenonSpec& Phenomenon : Spec.WeatherPhenomena)
		{
			if (Phenomenon.Phenomenon.Equals(TEXT("rain"), ESearchCase::IgnoreCase))
			{
				if (Phenomenon.AssetPathOverride.IsEmpty())
				{
					return false;
				}
				return LoadObject<UNiagaraSystem>(nullptr, *Phenomenon.AssetPathOverride) == nullptr;
			}
		}
		if (!Spec.bIncludeRain)
		{
			return false;
		}
		if (Spec.RainSystemPath.IsEmpty())
		{
			return Spec.WeatherPhenomena.Num() == 0;
		}
		return LoadObject<UNiagaraSystem>(nullptr, *Spec.RainSystemPath) == nullptr;
	}

	FString CheckFallbackPolicy(const FUeremcpEnvironmentBuildSpec& Spec)
	{
		if (AllowsApproximation(Spec.FallbackPolicy))
		{
			return FString();
		}
		if (WouldUseFoliageApproximation(Spec))
		{
			return Spec.MeshPath.IsEmpty()
				? TEXT("fallback_policy=prefer_real requires biome.mesh_path when forest is included")
				: TEXT("fallback_policy=prefer_real requires loadable biome.mesh_path when forest is included");
		}
		if (WouldUseRainApproximation(Spec))
		{
			return Spec.RainSystemPath.IsEmpty()
				? TEXT("fallback_policy=prefer_real requires weather.rain_system_path when rain is included")
				: TEXT("fallback_policy=prefer_real requires loadable weather.rain_system_path when rain is included");
		}
		return FString();
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

	double SchemaVersion = 2.0;
	if (Spec->TryGetNumberField(TEXT("schema_version"), SchemaVersion))
	{
		Out.SchemaVersion = int32(SchemaVersion);
	}

	// Returns whether the field was PRESENT, and always clears InOut first.
	//
	// It previously left InOut untouched when the field was absent. Every call
	// site shares one `Tmp`, so an absent field silently inherited the value of
	// whichever field was read before it, and the `if (Tmp > 0)` guard then
	// applied that unrelated number.
	//
	// Measured consequence: a request omitting vegetation.slope_limit_deg picked
	// up min_normalized_height's 0.95 and enforced a 0.95-degree slope limit,
	// rejecting effectively every candidate and scattering 0 instances -- while
	// reporting success. Same mechanism silently corrupted terrain scale_z from
	// terrain size.
	auto ReadObjNumber = [&](const FString& Obj, const FString& Key, double& InOut) -> bool
	{
		InOut = 0;
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Spec->TryGetObjectField(Obj, Child) && Child && (*Child)->HasField(Key))
		{
			InOut = (*Child)->GetNumberField(Key);
			return true;
		}
		return false;
	};

	double Tmp = 0;
	const TSharedPtr<FJsonObject>* Terrain = nullptr;
	if (Spec->TryGetObjectField(TEXT("terrain"), Terrain) && Terrain)
	{
		FString Profile;
		if ((*Terrain)->TryGetStringField(TEXT("profile"), Profile))
		{
			if (!ParseTerrainProfile(Profile, Out.TerrainProfile))
			{
				OutError = FString::Printf(
					TEXT("terrain.profile '%s' unsupported; use mountains|plateau|canyon|flat_with_mountains_ring"),
					*Profile);
				return false;
			}
		}
	}
	ReadObjNumber(TEXT("terrain"), TEXT("size_x"), Tmp); if (Tmp > 0) Out.SizeX = int32(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("size_y"), Tmp); if (Tmp > 0) Out.SizeY = int32(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("size"), Tmp);
	if (Tmp > 0) { Out.SizeX = int32(Tmp); Out.SizeY = int32(Tmp); }
	ReadObjNumber(TEXT("terrain"), TEXT("mountain_amplitude"), Tmp); if (Tmp > 0) Out.MountainAmplitude = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("mountain_weight"), Tmp); if (Tmp > 0) Out.MountainAmplitude = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("valley_depth"), Tmp); if (Tmp > 0) Out.ValleyDepth = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("scale_xy"), Tmp); if (Tmp > 0) Out.ScaleXY = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("scale_z"), Tmp); if (Tmp > 0) Out.ScaleZ = float(Tmp);
	ReadObjNumber(TEXT("terrain"), TEXT("z_scale"), Tmp); if (Tmp > 0) Out.ScaleZ = float(Tmp);

	const TSharedPtr<FJsonObject>* Hydrology = nullptr;
	if (Spec->TryGetObjectField(TEXT("hydrology"), Hydrology) && Hydrology)
	{
		const TSharedPtr<FJsonObject>* RiverObj = nullptr;
		if ((*Hydrology)->TryGetObjectField(TEXT("river"), RiverObj) && RiverObj)
		{
			Out.bIncludeRiver = true;
			if ((*RiverObj)->TryGetNumberField(TEXT("width"), Tmp) && Tmp > 0)
			{
				Out.RiverWidth = float(Tmp);
			}
		}
	}
	ReadObjNumber(TEXT("river"), TEXT("width"), Tmp); if (Tmp > 0) Out.RiverWidth = float(Tmp);

	const TSharedPtr<FJsonObject>* Vegetation = nullptr;
	if (Spec->TryGetObjectField(TEXT("vegetation"), Vegetation) && Vegetation)
	{
		(*Vegetation)->TryGetStringField(TEXT("mode"), Out.VegetationMode);
		ReadObjNumber(TEXT("vegetation"), TEXT("forest_bank_width"), Tmp);
		if (Tmp > 0) Out.ForestBankWidth = float(Tmp);
		ReadObjNumber(TEXT("vegetation"), TEXT("max_foliage_instances"), Tmp);
		if (Tmp > 0) Out.MaxFoliageInstances = int32(Tmp);
		ReadObjNumber(TEXT("vegetation"), TEXT("slope_limit_deg"), Tmp);
		if (Tmp > 0) Out.FoliageSlopeLimitDegrees = float(Tmp);
		if (ReadObjNumber(TEXT("vegetation"), TEXT("min_normalized_height"), Tmp) && Tmp >= 0)
		{
			Out.FoliageMinNormalizedHeight = float(Tmp);
		}
		ReadObjNumber(TEXT("vegetation"), TEXT("max_normalized_height"), Tmp);
		if (Tmp > 0) Out.FoliageMaxNormalizedHeight = float(Tmp);
		(*Vegetation)->TryGetStringField(TEXT("mesh_path"), Out.MeshPath);
	}

	ReadObjNumber(TEXT("biome"), TEXT("forest_bank_width"), Tmp); if (Tmp > 0) Out.ForestBankWidth = float(Tmp);
	ReadObjNumber(TEXT("biome"), TEXT("max_foliage_instances"), Tmp); if (Tmp > 0) Out.MaxFoliageInstances = int32(Tmp);
	ReadObjNumber(TEXT("biome"), TEXT("slope_limit_deg"), Tmp); if (Tmp > 0) Out.FoliageSlopeLimitDegrees = float(Tmp);
	if (ReadObjNumber(TEXT("biome"), TEXT("min_normalized_height"), Tmp) && Tmp >= 0) { Out.FoliageMinNormalizedHeight = float(Tmp); }
	ReadObjNumber(TEXT("biome"), TEXT("max_normalized_height"), Tmp); if (Tmp > 0) Out.FoliageMaxNormalizedHeight = float(Tmp);
	const TSharedPtr<FJsonObject>* Biome = nullptr;
	if (Spec->TryGetObjectField(TEXT("biome"), Biome) && Biome)
	{
		(*Biome)->TryGetStringField(TEXT("mesh_path"), Out.MeshPath);
	}

	const TArray<TSharedPtr<FJsonValue>>* WeatherArray = nullptr;
	if (Spec->TryGetArrayField(TEXT("weather"), WeatherArray) && WeatherArray)
	{
		for (const TSharedPtr<FJsonValue>& Value : *WeatherArray)
		{
			const TSharedPtr<FJsonObject>* PhenomenonObj = nullptr;
			if (!Value->TryGetObject(PhenomenonObj) || !PhenomenonObj)
			{
				continue;
			}
			FUeremcpWeatherPhenomenonSpec Phenomenon;
			if (!(*PhenomenonObj)->TryGetStringField(TEXT("phenomenon"), Phenomenon.Phenomenon))
			{
				OutError = TEXT("weather[] entries require phenomenon (rain|snow|hail|fog)");
				return false;
			}
			if (!IsSupportedWeatherPhenomenon(Phenomenon.Phenomenon))
			{
				OutError = FString::Printf(
					TEXT("weather phenomenon '%s' is not supported; use rain|snow|hail|fog"),
					*Phenomenon.Phenomenon);
				return false;
			}
			double Intensity = 0.5;
			if ((*PhenomenonObj)->TryGetNumberField(TEXT("intensity"), Intensity))
			{
				Phenomenon.Intensity = float(Intensity);
			}
			bool bFollow = true;
			if ((*PhenomenonObj)->TryGetBoolField(TEXT("follow_player"), bFollow))
			{
				Phenomenon.bFollowPlayer = bFollow;
			}
			(*PhenomenonObj)->TryGetStringField(TEXT("follow"), Phenomenon.FollowTarget);
			(*PhenomenonObj)->TryGetStringField(TEXT("asset_path"), Phenomenon.AssetPathOverride);
			const TArray<TSharedPtr<FJsonValue>>* Hints = nullptr;
			if ((*PhenomenonObj)->TryGetArrayField(TEXT("material_hints"), Hints) && Hints)
			{
				for (const TSharedPtr<FJsonValue>& Hint : *Hints)
				{
					FString HintStr;
					if (Hint->TryGetString(HintStr))
					{
						Phenomenon.MaterialHints.Add(HintStr);
					}
				}
			}
			Out.WeatherPhenomena.Add(Phenomenon);
		}
	}
	else
	{
		const TSharedPtr<FJsonObject>* WeatherObj = nullptr;
		if (Spec->TryGetObjectField(TEXT("weather"), WeatherObj) && WeatherObj)
		{
			FUeremcpWeatherPhenomenonSpec Rain;
			Rain.Phenomenon = TEXT("rain");
			(*WeatherObj)->TryGetStringField(TEXT("rain_system_path"), Rain.AssetPathOverride);
			Out.RainSystemPath = Rain.AssetPathOverride;
			(*WeatherObj)->TryGetStringField(TEXT("follow"), Rain.FollowTarget);
			Out.WeatherFollow = Rain.FollowTarget;
			double StreakCount = 0;
			if ((*WeatherObj)->TryGetNumberField(TEXT("streak_count"), StreakCount) && StreakCount > 0)
			{
				Out.RainStreakCount = int32(StreakCount);
			}
			Out.WeatherPhenomena.Add(Rain);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* StructuresArray = nullptr;
	if (Spec->TryGetArrayField(TEXT("structures"), StructuresArray) && StructuresArray)
	{
		for (const TSharedPtr<FJsonValue>& Value : *StructuresArray)
		{
			const TSharedPtr<FJsonObject>* StructObj = nullptr;
			if (!Value->TryGetObject(StructObj) || !StructObj)
			{
				continue;
			}
			FUeremcpStructurePlacementSpec Placement;
			if (!(*StructObj)->TryGetStringField(TEXT("kind"), Placement.Kind))
			{
				OutError = TEXT("structures[] entries require kind");
				return false;
			}
			if (!IsSupportedStructureKind(Placement.Kind))
			{
				OutError = FString::Printf(
					TEXT("structure kind '%s' unsupported; use ice_wall_ring|barrier_wall|box_along_river"),
					*Placement.Kind);
				return false;
			}
			double Count = Placement.Count;
			if ((*StructObj)->TryGetNumberField(TEXT("count"), Count) && Count > 0)
			{
				Placement.Count = int32(Count);
			}
			double Height = Placement.Height;
			if ((*StructObj)->TryGetNumberField(TEXT("height"), Height) && Height > 0)
			{
				Placement.Height = float(Height);
			}
			double Thickness = Placement.Thickness;
			if ((*StructObj)->TryGetNumberField(TEXT("thickness"), Thickness) && Thickness > 0)
			{
				Placement.Thickness = float(Thickness);
			}
			double Width = Placement.Width;
			if ((*StructObj)->TryGetNumberField(TEXT("width"), Width) && Width > 0)
			{
				Placement.Width = float(Width);
			}
			(*StructObj)->TryGetStringField(TEXT("placement"), Placement.Placement);
			(*StructObj)->TryGetStringField(TEXT("material_path"), Placement.MaterialPath);
			Out.Structures.Add(Placement);
		}
	}
	else
	{
		const TSharedPtr<FJsonObject>* LegacyStructures = nullptr;
		if (Spec->TryGetObjectField(TEXT("structures"), LegacyStructures) && LegacyStructures)
		{
			FUeremcpStructurePlacementSpec Placement;
			Placement.Kind = TEXT("box_along_river");
			double Count = 0;
			if ((*LegacyStructures)->TryGetNumberField(TEXT("count"), Count) && Count > 0)
			{
				Placement.Count = int32(Count);
			}
			Out.Structures.Add(Placement);
		}
	}

	const TSharedPtr<FJsonObject>* Lighting = nullptr;
	if (Spec->TryGetObjectField(TEXT("lighting"), Lighting) && Lighting)
	{
		(*Lighting)->TryGetStringField(TEXT("preset"), Out.LightingPreset);
	}

	const TSharedPtr<FJsonObject>* Viewpoint = nullptr;
	if (Spec->TryGetObjectField(TEXT("viewpoint"), Viewpoint) && Viewpoint)
	{
		(*Viewpoint)->TryGetStringField(TEXT("mode"), Out.ViewpointMode);
	}

	if (Spec->HasField(TEXT("fallback_policy")))
	{
		FString Policy;
		if (!Spec->TryGetStringField(TEXT("fallback_policy"), Policy))
		{
			OutError = TEXT("fallback_policy must be a string");
			return false;
		}
		Out.FallbackPolicy = Policy;
	}
	if (!Out.FallbackPolicy.Equals(TEXT("prefer_real"), ESearchCase::IgnoreCase)
		&& !Out.FallbackPolicy.Equals(TEXT("allow_approximate"), ESearchCase::IgnoreCase))
	{
		OutError = TEXT("fallback_policy must be prefer_real or allow_approximate");
		return false;
	}
	Spec->TryGetStringField(TEXT("destination_level_path"), Out.DestinationLevelPath);

	const bool bHasIncludeBlock = Spec->HasField(TEXT("include"));
	if (bHasIncludeBlock)
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
		if (Out.bIncludeRain && Out.WeatherPhenomena.Num() == 0)
		{
			FUeremcpWeatherPhenomenonSpec Rain;
			Rain.Phenomenon = TEXT("rain");
			Rain.AssetPathOverride = Out.RainSystemPath;
			Rain.FollowTarget = Out.WeatherFollow;
			Out.WeatherPhenomena.Add(Rain);
		}
	}
	else
	{
		if (Terrain && (*Terrain)->HasField(TEXT("profile")))
		{
			Out.bIncludeTerrain = true;
		}
		if (Spec->HasField(TEXT("hydrology")) || Spec->HasField(TEXT("river")))
		{
			Out.bIncludeRiver = true;
		}
		if (Spec->HasField(TEXT("vegetation")))
		{
			Out.bIncludeForest = !Out.VegetationMode.Equals(TEXT("none"), ESearchCase::IgnoreCase);
		}
		if (Out.WeatherPhenomena.Num() > 0)
		{
			Out.bIncludeRain = Out.WeatherPhenomena.ContainsByPredicate(
				[](const FUeremcpWeatherPhenomenonSpec& P)
				{
					return P.Phenomenon.Equals(TEXT("rain"), ESearchCase::IgnoreCase);
				});
		}
		if (Spec->HasField(TEXT("lighting")))
		{
			Out.bIncludeLighting = true;
		}
		if (Out.Structures.Num() > 0)
		{
			Out.bIncludeStructures = true;
		}
	}

	if (Spec->HasField(TEXT("biome")) && bHasIncludeBlock && Out.bIncludeForest)
	{
		Out.VegetationMode = TEXT("forest");
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
		OutError = TEXT("vegetation/biome min_normalized_height must be <= max_normalized_height");
		return false;
	}
	if (!Out.WeatherPhenomena.IsEmpty())
	{
		for (const FUeremcpWeatherPhenomenonSpec& Phenomenon : Out.WeatherPhenomena)
		{
			if (Phenomenon.FollowTarget != TEXT("player_camera")
				&& Phenomenon.FollowTarget != TEXT("player_pawn"))
			{
				OutError = TEXT("weather.follow must be player_camera or player_pawn");
				return false;
			}
		}
	}
	else if (Out.WeatherFollow != TEXT("player_camera") && Out.WeatherFollow != TEXT("player_pawn"))
	{
		OutError = TEXT("weather.follow must be player_camera or player_pawn");
		return false;
	}
	return ValidateIncludeDependencies(Out, OutError);
}

bool FUeremcpEnvironmentService::ValidateIncludeDependencies(
	const FUeremcpEnvironmentBuildSpec& Spec,
	FString& OutError)
{
	if (Spec.WantsVegetation() && !Spec.bIncludeRiver)
	{
		OutError = TEXT(
			"vegetation/forest bank scatter requires hydrology.river or include.river: true "
			"(bank scatter needs a river exclusion corridor; use vegetation.mode=none or add river)");
		return false;
	}
	for (const FUeremcpStructurePlacementSpec& Placement : Spec.Structures)
	{
		if (Placement.Kind.Equals(TEXT("box_along_river"), ESearchCase::IgnoreCase) && !Spec.bIncludeRiver)
		{
			OutError = TEXT(
				"structures box_along_river requires hydrology.river or include.river: true");
			return false;
		}
	}
	for (const FUeremcpWeatherPhenomenonSpec& Phenomenon : Spec.WeatherPhenomena)
	{
		const bool bRain = Phenomenon.Phenomenon.Equals(TEXT("rain"), ESearchCase::IgnoreCase);
		const bool bNeedsAsset = Phenomenon.AssetPathOverride.IsEmpty();
		if (bRain && bNeedsAsset && AllowApproximateFallback(Spec))
		{
			continue;
		}
		if ((bRain || Phenomenon.Phenomenon.Equals(TEXT("snow"), ESearchCase::IgnoreCase)
			|| Phenomenon.Phenomenon.Equals(TEXT("hail"), ESearchCase::IgnoreCase)
			|| Phenomenon.Phenomenon.Equals(TEXT("fog"), ESearchCase::IgnoreCase))
			&& Spec.FallbackPolicy.Equals(TEXT("prefer_real"), ESearchCase::CaseSensitive)
			&& bNeedsAsset)
		{
			// CreateNiagaraEffect will run at build time — no path required upfront.
			continue;
		}
	}
	if (Spec.bIncludeRain && Spec.WeatherPhenomena.Num() == 0
		&& Spec.RainSystemPath.IsEmpty()
		&& Spec.FallbackPolicy.Equals(TEXT("prefer_real"), ESearchCase::CaseSensitive))
	{
		// include.rain legacy: CreateNiagaraEffect at build time is acceptable.
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
			float Amplitude = Spec.MountainAmplitude;
			float Base = 0.45f;
			switch (Spec.TerrainProfile)
			{
			case EUeremcpTerrainProfile::Plateau:
				Amplitude *= 0.35f;
				Base = 0.55f;
				break;
			case EUeremcpTerrainProfile::Canyon:
				Amplitude *= 1.1f;
				Base = 0.35f;
				break;
			case EUeremcpTerrainProfile::FlatWithMountainsRing:
			{
				const float Dx = FMath::Abs(Nx - 0.5f) * 2.f;
				const float Dy = FMath::Abs(Ny - 0.5f) * 2.f;
				const float Edge = FMath::Max(Dx, Dy);
				Amplitude *= FMath::Clamp((Edge - 0.35f) / 0.65f, 0.f, 1.f);
				Base = 0.42f;
				break;
			}
			default:
				break;
			}
			H = Base + (H - 0.5f) * 2.f * Amplitude;

			const FVector World(float(X) * Spec.ScaleXY, float(Y) * Spec.ScaleXY, 0);
			const float Dist = River.DistanceToXY(World);
			const float HalfW = River.WidthAtClosest(World) * 0.5f;
			if (Dist < HalfW)
			{
				const float T = Dist / FMath::Max(1.f, HalfW);
				const float ValleyScale = Spec.TerrainProfile == EUeremcpTerrainProfile::Canyon ? 1.35f : 1.f;
				H -= Spec.ValleyDepth * ValleyScale * (1.f - T * T);
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
	OutMetrics->SetStringField(TEXT("terrain_profile"), TerrainProfileToString(Spec.TerrainProfile));

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
		TEXT("env-v2-1|%s|%llu|%dx%d|%.4f|%.4f|%.2f|%.2f|%d|%s|%d|%d"),
		*HeightmapHash,
		Spec.Seed,
		Spec.SizeX,
		Spec.SizeY,
		Spec.MountainAmplitude,
		Spec.ValleyDepth,
		Spec.RiverWidth,
		Spec.ForestBankWidth,
		Spec.MaxFoliageInstances,
		*TerrainProfileToString(Spec.TerrainProfile),
		Spec.WeatherPhenomena.Num(),
		Spec.Structures.Num()));
	Result.Revision = FString::Printf(TEXT("env:%08x"), RevisionCrc);
	Result.InternalOperations += 1;

	const bool bAnyStage = Spec.bIncludeTerrain || Spec.bIncludeRiver || Spec.WantsVegetation()
		|| Spec.WeatherPhenomena.Num() > 0 || Spec.bIncludeLighting || Spec.bCaptureScreenshot
		|| Spec.bIncludeStructures || Spec.Structures.Num() > 0;

	if (const FString FallbackError = CheckFallbackPolicy(Spec); !FallbackError.IsEmpty())
	{
		Result.Status = TEXT("rejected");
		Result.Summary = FallbackError;
		Result.CapabilityNotes.Add(
			TEXT("Set fallback_policy=allow_approximate to permit cube foliage or instanced-streak rain fallbacks."));
		return Result;
	}

	if (bDryRun)
	{
		if (Spec.bIncludeTerrain)
		{
			Tech.Add(MakeTech(
				TEXT("landscape_heightmap"),
				TEXT("real"),
				TEXT("ALandscape::Import heightmap path planned [VERIFIED: LandscapeProxy.h:1418-1420]")));
		}
#if UEREMCP_HAS_WATER
		if (Spec.bIncludeRiver)
		{
			Tech.Add(MakeTech(
				TEXT("water_river"),
				TEXT("real"),
				TEXT("AWaterBodyRiver planned [VERIFIED: WaterBodyRiverActor.h:28]")));
		}
#else
		if (Spec.bIncludeRiver)
		{
			Tech.Add(MakeTech(
				TEXT("water_river"),
				TEXT("blocked"),
				TEXT("Water headers unavailable at compile time")));
		}
#endif
		if (Spec.WantsVegetation())
		{
			Tech.Add(MakeTech(
				TEXT("foliage_scatter"),
				WouldUseFoliageApproximation(Spec) ? FString(TEXT("approximated")) : FString(TEXT("real")),
				TEXT("Seeded HISMC / InstancedFoliageActor scatter with exclusion corridor")));
		}
		for (const FUeremcpWeatherPhenomenonSpec& Phenomenon : Spec.WeatherPhenomena)
		{
			const bool bRain = Phenomenon.Phenomenon.Equals(TEXT("rain"), ESearchCase::IgnoreCase);
			const bool bCanApprox = bRain && AllowApproximateFallback(Spec);
			Tech.Add(MakeTech(
				FString::Printf(TEXT("weather_%s"), *Phenomenon.Phenomenon.ToLower()),
				(bCanApprox && Phenomenon.AssetPathOverride.IsEmpty())
					? FString(TEXT("approximated"))
					: FString(TEXT("real")),
				FString::Printf(
					TEXT("CreateNiagaraEffect precipitation (%s) + AUeremcpWeatherFollower"),
					*Phenomenon.Phenomenon.ToLower())));
		}
		if (Spec.bIncludeLighting)
		{
			Tech.Add(MakeTech(
				TEXT("lighting"),
				TEXT("real"),
				TEXT("DirectionalLight + SkyLight + ExponentialHeightFog")));
		}
		if (Spec.bIncludeStructures || Spec.Structures.Num() > 0)
		{
			Tech.Add(MakeTech(
				TEXT("structures_geometryscript"),
				TEXT("real"),
				TEXT("GeometryScript AppendBox — ice_wall_ring / barrier_wall [VERIFIED: MeshPrimitiveFunctions.h:168]")));
		}
		AddTechArray(Result.RealVsApproximated, TEXT("technologies"), Tech);
		if (AllowsApproximation(Spec.FallbackPolicy)
			&& (WouldUseFoliageApproximation(Spec) || WouldUseRainApproximation(Spec)))
		{
			Result.bApproximated = true;
			Result.RealVsApproximated->SetBoolField(TEXT("approximated"), true);
		}
		Result.ChangeManifest->SetStringField(TEXT("destination"), Dest);
		Result.ChangeManifest->SetNumberField(TEXT("seed"), double(Spec.Seed));
		Result.ChangeManifest->SetBoolField(TEXT("dry_run"), true);
		Result.ChangeManifest->SetStringField(TEXT("revision"), Result.Revision);
		Result.Status = TEXT("no_change_required");
		if (!bAnyStage)
		{
			Result.Summary = FString::Printf(
				TEXT("Dry-run build_environment seed=%llu destination=%s — no include.* stages requested (opt-in; set include.terrain/river/forest/rain/lighting true to build)"),
				Spec.Seed,
				*Dest);
			Result.CapabilityNotes.Add(
				TEXT("opt-in includes: omitting include builds only a level shell on mutate; no subsystems are implied."));
		}
		else
		{
			Result.Summary = FString::Printf(
				TEXT("Dry-run build_environment seed=%llu destination=%s — height_range=%.3f non_flat=%s river_len=%.0f stages=%s"),
				Spec.Seed,
				*Dest,
				HeightMetrics->GetNumberField(TEXT("height_range")),
				HeightMetrics->GetBoolField(TEXT("non_flat")) ? TEXT("true") : TEXT("false"),
				HeightMetrics->GetNumberField(TEXT("river_length")),
				*FString::Printf(
					TEXT("terrain=%s river=%s forest=%s rain=%s lighting=%s"),
					Spec.bIncludeTerrain ? TEXT("true") : TEXT("false"),
					Spec.bIncludeRiver ? TEXT("true") : TEXT("false"),
					Spec.bIncludeForest ? TEXT("true") : TEXT("false"),
					Spec.bIncludeRain ? TEXT("true") : TEXT("false"),
					Spec.bIncludeLighting ? TEXT("true") : TEXT("false")));
		}
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
	bool bUsedApproximation = false;
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
	if (Spec.WantsVegetation())
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
			bUsedApproximation = true;
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
				if (River.SignedSideToClosestXY(Local) >= 0.f)
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
		const FString Preset = Spec.LightingPreset.ToLower();
		float SunIntensity = 4.0f;
		FLinearColor SunColor(0.56f, 0.64f, 0.78f);
		float SkyIntensity = 0.65f;
		FLinearColor LowerHemisphere(0.04f, 0.06f, 0.09f);
		float FogDensity = 0.025f;
		FLinearColor FogColor(0.19f, 0.24f, 0.31f);
		if (Preset == TEXT("clear"))
		{
			SunIntensity = 6.5f;
			SunColor = FLinearColor(1.f, 0.95f, 0.85f);
			SkyIntensity = 1.1f;
			FogDensity = 0.005f;
			FogColor = FLinearColor(0.55f, 0.65f, 0.8f);
		}
		else if (Preset == TEXT("blizzard"))
		{
			SunIntensity = 2.8f;
			SunColor = FLinearColor(0.82f, 0.88f, 0.95f);
			SkyIntensity = 0.9f;
			LowerHemisphere = FLinearColor(0.15f, 0.18f, 0.22f);
			FogDensity = 0.045f;
			FogColor = FLinearColor(0.75f, 0.8f, 0.88f);
		}
		else if (Preset == TEXT("rainy_overcast"))
		{
			SunIntensity = 3.5f;
			FogDensity = 0.03f;
		}

		ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 500), FRotator(-50.f, 30.f, 0.f));
		if (Sun)
		{
			Sun->SetActorLabel(TEXT("UEREMCP_Sun"));
			Sun->GetLightComponent()->SetIntensity(SunIntensity);
			Sun->GetLightComponent()->SetLightColor(SunColor);
			CreatedLabels.Add(TEXT("UEREMCP_Sun"));
		}
		ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Sky)
		{
			Sky->SetActorLabel(TEXT("UEREMCP_SkyLight"));
			Sky->GetLightComponent()->SetIntensity(SkyIntensity);
			Sky->GetLightComponent()->SetLowerHemisphereColor(LowerHemisphere);
			CreatedLabels.Add(TEXT("UEREMCP_SkyLight"));
		}
		AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator);
		if (Fog)
		{
			Fog->SetActorLabel(TEXT("UEREMCP_AtmosphereFog"));
			Fog->GetComponent()->SetFogDensity(FogDensity);
			Fog->GetComponent()->SetFogHeightFalloff(0.12f);
			Fog->GetComponent()->SetFogInscatteringColor(FogColor);
			CreatedLabels.Add(TEXT("UEREMCP_AtmosphereFog"));
		}
		const FVector ViewLocation(
			-float(Spec.SizeX - 1) * Spec.ScaleXY * 0.36f,
			-float(Spec.SizeY - 1) * Spec.ScaleXY * 0.36f,
			6500.f);
		const FRotator ViewRotation = (FVector::ZeroVector - ViewLocation).Rotation();
		if (ACameraActor* Camera = World->SpawnActor<ACameraActor>(ViewLocation, ViewRotation))
		{
			Camera->SetActorLabel(TEXT("UEREMCP_Viewpoint"));
			CreatedLabels.Add(TEXT("UEREMCP_Viewpoint"));
		}
		if (APlayerStart* Start = World->SpawnActor<APlayerStart>(
			ViewLocation + FVector(0.f, 0.f, -500.f), ViewRotation))
		{
			Start->SetActorLabel(TEXT("UEREMCP_PlayerStart"));
			CreatedLabels.Add(TEXT("UEREMCP_PlayerStart"));
		}
		Tech.Add(MakeTech(TEXT("lighting"), TEXT("real"), TEXT("DirectionalLight + SkyLight + ExponentialHeightFog")));
		Result.StructuralMetrics->SetStringField(TEXT("lighting_preset"), Spec.LightingPreset);
		Result.StructuralMetrics->SetStringField(TEXT("viewpoint"), TEXT("UEREMCP_Viewpoint"));
		Result.InternalOperations += 5;
	}

	int32 WeatherActorsCreated = 0;
	TArray<TSharedPtr<FJsonValue>> WeatherBuilt;
	for (const FUeremcpWeatherPhenomenonSpec& Phenomenon : Spec.WeatherPhenomena)
	{
		FString SystemPath;
		FString CreateStatus;
		FString WeatherError;
		TArray<FString> WeatherNotes;
		const bool bAssetOk = EnsurePrecipitationNiagaraSystem(
			Phenomenon,
			Spec,
			Dest,
			Request.RequestId.IsEmpty() ? TEXT("env-build") : Request.RequestId,
			false,
			SystemPath,
			CreateStatus,
			WeatherError,
			Result.InternalOperations,
			WeatherNotes);
		for (const FString& Note : WeatherNotes)
		{
			Result.CapabilityNotes.Add(Note);
		}

		const bool bIsRain = Phenomenon.Phenomenon.Equals(TEXT("rain"), ESearchCase::IgnoreCase);
		if (!bAssetOk)
		{
			if (bIsRain && AllowApproximateFallback(Spec))
			{
				AUeremcpWeatherFollower* RainActor = World->SpawnActor<AUeremcpWeatherFollower>(
					FVector(0, 0, 400), FRotator::ZeroRotator);
				if (RainActor)
				{
					RainActor->SetActorLabel(TEXT("UEREMCP_Weather_rain"));
					RainActor->NiagaraRain->SetVisibility(false);
					RainActor->ConfigureFallbackRain(int32(Spec.Seed), Spec.RainStreakCount);
					bUsedApproximation = true;
					CreatedLabels.Add(RainActor->GetActorLabel());
					++WeatherActorsCreated;
					Tech.Add(MakeTech(
						TEXT("weather_rain"),
						TEXT("approximated"),
						TEXT("Niagara create failed; streak fallback (fallback_policy=allow_approximate)")));
					Result.Warnings.Add(WeatherError);
				}
				continue;
			}
			DestroyOwnedEnvironmentActors(World);
			Result.Status = TEXT("failed_validation");
			Result.Summary = FString::Printf(
				TEXT("weather.%s requires real Niagara (fallback_policy=%s): %s"),
				*Phenomenon.Phenomenon.ToLower(),
				*Spec.FallbackPolicy,
				WeatherError.IsEmpty() ? TEXT("CreateNiagaraEffect failed") : *WeatherError);
			Result.CapabilityNotes.Add(TEXT(
				"Snow/hail never use streak fallback. Set fallback_policy=allow_approximate only for rain."));
			return Result;
		}

		UNiagaraSystem* WeatherSys = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
		AUeremcpWeatherFollower* WeatherActor = World->SpawnActor<AUeremcpWeatherFollower>(
			FVector(0, 0, 400), FRotator::ZeroRotator);
		if (WeatherActor && WeatherSys)
		{
			WeatherActor->SetActorLabel(
				FString::Printf(TEXT("UEREMCP_Weather_%s"), *Phenomenon.Phenomenon.ToLower()));
			WeatherActor->NiagaraRain->SetAsset(WeatherSys);
			WeatherActor->FallbackRain->SetVisibility(false);
			CreatedLabels.Add(WeatherActor->GetActorLabel());
			++WeatherActorsCreated;
			TSharedPtr<FJsonObject> Built = MakeShared<FJsonObject>();
			Built->SetStringField(TEXT("phenomenon"), Phenomenon.Phenomenon.ToLower());
			Built->SetStringField(TEXT("asset_path"), SystemPath);
			Built->SetStringField(TEXT("create_status"), CreateStatus);
			WeatherBuilt.Add(MakeShared<FJsonValueObject>(Built));
			Tech.Add(MakeTech(
				FString::Printf(TEXT("weather_%s"), *Phenomenon.Phenomenon.ToLower()),
				TEXT("real"),
				FString::Printf(TEXT("CreateNiagaraEffect + AUeremcpWeatherFollower at %s"), *SystemPath)));
			Result.InternalOperations += 1;
		}
	}
	Result.StructuralMetrics->SetNumberField(TEXT("weather_actors"), WeatherActorsCreated);
	Result.StructuralMetrics->SetArrayField(TEXT("weather_built"), WeatherBuilt);
	if (WeatherActorsCreated > 0)
	{
		Result.StructuralMetrics->SetStringField(TEXT("weather_follow"), Spec.WeatherFollow);
		Result.StructuralMetrics->SetBoolField(TEXT("weather_follower_tick_enabled"), true);
	}

	int32 StructuresPlaced = 0;
	TArray<FUeremcpStructurePlacementSpec> StructureSpecs = Spec.Structures;
	if (Spec.bIncludeStructures && StructureSpecs.Num() == 0)
	{
		FUeremcpStructurePlacementSpec Legacy;
		Legacy.Kind = TEXT("box_along_river");
		Legacy.Count = Spec.StructureCount;
		StructureSpecs.Add(Legacy);
	}
	for (const FUeremcpStructurePlacementSpec& Placement : StructureSpecs)
	{
		const FString Kind = Placement.Kind.ToLower();
		if (Kind == TEXT("ice_wall_ring") || Kind == TEXT("barrier_wall"))
		{
			StructuresPlaced += PlaceIceWallRing(
				World, Spec, Placement, LandscapeOffset, Extents, CreatedLabels);
			Tech.Add(MakeTech(
				TEXT("structures_ice_wall_ring"),
				TEXT("real"),
				TEXT("GeometryScript ring around terrain bounds [VERIFIED: MeshPrimitiveFunctions.h:168]")));
		}
		else if (Kind == TEXT("box_along_river"))
		{
			const int32 Count = FMath::Clamp(Placement.Count, 1, 32);
			for (int32 I = 0; I < Count && I < River.Points.Num(); ++I)
			{
				const FVector Loc = River.Points[I].Location
					+ LandscapeOffset
					+ FVector(River.Points[I].Width * 0.75f, 0.f, 100.f);
				ADynamicMeshActor* Structure = World->SpawnActor<ADynamicMeshActor>(Loc, FRotator::ZeroRotator);
				if (!Structure) { continue; }
				Structure->SetActorLabel(FString::Printf(TEXT("UEREMCP_Structure_%d"), I));
				if (UDynamicMeshComponent* DMC = Structure->GetDynamicMeshComponent())
				{
					if (UDynamicMesh* Mesh = DMC->GetDynamicMesh())
					{
						FGeometryScriptPrimitiveOptions Opts;
						UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
							Mesh, Opts, FTransform::Identity,
							200.f, 200.f, 400.f, 0, 0, 0,
							EGeometryScriptPrimitiveOriginMode::Base, nullptr);
						DMC->NotifyMeshUpdated();
						++StructuresPlaced;
						CreatedLabels.Add(Structure->GetActorLabel());
					}
				}
			}
			Tech.Add(MakeTech(TEXT("structures_box_along_river"), TEXT("real"), TEXT("Legacy river spline boxes")));
		}
	}
	Result.StructuralMetrics->SetNumberField(TEXT("structures_placed"), StructuresPlaced);
	Result.InternalOperations += StructuresPlaced;

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
	const bool bNonFlatGate = !Spec.bIncludeTerrain || bNonFlat;
	const bool bForestOk = !Spec.WantsVegetation() || FoliageCount > 0;
	const bool bBothBanks = !Spec.WantsVegetation() || (LeftBankCount > 0 && RightBankCount > 0);
	const bool bOpenChannel = !Spec.WantsVegetation() || ExclusionViolations == 0;
	const bool bRiverOk = !Spec.bIncludeRiver || bRiverCreated;
	const bool bWeatherOk = Spec.WeatherPhenomena.Num() == 0
		|| WeatherActorsCreated >= Spec.WeatherPhenomena.Num();
	const bool bStructuresOk = StructureSpecs.Num() == 0
		|| StructuresPlaced > 0;
	const bool bAnyBuilt = Spec.bIncludeTerrain || Spec.bIncludeRiver || Spec.WantsVegetation()
		|| Spec.WeatherPhenomena.Num() > 0 || Spec.bIncludeLighting || Spec.bCaptureScreenshot
		|| StructureSpecs.Num() > 0;

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
	if (!bAnyBuilt)
	{
		Result.Status = TEXT("no_change_required");
		Result.Summary = FString::Printf(
			TEXT("No include.* stages requested for seed=%llu — level shell only (metadata); set include.terrain/river/forest/rain/lighting true to build subsystems"),
			Spec.Seed);
		Result.CapabilityNotes.Add(
			TEXT("opt-in includes: BuildEnvironment does not imply terrain/river/forest/rain unless explicitly requested."));
	}
	else if (bNonFlatGate && bForestOk && bBothBanks && bOpenChannel
		&& bRiverOk && bWeatherOk && bStructuresOk && bPersistenceOk)
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
			bNonFlatGate ? TEXT("true") : TEXT("false"),
			bForestOk ? TEXT("true") : TEXT("false"),
			bBothBanks ? TEXT("true") : TEXT("false"),
			bOpenChannel ? TEXT("true") : TEXT("false"),
			bRiverOk ? TEXT("true") : TEXT("false"),
			bWeatherOk ? TEXT("true") : TEXT("false"),
			bPersistenceOk ? TEXT("true") : TEXT("false"));
	}
	Result.CapabilityNotes.Add(TEXT("World verification leans on structural metrics + human review; screenshots are not a gate (BACKLOG 5.8)."));
	Result.CapabilityNotes.Add(TEXT("CaptureWorldFrames is the general world capture hook; BuildEnvironment capture is supplementary only."));
	if (bUsedApproximation && AllowsApproximation(Spec.FallbackPolicy))
	{
		Result.bApproximated = true;
		Result.RealVsApproximated->SetBoolField(TEXT("approximated"), true);
	}
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
		if (Label.StartsWith(TEXT("UEREMCP_Weather_")) || Label == TEXT("UEREMCP_Rain"))
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
