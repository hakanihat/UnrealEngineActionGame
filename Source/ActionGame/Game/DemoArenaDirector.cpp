#include "Game/DemoArenaDirector.h"
#include "Abilities/TelekineticProp.h"
#include "Characters/BossCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/RangedEnemyCharacter.h"
#include "Components/SceneComponent.h"
#include "Engine/TargetPoint.h"
#include "Game/ActionGameMode.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 NumSpawnPoints = 4;
}

ADemoArenaDirector::ADemoArenaDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	GruntClass = AEnemyCharacter::StaticClass();
	GunnerClass = ARangedEnemyCharacter::StaticClass();
	BossClass = ABossCharacter::StaticClass();
	PropClass = ATelekineticProp::StaticClass();
}

void ADemoArenaDirector::BeginPlay()
{
	Super::BeginPlay();

	SpawnSpawnPoints();
	SpawnProps();

	// Three waves that escalate: pure melee, then gunners mixed in so the player must prioritise.
	// Each wave arrives while one enemy of the previous wave is still alive, keeping pressure up.
	TArray<FEncounterWave> Waves;

	FEncounterWave First;
	First.StartDelay = 1.f;
	First.AdvanceWhenRemaining = 1;
	First.Spawns = { MakeSpawn(GruntClass, 0, 2), MakeSpawn(GruntClass, 1, 1) };
	Waves.Add(First);

	FEncounterWave Second;
	Second.StartDelay = 1.5f;
	Second.AdvanceWhenRemaining = 1;
	Second.Spawns = { MakeSpawn(GruntClass, 2, 2), MakeSpawn(GunnerClass, 3, 1) };
	Waves.Add(Second);

	FEncounterWave Third;
	Third.StartDelay = 2.f;
	Third.Spawns = { MakeSpawn(GruntClass, 0, 2), MakeSpawn(GruntClass, 2, 1), MakeSpawn(GunnerClass, 1, 1), MakeSpawn(GunnerClass, 3, 1) };
	Waves.Add(Third);

	WaveEncounter = SpawnEncounter(Waves, true);
	if (WaveEncounter)
	{
		WaveEncounter->OnCompleted.AddDynamic(this, &ADemoArenaDirector::HandleWavesCompleted);
	}

	FEncounterWave BossWave;
	BossWave.StartDelay = 0.5f;
	BossWave.Spawns = { MakeSpawn(BossClass, 2, 1) };
	BossEncounter = SpawnEncounter({ BossWave }, false);
}

void ADemoArenaDirector::SpawnSpawnPoints()
{
	for (int32 Index = 0; Index < NumSpawnPoints; ++Index)
	{
		const float Angle = 360.f * Index / NumSpawnPoints;
		const FVector Offset = FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector) * ArenaRadius * 0.85f;
		SpawnPoints.Add(GetWorld()->SpawnActor<ATargetPoint>(GetActorLocation() + Offset + FVector(0.f, 0.f, 100.f), FRotator::ZeroRotator));
	}
}

void ADemoArenaDirector::SpawnProps()
{
	if (!PropClass)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 Index = 0; Index < PropCount; ++Index)
	{
		// Props in a loose ring, keeping the arena center clear for movement.
		const float Angle = FMath::FRandRange(0.f, 360.f);
		const float Distance = FMath::FRandRange(0.3f, 0.8f) * ArenaRadius;
		const FVector Offset = FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector) * Distance;
		GetWorld()->SpawnActor<ATelekineticProp>(PropClass, GetActorLocation() + Offset + FVector(0.f, 0.f, 80.f),
			FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Params);
	}
}

FEncounterSpawn ADemoArenaDirector::MakeSpawn(TSubclassOf<AEnemyCharacter> EnemyClass, int32 PointIndex, int32 Count) const
{
	FEncounterSpawn Spawn;
	Spawn.EnemyClass = EnemyClass;
	Spawn.SpawnPoint = SpawnPoints.IsValidIndex(PointIndex) ? SpawnPoints[PointIndex] : nullptr;
	Spawn.Count = Count;
	return Spawn;
}

AArenaEncounter* ADemoArenaDirector::SpawnEncounter(const TArray<FEncounterWave>& Waves, bool bTriggeredByPlayer)
{
	// Deferred so waves are configured before the trigger can overlap the player.
	const FTransform SpawnTransform(GetActorLocation());
	AArenaEncounter* Encounter = GetWorld()->SpawnActorDeferred<AArenaEncounter>(AArenaEncounter::StaticClass(), SpawnTransform);
	if (Encounter)
	{
		Encounter->Configure(Waves, FVector(ArenaRadius * 0.6f, ArenaRadius * 0.6f, 400.f), 200.f);
		Encounter->SetTriggerEnabled(bTriggeredByPlayer);
		Encounter->FinishSpawning(SpawnTransform);
	}
	return Encounter;
}

void ADemoArenaDirector::HandleWavesCompleted()
{
	if (AActionGameMode* GameMode = GetWorld()->GetAuthGameMode<AActionGameMode>())
	{
		GameMode->Announce(NSLOCTEXT("ActionGame", "BossIncoming", "Something stirs..."), BossIntroDelay);
	}
	GetWorldTimerManager().SetTimer(BossIntroTimer, this, &ADemoArenaDirector::StartBossFight, FMath::Max(0.1f, BossIntroDelay), false);
}

void ADemoArenaDirector::StartBossFight()
{
	if (BossEncounter)
	{
		BossEncounter->StartEncounter();
	}
}
