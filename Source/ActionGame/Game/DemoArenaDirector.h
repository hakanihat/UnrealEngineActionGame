#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Game/ArenaEncounter.h"
#include "DemoArenaDirector.generated.h"

class ABossCharacter;
class AEnemyCharacter;
class ATelekineticProp;

/**
 * One-actor demo: drop it on the floor of any level and press Play.
 * It scatters throwable props, runs three escalating waves (grunts, then gunners mixed in),
 * then announces and starts the boss fight. Every class it spawns can be swapped for your
 * Blueprints, so the same director keeps working as art is added.
 *
 * For hand-authored levels, place AArenaEncounter actors directly instead.
 */
UCLASS()
class ACTIONGAME_API ADemoArenaDirector : public AActor
{
	GENERATED_BODY()

public:
	ADemoArenaDirector();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Category = "Demo|Classes")
	TSubclassOf<AEnemyCharacter> GruntClass;

	UPROPERTY(EditAnywhere, Category = "Demo|Classes")
	TSubclassOf<AEnemyCharacter> GunnerClass;

	UPROPERTY(EditAnywhere, Category = "Demo|Classes")
	TSubclassOf<ABossCharacter> BossClass;

	UPROPERTY(EditAnywhere, Category = "Demo|Classes")
	TSubclassOf<ATelekineticProp> PropClass;

	/** Radius of the fighting area around this actor. */
	UPROPERTY(EditAnywhere, Category = "Demo", meta = (ClampMin = "500"))
	float ArenaRadius = 1800.f;

	UPROPERTY(EditAnywhere, Category = "Demo", meta = (ClampMin = "0"))
	int32 PropCount = 16;

	/** Breather between the last wave and the boss entrance. */
	UPROPERTY(EditAnywhere, Category = "Demo", meta = (ClampMin = "0"))
	float BossIntroDelay = 3.f;

private:
	UFUNCTION()
	void HandleWavesCompleted();

	void StartBossFight();
	void SpawnProps();
	void SpawnSpawnPoints();
	AArenaEncounter* SpawnEncounter(const TArray<FEncounterWave>& Waves, bool bTriggeredByPlayer);
	FEncounterSpawn MakeSpawn(TSubclassOf<AEnemyCharacter> EnemyClass, int32 PointIndex, int32 Count) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> SpawnPoints;

	UPROPERTY(Transient)
	TObjectPtr<AArenaEncounter> WaveEncounter;

	UPROPERTY(Transient)
	TObjectPtr<AArenaEncounter> BossEncounter;

	FTimerHandle BossIntroTimer;
};
