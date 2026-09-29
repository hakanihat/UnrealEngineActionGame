#include "Combat/TargetingComponent.h"
#include "Combat/CombatLibrary.h"
#include "Combat/CombatantInterface.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UTargetingComponent::UTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f; // Plenty for UI highlighting and lock validation.
}

void UTargetingComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const AController* Controller = Pawn ? Pawn->GetController() : nullptr)
	{
		Controller->GetPlayerViewPoint(OutLocation, OutRotation);
		return;
	}
	OutLocation = GetOwner()->GetActorLocation();
	OutRotation = GetOwner()->GetActorRotation();
}

void UTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (AActor* Locked = LockedTarget.Get())
	{
		const bool bTooFar = FVector::Dist(Locked->GetActorLocation(), GetOwner()->GetActorLocation()) > LockOnBreakRange;
		if (!UCombatLibrary::IsAlive(Locked) || bTooFar)
		{
			// Auto-retarget on kill keeps the fight flowing instead of dropping the camera.
			FVector ViewLocation;
			FRotator ViewRotation;
			GetViewPoint(ViewLocation, ViewRotation);
			SetLockedTarget(FindBest(ViewLocation, ViewRotation.Vector(), LockOnRange, LockOnAngle, true));
		}
	}

	SoftTarget = GetAimTarget(LockOnRange);
}

void UTargetingComponent::ToggleLockOn()
{
	if (IsLockedOn())
	{
		ClearLockOn();
		return;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);
	SetLockedTarget(FindBest(ViewLocation, ViewRotation.Vector(), LockOnRange, LockOnAngle, true));
}

void UTargetingComponent::ClearLockOn()
{
	SetLockedTarget(nullptr);
}

void UTargetingComponent::SetLockedTarget(AActor* NewTarget)
{
	if (LockedTarget.Get() != NewTarget)
	{
		LockedTarget = NewTarget;
		OnLockTargetChanged.Broadcast(NewTarget);
	}
}

void UTargetingComponent::SwitchTarget(float Direction)
{
	AActor* Current = LockedTarget.Get();
	if (!Current || FMath::IsNearlyZero(Direction))
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);
	const FVector Right = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
	const FVector Forward = ViewRotation.Vector();

	auto ScreenAngle = [&](const AActor* Actor)
	{
		const FVector ToActor = (UCombatLibrary::GetTargetPoint(Actor) - ViewLocation).GetSafeNormal();
		return FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(ToActor, Right), FVector::DotProduct(ToActor, Forward)));
	};

	const float CurrentAngle = ScreenAngle(Current);
	AActor* Best = nullptr;
	float BestDelta = TNumericLimits<float>::Max();

	TArray<AActor*> Candidates;
	GatherCandidates(LockOnRange, Candidates);
	for (AActor* Candidate : Candidates)
	{
		if (Candidate == Current)
		{
			continue;
		}
		const float Delta = (ScreenAngle(Candidate) - CurrentAngle) * FMath::Sign(Direction);
		if (Delta > 0.f && Delta < BestDelta && HasLineOfSight(ViewLocation, Candidate))
		{
			BestDelta = Delta;
			Best = Candidate;
		}
	}

	if (Best)
	{
		SetLockedTarget(Best);
	}
}

AActor* UTargetingComponent::GetMeleeTarget(FVector InputDirection) const
{
	if (AActor* Locked = LockedTarget.Get())
	{
		return Locked;
	}
	FVector Direction = InputDirection.GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		GetViewPoint(ViewLocation, ViewRotation);
		Direction = ViewRotation.Vector().GetSafeNormal2D();
	}
	return FindBest(GetOwner()->GetActorLocation(), Direction, MeleeAssistRange, MeleeAssistAngle, false);
}

AActor* UTargetingComponent::GetAimTarget(float Range) const
{
	if (AActor* Locked = LockedTarget.Get())
	{
		return Locked;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);
	return FindBest(ViewLocation, ViewRotation.Vector(), Range, AimAssistAngle, true);
}

void UTargetingComponent::GatherCandidates(float Range, TArray<AActor*>& OutCandidates) const
{
	OutCandidates.Reset();
	AActor* Owner = GetOwner();

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TargetingOverlap), false, Owner);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Owner->GetActorLocation(), FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Range), Params);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (Actor && Cast<ICombatant>(Actor) && !OutCandidates.Contains(Actor)
			&& UCombatLibrary::GetTeam(Actor) != ECombatTeam::Neutral
			&& UCombatLibrary::CanDamage(Owner, Actor) && UCombatLibrary::IsAlive(Actor))
		{
			OutCandidates.Add(Actor);
		}
	}
}

AActor* UTargetingComponent::FindBest(const FVector& Origin, const FVector& Direction, float Range, float MaxAngle, bool bRequireLineOfSight) const
{
	TArray<AActor*> Candidates;
	GatherCandidates(Range, Candidates);

	const FVector Dir = Direction.GetSafeNormal();
	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	for (AActor* Candidate : Candidates)
	{
		const FVector ToTarget = UCombatLibrary::GetTargetPoint(Candidate) - Origin;
		const float Distance = ToTarget.Size();
		if (Distance > Range)
		{
			continue;
		}
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToTarget.GetSafeNormal(), Dir), -1.f, 1.f)));
		if (Angle > MaxAngle)
		{
			continue;
		}
		const float Score = (Angle / FMath::Max(MaxAngle, 1.f)) * 0.7f + (Distance / Range) * 0.3f;
		if (Score < BestScore && (!bRequireLineOfSight || HasLineOfSight(Origin, Candidate)))
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

bool UTargetingComponent::HasLineOfSight(const FVector& From, const AActor* Target) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TargetingLOS), false, GetOwner());
	Params.AddIgnoredActor(Target);
	return !GetWorld()->LineTraceTestByChannel(From, UCombatLibrary::GetTargetPoint(Target), ECC_Visibility, Params);
}
