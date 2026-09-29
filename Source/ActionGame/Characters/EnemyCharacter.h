#pragma once

#include "CoreMinimal.h"
#include "Characters/ActionCharacterBase.h"
#include "Combat/CombatFeedbackConfig.h"
#include "Combat/MeleeTypes.h"
#include "EnemyCharacter.generated.h"

class AActionProjectile;
class AHealthOrb;
class USoundBase;

/**
 * One enemy attack. The montage drives timing through notifies:
 *  - Combat Event "Event.Telegraph"      -> glow + sound warning (place ~0.4s before impact)
 *  - Melee Hit Window                    -> melee damage using Attack.Damage
 *  - Combat Event "Event.FireProjectile" -> spawns the projectiles below
 *  - Combat Event "Event.AreaImpact"     -> area damage below
 *  - Combat Events "Event.ChargeStart/End" -> dash toward the target
 * With no montage, a timed fallback (telegraph -> wind-up -> strike) is used so enemies
 * are testable before animations exist.
 */
USTRUCT(BlueprintType)
struct FEnemyAttack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FName Name = TEXT("Attack");

	/** Montage, melee damage and magnetism. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FMeleeAttack Attack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0"))
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0"))
	float MaxRange = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0"))
	float Cooldown = 2.f;

	/** Relative chance of being picked among valid attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0"))
	float Weight = 1.f;

	/** Cannot be interrupted by normal hits (only poise breaks). Use for big, telegraphed attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	bool bSuperArmor = false;

	// --- Projectiles ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<AActionProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "1"))
	int32 ProjectileCount = 1;

	/** Total fan angle in degrees for multi-projectile volleys. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0"))
	float ProjectileSpread = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
	FName ProjectileSocket = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
	bool bHomingProjectiles = false;

	// --- Area ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area", meta = (ClampMin = "0"))
	float AreaRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	FCombatDamageSpec AreaDamage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Area")
	FImpactEffect AreaEffect;

	// --- Charge ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge", meta = (ClampMin = "0"))
	float ChargeSpeed = 0.f;

	// --- Fallback timing (only used without a montage) ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fallback", meta = (ClampMin = "0"))
	float FallbackWindup = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fallback", meta = (ClampMin = "0"))
	float FallbackRecovery = 0.6f;
};

/**
 * Data-driven enemy. The same class covers melee grunts, ranged shooters and heavies:
 * give each Blueprint subclass different attacks, ranges and hit resistance.
 * Decision making lives in AEnemyAIController; this class only knows how to perform attacks.
 */
UCLASS()
class ACTIONGAME_API AEnemyCharacter : public AActionCharacterBase
{
	GENERATED_BODY()

public:
	AEnemyCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;
	virtual void HandleCombatEvent(FGameplayTag EventTag) override;

	/** The attacks currently available (bosses swap these per phase). */
	virtual const TArray<FEnemyAttack>& GetAttacks() const { return Attacks; }

	/** Weighted random pick among off-cooldown attacks. INDEX_NONE if none are ready. */
	int32 ChooseAttack() const;

	bool IsAttackInRange(int32 AttackIndex, float Distance) const;
	bool ExecuteAttack(int32 AttackIndex, AActor* Target);
	virtual bool IsExecutingAttack() const;

	/** Whether this enemy must hold an attack token to attack (bosses don't). */
	virtual bool UsesAttackTokens() const { return true; }

	/** Called by the AI the first time it starts fighting. */
	virtual void OnEngaged() {}

	float GetAggroRange() const { return AggroRange; }
	float GetPreferredRange() const { return PreferredRange; }
	float GetStrafeSpeed() const { return StrafeSpeed; }
	float GetChaseSpeed() const { return ChaseSpeed; }
	float GetRecoveryTime() const { return FMath::FRandRange(RecoveryTimeMin, RecoveryTimeMax); }
	bool IsRanged() const { return PreferredRange > 700.f; }

protected:
	virtual void BeginPlay() override;
	virtual void HandleDeath(AActor* DeadActor, const FCombatHit& KillingHit) override;
	virtual void HandleInterrupted(EHitReaction Reaction, bool bStaggered) override;

	void PlayTelegraph();
	void FireProjectiles(const FEnemyAttack& Attack);
	void ApplyAreaImpact(const FEnemyAttack& Attack);
	void FinishAttack();

	UFUNCTION()
	void HandleMeleeAttackEnded(bool bInterrupted);

	/** Forget cooldowns (e.g. when a boss swaps its attack list between phases). */
	void ResetAttackCooldowns() { AttackReadyTimes.Reset(); }

	UPROPERTY(EditAnywhere, Category = "Enemy|Attacks")
	TArray<FEnemyAttack> Attacks;

	UPROPERTY(EditAnywhere, Category = "Enemy|AI", meta = (ClampMin = "0"))
	float AggroRange = 2500.f;

	/** Distance this enemy likes to keep while waiting to attack (circle radius). */
	UPROPERTY(EditAnywhere, Category = "Enemy|AI", meta = (ClampMin = "0"))
	float PreferredRange = 450.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|AI", meta = (ClampMin = "0"))
	float StrafeSpeed = 260.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|AI", meta = (ClampMin = "0"))
	float ChaseSpeed = 520.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|AI", meta = (ClampMin = "0"))
	float RecoveryTimeMin = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Enemy|AI", meta = (ClampMin = "0"))
	float RecoveryTimeMax = 1.1f;

	// --- Telegraph ---

	/** Scalar material parameter pulsed as an attack warning (add it to the enemy material). */
	UPROPERTY(EditAnywhere, Category = "Enemy|Telegraph")
	FName TelegraphParameterName = TEXT("TelegraphGlow");

	UPROPERTY(EditAnywhere, Category = "Enemy|Telegraph", meta = (ClampMin = "0.05"))
	float TelegraphDuration = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Telegraph")
	FImpactEffect TelegraphEffect;

	// --- Rewards ---

	/** Control-style health drops: aggression is rewarded with healing. */
	UPROPERTY(EditAnywhere, Category = "Enemy|Rewards")
	TSubclassOf<AHealthOrb> HealthOrbClass;

	UPROPERTY(EditAnywhere, Category = "Enemy|Rewards", meta = (ClampMin = "0"))
	int32 HealthOrbCount = 2;

private:
	void BeginFallbackAttack();
	void PerformFallbackStrike();
	void EndFallbackAttack();
	const FEnemyAttack* GetCurrentAttack() const;

	int32 CurrentAttackIndex = INDEX_NONE;
	TWeakObjectPtr<AActor> CurrentTarget;
	TMap<int32, float> AttackReadyTimes;

	bool bFallbackAttackActive = false;
	bool bSuperArmorApplied = false;
	bool bCharging = false;
	FVector ChargeDirection = FVector::ZeroVector;
	float TelegraphAlpha = 0.f;
	FTimerHandle FallbackTimer;
};
