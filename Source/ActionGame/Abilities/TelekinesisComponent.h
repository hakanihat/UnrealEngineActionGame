#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TelekinesisComponent.generated.h"

class AActionCharacterBase;
class ATelekineticProp;
class UAnimMontage;
class UEnergyComponent;

UENUM(BlueprintType)
enum class ETelekinesisState : uint8
{
	None,
	/** Object is flying toward the hold point. */
	Pulling,
	/** Object is orbiting the hold point, ready to throw. */
	Holding
};

/**
 * Control's "Launch": grab the object nearest the crosshair, hold it over the shoulder, throw
 * it at the aimed enemy.
 *  - Tap to grab-and-throw in one motion (throw fires as soon as the object arrives).
 *  - Hold to keep the object floating, release to throw.
 *  - If nothing is in range, a chunk of debris is ripped from the ground, so the power
 *    is never "dead" in empty arenas.
 *  - Throws are aim-assisted toward the target with velocity lead.
 */
UCLASS(ClassGroup = (Abilities), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UTelekinesisComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTelekinesisComponent();

	UFUNCTION(BlueprintCallable, Category = "Telekinesis")
	bool BeginGrab();

	/** Throws the held object now, or as soon as it arrives. AimTarget may be null (crosshair throw). */
	UFUNCTION(BlueprintCallable, Category = "Telekinesis")
	void RequestThrow(AActor* AimTarget);

	/** Drops whatever is held without throwing it. */
	UFUNCTION(BlueprintCallable, Category = "Telekinesis")
	void Release();

	UFUNCTION(BlueprintPure, Category = "Telekinesis")
	ETelekinesisState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Telekinesis")
	bool IsBusy() const { return State != ETelekinesisState::None; }

	/** Object that would be grabbed right now (HUD highlight). */
	UFUNCTION(BlueprintPure, Category = "Telekinesis")
	AActor* GetHighlightedTarget() const { return HighlightedTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Telekinesis")
	AActor* GetHeldObject() const { return HeldObject.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Telekinesis", meta = (ClampMin = "0"))
	float GrabRange = 2500.f;

	/** Max angle from the crosshair for an object to be selected. */
	UPROPERTY(EditAnywhere, Category = "Telekinesis", meta = (ClampMin = "0", ClampMax = "90"))
	float GrabAngle = 30.f;

	/** Hold point relative to the camera yaw: X forward, Y right, Z up. Over-the-shoulder by default. */
	UPROPERTY(EditAnywhere, Category = "Telekinesis")
	FVector HoldOffset = FVector(60.f, 120.f, 170.f);

	/** Gentle bob amplitude of the held object (cm). */
	UPROPERTY(EditAnywhere, Category = "Telekinesis", meta = (ClampMin = "0"))
	float HoldBobAmplitude = 8.f;

	UPROPERTY(EditAnywhere, Category = "Telekinesis", meta = (ClampMin = "0"))
	float ArriveDistance = 100.f;

	UPROPERTY(EditAnywhere, Category = "Telekinesis", meta = (ClampMin = "0"))
	float ThrowSpeed = 4800.f;

	UPROPERTY(EditAnywhere, Category = "Telekinesis", meta = (ClampMin = "0"))
	float ThrowEnergyCost = 20.f;

	/** Pulling longer than this aborts (object stuck behind geometry). */
	UPROPERTY(EditAnywhere, Category = "Telekinesis", meta = (ClampMin = "0.1"))
	float MaxPullTime = 1.5f;

	/** Spawned and grabbed when there is nothing to pick up. Leave empty to disable. */
	UPROPERTY(EditAnywhere, Category = "Telekinesis|Debris")
	TSubclassOf<ATelekineticProp> DebrisClass;

	UPROPERTY(EditAnywhere, Category = "Telekinesis|Debris", meta = (ClampMin = "0"))
	float DebrisSpawnDistance = 250.f;

	UPROPERTY(EditAnywhere, Category = "Telekinesis|Animation")
	TObjectPtr<UAnimMontage> GrabMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Telekinesis|Animation")
	TObjectPtr<UAnimMontage> ThrowMontage = nullptr;

private:
	AActor* FindBestTarget() const;
	AActor* SpawnDebris() const;
	FVector GetHoldLocation() const;
	FVector ComputeThrowVelocity(const FVector& From, AActor* AimTarget) const;
	void Throw();
	void Reset();

	UPROPERTY()
	TObjectPtr<AActionCharacterBase> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UEnergyComponent> Energy;

	ETelekinesisState State = ETelekinesisState::None;
	TWeakObjectPtr<AActor> HeldObject;
	TWeakObjectPtr<AActor> HighlightedTarget;
	TWeakObjectPtr<AActor> QueuedAimTarget;
	bool bThrowQueued = false;
	float StateTime = 0.f;
	float HighlightTimer = 0.f;
};
