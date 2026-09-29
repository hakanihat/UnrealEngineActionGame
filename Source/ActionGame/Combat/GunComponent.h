#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatTypes.h"
#include "Combat/CombatFeedbackConfig.h"
#include "GunComponent.generated.h"

class UNiagaraSystem;
class USceneComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnShotFired, int32, MuzzleIndex, bool, bHitTarget);

/**
 * Dual-pistol hitscan weapon.
 *  - Alternates between muzzles (left/right hand) for a rhythmic, readable fire cadence.
 *  - Aims from the camera, then traces from the muzzle to the aim point so walls near the
 *    character still block shots correctly.
 *  - Spread "blooms" while firing and recovers quickly: rewards controlled bursts.
 *  - Ammo regenerates when not firing (like Control's Service Weapon), so guns are always
 *    available but can't replace the blade.
 *  - Bullets chip damage and poise with no interrupting reaction; they set up staggers,
 *    the blade and telekinesis cash them in. Headshots crit.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UGunComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGunComponent();

	UFUNCTION(BlueprintCallable, Category = "Gun")
	void StartFiring();

	UFUNCTION(BlueprintCallable, Category = "Gun")
	void StopFiring();

	/** Muzzle scene components (usually the two gun meshes). Index 0 fires first. */
	UFUNCTION(BlueprintCallable, Category = "Gun")
	void SetMuzzles(const TArray<USceneComponent*>& InMuzzles);

	UFUNCTION(BlueprintPure, Category = "Gun")
	bool IsFiring() const { return bWantsToFire; }

	UFUNCTION(BlueprintPure, Category = "Gun")
	float GetAmmo() const { return Ammo; }

	UFUNCTION(BlueprintPure, Category = "Gun")
	float GetMaxAmmo() const { return MaxAmmo; }

	/** Current spread in degrees; the HUD crosshair expands with it. */
	UFUNCTION(BlueprintPure, Category = "Gun")
	float GetCurrentSpread() const { return CurrentSpread; }

	UPROPERTY(BlueprintAssignable, Category = "Gun")
	FOnShotFired OnShotFired;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "Gun")
	FCombatDamageSpec BulletDamage;

	/** Shots per second across both guns. */
	UPROPERTY(EditAnywhere, Category = "Gun", meta = (ClampMin = "0.1"))
	float FireRate = 9.f;

	UPROPERTY(EditAnywhere, Category = "Gun", meta = (ClampMin = "0"))
	float Range = 8000.f;

	UPROPERTY(EditAnywhere, Category = "Gun|Ammo", meta = (ClampMin = "1"))
	float MaxAmmo = 30.f;

	UPROPERTY(EditAnywhere, Category = "Gun|Ammo", meta = (ClampMin = "0"))
	float AmmoRegenRate = 14.f;

	UPROPERTY(EditAnywhere, Category = "Gun|Ammo", meta = (ClampMin = "0"))
	float AmmoRegenDelay = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Gun|Accuracy", meta = (ClampMin = "0"))
	float BaseSpread = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Gun|Accuracy", meta = (ClampMin = "0"))
	float SpreadPerShot = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Gun|Accuracy", meta = (ClampMin = "0"))
	float MaxSpread = 3.5f;

	/** Degrees of spread recovered per second. */
	UPROPERTY(EditAnywhere, Category = "Gun|Accuracy", meta = (ClampMin = "0"))
	float SpreadRecovery = 10.f;

	/** Camera kick per shot (degrees). Small values feel punchy without fighting the player's aim. */
	UPROPERTY(EditAnywhere, Category = "Gun|Recoil", meta = (ClampMin = "0"))
	float RecoilPitch = 0.3f;

	UPROPERTY(EditAnywhere, Category = "Gun|Recoil", meta = (ClampMin = "0"))
	float RecoilYawJitter = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Gun|Recoil", meta = (ClampMin = "0", ClampMax = "1"))
	float CameraTraumaPerShot = 0.04f;

	UPROPERTY(EditAnywhere, Category = "Gun|Critical")
	TArray<FName> CriticalBones = { TEXT("head"), TEXT("neck_01") };

	UPROPERTY(EditAnywhere, Category = "Gun|Critical", meta = (ClampMin = "1"))
	float CriticalMultiplier = 2.f;

	/** Socket on each muzzle component; falls back to the component origin. */
	UPROPERTY(EditAnywhere, Category = "Gun|Effects")
	FName MuzzleSocket = TEXT("Muzzle");

	UPROPERTY(EditAnywhere, Category = "Gun|Effects")
	TObjectPtr<UNiagaraSystem> MuzzleFlash = nullptr;

	/** Beam/tracer effect. Receives the end point through a Vector user parameter. */
	UPROPERTY(EditAnywhere, Category = "Gun|Effects")
	TObjectPtr<UNiagaraSystem> Tracer = nullptr;

	UPROPERTY(EditAnywhere, Category = "Gun|Effects")
	FName TracerEndParameter = TEXT("BeamEnd");

	/** Played when hitting the world (character hits use the feedback config instead). */
	UPROPERTY(EditAnywhere, Category = "Gun|Effects")
	FImpactEffect WorldImpact;

	UPROPERTY(EditAnywhere, Category = "Gun|Effects")
	TObjectPtr<USoundBase> FireSound = nullptr;

	UPROPERTY(EditAnywhere, Category = "Gun|Effects")
	TObjectPtr<USoundBase> EmptySound = nullptr;

private:
	void FireShot();
	FVector GetMuzzleLocation(int32 Index) const;
	void ApplyRecoil();
	void PlayShotEffects(const FVector& MuzzleLocation, const FVector& EndPoint, const FHitResult* Hit, bool bHitCharacter);

	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> Muzzles;

	float Ammo = 0.f;
	float CurrentSpread = 0.f;
	float ShotCooldown = 0.f;
	float LastShotTime = -1000.f;
	int32 NextMuzzle = 0;
	bool bWantsToFire = false;
};
