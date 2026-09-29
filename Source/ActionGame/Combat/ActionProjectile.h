#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Abilities/TelekinesisTarget.h"
#include "Combat/CombatTypes.h"
#include "Combat/CombatFeedbackConfig.h"
#include "ActionProjectile.generated.h"

class UNiagaraComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Slow, readable projectile for enemies. Visible projectiles (instead of hitscan) give the
 * player time to read and dodge, which is what makes ranged enemies fair in an action game.
 * Can be caught with telekinesis and thrown back for bonus damage.
 */
UCLASS()
class ACTIONGAME_API AActionProjectile : public AActor, public ITelekinesisTarget
{
	GENERATED_BODY()

public:
	AActionProjectile();

	/** Sends the projectile flying. HomingTarget is optional (mild homing only). */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void Launch(AActor* InInstigator, const FVector& Direction, AActor* HomingTarget);

	// ITelekinesisTarget
	virtual bool CanBeGrabbed(const AActor* Grabber) const override;
	virtual void OnTelekinesisGrabbed(AActor* Grabber) override;
	virtual void TelekinesisMoveTo(const FVector& HoldLocation, float DeltaTime) override;
	virtual void OnTelekinesisThrown(AActor* Thrower, const FVector& Velocity) override;
	virtual void OnTelekinesisReleased() override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> Trail;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	FCombatDamageSpec Damage;

	UPROPERTY(EditAnywhere, Category = "Projectile", meta = (ClampMin = "0"))
	float Speed = 1600.f;

	/** Keep low: strong homing makes projectiles undodgeable and frustrating. */
	UPROPERTY(EditAnywhere, Category = "Projectile", meta = (ClampMin = "0"))
	float HomingAcceleration = 600.f;

	/** 0 = single-target hit, otherwise explodes for area damage. */
	UPROPERTY(EditAnywhere, Category = "Projectile", meta = (ClampMin = "0"))
	float ExplosionRadius = 0.f;

	UPROPERTY(EditAnywhere, Category = "Projectile", meta = (ClampMin = "0.1"))
	float MaxLifetime = 6.f;

	UPROPERTY(EditAnywhere, Category = "Projectile|Telekinesis")
	bool bCanBeCaught = true;

	/** Damage multiplier when thrown back at its owner's team: rewards the risky catch. */
	UPROPERTY(EditAnywhere, Category = "Projectile|Telekinesis", meta = (ClampMin = "1"))
	float ReflectDamageMultiplier = 2.f;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	FImpactEffect ExplosionEffect;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);

	void Explode(const FVector& Location, const FVector& Normal, AActor* DirectHitActor);

	UPROPERTY()
	TObjectPtr<AActor> OwnerInstigator;

	bool bFlying = false;
	bool bHeld = false;
	bool bReflected = false;
};
