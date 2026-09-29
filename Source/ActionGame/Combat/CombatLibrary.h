#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/CombatTypes.h"
#include "CombatLibrary.generated.h"

class UHealthComponent;

/**
 * The single entry point for dealing damage. Routing every hit through here guarantees that
 * team rules, instigator callbacks and hit feedback (hitstop, shake, VFX) are always applied
 * consistently, whether the source is a sword, a bullet, a thrown crate or a boss slam.
 */
UCLASS()
class ACTIONGAME_API UCombatLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Combat")
	static FCombatDamageResult ApplyDamage(const FCombatHit& Hit);

	/**
	 * Damages every valid target within Radius of Origin, with linear falloff down to MinFalloff.
	 * Also pushes loose physics objects so explosions and slams move the environment.
	 * @return number of targets damaged.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat", meta = (WorldContext = "WorldContextObject"))
	static int32 ApplyRadialDamage(const UObject* WorldContextObject, const FCombatDamageSpec& Spec, FVector Origin, float Radius,
		AActor* Instigator, AActor* DamageCauser, float MinFalloff = 0.35f);

	UFUNCTION(BlueprintPure, Category = "Combat")
	static ECombatTeam GetTeam(const AActor* Actor);

	/** True if Instigator is allowed to hurt Target (different teams, or target is neutral). */
	UFUNCTION(BlueprintPure, Category = "Combat")
	static bool CanDamage(const AActor* Instigator, const AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Combat")
	static UHealthComponent* GetHealthComponent(const AActor* Actor);

	UFUNCTION(BlueprintPure, Category = "Combat")
	static bool IsAlive(const AActor* Actor);

	/** Center-mass aim point for Actor (falls back to its bounds center). */
	UFUNCTION(BlueprintPure, Category = "Combat")
	static FVector GetTargetPoint(const AActor* Actor);
};
