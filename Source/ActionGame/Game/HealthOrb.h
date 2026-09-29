#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HealthOrb.generated.h"

class USoundBase;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Health pickup dropped by enemies (Control's health shards). It pops out, then homes in on
 * a nearby player. Healing comes from killing, not from hiding, so players are pushed toward
 * aggressive play, which is where the combat is most fun.
 */
UCLASS()
class ACTIONGAME_API AHealthOrb : public AActor
{
	GENERATED_BODY()

public:
	AHealthOrb();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(EditAnywhere, Category = "Health Orb", meta = (ClampMin = "0"))
	float HealAmount = 8.f;

	/** Seconds after spawning before the orb starts homing (lets the pop-out read). */
	UPROPERTY(EditAnywhere, Category = "Health Orb", meta = (ClampMin = "0"))
	float MagnetDelay = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Health Orb", meta = (ClampMin = "0"))
	float MagnetRadius = 900.f;

	UPROPERTY(EditAnywhere, Category = "Health Orb", meta = (ClampMin = "0"))
	float MagnetAcceleration = 5000.f;

	UPROPERTY(EditAnywhere, Category = "Health Orb", meta = (ClampMin = "0"))
	float PickupRadius = 80.f;

	UPROPERTY(EditAnywhere, Category = "Health Orb")
	TObjectPtr<USoundBase> PickupSound = nullptr;

private:
	FVector Velocity = FVector::ZeroVector;
	float Age = 0.f;
};
