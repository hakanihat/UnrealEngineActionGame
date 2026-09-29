#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatTypes.h"
#include "HealthComponent.generated.h"

class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHealthValueChanged, UHealthComponent*, HealthComponent, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamageTaken, const FCombatHit&, Hit, const FCombatDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCombatantDeath, AActor*, DeadActor, const FCombatHit&, KillingHit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatHitEvent, const FCombatHit&, Hit);

/**
 * Health + poise for anything that can be damaged (characters, explosive props...).
 *
 * Poise is a hidden "stagger meter": every hit chips it, and when it breaks the victim is
 * staggered for a long punish window. This rewards aggression and focused fire, and lets
 * bosses shrug off light hits while still being breakable (a staple of modern action games).
 *
 * The component only does bookkeeping and broadcasts events; reactions, UI and rewards
 * subscribe to those events, so each concern lives in its own class.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	/** Processes a hit. Prefer UCombatLibrary::ApplyDamage, which also triggers feedback. */
	FCombatDamageResult ApplyDamage(const FCombatHit& Hit);

	UFUNCTION(BlueprintCallable, Category = "Health")
	void Heal(float Amount);

	/** Kills instantly, ignoring invulnerability (kill volumes, scripted events). */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void Kill(AActor* Instigator = nullptr);

	/** Refills poise; called when a stagger ends. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	void ResetPoise();

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsAlive() const { return Health > 0.f; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthPercent() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetPoisePercent() const { return MaxPoise > 0.f ? Poise / MaxPoise : 0.f; }

	/** World time of the last damage taken (HUD uses it to show enemy health bars briefly). */
	float GetLastDamageTime() const { return LastDamageTime; }

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthValueChanged OnHealthChanged;

	/** Fired for every applied hit, with the resolved result. Main hook for reactions. */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnDamageTaken OnDamageTaken;

	/** Fired when a hit was ignored because the owner was invulnerable (used for perfect dodges). */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnCombatHitEvent OnDamageNegated;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnCombatHitEvent OnPoiseBroken;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnCombatantDeath OnDeath;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poise", meta = (ClampMin = "1"))
	float MaxPoise = 50.f;

	/** Seconds without poise damage before poise starts regenerating. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poise", meta = (ClampMin = "0"))
	float PoiseRegenDelay = 2.f;

	/** Poise regenerated per second after the delay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poise", meta = (ClampMin = "0"))
	float PoiseRegenRate = 25.f;

	/** Damage multiplier while staggered: the payoff for breaking poise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poise", meta = (ClampMin = "1"))
	float StaggeredDamageMultiplier = 1.5f;

private:
	bool IsOwnerTagged(const FGameplayTag& Tag) const;
	void SetHealth(float NewHealth);
	void HandleDeath(const FCombatHit& KillingHit);

	UPROPERTY(VisibleInstanceOnly, Category = "Health")
	float Health = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "Poise")
	float Poise = 0.f;

	float LastPoiseDamageTime = -1000.f;
	float LastDamageTime = -1000.f;
};
