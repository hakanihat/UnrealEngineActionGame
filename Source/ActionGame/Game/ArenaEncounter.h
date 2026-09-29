#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/CombatTypes.h"
#include "Combat/CombatFeedbackConfig.h"
#include "ArenaEncounter.generated.h"

class AEnemyCharacter;
class UBoxComponent;

USTRUCT(BlueprintType)
struct FEncounterSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TSubclassOf<AEnemyCharacter> EnemyClass;

	/** Any actor works as a marker (Target Point recommended). Empty = the encounter's location. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TObjectPtr<AActor> SpawnPoint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn", meta = (ClampMin = "1"))
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct FEncounterWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	TArray<FEncounterSpawn> Spawns;

	/** Pause before this wave starts spawning: a breather between fights. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0"))
	float StartDelay = 1.5f;

	/** The next wave starts when this many (or fewer) enemies of this wave remain. 0 = clear it fully. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave", meta = (ClampMin = "0"))
	int32 AdvanceWhenRemaining = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEncounterEvent);

/**
 * A combat arena. When the player walks in:
 *  1. Blockers (any actors, e.g. walls/doors) become solid, sealing the arena.
 *  2. Waves spawn one after another, staggered so enemies don't pop in all at once.
 *  3. The final kill gets a slow-motion beat, blockers open, OnCompleted fires.
 * The boss fight is simply an encounter whose wave contains the boss.
 */
UCLASS()
class ACTIONGAME_API AArenaEncounter : public AActor
{
	GENERATED_BODY()

public:
	AArenaEncounter();

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void StartEncounter();

	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool IsActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool IsCompleted() const { return bCompleted; }

	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FOnEncounterEvent OnStarted;

	UPROPERTY(BlueprintAssignable, Category = "Encounter")
	FOnEncounterEvent OnCompleted;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(EditAnywhere, Category = "Encounter")
	TArray<FEncounterWave> Waves;

	/** Hidden and non-colliding until the encounter starts; restored to that state on completion. */
	UPROPERTY(EditAnywhere, Category = "Encounter")
	TArray<TObjectPtr<AActor>> Blockers;

	/** Delay between individual spawns in a wave. */
	UPROPERTY(EditAnywhere, Category = "Encounter", meta = (ClampMin = "0"))
	float SpawnInterval = 0.35f;

	/** Random horizontal scatter around each spawn point. */
	UPROPERTY(EditAnywhere, Category = "Encounter", meta = (ClampMin = "0"))
	float SpawnScatter = 150.f;

	UPROPERTY(EditAnywhere, Category = "Encounter")
	FImpactEffect SpawnEffect;

	UPROPERTY(EditAnywhere, Category = "Encounter|Feel", meta = (ClampMin = "0.01", ClampMax = "1"))
	float FinalKillTimeScale = 0.2f;

	UPROPERTY(EditAnywhere, Category = "Encounter|Feel", meta = (ClampMin = "0"))
	float FinalKillSlowMoDuration = 0.8f;

private:
	UFUNCTION()
	void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEnemyDeath(AActor* DeadActor, const FCombatHit& KillingHit);

	void StartWave(int32 WaveIndex);
	void SpawnNext();
	void CheckWaveProgress();
	void Complete();
	void SetBlockersActive(bool bBlockersActive);

	struct FPendingSpawn
	{
		TSubclassOf<AEnemyCharacter> EnemyClass;
		FVector Location;
	};

	TArray<FPendingSpawn> SpawnQueue;
	int32 CurrentWave = INDEX_NONE;
	int32 AliveInWave = 0;
	int32 AliveTotal = 0;
	bool bActive = false;
	bool bCompleted = false;
	FTimerHandle SpawnTimer;
};
