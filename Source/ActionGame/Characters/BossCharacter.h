#pragma once

#include "CoreMinimal.h"
#include "Characters/EnemyCharacter.h"
#include "BossCharacter.generated.h"

class UAnimMontage;

/** A stage of the boss fight, entered when health drops to HealthThreshold. */
USTRUCT(BlueprintType)
struct FBossPhase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	FName Name = TEXT("Phase");

	/** Phase starts when health percent drops to or below this (1 = from the start). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase", meta = (ClampMin = "0", ClampMax = "1"))
	float HealthThreshold = 1.f;

	/** Attack set for this phase. Empty = keep the base Attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase")
	TArray<FEnemyAttack> Attacks;

	/** Movement speed multiplier: later phases get more aggressive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Phase", meta = (ClampMin = "0.1"))
	float SpeedMultiplier = 1.f;

	/** Roar / transformation played on entering the phase (boss is invulnerable meanwhile). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	TObjectPtr<UAnimMontage> TransitionMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition", meta = (ClampMin = "0"))
	float TransitionDuration = 2.f;

	/** Shockwave that pushes the player away at the start of the transition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition", meta = (ClampMin = "0"))
	float ShockwaveRadius = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	FCombatDamageSpec ShockwaveDamage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	FImpactEffect TransitionEffect;

	/** Adds summoned when the phase begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Transition")
	TArray<TSubclassOf<AEnemyCharacter>> Reinforcements;
};

/**
 * Multi-phase boss. Design intent:
 *  - Armored: ordinary hits don't interrupt it, but they do chip poise. Breaking poise
 *    staggers it for a big punish window, so the player's goal is clear.
 *  - Phases escalate: new attacks, more speed, reinforcements. Each transition is a
 *    dramatic beat (invulnerable roar, shockwave, slow motion) that also gives the player
 *    a moment to breathe.
 *  - Every default attack works without animations (timed fallback), so the fight is
 *    playable immediately and can be dressed up later.
 */
UCLASS()
class ACTIONGAME_API ABossCharacter : public AEnemyCharacter
{
	GENERATED_BODY()

public:
	ABossCharacter(const FObjectInitializer& ObjectInitializer);

	virtual const TArray<FEnemyAttack>& GetAttacks() const override;
	virtual bool UsesAttackTokens() const override { return false; }
	virtual bool IsExecutingAttack() const override;
	virtual void OnEngaged() override;

	UFUNCTION(BlueprintPure, Category = "Boss")
	int32 GetCurrentPhase() const { return CurrentPhase; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	bool IsInPhaseTransition() const { return bInTransition; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Boss")
	FText BossName;

	/** Ordered by descending HealthThreshold. Phase 0 is active from the start. */
	UPROPERTY(EditAnywhere, Category = "Boss")
	TArray<FBossPhase> Phases;

private:
	UFUNCTION()
	void HandleHealthChanged(UHealthComponent* Health, float NewHealth, float Delta);

	void EnterPhase(int32 PhaseIndex);
	void EndTransition();
	void SpawnReinforcements(const FBossPhase& Phase);
	void BuildDefaultPhases();

	int32 CurrentPhase = 0;
	bool bInTransition = false;
	float BaseStrafeSpeed = 0.f;
	float BaseChaseSpeed = 0.f;
	FTimerHandle TransitionTimer;
};
