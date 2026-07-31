// UEREMCP — runtime PIE weather follower for environment acceptance.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "UeremcpWeatherFollower.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UNiagaraComponent;
class USceneComponent;

/**
 * Saved with generated scratch worlds and copied into PIE.
 * During play it follows player camera 0 (or pawn 0 as a fallback), providing
 * a measurable transform proof instead of assuming that rain follows.
 */
UCLASS()
class UEREMCPENVIRONMENT_API AUeremcpWeatherFollower : public AActor
{
	GENERATED_BODY()

public:
	AUeremcpWeatherFollower();

	virtual void Tick(float DeltaSeconds) override;

	void ConfigureFallbackRain(int32 Seed, int32 StreakCount);

	UPROPERTY(VisibleAnywhere, Category = "UEREMCP|Weather")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "UEREMCP|Weather")
	TObjectPtr<UNiagaraComponent> NiagaraRain;

	UPROPERTY(VisibleAnywhere, Category = "UEREMCP|Weather")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FallbackRain;

	UPROPERTY(VisibleAnywhere, Category = "UEREMCP|Weather")
	FVector FirstTrackedLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "UEREMCP|Weather")
	FVector LastTrackedLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "UEREMCP|Weather")
	int32 FollowSamples = 0;

	UPROPERTY(EditAnywhere, Category = "UEREMCP|Weather")
	FVector FollowOffset = FVector(0.0, 0.0, 350.0);
};
