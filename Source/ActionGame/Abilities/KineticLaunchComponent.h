#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatTypes.h"
#include "KineticLaunchComponent.generated.h"

class AActionCharacterBase;
class UAnimMontage;

/**
 * "Throw yourself": the character is hurled telekinetically toward the aimed enemy (or the
 * crosshair point), homing on the target, and crashes into it with a heavy kinetic hit.
 * On impact the character bounces up and away, which chains naturally into levitation,
 * an air combo or a ground slam. Invulnerable in flight, so it doubles as an aggressive escape.
 */
UCLASS(ClassGroup = (Abilities), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UKineticLaunchComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKineticLaunchComponent();

	/**
	 * @param Target        Enemy to crash into (may be null).
	 * @param AimDirection  Used when there is no target: fly this way for MaxRange.
	 */
	UFUNCTION(BlueprintCallable, Category = "Kinetic Launch")
	bool TryLaunch(AActor* Target, FVector AimDirection);

	UFUNCTION(BlueprintPure, Category = "Kinetic Launch")
	bool IsLaunching() const { return bLaunching; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Kinetic Launch", meta = (ClampMin = "0"))
	float EnergyCost = 20.f;

	UPROPERTY(EditAnywhere, Category = "Kinetic Launch", meta = (ClampMin = "0"))
	float LaunchSpeed = 3400.f;

	UPROPERTY(EditAnywhere, Category = "Kinetic Launch", meta = (ClampMin = "0"))
	float MaxRange = 1800.f;

	/** Distance from the target's center at which the impact happens. */
	UPROPERTY(EditAnywhere, Category = "Kinetic Launch", meta = (ClampMin = "0"))
	float ImpactDistance = 160.f;

	/** Speed kept when a launch ends without hitting anything (momentum carry). */
	UPROPERTY(EditAnywhere, Category = "Kinetic Launch", meta = (ClampMin = "0", ClampMax = "1"))
	float MomentumKept = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Kinetic Launch")
	FCombatDamageSpec ImpactDamage;

	/** Horizontal speed pushing the character back off the target after impact. */
	UPROPERTY(EditAnywhere, Category = "Kinetic Launch", meta = (ClampMin = "0"))
	float BounceBackSpeed = 450.f;

	UPROPERTY(EditAnywhere, Category = "Kinetic Launch", meta = (ClampMin = "0"))
	float BounceUpSpeed = 750.f;

	UPROPERTY(EditAnywhere, Category = "Kinetic Launch")
	float LaunchFOVKick = 12.f;

	UPROPERTY(EditAnywhere, Category = "Kinetic Launch")
	TObjectPtr<UAnimMontage> LaunchMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Kinetic Launch")
	TObjectPtr<UAnimMontage> ImpactMontage = nullptr;

private:
	FVector GetDestination() const;
	void Impact();
	void EndLaunch(bool bKeepMomentum);

	UPROPERTY()
	TObjectPtr<AActionCharacterBase> OwnerCharacter;

	TWeakObjectPtr<AActor> LaunchTarget;
	FVector FixedDestination = FVector::ZeroVector;
	FVector LastLocation = FVector::ZeroVector;
	float Elapsed = 0.f;
	float MaxDuration = 0.f;
	bool bLaunching = false;
};
