#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatTypes.h"
#include "Combat/CombatFeedbackConfig.h"
#include "LevitationComponent.generated.h"

class AActionCharacterBase;
class UAnimMontage;
class UEnergyComponent;

/**
 * Control-style levitation and ground slam.
 *  - Levitate: hold jump in the air to hover. Gravity is removed and vertical speed eases to a
 *    slow drift, so the player can aim and shoot from above. Drains energy.
 *  - Ground slam: dive straight down; on landing, a shockwave damages and launches enemies
 *    (setting up air combos) and shoves props. Damage grows with dive height, rewarding
 *    players who gain altitude first.
 */
UCLASS(ClassGroup = (Abilities), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API ULevitationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULevitationComponent();

	UFUNCTION(BlueprintCallable, Category = "Levitation")
	bool StartLevitation();

	UFUNCTION(BlueprintCallable, Category = "Levitation")
	void StopLevitation();

	UFUNCTION(BlueprintCallable, Category = "Levitation")
	bool TryGroundSlam();

	UFUNCTION(BlueprintPure, Category = "Levitation")
	bool IsLevitating() const { return bLevitating; }

	UFUNCTION(BlueprintPure, Category = "Levitation")
	bool IsSlamming() const { return bSlamming; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Levitation ---

	/** Vertical speed the hover settles at (slightly negative = gentle sink). */
	UPROPERTY(EditAnywhere, Category = "Levitation")
	float HoverVerticalSpeed = -40.f;

	/** How quickly vertical speed eases to the hover speed. */
	UPROPERTY(EditAnywhere, Category = "Levitation", meta = (ClampMin = "0"))
	float HoverVerticalDamping = 6.f;

	/** Small upward pop when levitation starts: makes activation feel intentional. */
	UPROPERTY(EditAnywhere, Category = "Levitation", meta = (ClampMin = "0"))
	float StartLift = 180.f;

	UPROPERTY(EditAnywhere, Category = "Levitation", meta = (ClampMin = "0", ClampMax = "1"))
	float HoverAirControl = 0.9f;

	UPROPERTY(EditAnywhere, Category = "Levitation", meta = (ClampMin = "0"))
	float EnergyDrainPerSecond = 12.f;

	UPROPERTY(EditAnywhere, Category = "Levitation", meta = (ClampMin = "0"))
	float MinEnergyToStart = 10.f;

	// --- Ground slam ---

	UPROPERTY(EditAnywhere, Category = "Ground Slam", meta = (ClampMin = "0"))
	float SlamEnergyCost = 25.f;

	UPROPERTY(EditAnywhere, Category = "Ground Slam", meta = (ClampMin = "0"))
	float SlamDiveSpeed = 3200.f;

	UPROPERTY(EditAnywhere, Category = "Ground Slam", meta = (ClampMin = "0"))
	float SlamRadius = 480.f;

	/** Dive height (cm) at which the slam reaches its maximum damage bonus. */
	UPROPERTY(EditAnywhere, Category = "Ground Slam", meta = (ClampMin = "1"))
	float SlamHeightForMaxBonus = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Ground Slam", meta = (ClampMin = "0"))
	float SlamMaxHeightBonus = 1.f;

	UPROPERTY(EditAnywhere, Category = "Ground Slam")
	FCombatDamageSpec SlamDamage;

	UPROPERTY(EditAnywhere, Category = "Ground Slam")
	TObjectPtr<UAnimMontage> SlamDiveMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Ground Slam")
	TObjectPtr<UAnimMontage> SlamLandMontage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Ground Slam")
	FImpactEffect SlamImpactEffect;

private:
	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	void PerformSlamImpact();
	void EndSlam();
	void RefreshTickEnabled();

	UPROPERTY()
	TObjectPtr<AActionCharacterBase> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UEnergyComponent> Energy;

	bool bLevitating = false;
	bool bSlamming = false;
	float SavedGravityScale = 1.f;
	float SavedAirControl = 0.5f;
	float SlamStartZ = 0.f;
	float SlamElapsed = 0.f;
};
