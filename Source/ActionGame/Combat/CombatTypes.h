#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CombatTypes.generated.h"

/** Which side a combatant fights for. Neutral actors (props, barrels) can be damaged by anyone. */
UENUM(BlueprintType)
enum class ECombatTeam : uint8
{
	Neutral,
	Player,
	Enemy
};

/**
 * How strongly a hit wants to interrupt its victim, ordered from weakest to strongest.
 * The victim's HitReactionComponent compares this against its own resistance, so the same
 * attack can make a grunt flinch while a boss only twitches.
 */
UENUM(BlueprintType)
enum class EHitReaction : uint8
{
	/** Physical twitch only, never interrupts. Ideal for bullets. */
	None,
	/** Short interrupt. Light sword hits. */
	Flinch,
	/** Longer interrupt with a bigger animation. Heavy hits and finishers. */
	Heavy,
	/** Pushes the victim away along the ground. */
	Knockback,
	/** Pops the victim into the air (enables air juggles). */
	Launch,
	/** Knocks the victim to the ground; they must get up. */
	Knockdown
};

/**
 * Designer-authored description of what a hit does. Lives on attacks, projectiles, props, etc.
 * Keeping every tunable in one struct makes attacks comparable and easy to balance in a spreadsheet.
 */
USTRUCT(BlueprintType)
struct FCombatDamageSpec
{
	GENERATED_BODY()

	/** Health removed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0"))
	float Damage = 10.f;

	/** Poise removed. When poise hits zero the victim is staggered (a long punish window). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0"))
	float PoiseDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	EHitReaction Reaction = EHitReaction::Flinch;

	/** Horizontal push applied to the victim (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0"))
	float KnockbackStrength = 0.f;

	/** Vertical pop applied to the victim (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (ClampMin = "0"))
	float LaunchStrength = 0.f;

	/** Freeze-frame duration on attacker and victim, in real seconds. The core of "impact". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0", ClampMax = "0.5"))
	float HitStop = 0.06f;

	/** Camera trauma added when the local player is involved (0..1, shake grows with trauma squared). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0", ClampMax = "1"))
	float CameraTrauma = 0.15f;

	/** Strength of the procedural (physics) flinch on the struck bone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feel", meta = (ClampMin = "0"))
	float PhysicalImpulse = 400.f;

	/** Selects impact VFX/SFX in the feedback config (Damage.Blade, Damage.Bullet, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage", meta = (Categories = "Damage"))
	FGameplayTag DamageType;
};

/** A single resolved hit at runtime: the spec plus where and by whom it happened. */
USTRUCT(BlueprintType)
struct FCombatHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	FCombatDamageSpec Spec;

	/** The character responsible (for team checks, rewards and hit markers). */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	TObjectPtr<AActor> Instigator = nullptr;

	/** The thing that physically made contact (a sword owner, a projectile, a thrown crate). */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	TObjectPtr<AActor> DamageCauser = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	TObjectPtr<AActor> Target = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	FVector ImpactPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	FVector ImpactNormal = FVector::UpVector;

	/** Direction the force travels (attacker -> victim, or swing direction). Normalized. */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	FVector HitDirection = FVector::ForwardVector;

	/** Bone that was struck, if known. Used for physical reactions and critical hits. */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	FName BoneName = NAME_None;

	/** Final multiplier applied on top of Spec.Damage (critical hits, charge levels...). */
	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	float DamageMultiplier = 1.f;

	UPROPERTY(BlueprintReadWrite, Category = "Hit")
	bool bCritical = false;
};

/** What actually happened after a hit was processed by the victim. */
USTRUCT(BlueprintType)
struct FCombatDamageResult
{
	GENERATED_BODY()

	/** False when the hit was ignored (dead target, same team, no health component). */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bApplied = false;

	/** True when the target was invulnerable (e.g. mid-dodge). */
	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bNegated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	float DamageDealt = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bKilled = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result")
	bool bPoiseBroken = false;
};
