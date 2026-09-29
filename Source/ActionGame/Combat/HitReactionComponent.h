#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "Combat/CombatTypes.h"
#include "HitReactionComponent.generated.h"

class ACharacter;
class UAnimMontage;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EHitDirection : uint8
{
	Front,
	Back,
	Left,
	Right
};

/** One montage per side the hit came from. Missing entries fall back to Front. */
USTRUCT(BlueprintType)
struct FDirectionalMontages
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	TObjectPtr<UAnimMontage> Front = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	TObjectPtr<UAnimMontage> Back = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	TObjectPtr<UAnimMontage> Left = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	TObjectPtr<UAnimMontage> Right = nullptr;

	UAnimMontage* Pick(EHitDirection Direction) const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHitReaction, EHitReaction, Reaction, bool, bStaggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHitRecovered);

/**
 * Makes characters react to damage in a readable, satisfying way. Layers, from subtle to strong:
 *  1. Hit flash       - material parameter pulse on every hit (instant confirmation).
 *  2. Physical flinch - the struck bone chain is briefly simulated and kicked by the hit impulse,
 *                       then blended back to animation. Every hit looks unique and location-aware,
 *                       even with no hit animations authored yet.
 *  3. Interrupt       - directional montage + stun state, gated by the victim's resistance so
 *                       grunts flinch from everything while heavies/bosses need big hits.
 *  4. Displacement    - knockback, launch and air-juggle lift.
 *  5. Stagger         - on poise break: long vulnerable window with bonus damage.
 *  6. Death           - full ragdoll that inherits momentum from the killing blow.
 * Physics kicks and ragdolls wait until hitstop ends so the impact reads as "freeze, then snap".
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UHitReactionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitReactionComponent();

	/** True while in hit stun, staggered or knocked down. */
	UFUNCTION(BlueprintPure, Category = "Hit Reaction")
	bool IsStunned() const { return ActiveStunTag.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Hit Reaction")
	static EHitDirection ComputeHitDirection(const AActor* Victim, const FVector& HitDirection);

	void SetPostHitInvulnerability(float Seconds) { PostHitInvulnerability = FMath::Max(0.f, Seconds); }
	void SetCorpseLifeSpan(float Seconds) { CorpseLifeSpan = FMath::Max(0.f, Seconds); }
	void SetInterruptThreshold(EHitReaction Threshold) { InterruptThreshold = Threshold; }
	void SetKnockbackResistance(float Resistance) { KnockbackResistance = FMath::Clamp(Resistance, 0.f, 1.f); }

	/** Fired whenever the owner is interrupted (not for twitch-only hits). */
	UPROPERTY(BlueprintAssignable, Category = "Hit Reaction")
	FOnHitReaction OnHitReaction;

	/** Fired when a stun/stagger/knockdown ends and the owner can act again. */
	UPROPERTY(BlueprintAssignable, Category = "Hit Reaction")
	FOnHitRecovered OnRecovered;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// --- Interrupt rules ---

	/** Hits weaker than this only twitch the owner. Grunts: Flinch. Heavies: Heavy. Bosses: Knockdown. */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Rules")
	EHitReaction InterruptThreshold = EHitReaction::Flinch;

	/** 0 = full knockback, 1 = immovable. */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Rules", meta = (ClampMin = "0", ClampMax = "1"))
	float KnockbackResistance = 0.f;

	/** Upward velocity given when hit while airborne, keeping juggled enemies afloat. */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Rules", meta = (ClampMin = "0"))
	float AirJuggleLift = 380.f;

	/** Invulnerability after being interrupted. Prevents unfair stun-locks (use on the player). */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Rules", meta = (ClampMin = "0"))
	float PostHitInvulnerability = 0.f;

	// --- Animation ---

	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Montages")
	FDirectionalMontages LightHitMontages;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Montages")
	FDirectionalMontages HeavyHitMontages;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Montages")
	TObjectPtr<UAnimMontage> StaggerMontage = nullptr;

	/** Played for Launch and Knockdown reactions (fall, lie, get up). */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Montages")
	TObjectPtr<UAnimMontage> KnockdownMontage = nullptr;

	/** Stun durations. When a montage is set its length is used instead. */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Timing", meta = (ClampMin = "0"))
	float FlinchStunTime = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Timing", meta = (ClampMin = "0"))
	float HeavyStunTime = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Timing", meta = (ClampMin = "0"))
	float StaggerTime = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Timing", meta = (ClampMin = "0"))
	float KnockdownTime = 1.6f;

	// --- Physical (procedural) reactions ---

	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Physics")
	bool bEnablePhysicalHits = true;

	/**
	 * Roots of independently simulated bone chains. A hit simulates the chain containing the
	 * struck bone (e.g. a head shot simulates everything below spine_01). Defaults match UE5 Manny.
	 */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Physics")
	TArray<FName> PhysicalReactionRoots = { TEXT("spine_01"), TEXT("thigh_l"), TEXT("thigh_r") };

	/** Peak physics blend weight right after a hit (1 = full ragdoll on that chain). */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Physics", meta = (ClampMin = "0", ClampMax = "1"))
	float PhysicalBlendWeight = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Physics", meta = (ClampMin = "0.05"))
	float PhysicalBlendOutTime = 0.45f;

	/** Bone below which physical animation motors are applied (pulls simulated bones back to the pose). */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Physics")
	FName PhysicalAnimationRootBone = TEXT("pelvis");

	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Physics")
	FPhysicalAnimationData PhysicalAnimationProfile;

	// --- Hit flash ---

	/** Scalar material parameter pulsed on hit. Add it to your character material (0 = off, 1 = full flash). */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Flash")
	FName FlashParameterName = TEXT("HitFlash");

	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Flash", meta = (ClampMin = "0.01"))
	float FlashDuration = 0.15f;

	// --- Death ---

	/** Extra ragdoll velocity along the killing blow's direction (cm/s). */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Death", meta = (ClampMin = "0"))
	float DeathKickSpeed = 450.f;

	/** Seconds the corpse stays before being removed (0 = keep forever). */
	UPROPERTY(EditAnywhere, Category = "Hit Reaction|Death", meta = (ClampMin = "0"))
	float CorpseLifeSpan = 10.f;

private:
	struct FActivePhysicalHit
	{
		FName RootBone;
		float Weight = 0.f;
	};

	struct FPendingImpulse
	{
		FName Bone;
		FVector Impulse;
	};

	UFUNCTION()
	void HandleDamageTaken(const FCombatHit& Hit, const FCombatDamageResult& Result);

	UFUNCTION()
	void HandleDeath(AActor* DeadActor, const FCombatHit& KillingHit);

	EHitReaction ResolveReaction(const FCombatHit& Hit, const FCombatDamageResult& Result) const;
	void ApplyDisplacement(const FCombatHit& Hit, EHitReaction Reaction);
	void PlayReaction(const FCombatHit& Hit, EHitReaction Reaction, bool bStagger);
	void BeginStun(const FGameplayTag& StunTag, float Duration);
	void EndStun();
	void GrantMercyInvulnerability();

	void QueuePhysicalHit(const FCombatHit& Hit);
	void ApplyPhysicalHit(const FPendingImpulse& Pending);
	void TickPhysicalHits(float DeltaTime);
	void StartRagdoll();
	FName FindReactionRoot(FName Bone) const;
	FName FindBoneWithBody(FName Bone) const;

	void RefreshTickEnabled();
	bool IsOwnerFrozen() const;

	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimation;

	TArray<FActivePhysicalHit> ActivePhysicalHits;
	TArray<FPendingImpulse> PendingImpulses;

	FGameplayTag ActiveStunTag;
	FTimerHandle StunTimer;
	FTimerHandle MercyTimer;
	bool bHasMercyInvulnerability = false;

	float FlashAlpha = 0.f;

	bool bRagdollPending = false;
	FVector RagdollVelocity = FVector::ZeroVector;
	FName RagdollKickBone = NAME_None;
};
