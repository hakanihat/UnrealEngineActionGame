#include "Combat/MeleeComponent.h"
#include "ActionGame.h"
#include "ActionGameTags.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/ActionCharacterBase.h"
#include "Combat/CombatLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"

UMeleeComponent::UMeleeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Trace after animation has moved the weapon this frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UMeleeComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<ACharacter>(GetOwner());

	if (!TraceComponent && OwnerCharacter)
	{
		SetTraceSource(OwnerCharacter->GetMesh(), DefaultStartSocket, DefaultEndSocket);
	}
}

void UMeleeComponent::SetTraceSource(UPrimitiveComponent* Component, FName StartSocket, FName EndSocket)
{
	TraceComponent = Component;
	TraceStartSocket = StartSocket;
	TraceEndSocket = EndSocket;
}

// ---------------------------------------------------------------------------------------------
// Attack flow
// ---------------------------------------------------------------------------------------------

bool UMeleeComponent::RequestAttack(EMeleeAttackKind Kind, AActor* Target, FVector DesiredFacing)
{
	if (!Moveset || !OwnerCharacter)
	{
		return false;
	}

	if (bIsAttacking)
	{
		// Remember the press; it fires as soon as the combo window opens (or now, if it's open).
		bHasBufferedInput = true;
		BufferedKind = Kind;
		BufferedTarget = Target;
		BufferedFacing = DesiredFacing;
		BufferedTime = GetWorld()->GetTimeSeconds();
		TryConsumeBufferedInput();
		return true;
	}

	const FMeleeAttack* Attack = SelectAttack(Kind);
	return Attack && StartAttack(*Attack, Kind, Target, DesiredFacing);
}

bool UMeleeComponent::PerformAttack(const FMeleeAttack& Attack, AActor* Target)
{
	const FVector Facing = Target ? (Target->GetActorLocation() - GetOwner()->GetActorLocation()) : GetOwner()->GetActorForwardVector();
	return StartAttack(Attack, EMeleeAttackKind::Light, Target, Facing);
}

const FMeleeAttack* UMeleeComponent::SelectAttack(EMeleeAttackKind Kind)
{
	switch (Kind)
	{
	case EMeleeAttackKind::Light:
		if (Moveset->LightCombo.Num() > 0)
		{
			const int32 Index = LightIndex % Moveset->LightCombo.Num();
			LightIndex = Index + 1;
			return &Moveset->LightCombo[Index];
		}
		break;

	case EMeleeAttackKind::Heavy:
		if (Moveset->HeavyFinishers.Num() > 0)
		{
			const int32 Index = FMath::Min(LightIndex, Moveset->HeavyFinishers.Num() - 1);
			LightIndex = 0; // A finisher ends the chain.
			return &Moveset->HeavyFinishers[Index];
		}
		break;

	case EMeleeAttackKind::Air:
		if (Moveset->AirCombo.Num() > 0)
		{
			const int32 Index = AirIndex % Moveset->AirCombo.Num();
			AirIndex = Index + 1;
			return &Moveset->AirCombo[Index];
		}
		break;

	case EMeleeAttackKind::Dash:
		if (Moveset->DashAttack.Montage)
		{
			LightIndex = 1; // Flow straight into the second light attack.
			return &Moveset->DashAttack;
		}
		// No dash attack authored: fall back to the light combo.
		return SelectAttack(EMeleeAttackKind::Light);
	}
	return nullptr;
}

bool UMeleeComponent::StartAttack(const FMeleeAttack& Attack, EMeleeAttackKind Kind, AActor* Target, const FVector& DesiredFacing)
{
	if (!OwnerCharacter || !Attack.Montage)
	{
		UE_LOG(LogActionGame, Warning, TEXT("%s: melee attack has no montage assigned."), *GetNameSafe(GetOwner()));
		return false;
	}

	// Chaining: close the previous attack's windows without ending the attacking state.
	EndHitWindow();
	CloseComboWindow();
	bHasBufferedInput = false;
	RestoreGravity();

	CurrentAttack = Attack;
	const int32 ThisAttackId = ++AttackId;

	if (OwnerCharacter->PlayAnimMontage(Attack.Montage, Attack.PlayRate) <= 0.f)
	{
		if (bIsAttacking)
		{
			FinishAttack(true); // The previous attack's end callback was invalidated above.
		}
		return false;
	}

	if (UAnimInstance* AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance())
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &UMeleeComponent::HandleMontageEnded, ThisAttackId);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, Attack.Montage);
	}

	if (!bIsAttacking)
	{
		bIsAttacking = true;
		if (AActionCharacterBase* ActionOwner = Cast<AActionCharacterBase>(OwnerCharacter))
		{
			ActionOwner->AddStateTag(ActionGameTags::State_Attacking);
		}
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (Movement && Movement->IsFalling() && Attack.AirGravityScale >= 0.f)
	{
		SavedGravityScale = Movement->GravityScale;
		Movement->GravityScale = Attack.AirGravityScale;
		Movement->Velocity.Z = FMath::Max(Movement->Velocity.Z, 0.f);
	}

	StartMagnetism(Target, DesiredFacing);
	OnAttackStarted.Broadcast(Kind);
	RefreshTickEnabled();
	return true;
}

void UMeleeComponent::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 EndedAttackId)
{
	// A newer attack interrupted this one as part of a combo; nothing to clean up.
	if (EndedAttackId != AttackId)
	{
		return;
	}
	FinishAttack(bInterrupted);
}

void UMeleeComponent::CancelAttack()
{
	if (!bIsAttacking)
	{
		return;
	}
	++AttackId; // Invalidate the pending end delegate.
	if (OwnerCharacter && CurrentAttack.Montage)
	{
		OwnerCharacter->StopAnimMontage(CurrentAttack.Montage);
	}
	FinishAttack(true);
}

void UMeleeComponent::FinishAttack(bool bInterrupted)
{
	EndHitWindow();
	CloseComboWindow();
	RestoreGravity();
	bMagnetizing = false;
	bHasBufferedInput = false;

	if (bIsAttacking)
	{
		bIsAttacking = false;
		if (AActionCharacterBase* ActionOwner = Cast<AActionCharacterBase>(OwnerCharacter))
		{
			ActionOwner->RemoveStateTag(ActionGameTags::State_Attacking);
		}
	}

	// Waiting for the animation to fully finish drops the chain; the next press starts over.
	ResetCombo();
	OnAttackEnded.Broadcast(bInterrupted);
	RefreshTickEnabled();
}

void UMeleeComponent::ResetCombo()
{
	LightIndex = 0;
}

void UMeleeComponent::RestoreGravity()
{
	if (SavedGravityScale >= 0.f && OwnerCharacter)
	{
		OwnerCharacter->GetCharacterMovement()->GravityScale = SavedGravityScale;
	}
	SavedGravityScale = -1.f;
}

// ---------------------------------------------------------------------------------------------
// Combo window / input buffer
// ---------------------------------------------------------------------------------------------

void UMeleeComponent::OpenComboWindow()
{
	bComboWindowOpen = true;
	TryConsumeBufferedInput();
}

void UMeleeComponent::CloseComboWindow()
{
	bComboWindowOpen = false;
}

void UMeleeComponent::TryConsumeBufferedInput()
{
	if (!bComboWindowOpen || !bHasBufferedInput || !Moveset)
	{
		return;
	}

	bHasBufferedInput = false;
	if (GetWorld()->GetTimeSeconds() - BufferedTime > Moveset->InputBufferTime)
	{
		return; // Stale press: ignoring it avoids attacks the player no longer wants.
	}

	if (const FMeleeAttack* Attack = SelectAttack(BufferedKind))
	{
		StartAttack(*Attack, BufferedKind, BufferedTarget.Get(), BufferedFacing);
	}
}

// ---------------------------------------------------------------------------------------------
// Magnetism
// ---------------------------------------------------------------------------------------------

void UMeleeComponent::StartMagnetism(AActor* Target, const FVector& DesiredFacing)
{
	AActor* Owner = GetOwner();
	const FVector OwnerLocation = Owner->GetActorLocation();
	bMagnetizing = false;

	if (Target && CurrentAttack.MagnetismRange > 0.f)
	{
		const FVector ToTarget = (Target->GetActorLocation() - OwnerLocation).GetSafeNormal2D();
		const float Distance = FVector::Dist2D(Target->GetActorLocation(), OwnerLocation);

		if (Distance <= CurrentAttack.MagnetismRange)
		{
			Owner->SetActorRotation(ToTarget.Rotation());

			if (Distance > CurrentAttack.MagnetismStopDistance)
			{
				MagnetStart = OwnerLocation;
				MagnetEnd = OwnerLocation + ToTarget * (Distance - CurrentAttack.MagnetismStopDistance);
				MagnetElapsed = 0.f;
				bMagnetizing = true;
			}
			return;
		}
	}

	const FVector Facing = DesiredFacing.GetSafeNormal2D();
	if (!Facing.IsNearlyZero())
	{
		Owner->SetActorRotation(Facing.Rotation());
	}
}

void UMeleeComponent::TickMagnetism(float DeltaTime)
{
	if (!bMagnetizing)
	{
		return;
	}

	MagnetElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(MagnetElapsed / MagnetismDuration, 0.f, 1.f);
	FVector NewLocation = FMath::Lerp(MagnetStart, MagnetEnd, FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f));
	NewLocation.Z = GetOwner()->GetActorLocation().Z; // Let gravity/root motion own the vertical axis.

	// Sweep so we slide along walls instead of passing through them.
	GetOwner()->SetActorLocation(NewLocation, true);

	if (Alpha >= 1.f)
	{
		bMagnetizing = false;
	}
}

// ---------------------------------------------------------------------------------------------
// Hit window / tracing
// ---------------------------------------------------------------------------------------------

void UMeleeComponent::BeginHitWindow(FName StartSocketOverride, FName EndSocketOverride, float RadiusOverride)
{
	ActiveStartSocket = StartSocketOverride.IsNone() ? TraceStartSocket : StartSocketOverride;
	ActiveEndSocket = StartSocketOverride.IsNone() ? TraceEndSocket : EndSocketOverride;
	ActiveTraceRadius = RadiusOverride > 0.f ? RadiusOverride : TraceRadius;

	HitActors.Reset();
	bHitWindowActive = true;
	GatherSamplePoints(PreviousSamplePoints);

	OnHitWindowChanged.Broadcast(true);
	RefreshTickEnabled();
}

void UMeleeComponent::EndHitWindow()
{
	if (!bHitWindowActive)
	{
		return;
	}
	bHitWindowActive = false;
	PreviousSamplePoints.Reset();
	OnHitWindowChanged.Broadcast(false);
	RefreshTickEnabled();
}

void UMeleeComponent::GatherSamplePoints(TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();

	// Notify overrides may point at owner-mesh bones (e.g. a kick) that the weapon mesh doesn't have.
	const UPrimitiveComponent* Source = TraceComponent;
	if (Source && !ActiveStartSocket.IsNone() && !Source->DoesSocketExist(ActiveStartSocket) && OwnerCharacter)
	{
		Source = OwnerCharacter->GetMesh();
	}
	if (!Source)
	{
		return;
	}

	const FVector Start = ActiveStartSocket.IsNone() ? Source->GetComponentLocation() : Source->GetSocketLocation(ActiveStartSocket);
	const FVector End = (!ActiveEndSocket.IsNone() && Source->DoesSocketExist(ActiveEndSocket))
		? Source->GetSocketLocation(ActiveEndSocket)
		: Start + GetOwner()->GetActorForwardVector() * FallbackReach;

	for (int32 Index = 0; Index < TraceSamples; ++Index)
	{
		OutPoints.Add(FMath::Lerp(Start, End, static_cast<float>(Index) / (TraceSamples - 1)));
	}
}

void UMeleeComponent::TickWeaponTrace()
{
	TArray<FVector> CurrentPoints;
	GatherSamplePoints(CurrentPoints);
	if (CurrentPoints.Num() != PreviousSamplePoints.Num())
	{
		PreviousSamplePoints = CurrentPoints;
		return;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MeleeTrace), false, GetOwner());
	const FCollisionShape Shape = FCollisionShape::MakeSphere(ActiveTraceRadius);

	for (int32 Index = 0; Index < CurrentPoints.Num(); ++Index)
	{
		const FVector From = PreviousSamplePoints[Index];
		const FVector To = CurrentPoints[Index];
		const FVector SwingDirection = (To - From).GetSafeNormal();

		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByObjectType(Hits, From, To, FQuat::Identity, ObjectParams, Shape, QueryParams);

		for (const FHitResult& Hit : Hits)
		{
			ProcessHit(Hit, SwingDirection);
		}

#if ENABLE_DRAW_DEBUG
		if (bDrawDebugTraces)
		{
			DrawDebugLine(GetWorld(), From, To, Hits.Num() > 0 ? FColor::Red : FColor::Green, false, 1.f, 0, 1.f);
		}
#endif
	}

	PreviousSamplePoints = MoveTemp(CurrentPoints);
}

void UMeleeComponent::ProcessHit(const FHitResult& HitResult, const FVector& SwingDirection)
{
	AActor* HitActor = HitResult.GetActor();
	if (!HitActor || HitActors.Contains(HitActor))
	{
		return;
	}

	const FVector ImpactPoint = HitResult.bStartPenetrating ? HitResult.TraceStart : FVector(HitResult.ImpactPoint);

	// Loose physics props get knocked around by the blade.
	UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	if (HitComponent && HitComponent->IsSimulatingPhysics() && !UCombatLibrary::GetHealthComponent(HitActor))
	{
		HitActors.Add(HitActor);
		HitComponent->AddImpulseAtLocation(SwingDirection * CurrentAttack.Damage.PhysicalImpulse, ImpactPoint, NAME_None);
		return;
	}

	if (!UCombatLibrary::CanDamage(GetOwner(), HitActor) || !UCombatLibrary::IsAlive(HitActor))
	{
		return;
	}
	HitActors.Add(HitActor);

	// Mostly "away from attacker" (readable knockback), tinted by the swing so physics flinches follow the blade.
	const FVector AwayFromAttacker = (HitActor->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D();

	FCombatHit Hit;
	Hit.Spec = CurrentAttack.Damage;
	Hit.Instigator = GetOwner();
	Hit.DamageCauser = GetOwner();
	Hit.Target = HitActor;
	Hit.ImpactPoint = ImpactPoint;
	Hit.ImpactNormal = HitResult.ImpactNormal;
	Hit.HitDirection = (AwayFromAttacker + SwingDirection * 0.5f).GetSafeNormal();
	Hit.BoneName = HitResult.BoneName;

	const FCombatDamageResult Result = UCombatLibrary::ApplyDamage(Hit);
	if (Result.bApplied)
	{
		OnHitLanded.Broadcast(Hit, Result);
	}
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void UMeleeComponent::RefreshTickEnabled()
{
	SetComponentTickEnabled(bMagnetizing || bHitWindowActive);
}

void UMeleeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickMagnetism(DeltaTime);
	if (bHitWindowActive)
	{
		TickWeaponTrace();
	}
	RefreshTickEnabled();
}
