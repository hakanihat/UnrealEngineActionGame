#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CombatFeedbackConfig.generated.h"

class UNiagaraSystem;
class USoundBase;

/** A VFX + SFX pair played at an impact point. */
USTRUCT(BlueprintType)
struct FImpactEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<UNiagaraSystem> Niagara = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impact", meta = (ClampMin = "0.1"))
	float Scale = 1.f;

	bool IsSet() const { return Niagara != nullptr || Sound != nullptr; }
};

/**
 * Central "game feel" tuning asset. Every hit in the game routes through the
 * CombatFeedbackSubsystem, which reads this asset, so impact feel stays consistent
 * and can be tuned by a designer without touching code.
 */
UCLASS(BlueprintType)
class ACTIONGAME_API UCombatFeedbackConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Impact effects per damage type (Damage.Blade, Damage.Bullet, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impacts", meta = (Categories = "Damage"))
	TMap<FGameplayTag, FImpactEffect> ImpactEffects;

	/** Used when a damage type has no entry above. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impacts")
	FImpactEffect DefaultImpact;

	/** Layered on top of the normal impact for critical hits (headshots). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impacts")
	FImpactEffect CriticalImpact;

	/** Layered on top when the hit breaks the victim's poise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impacts")
	FImpactEffect PoiseBreakImpact;

	/** Layered on top for killing blows. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Impacts")
	FImpactEffect KillImpact;

	/** Time scale applied to actors frozen by hitstop. Near zero reads as a crisp "freeze frame". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Stop", meta = (ClampMin = "0", ClampMax = "1"))
	float HitStopTimeScale = 0.02f;

	/** Extra hitstop added on killing blows and poise breaks so they feel heavier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Stop", meta = (ClampMin = "0"))
	float BigHitStopBonus = 0.05f;

	/** Hard cap so stacked bonuses never make the game feel laggy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit Stop", meta = (ClampMin = "0"))
	float MaxHitStop = 0.2f;

	/** Extra camera trauma for killing blows and poise breaks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (ClampMin = "0", ClampMax = "1"))
	float BigHitTraumaBonus = 0.15f;

	/** Random pitch variance on impact sounds. Avoids the "machine gun" effect of identical sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0", ClampMax = "0.5"))
	float SoundPitchVariance = 0.08f;

	const FImpactEffect& GetImpactEffect(const FGameplayTag& DamageType) const;
};
