#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/CombatTypes.h"
#include "Combat/CombatFeedbackConfig.h"
#include "CombatFeedbackSubsystem.generated.h"

/**
 * Owns every "juice" effect so they are consistent and never fight each other:
 *  - Hitstop: freezes attacker and victim for a few frames by scaling their CustomTimeDilation.
 *    Per-actor (not global) so the rest of the world keeps moving and the game never feels laggy.
 *  - Slow motion: global time dilation with a smooth blend back (perfect dodges, final kills).
 *  - Camera trauma: forwarded to the local player's camera manager.
 *  - Impact VFX/SFX: looked up by damage type in the UCombatFeedbackConfig asset.
 */
UCLASS()
class ACTIONGAME_API UCombatFeedbackSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UCombatFeedbackSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Plays all feedback for a resolved hit. Called automatically by UCombatLibrary::ApplyDamage. */
	void HandleHit(const FCombatHit& Hit, const FCombatDamageResult& Result);

	/** Freezes Actor for Duration real seconds (extends, never shortens, an active freeze). */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void ApplyHitStop(AActor* Actor, float Duration);

	/** Global slow motion. A stronger (lower) request overrides a weaker active one. */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void PlaySlowMotion(float TimeScale, float Duration, float BlendOutTime = 0.2f);

	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void AddCameraTrauma(float Amount);

	/** Trauma that falls off with the local player's distance from Location. */
	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void AddCameraTraumaAtLocation(FVector Location, float Amount, float Radius);

	UFUNCTION(BlueprintCallable, Category = "Feedback")
	void PlayImpactEffect(const FImpactEffect& Effect, FVector Location, FRotator Rotation);

	const UCombatFeedbackConfig& GetConfig() const { return *Config; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	bool IsLocalPlayer(const AActor* Actor) const;
	void RestoreActorTime(AActor* Actor);
	void TickHitStop(float RealDeltaTime);
	void TickSlowMotion(float RealDeltaTime);

	UPROPERTY()
	TObjectPtr<UCombatFeedbackConfig> Config;

	/** Remaining real-time seconds of hitstop per frozen actor. */
	TMap<TWeakObjectPtr<AActor>, float> HitStopTimers;

	float SlowMoScale = 1.f;
	float SlowMoRemaining = 0.f;
	float SlowMoBlendOutTime = 0.f;
	float SlowMoBlendElapsed = 0.f;
	bool bSlowMoActive = false;
};
