#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Combat/CombatTypes.h"
#include "ActionGameMode.generated.h"

class AActionCharacterBase;

UENUM(BlueprintType)
enum class EDemoState : uint8
{
	Playing,
	Victory,
	Defeated
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDemoStateChanged, EDemoState, NewState);

/**
 * Demo flow: tracks the player's and the boss's fate.
 * Player death -> short beat -> level restart. Boss death -> dramatic slow motion -> victory.
 * Also exposes the active boss so the HUD can show a boss health bar.
 */
UCLASS()
class ACTIONGAME_API AActionGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AActionGameMode();

	virtual void SetPlayerDefaults(APawn* PlayerPawn) override;

	/** Called by a boss when its fight begins. Shows the boss bar and tracks its death. */
	UFUNCTION(BlueprintCallable, Category = "Demo")
	void RegisterBoss(AActionCharacterBase* Boss, FText DisplayName);

	UFUNCTION(BlueprintPure, Category = "Demo")
	AActionCharacterBase* GetActiveBoss() const { return ActiveBoss.Get(); }

	UFUNCTION(BlueprintPure, Category = "Demo")
	FText GetBossName() const { return BossName; }

	UFUNCTION(BlueprintPure, Category = "Demo")
	EDemoState GetDemoState() const { return DemoState; }

	UPROPERTY(BlueprintAssignable, Category = "Demo")
	FOnDemoStateChanged OnDemoStateChanged;

protected:
	/** Seconds between the player's death and the level restart. */
	UPROPERTY(EditAnywhere, Category = "Demo", meta = (ClampMin = "0"))
	float RestartDelay = 4.f;

	UPROPERTY(EditAnywhere, Category = "Demo", meta = (ClampMin = "0.01", ClampMax = "1"))
	float BossKillTimeScale = 0.15f;

	/** Real seconds of slow motion after the boss dies. */
	UPROPERTY(EditAnywhere, Category = "Demo", meta = (ClampMin = "0"))
	float BossKillSlowMoDuration = 2.5f;

private:
	UFUNCTION()
	void HandlePlayerDeath(AActor* DeadActor, const FCombatHit& KillingHit);

	UFUNCTION()
	void HandleBossDeath(AActor* DeadActor, const FCombatHit& KillingHit);

	void SetDemoState(EDemoState NewState);
	void RestartCurrentLevel();

	TWeakObjectPtr<AActionCharacterBase> ActiveBoss;
	FText BossName;
	EDemoState DemoState = EDemoState::Playing;
	FTimerHandle RestartTimer;
};
