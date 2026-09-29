#include "Characters/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "ActionGameTags.h"
#include "Combat/ActionProjectile.h"
#include "Combat/CombatFeedbackSubsystem.h"
#include "Combat/CombatLibrary.h"
#include "Combat/MeleeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Game/HealthOrb.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

AEnemyCharacter::AEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Team = ECombatTeam::Enemy;

	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(35.f, 90.f);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));

	// Enemies face their focus (the player) while strafing, which keeps them readable.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;
	Movement->bUseControllerDesiredRotation = true;
	Movement->RotationRate = FRotator(0.f, 540.f, 0.f);
	Movement->MaxWalkSpeed = StrafeSpeed;
	Movement->bUseRVOAvoidance = true; // Keeps groups from stacking on top of each other.
	Movement->AvoidanceConsiderationRadius = 250.f;

	// A default swipe so a bare enemy is a working grunt before any data is authored.
	FEnemyAttack DefaultAttack;
	DefaultAttack.Name = TEXT("Swipe");
	DefaultAttack.MaxRange = 220.f;
	DefaultAttack.Cooldown = 1.5f;
	DefaultAttack.Attack.Damage.Damage = 12.f;
	DefaultAttack.Attack.Damage.PoiseDamage = 20.f;
	DefaultAttack.Attack.Damage.Reaction = EHitReaction::Flinch;
	DefaultAttack.Attack.Damage.KnockbackStrength = 350.f;
	DefaultAttack.Attack.Damage.HitStop = 0.07f;
	DefaultAttack.Attack.Damage.CameraTrauma = 0.3f;
	DefaultAttack.Attack.Damage.DamageType = ActionGameTags::Damage_Blunt;
	Attacks.Add(DefaultAttack);
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	MeleeComponent->OnAttackEnded.AddDynamic(this, &AEnemyCharacter::HandleMeleeAttackEnded);
}

// ---------------------------------------------------------------------------------------------
// Attack selection & execution
// ---------------------------------------------------------------------------------------------

int32 AEnemyCharacter::ChooseAttack() const
{
	const TArray<FEnemyAttack>& Available = GetAttacks();
	const float Now = GetWorld()->GetTimeSeconds();

	TArray<int32, TInlineAllocator<8>> Candidates;
	float TotalWeight = 0.f;
	for (int32 Index = 0; Index < Available.Num(); ++Index)
	{
		const float* ReadyTime = AttackReadyTimes.Find(Index);
		if (Available[Index].Weight > 0.f && (!ReadyTime || Now >= *ReadyTime))
		{
			Candidates.Add(Index);
			TotalWeight += Available[Index].Weight;
		}
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (const int32 Index : Candidates)
	{
		Roll -= Available[Index].Weight;
		if (Roll <= 0.f)
		{
			return Index;
		}
	}
	return Candidates.Num() > 0 ? Candidates.Last() : INDEX_NONE;
}

bool AEnemyCharacter::IsAttackInRange(int32 AttackIndex, float Distance) const
{
	const TArray<FEnemyAttack>& Available = GetAttacks();
	return Available.IsValidIndex(AttackIndex)
		&& Distance >= Available[AttackIndex].MinRange
		&& Distance <= Available[AttackIndex].MaxRange;
}

bool AEnemyCharacter::ExecuteAttack(int32 AttackIndex, AActor* Target)
{
	const TArray<FEnemyAttack>& Available = GetAttacks();
	if (!CanAct() || IsExecutingAttack() || !Available.IsValidIndex(AttackIndex))
	{
		return false;
	}

	const FEnemyAttack& Attack = Available[AttackIndex];
	CurrentAttackIndex = AttackIndex;
	CurrentTarget = Target;
	AttackReadyTimes.Add(AttackIndex, GetWorld()->GetTimeSeconds() + Attack.Cooldown);

	if (Attack.bSuperArmor)
	{
		AddStateTag(ActionGameTags::State_SuperArmor);
		bSuperArmorApplied = true;
	}

	if (Attack.Attack.Montage)
	{
		if (MeleeComponent->PerformAttack(Attack.Attack, Target))
		{
			return true;
		}
		FinishAttack();
		return false;
	}

	BeginFallbackAttack();
	return true;
}

bool AEnemyCharacter::IsExecutingAttack() const
{
	return MeleeComponent->IsAttacking() || bFallbackAttackActive;
}

const FEnemyAttack* AEnemyCharacter::GetCurrentAttack() const
{
	const TArray<FEnemyAttack>& Available = GetAttacks();
	return Available.IsValidIndex(CurrentAttackIndex) ? &Available[CurrentAttackIndex] : nullptr;
}

void AEnemyCharacter::HandleMeleeAttackEnded(bool bInterrupted)
{
	if (!bFallbackAttackActive)
	{
		FinishAttack();
	}
}

void AEnemyCharacter::FinishAttack()
{
	if (bSuperArmorApplied)
	{
		RemoveStateTag(ActionGameTags::State_SuperArmor);
		bSuperArmorApplied = false;
	}
	bCharging = false;
	CurrentAttackIndex = INDEX_NONE;
}

// ---------------------------------------------------------------------------------------------
// Animation-driven events
// ---------------------------------------------------------------------------------------------

void AEnemyCharacter::HandleCombatEvent(FGameplayTag EventTag)
{
	const FEnemyAttack* Attack = GetCurrentAttack();

	if (EventTag == ActionGameTags::Event_Telegraph)
	{
		PlayTelegraph();
	}
	else if (EventTag == ActionGameTags::Event_FireProjectile && Attack)
	{
		FireProjectiles(*Attack);
	}
	else if (EventTag == ActionGameTags::Event_AreaImpact && Attack)
	{
		ApplyAreaImpact(*Attack);
	}
	else if (EventTag == ActionGameTags::Event_ChargeStart && Attack && Attack->ChargeSpeed > 0.f)
	{
		const AActor* Target = CurrentTarget.Get();
		ChargeDirection = Target ? (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D() : GetActorForwardVector();
		SetActorRotation(ChargeDirection.Rotation());
		bCharging = true;
	}
	else if (EventTag == ActionGameTags::Event_ChargeEnd)
	{
		bCharging = false;
	}
}

void AEnemyCharacter::PlayTelegraph()
{
	TelegraphAlpha = 1.f;
	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		Feedback->PlayImpactEffect(TelegraphEffect, GetActorLocation() + FVector(0.f, 0.f, 60.f), GetActorRotation());
	}
}

void AEnemyCharacter::FireProjectiles(const FEnemyAttack& Attack)
{
	if (!Attack.ProjectileClass)
	{
		return;
	}

	const USkeletalMeshComponent* MeshComp = GetMesh();
	const FVector SpawnLocation = MeshComp->DoesSocketExist(Attack.ProjectileSocket)
		? MeshComp->GetSocketLocation(Attack.ProjectileSocket)
		: GetActorLocation() + GetActorForwardVector() * 60.f + FVector(0.f, 0.f, 40.f);

	AActor* Target = CurrentTarget.Get();
	const FVector AimPoint = Target ? UCombatLibrary::GetTargetPoint(Target) : SpawnLocation + GetActorForwardVector() * 1000.f;
	const FVector BaseDirection = (AimPoint - SpawnLocation).GetSafeNormal();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const int32 Count = FMath::Max(1, Attack.ProjectileCount);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Alpha = Count > 1 ? static_cast<float>(Index) / (Count - 1) : 0.5f;
		const float YawOffset = FMath::Lerp(-Attack.ProjectileSpread * 0.5f, Attack.ProjectileSpread * 0.5f, Alpha);
		const FVector Direction = BaseDirection.RotateAngleAxis(YawOffset, FVector::UpVector);

		if (AActionProjectile* Projectile = GetWorld()->SpawnActor<AActionProjectile>(Attack.ProjectileClass, SpawnLocation, Direction.Rotation(), Params))
		{
			Projectile->Launch(this, Direction, Attack.bHomingProjectiles ? Target : nullptr);
		}
	}
}

void AEnemyCharacter::ApplyAreaImpact(const FEnemyAttack& Attack)
{
	if (Attack.AreaRadius <= 0.f)
	{
		return;
	}
	const FVector Origin = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	UCombatLibrary::ApplyRadialDamage(this, Attack.AreaDamage, Origin, Attack.AreaRadius, this, this);

	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		Feedback->PlayImpactEffect(Attack.AreaEffect, Origin, FRotator::ZeroRotator);
		Feedback->AddCameraTraumaAtLocation(Origin, 0.5f, Attack.AreaRadius * 3.f);
	}
}

// ---------------------------------------------------------------------------------------------
// Fallback (no montage) attacks
// ---------------------------------------------------------------------------------------------

void AEnemyCharacter::BeginFallbackAttack()
{
	const FEnemyAttack* Attack = GetCurrentAttack();
	if (!Attack)
	{
		return;
	}
	bFallbackAttackActive = true;
	PlayTelegraph();
	GetWorldTimerManager().SetTimer(FallbackTimer, this, &AEnemyCharacter::PerformFallbackStrike, FMath::Max(0.05f, Attack->FallbackWindup), false);
}

void AEnemyCharacter::PerformFallbackStrike()
{
	const FEnemyAttack* Attack = GetCurrentAttack();
	if (!Attack || !CanAct())
	{
		EndFallbackAttack();
		return;
	}

	if (Attack->ProjectileClass)
	{
		FireProjectiles(*Attack);
	}
	else if (Attack->AreaRadius > 0.f)
	{
		ApplyAreaImpact(*Attack);
	}
	else
	{
		// Simple frontal sweep standing in for an animated swing.
		const float Reach = FMath::Max(Attack->MaxRange, 120.f);
		const FVector Center = GetActorLocation() + GetActorForwardVector() * Reach * 0.5f;

		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyFallbackStrike), false, this);
		GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn),
			FCollisionShape::MakeSphere(Reach * 0.5f + 40.f), Params);

		TSet<AActor*> Damaged;
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Victim = Overlap.GetActor();
			if (!Victim || Damaged.Contains(Victim) || !UCombatLibrary::CanDamage(this, Victim))
			{
				continue;
			}
			Damaged.Add(Victim);

			FCombatHit Hit;
			Hit.Spec = Attack->Attack.Damage;
			Hit.Instigator = this;
			Hit.DamageCauser = this;
			Hit.Target = Victim;
			Hit.ImpactPoint = UCombatLibrary::GetTargetPoint(Victim);
			Hit.HitDirection = (Victim->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
			Hit.ImpactNormal = -Hit.HitDirection;
			UCombatLibrary::ApplyDamage(Hit);
		}
	}

	GetWorldTimerManager().SetTimer(FallbackTimer, this, &AEnemyCharacter::EndFallbackAttack, FMath::Max(0.05f, Attack->FallbackRecovery), false);
}

void AEnemyCharacter::EndFallbackAttack()
{
	GetWorldTimerManager().ClearTimer(FallbackTimer);
	bFallbackAttackActive = false;
	FinishAttack();
}

// ---------------------------------------------------------------------------------------------
// Reactions / death / tick
// ---------------------------------------------------------------------------------------------

void AEnemyCharacter::HandleInterrupted(EHitReaction Reaction, bool bStaggered)
{
	Super::HandleInterrupted(Reaction, bStaggered);
	if (bFallbackAttackActive)
	{
		EndFallbackAttack();
	}
	FinishAttack();
	TelegraphAlpha = 0.f;
}

void AEnemyCharacter::HandleDeath(AActor* DeadActor, const FCombatHit& KillingHit)
{
	Super::HandleDeath(DeadActor, KillingHit);
	GetWorldTimerManager().ClearTimer(FallbackTimer);
	bFallbackAttackActive = false;
	FinishAttack();

	UClass* OrbClass = HealthOrbClass ? HealthOrbClass.Get() : AHealthOrb::StaticClass();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Index = 0; Index < HealthOrbCount; ++Index)
	{
		GetWorld()->SpawnActor<AHealthOrb>(OrbClass, GetActorLocation(), FRotator::ZeroRotator, Params);
	}

	DetachFromControllerPendingDestroy();
}

void AEnemyCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (TelegraphAlpha > 0.f)
	{
		TelegraphAlpha = FMath::Max(0.f, TelegraphAlpha - DeltaTime / TelegraphDuration);
		GetMesh()->SetScalarParameterValueOnMaterials(TelegraphParameterName, TelegraphAlpha);
	}

	if (bCharging)
	{
		const FEnemyAttack* Attack = GetCurrentAttack();
		if (!Attack || !CanAct())
		{
			bCharging = false;
			return;
		}
		UCharacterMovementComponent* Movement = GetCharacterMovement();
		Movement->Velocity.X = ChargeDirection.X * Attack->ChargeSpeed;
		Movement->Velocity.Y = ChargeDirection.Y * Attack->ChargeSpeed;
	}
}
