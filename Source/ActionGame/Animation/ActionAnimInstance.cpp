#include "Animation/ActionAnimInstance.h"
#include "ActionGameTags.h"
#include "Characters/ActionCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"

void UActionAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Character = Cast<AActionCharacterBase>(TryGetPawnOwner());
	if (Character)
	{
		PreviousYaw = Character->GetActorRotation().Yaw;
	}
}

void UActionAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!Character || DeltaSeconds <= 0.f)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const FVector Velocity = Character->GetVelocity();
	const FRotator ActorRotation = Character->GetActorRotation();

	GroundSpeed = Velocity.Size2D();
	VerticalSpeed = Velocity.Z;
	bIsFalling = Movement && Movement->IsFalling();
	bShouldMove = GroundSpeed > 3.f && Movement && !Movement->GetCurrentAcceleration().IsNearlyZero();
	Direction = GroundSpeed > 3.f ? (Velocity.Rotation() - ActorRotation).GetNormalized().Yaw : 0.f;

	const FRotator AimDelta = (Character->GetBaseAimRotation() - ActorRotation).GetNormalized();
	AimPitch = AimDelta.Pitch;
	AimYaw = AimDelta.Yaw;

	// Lean proportional to turn rate, scaled by speed so idle turning doesn't lean.
	const float YawRate = FRotator::NormalizeAxis(ActorRotation.Yaw - PreviousYaw) / DeltaSeconds;
	PreviousYaw = ActorRotation.Yaw;
	const float SpeedAlpha = FMath::Clamp(GroundSpeed / 600.f, 0.f, 1.f);
	const float TargetLean = FMath::Clamp(YawRate * LeanFactor * SpeedAlpha, -MaxLeanAngle, MaxLeanAngle);
	LeanAngle = FMath::FInterpTo(LeanAngle, TargetLean, DeltaSeconds, LeanInterpSpeed);

	bIsAiming = Character->HasStateTag(ActionGameTags::State_Aiming);
	bIsLevitating = Character->HasStateTag(ActionGameTags::State_Levitating);
	bIsDodging = Character->HasStateTag(ActionGameTags::State_Dodging);
	bIsAttacking = Character->HasStateTag(ActionGameTags::State_Attacking);
	bIsDead = Character->HasStateTag(ActionGameTags::State_Dead);
	bIsStunned = !bIsDead && !Character->CanAct();
}
