#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Combat/CombatTypes.h"
#include "MeleeTypes.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EMeleeAttackKind : uint8
{
	Light,
	Heavy,
	Air,
	Dash
};

/**
 * One attack. Timing (active frames, combo windows) is authored in the montage with
 * AnimNotifyState_MeleeHitWindow and AnimNotifyState_ComboWindow, so animators own the feel
 * and code never hardcodes frame numbers.
 */
USTRUCT(BlueprintType)
struct FMeleeAttack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack", meta = (ClampMin = "0.1"))
	float PlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	FCombatDamageSpec Damage;

	/** Targets closer than this pull the attacker toward them ("attack magnetism"). 0 disables. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetism", meta = (ClampMin = "0"))
	float MagnetismRange = 400.f;

	/** Distance from the target where magnetism stops (roughly the weapon's reach). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetism", meta = (ClampMin = "0"))
	float MagnetismStopDistance = 130.f;

	/** Gravity scale while this attack plays in the air. Low values let air combos "hang". <0 = unchanged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air")
	float AirGravityScale = -1.f;
};

/** A character's complete melee moveset. Create one data asset per character type. */
UCLASS(BlueprintType)
class ACTIONGAME_API UMeleeMoveset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Played in order on repeated light presses; wraps after the last one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moveset")
	TArray<FMeleeAttack> LightCombo;

	/**
	 * Heavy attack branches: index N is used after N light hits in the current chain.
	 * [0] = standalone heavy, [1] = after light 1, ... Gives combo variety from two buttons.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moveset")
	TArray<FMeleeAttack> HeavyFinishers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moveset")
	TArray<FMeleeAttack> AirCombo;

	/** Performed when attacking right after a dodge: a fast gap closer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Moveset")
	FMeleeAttack DashAttack;

	/**
	 * How long an early button press is remembered. Buffering makes combos feel responsive
	 * without requiring frame-perfect timing; expiring stale presses avoids "ghost" attacks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (ClampMin = "0"))
	float InputBufferTime = 0.35f;
};
