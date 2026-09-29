#include "Characters/BossCharacter.h"
#include "AI/EnemyAIController.h"
#include "ActionGameTags.h"
#include "Combat/ActionProjectile.h"
#include "Combat/CombatFeedbackSubsystem.h"
#include "Combat/CombatLibrary.h"
#include "Combat/HealthComponent.h"
#include "Combat/HitReactionComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Game/ActionGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

namespace BossDefaults
{
	FEnemyAttack MakeSwipe(float Windup, float Damage)
	{
		FEnemyAttack Attack;
		Attack.Name = TEXT("Cleave");
		Attack.MaxRange = 300.f;
		Attack.Cooldown = 1.2f;
		Attack.Weight = 2.f;
		Attack.FallbackWindup = Windup;
		Attack.FallbackRecovery = 0.7f;
		Attack.Attack.Damage.Damage = Damage;
		Attack.Attack.Damage.PoiseDamage = 40.f;
		Attack.Attack.Damage.Reaction = EHitReaction::Heavy;
		Attack.Attack.Damage.KnockbackStrength = 750.f;
		Attack.Attack.Damage.HitStop = 0.1f;
		Attack.Attack.Damage.CameraTrauma = 0.45f;
		Attack.Attack.Damage.DamageType = ActionGameTags::Damage_Blunt;
		Attack.Attack.MagnetismRange = 500.f;
		Attack.Attack.MagnetismStopDistance = 180.f;
		return Attack;
	}

	FEnemyAttack MakeSlam(float Windup, float Radius)
	{
		FEnemyAttack Attack;
		Attack.Name = TEXT("Slam");
		Attack.MaxRange = Radius * 0.8f;
		Attack.Cooldown = 5.f;
		Attack.Weight = 1.f;
		Attack.bSuperArmor = true; // Big telegraphed move: commit to it, force the player to dodge.
		Attack.FallbackWindup = Windup;
		Attack.FallbackRecovery = 1.2f; // Long recovery = the reward for dodging it.
		Attack.AreaRadius = Radius;
		Attack.AreaDamage.Damage = 28.f;
		Attack.AreaDamage.PoiseDamage = 60.f;
		Attack.AreaDamage.Reaction = EHitReaction::Knockback;
		Attack.AreaDamage.KnockbackStrength = 900.f;
		Attack.AreaDamage.LaunchStrength = 350.f;
		Attack.AreaDamage.HitStop = 0.08f;
		Attack.AreaDamage.CameraTrauma = 0.6f;
		Attack.AreaDamage.DamageType = ActionGameTags::Damage_Slam;
		return Attack;
	}

	FEnemyAttack MakeVolley(int32 Count, float Spread, bool bHoming, float Windup)
	{
		FEnemyAttack Attack;
		Attack.Name = TEXT("Volley");
		Attack.MinRange = 400.f;
		Attack.MaxRange = 2500.f;
		Attack.Cooldown = 3.5f;
		Attack.Weight = 1.5f;
		Attack.FallbackWindup = Windup;
		Attack.FallbackRecovery = 0.8f;
		Attack.ProjectileClass = AActionProjectile::StaticClass();
		Attack.ProjectileCount = Count;
		Attack.ProjectileSpread = Spread;
		Attack.bHomingProjectiles = bHoming;
		return Attack;
	}
}

ABossCharacter::ABossCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	BossName = NSLOCTEXT("ActionGame", "DefaultBossName", "The Hollow Warden");

	GetCapsuleComponent()->InitCapsuleSize(55.f, 125.f);
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -125.f));
	GetMesh()->SetRelativeScale3D(FVector(1.4f));

	AggroRange = 3500.f;
	PreferredRange = 550.f;
	StrafeSpeed = 300.f;
	ChaseSpeed = 700.f;
	RecoveryTimeMin = 0.4f;
	RecoveryTimeMax = 0.9f;
	HealthOrbCount = 0;

	HealthComponent->SetMaxHealth(1500.f);
	HealthComponent->SetMaxPoise(260.f);

	// Only poise breaks (or true knockdowns) interrupt the boss; it barely moves when hit.
	HitReactionComponent->SetInterruptThreshold(EHitReaction::Knockdown);
	HitReactionComponent->SetKnockbackResistance(0.9f);
	HitReactionComponent->SetCorpseLifeSpan(0.f);

	BuildDefaultPhases();
}

void ABossCharacter::BuildDefaultPhases()
{
	using namespace BossDefaults;

	FBossPhase Phase1;
	Phase1.Name = TEXT("Awakened");
	Phase1.HealthThreshold = 1.f;
	Phase1.Attacks = { MakeSwipe(0.8f, 22.f), MakeSlam(1.1f, 480.f), MakeVolley(3, 30.f, false, 0.8f) };

	FBossPhase Phase2;
	Phase2.Name = TEXT("Enraged");
	Phase2.HealthThreshold = 0.6f;
	Phase2.SpeedMultiplier = 1.25f;
	Phase2.Attacks = { MakeSwipe(0.65f, 26.f), MakeSlam(0.95f, 560.f), MakeVolley(5, 50.f, false, 0.7f) };
	Phase2.Reinforcements = { AEnemyCharacter::StaticClass(), AEnemyCharacter::StaticClass() };

	FBossPhase Phase3;
	Phase3.Name = TEXT("Desperate");
	Phase3.HealthThreshold = 0.3f;
	Phase3.SpeedMultiplier = 1.5f;
	Phase3.Attacks = { MakeSwipe(0.5f, 30.f), MakeSlam(0.8f, 640.f), MakeVolley(7, 70.f, true, 0.6f) };

	for (FBossPhase* Phase : { &Phase1, &Phase2, &Phase3 })
	{
		Phase->ShockwaveDamage.Damage = 0.f;
		Phase->ShockwaveDamage.PoiseDamage = 0.f;
		Phase->ShockwaveDamage.Reaction = EHitReaction::Knockback;
		Phase->ShockwaveDamage.KnockbackStrength = 1200.f;
		Phase->ShockwaveDamage.LaunchStrength = 300.f;
		Phase->ShockwaveDamage.HitStop = 0.f;
		Phase->ShockwaveDamage.CameraTrauma = 0.f;
		Phases.Add(*Phase);
	}
}

void ABossCharacter::BeginPlay()
{
	Super::BeginPlay();
	BaseStrafeSpeed = StrafeSpeed;
	BaseChaseSpeed = ChaseSpeed;
	HealthComponent->OnHealthChanged.AddDynamic(this, &ABossCharacter::HandleHealthChanged);
	if (Phases.Num() > 0)
	{
		EnterPhase(0);
	}
}

const TArray<FEnemyAttack>& ABossCharacter::GetAttacks() const
{
	if (Phases.IsValidIndex(CurrentPhase) && Phases[CurrentPhase].Attacks.Num() > 0)
	{
		return Phases[CurrentPhase].Attacks;
	}
	return Super::GetAttacks();
}

bool ABossCharacter::IsExecutingAttack() const
{
	return bInTransition || Super::IsExecutingAttack();
}

void ABossCharacter::OnEngaged()
{
	if (AActionGameMode* GameMode = GetWorld()->GetAuthGameMode<AActionGameMode>())
	{
		GameMode->RegisterBoss(this, BossName);
		GameMode->Announce(BossName, 3.f);
	}
}

void ABossCharacter::HandleHealthChanged(UHealthComponent* Health, float NewHealth, float Delta)
{
	if (!Health || !Health->IsAlive())
	{
		return;
	}
	// Jump straight to the deepest phase reached (a huge hit can skip a phase).
	const float Percent = Health->GetHealthPercent();
	for (int32 Index = Phases.Num() - 1; Index > CurrentPhase; --Index)
	{
		if (Percent <= Phases[Index].HealthThreshold)
		{
			EnterPhase(Index);
			break;
		}
	}
}

void ABossCharacter::EnterPhase(int32 PhaseIndex)
{
	if (!Phases.IsValidIndex(PhaseIndex))
	{
		return;
	}
	CurrentPhase = PhaseIndex;
	const FBossPhase& Phase = Phases[PhaseIndex];

	ResetAttackCooldowns();
	StrafeSpeed = BaseStrafeSpeed * Phase.SpeedMultiplier;
	ChaseSpeed = BaseChaseSpeed * Phase.SpeedMultiplier;

	if (PhaseIndex == 0)
	{
		return; // The opening phase starts silently.
	}

	// Transition beat: interrupt, become untouchable, roar, push the player away.
	HandleInterrupted(EHitReaction::None, false);
	bInTransition = true;
	AddStateTag(ActionGameTags::State_Invulnerable);
	AddStateTag(ActionGameTags::State_SuperArmor);
	GetCharacterMovement()->StopMovementImmediately();

	if (Phase.TransitionMontage)
	{
		PlayAnimMontage(Phase.TransitionMontage);
	}

	UCombatLibrary::ApplyRadialDamage(this, Phase.ShockwaveDamage, GetActorLocation(), Phase.ShockwaveRadius, this, this);

	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		Feedback->PlayImpactEffect(Phase.TransitionEffect, GetActorLocation(), FRotator::ZeroRotator);
		Feedback->PlaySlowMotion(0.35f, 0.5f, 0.3f);
		Feedback->AddCameraTrauma(0.5f);
	}

	SpawnReinforcements(Phase);
	GetWorldTimerManager().SetTimer(TransitionTimer, this, &ABossCharacter::EndTransition, FMath::Max(0.1f, Phase.TransitionDuration), false);
}

void ABossCharacter::EndTransition()
{
	if (!bInTransition)
	{
		return;
	}
	bInTransition = false;
	RemoveStateTag(ActionGameTags::State_Invulnerable);
	RemoveStateTag(ActionGameTags::State_SuperArmor);
}

void ABossCharacter::SpawnReinforcements(const FBossPhase& Phase)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (int32 Index = 0; Index < Phase.Reinforcements.Num(); ++Index)
	{
		if (!Phase.Reinforcements[Index])
		{
			continue;
		}
		const float Angle = 360.f * Index / Phase.Reinforcements.Num();
		const FVector Offset = FVector::ForwardVector.RotateAngleAxis(Angle, FVector::UpVector) * 700.f;
		AEnemyCharacter* Add = GetWorld()->SpawnActor<AEnemyCharacter>(Phase.Reinforcements[Index], GetActorLocation() + Offset,
			GetActorRotation(), Params);
		if (AEnemyAIController* AI = Add ? Cast<AEnemyAIController>(Add->GetController()) : nullptr)
		{
			AI->ForceAggro();
		}
	}
}
