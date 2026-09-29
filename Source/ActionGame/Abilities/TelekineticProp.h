#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Abilities/TelekinesisTarget.h"
#include "Combat/CombatTypes.h"
#include "Combat/CombatFeedbackConfig.h"
#include "TelekineticProp.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ETelekineticPropState : uint8
{
	Idle,
	Held,
	Thrown
};

/**
 * A physics object that can be ripped from the environment, held and thrown.
 * Deals damage scaled by impact speed and (optionally) shatters on impact, like Control's
 * Launch. Place Blueprint subclasses around arenas; they double as ammo and cover.
 */
UCLASS()
class ACTIONGAME_API ATelekineticProp : public AActor, public ITelekinesisTarget
{
	GENERATED_BODY()

public:
	ATelekineticProp();

	// ITelekinesisTarget
	virtual bool CanBeGrabbed(const AActor* Grabber) const override { return State == ETelekineticPropState::Idle; }
	virtual void OnTelekinesisGrabbed(AActor* Grabber) override;
	virtual void TelekinesisMoveTo(const FVector& HoldLocation, float DeltaTime) override;
	virtual void OnTelekinesisThrown(AActor* Thrower, const FVector& Velocity) override;
	virtual void OnTelekinesisReleased() override;

	UFUNCTION(BlueprintPure, Category = "Telekinesis")
	ETelekineticPropState GetPropState() const { return State; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Damage at ReferenceSpeed; scales linearly with actual impact speed (clamped 0.5x..1.5x). */
	UPROPERTY(EditAnywhere, Category = "Telekinesis")
	FCombatDamageSpec ImpactDamage;

	UPROPERTY(EditAnywhere, Category = "Telekinesis", meta = (ClampMin = "1"))
	float ReferenceSpeed = 4000.f;

	/** Impacts slower than this never deal damage (a dropped crate shouldn't hurt anyone). */
	UPROPERTY(EditAnywhere, Category = "Telekinesis", meta = (ClampMin = "0"))
	float MinDamageSpeed = 1200.f;

	/** How tightly the prop follows the hold point while held. */
	UPROPERTY(EditAnywhere, Category = "Telekinesis", meta = (ClampMin = "0"))
	float FollowStiffness = 10.f;

	UPROPERTY(EditAnywhere, Category = "Telekinesis", meta = (ClampMin = "0"))
	float MaxFollowSpeed = 3500.f;

	/** Lazy tumble while held: sells the "levitating" look. Degrees per second. */
	UPROPERTY(EditAnywhere, Category = "Telekinesis")
	float HoldSpinSpeed = 90.f;

	UPROPERTY(EditAnywhere, Category = "Telekinesis")
	bool bShatterOnImpact = true;

	UPROPERTY(EditAnywhere, Category = "Telekinesis")
	FImpactEffect ShatterEffect;

private:
	void SetHeldCollision(bool bHeld);
	void Shatter();
	void ReturnToIdle();

	UPROPERTY()
	TObjectPtr<AActor> ThrowInstigator;

	ETelekineticPropState State = ETelekineticPropState::Idle;
	FVector SpinAxis = FVector::UpVector;
	FTimerHandle ThrownTimeoutHandle;
};
