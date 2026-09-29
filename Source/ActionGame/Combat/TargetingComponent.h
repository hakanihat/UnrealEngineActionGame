#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TargetingComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockTargetChanged, AActor*, NewTarget);

/**
 * Answers "what is the player trying to hit?" for every system:
 *  - Hard lock-on (toggle) with left/right target switching.
 *  - Melee target: the enemy closest to the stick direction, for attack magnetism.
 *  - Aim target: the enemy closest to the crosshair, for throw/launch aim assist.
 * Generous assistance is a deliberate choice: in melee-heavy games players judge the result
 * ("did I hit what I meant to?"), not the precision of their input.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTargetingComponent();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void ToggleLockOn();

	/** Switch to the next target to the left (Direction < 0) or right (> 0) on screen. */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SwitchTarget(float Direction);

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void ClearLockOn();

	UFUNCTION(BlueprintPure, Category = "Targeting")
	AActor* GetLockedTarget() const { return LockedTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Targeting")
	bool IsLockedOn() const { return LockedTarget.IsValid(); }

	/** Best enemy for a melee swing toward InputDirection (falls back to the camera direction). */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	AActor* GetMeleeTarget(FVector InputDirection) const;

	/** Best enemy near the crosshair within Range (the locked target wins if set). */
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	AActor* GetAimTarget(float Range) const;

	/** Enemy currently nearest the crosshair; refreshed every tick for HUD highlighting. */
	UFUNCTION(BlueprintPure, Category = "Targeting")
	AActor* GetSoftTarget() const { return SoftTarget.Get(); }

	/** Camera location and rotation of the owning controller. */
	void GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FOnLockTargetChanged OnLockTargetChanged;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0"))
	float LockOnRange = 2500.f;

	/** Lock breaks when the target gets further than this. */
	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0"))
	float LockOnBreakRange = 3500.f;

	UPROPERTY(EditAnywhere, Category = "Targeting", meta = (ClampMin = "0", ClampMax = "180"))
	float LockOnAngle = 45.f;

	UPROPERTY(EditAnywhere, Category = "Targeting|Melee", meta = (ClampMin = "0"))
	float MeleeAssistRange = 550.f;

	UPROPERTY(EditAnywhere, Category = "Targeting|Melee", meta = (ClampMin = "0", ClampMax = "180"))
	float MeleeAssistAngle = 70.f;

	UPROPERTY(EditAnywhere, Category = "Targeting|Aim", meta = (ClampMin = "0", ClampMax = "90"))
	float AimAssistAngle = 12.f;

private:
	/** Hostile, living, visible combatants within Range of the owner. */
	void GatherCandidates(float Range, TArray<AActor*>& OutCandidates) const;

	/** Lowest score wins: mostly angle from Direction, partly distance. */
	AActor* FindBest(const FVector& Origin, const FVector& Direction, float Range, float MaxAngle, bool bRequireLineOfSight) const;

	bool HasLineOfSight(const FVector& From, const AActor* Target) const;
	void SetLockedTarget(AActor* NewTarget);

	TWeakObjectPtr<AActor> LockedTarget;
	TWeakObjectPtr<AActor> SoftTarget;
};
