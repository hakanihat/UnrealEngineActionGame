#include "Game/ArenaEncounter.h"
#include "AI/EnemyAIController.h"
#include "Characters/EnemyCharacter.h"
#include "Combat/CombatFeedbackSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

AArenaEncounter::AArenaEncounter()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(800.f, 800.f, 300.f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = Trigger;
}

void AArenaEncounter::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AArenaEncounter::HandleTriggerOverlap);
	SetBlockersActive(false);
}

void AArenaEncounter::HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		StartEncounter();
	}
}

void AArenaEncounter::StartEncounter()
{
	if (bActive || bCompleted)
	{
		return;
	}
	bActive = true;
	SetBlockersActive(true);
	OnStarted.Broadcast();

	if (Waves.Num() == 0)
	{
		Complete();
		return;
	}
	StartWave(0);
}

void AArenaEncounter::StartWave(int32 WaveIndex)
{
	CurrentWave = WaveIndex;
	AliveInWave = 0;
	SpawnQueue.Reset();

	const FEncounterWave& Wave = Waves[WaveIndex];
	for (const FEncounterSpawn& Spawn : Wave.Spawns)
	{
		if (!Spawn.EnemyClass)
		{
			continue;
		}
		const FVector Base = Spawn.SpawnPoint ? Spawn.SpawnPoint->GetActorLocation() : GetActorLocation();
		for (int32 Index = 0; Index < Spawn.Count; ++Index)
		{
			const FVector2D Scatter = FMath::RandPointInCircle(SpawnScatter);
			SpawnQueue.Add({ Spawn.EnemyClass, Base + FVector(Scatter.X, Scatter.Y, 0.f) });
		}
	}

	GetWorldTimerManager().SetTimer(SpawnTimer, this, &AArenaEncounter::SpawnNext, FMath::Max(0.01f, Wave.StartDelay), false);
}

void AArenaEncounter::SpawnNext()
{
	if (SpawnQueue.Num() == 0)
	{
		CheckWaveProgress();
		return;
	}

	const FPendingSpawn Pending = SpawnQueue[0];
	SpawnQueue.RemoveAt(0);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Face the arena center so enemies enter looking at the fight.
	const FRotator Facing = (GetActorLocation() - Pending.Location).GetSafeNormal2D().Rotation();
	if (AEnemyCharacter* Enemy = GetWorld()->SpawnActor<AEnemyCharacter>(Pending.EnemyClass, Pending.Location, Facing, Params))
	{
		++AliveInWave;
		++AliveTotal;
		Enemy->GetHealthComponent()->OnDeath.AddDynamic(this, &AArenaEncounter::HandleEnemyDeath);
		if (AEnemyAIController* AI = Cast<AEnemyAIController>(Enemy->GetController()))
		{
			AI->ForceAggro();
		}
		if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
		{
			Feedback->PlayImpactEffect(SpawnEffect, Pending.Location, FRotator::ZeroRotator);
		}
	}

	if (SpawnQueue.Num() > 0)
	{
		GetWorldTimerManager().SetTimer(SpawnTimer, this, &AArenaEncounter::SpawnNext, FMath::Max(0.01f, SpawnInterval), false);
	}
	else
	{
		CheckWaveProgress();
	}
}

void AArenaEncounter::HandleEnemyDeath(AActor* DeadActor, const FCombatHit& KillingHit)
{
	AliveInWave = FMath::Max(0, AliveInWave - 1);
	AliveTotal = FMath::Max(0, AliveTotal - 1);
	CheckWaveProgress();
}

void AArenaEncounter::CheckWaveProgress()
{
	if (!bActive || !Waves.IsValidIndex(CurrentWave) || SpawnQueue.Num() > 0)
	{
		return;
	}

	const bool bLastWave = CurrentWave == Waves.Num() - 1;
	if (bLastWave)
	{
		if (AliveTotal == 0)
		{
			Complete();
		}
		return;
	}

	if (AliveInWave <= Waves[CurrentWave].AdvanceWhenRemaining && !GetWorldTimerManager().IsTimerActive(SpawnTimer))
	{
		StartWave(CurrentWave + 1);
	}
}

void AArenaEncounter::Complete()
{
	bActive = false;
	bCompleted = true;
	SetBlockersActive(false);

	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		Feedback->PlaySlowMotion(FinalKillTimeScale, FinalKillSlowMoDuration, 0.4f);
	}
	OnCompleted.Broadcast();
}

void AArenaEncounter::SetBlockersActive(bool bBlockersActive)
{
	for (AActor* Blocker : Blockers)
	{
		if (Blocker)
		{
			Blocker->SetActorHiddenInGame(!bBlockersActive);
			Blocker->SetActorEnableCollision(bBlockersActive);
		}
	}
}
