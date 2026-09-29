#include "AI/EnemyAIController.h"
#include "AI/AttackTokenSubsystem.h"
#include "Characters/EnemyCharacter.h"
#include "Combat/CombatLibrary.h"
#include "Combat/MeleeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bSetControlRotationFromPawnOrientation = false;
}

AEnemyCharacter* AEnemyAIController::GetEnemy() const
{
	return Cast<AEnemyCharacter>(GetPawn());
}

void AEnemyAIController::ForceAggro()
{
	bAggro = true;
}

void AEnemyAIController::OnUnPossess()
{
	ReleaseToken();
	Super::OnUnPossess();
}

void AEnemyAIController::EnterState(EEnemyAIState NewState)
{
	if (State == NewState)
	{
		return;
	}
	State = NewState;
	StateTime = 0.f;
	bDirectMove = false;

	switch (NewState)
	{
	case EEnemyAIState::Engage:
		RepositionTimer = 0.f;
		DecisionTimer = FMath::FRandRange(0.3f, 1.f) * DecisionInterval;
		break;
	case EEnemyAIState::Attack:
		bAttackStarted = false;
		bAttackCommitted = false;
		StopMovement(); // Drop the strafe move so the approach request isn't skipped.
		break;
	case EEnemyAIState::Recover:
		StopMovement();
		break;
	case EEnemyAIState::Stunned:
	case EEnemyAIState::Dead:
	case EEnemyAIState::Idle:
		StopMovement();
		ReleaseToken();
		PendingAttack = INDEX_NONE;
		break;
	}
}

void AEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AEnemyCharacter* Enemy = GetEnemy();
	if (!Enemy)
	{
		return;
	}
	StateTime += DeltaTime;

	if (!Enemy->IsAlive())
	{
		EnterState(EEnemyAIState::Dead);
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}
	if (!Enemy->CanAct())
	{
		EnterState(EEnemyAIState::Stunned);
		return;
	}

	AActor* Target = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Target || !UCombatLibrary::IsAlive(Target))
	{
		EnterState(EEnemyAIState::Idle);
		ClearFocus(EAIFocusPriority::Gameplay);
		return;
	}
	const float Distance = FVector::Dist(Target->GetActorLocation(), Enemy->GetActorLocation());

	switch (State)
	{
	case EEnemyAIState::Idle:
		if (bAggro || Distance <= Enemy->GetAggroRange())
		{
			bAggro = true;
			if (!bEngagedOnce)
			{
				bEngagedOnce = true;
				Enemy->OnEngaged();
			}
			EnterState(EEnemyAIState::Engage);
		}
		break;
	case EEnemyAIState::Stunned:
		EnterState(EEnemyAIState::Engage);
		break;
	case EEnemyAIState::Engage:
		TickEngage(DeltaTime, Target, Distance);
		break;
	case EEnemyAIState::Attack:
		TickAttack(DeltaTime, Target, Distance);
		break;
	case EEnemyAIState::Recover:
		TickRecover(DeltaTime);
		break;
	default:
		break;
	}

	TickDirectMove();
}

void AEnemyAIController::TickEngage(float DeltaTime, AActor* Target, float Distance)
{
	AEnemyCharacter* Enemy = GetEnemy();
	SetFocus(Target);
	SetSpeed(Enemy->GetStrafeSpeed());

	// Keep a readable ring around the player, drifting sideways so enemies feel alive.
	RepositionTimer -= DeltaTime;
	if (RepositionTimer <= 0.f)
	{
		RepositionTimer = RepositionInterval * FMath::FRandRange(0.7f, 1.3f);
		if (Distance > Enemy->GetPreferredRange() * 1.6f)
		{
			SetSpeed(Enemy->GetChaseSpeed());
			MoveToTarget(Target, Enemy->GetPreferredRange());
		}
		else
		{
			PickStrafeLocation(Target);
		}
	}

	DecisionTimer -= DeltaTime;
	if (DecisionTimer > 0.f)
	{
		return;
	}
	DecisionTimer = DecisionInterval * FMath::FRandRange(0.5f, 1.5f);

	const int32 AttackIndex = Enemy->ChooseAttack();
	if (AttackIndex == INDEX_NONE)
	{
		return;
	}

	if (Enemy->UsesAttackTokens())
	{
		UAttackTokenSubsystem* Tokens = UAttackTokenSubsystem::Get(this);
		const EAttackTokenType TokenType = Enemy->IsRanged() ? EAttackTokenType::Ranged : EAttackTokenType::Melee;
		if (!Tokens || !Tokens->RequestToken(Enemy, TokenType))
		{
			return; // Someone else is attacking; keep circling.
		}
	}

	PendingAttack = AttackIndex;
	EnterState(EEnemyAIState::Attack);
}

void AEnemyAIController::TickAttack(float DeltaTime, AActor* Target, float Distance)
{
	AEnemyCharacter* Enemy = GetEnemy();

	if (!bAttackStarted)
	{
		SetFocus(Target);
		if (Enemy->IsAttackInRange(PendingAttack, Distance))
		{
			StopMovement();
			bDirectMove = false;
			bAttackStarted = Enemy->ExecuteAttack(PendingAttack, Target);
			if (!bAttackStarted)
			{
				ReleaseToken();
				EnterState(EEnemyAIState::Engage);
			}
		}
		else if (StateTime > MaxApproachTime)
		{
			ReleaseToken();
			EnterState(EEnemyAIState::Engage);
		}
		else
		{
			SetSpeed(Enemy->GetChaseSpeed());
			if (bDirectMove)
			{
				DirectMoveGoal = Target->GetActorLocation();
			}
			else if (GetMoveStatus() != EPathFollowingStatus::Moving)
			{
				// MoveToActor keeps following the target, so one request per approach is enough.
				const TArray<FEnemyAttack>& Attacks = Enemy->GetAttacks();
				const float Reach = Attacks.IsValidIndex(PendingAttack) ? Attacks[PendingAttack].MaxRange : 150.f;
				MoveToTarget(Target, Reach * 0.7f);
			}
		}
		return;
	}

	// Commit: once the active frames start, stop turning so the player can sidestep.
	if (!bAttackCommitted && Enemy->GetMeleeComponent()->IsHitWindowActive())
	{
		bAttackCommitted = true;
		ClearFocus(EAIFocusPriority::Gameplay);
	}

	if (!Enemy->IsExecutingAttack())
	{
		ReleaseToken();
		RecoverTimer = Enemy->GetRecoveryTime();
		EnterState(EEnemyAIState::Recover);
	}
}

void AEnemyAIController::TickRecover(float DeltaTime)
{
	RecoverTimer -= DeltaTime;
	if (RecoverTimer <= 0.f)
	{
		EnterState(EEnemyAIState::Engage);
	}
}

void AEnemyAIController::PickStrafeLocation(const AActor* Target)
{
	const AEnemyCharacter* Enemy = GetEnemy();
	if (FMath::FRand() < 0.3f)
	{
		StrafeSide = -StrafeSide;
	}
	const FVector FromTarget = (Enemy->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
	const float Angle = FMath::FRandRange(20.f, 45.f) * StrafeSide;
	const FVector Offset = FromTarget.RotateAngleAxis(Angle, FVector::UpVector) * Enemy->GetPreferredRange() * FMath::FRandRange(0.9f, 1.15f);
	MoveToward(Target->GetActorLocation() + Offset, 50.f);
}

void AEnemyAIController::MoveToward(const FVector& Destination, float AcceptanceRadius)
{
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Destination, AcceptanceRadius, true, true, true);
	bDirectMove = Result == EPathFollowingRequestResult::Failed;
	DirectMoveGoal = Destination;
	DirectMoveAcceptance = AcceptanceRadius;
}

void AEnemyAIController::MoveToTarget(AActor* Target, float AcceptanceRadius)
{
	const EPathFollowingRequestResult::Type Result = MoveToActor(Target, AcceptanceRadius, true, true, true);
	bDirectMove = Result == EPathFollowingRequestResult::Failed;
	DirectMoveGoal = Target->GetActorLocation();
	DirectMoveAcceptance = AcceptanceRadius;
}

void AEnemyAIController::TickDirectMove()
{
	APawn* ControlledPawn = GetPawn();
	if (!bDirectMove || !ControlledPawn)
	{
		return;
	}
	const FVector ToGoal = DirectMoveGoal - ControlledPawn->GetActorLocation();
	if (ToGoal.Size2D() <= DirectMoveAcceptance)
	{
		bDirectMove = false;
		return;
	}
	ControlledPawn->AddMovementInput(ToGoal.GetSafeNormal2D());
}

void AEnemyAIController::SetSpeed(float Speed)
{
	if (const AEnemyCharacter* Enemy = GetEnemy())
	{
		Enemy->GetCharacterMovement()->MaxWalkSpeed = Speed;
	}
}

void AEnemyAIController::ReleaseToken()
{
	if (UAttackTokenSubsystem* Tokens = UAttackTokenSubsystem::Get(this))
	{
		Tokens->ReleaseToken(GetPawn());
	}
}
