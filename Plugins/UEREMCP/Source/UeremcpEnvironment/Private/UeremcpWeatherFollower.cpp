#include "UeremcpWeatherFollower.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "UeremcpNoise.h"

AUeremcpWeatherFollower::AUeremcpWeatherFollower()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	NiagaraRain = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraRain"));
	NiagaraRain->SetupAttachment(SceneRoot);
	NiagaraRain->SetAutoActivate(true);

	FallbackRain = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("FallbackRain"));
	FallbackRain->SetupAttachment(SceneRoot);
	FallbackRain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FallbackRain->SetCastShadow(false);
}

void AUeremcpWeatherFollower::ConfigureFallbackRain(int32 Seed, int32 StreakCount)
{
	FallbackRain->ClearInstances();
	UStaticMesh* StreakMesh = LoadObject<UStaticMesh>(
		nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	FallbackRain->SetStaticMesh(StreakMesh);
	if (!StreakMesh)
	{
		return;
	}

	const int32 BoundedCount = FMath::Clamp(StreakCount, 32, 512);
	for (int32 Index = 0; Index < BoundedCount; ++Index)
	{
		const float X = FMath::Lerp(
			-1400.0f, 1400.0f,
			UeremcpNoise::ValueNoise2D(uint64(Seed) ^ 0xA11CEull, Index, 0));
		const float Y = FMath::Lerp(
			-1400.0f, 1400.0f,
			UeremcpNoise::ValueNoise2D(uint64(Seed) ^ 0xBEEFull, Index, 1));
		const float Z = FMath::Lerp(
			-250.0f, 900.0f,
			UeremcpNoise::ValueNoise2D(uint64(Seed) ^ 0xC10Dull, Index, 2));
		const float Length = FMath::Lerp(
			45.0f, 130.0f,
			UeremcpNoise::ValueNoise2D(uint64(Seed) ^ 0xD00Dull, Index, 3));

		FTransform Transform;
		Transform.SetLocation(FVector(X, Y, Z));
		Transform.SetScale3D(FVector(0.0125f, 0.0125f, Length / 100.0f));
		FallbackRain->AddInstance(Transform);
	}
}

void AUeremcpWeatherFollower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	FVector TrackedLocation = FVector::ZeroVector;
	bool bFoundTarget = false;

	// [VERIFIED: GameplayStatics.h:219] GetPlayerCameraManager
	// [VERIFIED: PlayerCameraManager.h:705] GetCameraLocation
	if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(World, 0))
	{
		TrackedLocation = Camera->GetCameraLocation();
		bFoundTarget = true;
	}
	else if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		// [VERIFIED: GameplayStatics.h:201] GetPlayerPawn
		TrackedLocation = Pawn->GetActorLocation();
		bFoundTarget = true;
	}

	if (!bFoundTarget)
	{
		return;
	}

	if (FollowSamples == 0)
	{
		FirstTrackedLocation = TrackedLocation;
	}
	LastTrackedLocation = TrackedLocation;
	++FollowSamples;
	SetActorLocation(TrackedLocation + FollowOffset);
}
