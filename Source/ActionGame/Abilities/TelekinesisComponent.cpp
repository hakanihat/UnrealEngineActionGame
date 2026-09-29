#include "Abilities/TelekinesisComponent.h"
#include "Abilities/EnergyComponent.h"
#include "Abilities/TelekineticProp.h"
#include "Abilities/TelekinesisTarget.h"
#include "Characters/ActionCharacterBase.h"
#include "Combat/CombatLibrary.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

namespace
{
	constexpr float HighlightRefreshInterval = 0.1f;
	constexpr float AimTraceDistance = 10000.f;
}

UTelekinesisComponent::UTelekinesisComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	DebrisClass = ATelekineticProp::StaticClass();
}

void UTelekinesisComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<AActionCharacterBase>(GetOwner());
	Energy = GetOwner()->FindComponentByClass<UEnergyComponent>();
}

bool UTelekinesisComponent::BeginGrab()
{
	if (!OwnerCharacter || IsBusy() || !OwnerCharacter->CanAct())
	{
		return false;
	}
	if (Energy && !Energy->HasEnergy(ThrowEnergyCost))
	{
		return false; // Checked up front so the player never grabs something they can't throw.
	}

	AActor* Target = FindBestTarget();
	if (!Target)
	{
		Target = SpawnDebris();
	}
	ITelekinesisTarget* Grabbable = Cast<ITelekinesisTarget>(Target);
	if (!Grabbable)
	{
		return false;
	}

	Grabbable->OnTelekinesisGrabbed(OwnerCharacter);
	HeldObject = Target;
	State = ETelekinesisState::Pulling;
	StateTime = 0.f;
	bThrowQueued = false;

	if (GrabMontage)
	{
		OwnerCharacter->PlayAnimMontage(GrabMontage);
	}
	return true;
}

void UTelekinesisComponent::RequestThrow(AActor* AimTarget)
{
	if (State == ETelekinesisState::Holding)
	{
		QueuedAimTarget = AimTarget;
		Throw();
	}
	else if (State == ETelekinesisState::Pulling)
	{
		QueuedAimTarget = AimTarget;
		bThrowQueued = true;
	}
}

void UTelekinesisComponent::Release()
{
	if (ITelekinesisTarget* Held = Cast<ITelekinesisTarget>(HeldObject.Get()))
	{
		Held->OnTelekinesisReleased();
	}
	Reset();
}

void UTelekinesisComponent::Reset()
{
	State = ETelekinesisState::None;
	HeldObject.Reset();
	QueuedAimTarget.Reset();
	bThrowQueued = false;
	StateTime = 0.f;
}

void UTelekinesisComponent::Throw()
{
	AActor* Object = HeldObject.Get();
	ITelekinesisTarget* Held = Cast<ITelekinesisTarget>(Object);
	if (!Held)
	{
		Reset();
		return;
	}
	if (Energy && !Energy->TryConsume(ThrowEnergyCost))
	{
		Release();
		return;
	}

	AActor* AimTarget = QueuedAimTarget.Get();
	Held->OnTelekinesisThrown(OwnerCharacter, ComputeThrowVelocity(Object->GetActorLocation(), AimTarget));

	if (ThrowMontage)
	{
		OwnerCharacter->PlayAnimMontage(ThrowMontage);
	}
	Reset();
}

FVector UTelekinesisComponent::ComputeThrowVelocity(const FVector& From, AActor* AimTarget) const
{
	FVector AimPoint;
	if (AimTarget && UCombatLibrary::IsAlive(AimTarget))
	{
		// Lead the target so throws at strafing enemies still connect.
		const FVector TargetPoint = UCombatLibrary::GetTargetPoint(AimTarget);
		const float TimeToHit = FVector::Dist(From, TargetPoint) / ThrowSpeed;
		AimPoint = TargetPoint + AimTarget->GetVelocity() * TimeToHit;
	}
	else
	{
		FVector ViewLocation = OwnerCharacter->GetActorLocation();
		FRotator ViewRotation = OwnerCharacter->GetActorRotation();
		if (const AController* Controller = OwnerCharacter->GetController())
		{
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
		const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * AimTraceDistance;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(TelekinesisAim), false, OwnerCharacter);
		Params.AddIgnoredActor(HeldObject.Get());
		FHitResult Hit;
		AimPoint = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params) ? FVector(Hit.ImpactPoint) : TraceEnd;
	}
	return (AimPoint - From).GetSafeNormal() * ThrowSpeed;
}

FVector UTelekinesisComponent::GetHoldLocation() const
{
	FRotator ViewRotation = OwnerCharacter->GetActorRotation();
	if (const AController* Controller = OwnerCharacter->GetController())
	{
		ViewRotation = Controller->GetControlRotation();
	}
	const FRotator YawOnly(0.f, ViewRotation.Yaw, 0.f);
	const float Bob = FMath::Sin(GetWorld()->GetTimeSeconds() * 3.f) * HoldBobAmplitude;
	return OwnerCharacter->GetActorLocation() + YawOnly.RotateVector(HoldOffset) + FVector(0.f, 0.f, Bob);
}

AActor* UTelekinesisComponent::FindBestTarget() const
{
	const AController* Controller = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;
	if (!Controller)
	{
		return nullptr;
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TelekinesisSearch), false, OwnerCharacter);
	GetWorld()->OverlapMultiByObjectType(Overlaps, OwnerCharacter->GetActorLocation(), FQuat::Identity, ObjectParams,
		FCollisionShape::MakeSphere(GrabRange), Params);

	AActor* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		const ITelekinesisTarget* Grabbable = Cast<ITelekinesisTarget>(Candidate);
		if (!Grabbable || !Grabbable->CanBeGrabbed(OwnerCharacter))
		{
			continue;
		}
		const FVector ToCandidate = Candidate->GetActorLocation() - ViewLocation;
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(ToCandidate.GetSafeNormal(), ViewDirection), -1.f, 1.f)));
		if (Angle > GrabAngle)
		{
			continue;
		}
		const float Score = (Angle / GrabAngle) * 0.6f + (ToCandidate.Size() / GrabRange) * 0.4f;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}
	return Best;
}

AActor* UTelekinesisComponent::SpawnDebris() const
{
	if (!DebrisClass)
	{
		return nullptr;
	}
	const FVector Forward = OwnerCharacter->GetActorForwardVector();
	const FVector Start = OwnerCharacter->GetActorLocation() + Forward * DebrisSpawnDistance;

	// Find the floor so the chunk appears to be torn out of it.
	FHitResult Floor;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DebrisFloor), false, OwnerCharacter);
	const bool bHitFloor = GetWorld()->LineTraceSingleByChannel(Floor, Start, Start - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params);
	const FVector SpawnLocation = (bHitFloor ? FVector(Floor.ImpactPoint) : Start) + FVector(0.f, 0.f, 30.f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	return GetWorld()->SpawnActor<ATelekineticProp>(DebrisClass, SpawnLocation, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), SpawnParams);
}

void UTelekinesisComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!OwnerCharacter)
	{
		return;
	}

	if (State == ETelekinesisState::None)
	{
		HighlightTimer -= DeltaTime;
		if (HighlightTimer <= 0.f)
		{
			HighlightTimer = HighlightRefreshInterval;
			HighlightedTarget = FindBestTarget();
		}
		return;
	}

	HighlightedTarget.Reset();
	AActor* Object = HeldObject.Get();
	ITelekinesisTarget* Held = Cast<ITelekinesisTarget>(Object);
	if (!Held || !OwnerCharacter->IsAlive())
	{
		Release();
		return;
	}

	StateTime += DeltaTime;
	const FVector HoldLocation = GetHoldLocation();
	Held->TelekinesisMoveTo(HoldLocation, DeltaTime);

	if (State == ETelekinesisState::Pulling)
	{
		if (FVector::Dist(Object->GetActorLocation(), HoldLocation) <= ArriveDistance)
		{
			State = ETelekinesisState::Holding;
			StateTime = 0.f;
			if (bThrowQueued)
			{
				Throw();
			}
		}
		else if (StateTime > MaxPullTime)
		{
			Release();
		}
	}
}
