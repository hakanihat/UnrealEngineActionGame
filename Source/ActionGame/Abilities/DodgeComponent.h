#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatTypes.h"
#include "Combat/HitReactionComponent.h"
#include "DodgeComponent.generated.h"

class AActionCharacterBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPerfectDodge);

/**
 * Fast evade with invulnerability frames.
 *  - Code-driven burst (ease-out speed profile) so it feels snappy with or without animations.
 *  - Cancels melee recovery, so players can always react (responsiveness > commitment for a hero).
 *  - One air dash per jump.
 *  - Perfect dodge: getting "hit" during the first frames of i-frames triggers slow motion and
 *    refunds energy. Rewards reading enemy telegraphs instead of mashing dodge.
 *  - Attacking right after a dodge performs the dash attack (see IsInDashAttackWindow).
 */
UCLASS(ClassGroup = (Abilities), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UDodgeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDodgeComponent();

	/** @param WorldDirection Desired direction; zero performs a backstep. */
	UFUNCTION(BlueprintCallable, Category = "Dodge")
	bool TryDodge(FVector WorldDirection);

	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsDodging() const { return bIsDodging; }

	UFUNCTION(BlueprintPure, Category = "Dodge")
	bool IsInDashAttackWindow() const;

	void ResetAirDodge() { bAirDodgeUsed = false; }

	UPROPERTY(BlueprintAssignable, Category = "Dodge")
	FOnPerfectDodge OnPerfectDodge;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = "0"))
	float DodgeDistance = 600.f;

	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = "0.05"))
	float DodgeDuration = 0.32f;

	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = "0"))
	float InvulnerabilityTime = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = "0"))
	float Cooldown = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Dodge")
	bool bAllowAirDodge = true;

	/** Seconds after the dodge ends in which an attack becomes the dash attack. */
	UPROPERTY(EditAnywhere, Category = "Dodge", meta = (ClampMin = "0"))
	float DashAttackWindow = 0.3f;

	/** Directional dodge animations. Front is used when the character turns into the dodge. */
	UPROPERTY(EditAnywhere, Category = "Dodge|Animation")
	FDirectionalMontages DodgeMontages;

	/** Perfect dodge = negated hit within this many seconds of starting the dodge. */
	UPROPERTY(EditAnywhere, Category = "Dodge|Perfect", meta = (ClampMin = "0"))
	float PerfectDodgeWindow = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Dodge|Perfect", meta = (ClampMin = "0.05", ClampMax = "1"))
	float PerfectDodgeTimeScale = 0.25f;

	/** Real-time seconds of slow motion. */
	UPROPERTY(EditAnywhere, Category = "Dodge|Perfect", meta = (ClampMin = "0"))
	float PerfectDodgeSlowMoDuration = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Dodge|Perfect", meta = (ClampMin = "0"))
	float PerfectDodgeEnergyReward = 25.f;

private:
	UFUNCTION()
	void HandleDamageNegated(const FCombatHit& Hit);

	void EndInvulnerability();
	void EndDodge();

	UPROPERTY()
	TObjectPtr<AActionCharacterBase> OwnerCharacter;

	FVector DodgeDirection = FVector::ZeroVector;
	float DodgeElapsed = 0.f;
	float LastDodgeEndTime = -1000.f;
	bool bIsDodging = false;
	bool bInvulnerable = false;
	bool bAirDodgeUsed = false;
	bool bPerfectDodgeTriggered = false;
};
