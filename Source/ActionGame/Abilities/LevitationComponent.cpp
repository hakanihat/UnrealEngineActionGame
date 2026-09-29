#include "Abilities/LevitationComponent.h"
#include "Abilities/EnergyComponent.h"
#include "ActionGameTags.h"
#include "Camera/ActionPlayerCameraManager.h"
#include "Characters/ActionCharacterBase.h"
#include "Combat/CombatFeedbackSubsystem.h"
#include "Combat/CombatLibrary.h"
#include "Combat/MeleeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
	constexpr float MaxSlamDiveTime = 2.5f; // Safety net if the slam never finds ground.
}

ULevitationComponent::ULevitationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SlamDamage.Damage = 35.f;
	SlamDamage.PoiseDamage = 60.f;
	SlamDamage.Reaction = EHitReaction::Launch;
	SlamDamage.KnockbackStrength = 350.f;
	SlamDamage.LaunchStrength = 900.f;
	SlamDamage.HitStop = 0.1f;
	SlamDamage.CameraTrauma = 0.45f;
	SlamDamage.PhysicalImpulse = 900.f;
	SlamDamage.DamageType = ActionGameTags::Damage_Slam;
}

void ULevitationComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<AActionCharacterBase>(GetOwner());
	Energy = GetOwner()->FindComponentByClass<UEnergyComponent>();
	if (OwnerCharacter)
	{
		OwnerCharacter->LandedDelegate.AddDynamic(this, &ULevitationComponent::HandleLanded);
	}
}

bool ULevitationComponent::StartLevitation()
{
	if (!OwnerCharacter || bLevitating || bSlamming || !OwnerCharacter->CanAct())
	{
		return false;
	}
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!Movement->IsFalling() || (Energy && !Energy->HasEnergy(MinEnergyToStart)))
	{
		return false;
	}

	bLevitating = true;
	SavedGravityScale = Movement->GravityScale;
	SavedAirControl = Movement->AirControl;
	Movement->GravityScale = 0.f;
	Movement->AirControl = HoverAirControl;
	Movement->Velocity.Z = FMath::Max(Movement->Velocity.Z, 0.f) + StartLift;

	OwnerCharacter->AddStateTag(ActionGameTags::State_Levitating);
	RefreshTickEnabled();
	return true;
}

void ULevitationComponent::StopLevitation()
{
	if (!bLevitating)
	{
		return;
	}
	bLevitating = false;

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	Movement->GravityScale = SavedGravityScale;
	Movement->AirControl = SavedAirControl;

	OwnerCharacter->RemoveStateTag(ActionGameTags::State_Levitating);
	RefreshTickEnabled();
}

bool ULevitationComponent::TryGroundSlam()
{
	if (!OwnerCharacter || bSlamming || !OwnerCharacter->CanAct())
	{
		return false;
	}
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	if (!Movement->IsFalling() || (Energy && !Energy->TryConsume(SlamEnergyCost)))
	{
		return false;
	}

	StopLevitation();
	if (UMeleeComponent* Melee = OwnerCharacter->GetMeleeComponent())
	{
		Melee->CancelAttack();
	}

	bSlamming = true;
	SlamElapsed = 0.f;
	SlamStartZ = OwnerCharacter->GetActorLocation().Z;
	Movement->Velocity = FVector(0.f, 0.f, -SlamDiveSpeed);

	// Super armor: a committed dive shouldn't be knocked out of the air by stray bullets.
	OwnerCharacter->AddStateTag(ActionGameTags::State_Slamming);
	OwnerCharacter->AddStateTag(ActionGameTags::State_SuperArmor);

	if (SlamDiveMontage)
	{
		OwnerCharacter->PlayAnimMontage(SlamDiveMontage);
	}
	RefreshTickEnabled();
	return true;
}

void ULevitationComponent::HandleLanded(const FHitResult& Hit)
{
	if (bSlamming)
	{
		PerformSlamImpact();
	}
	StopLevitation();
}

void ULevitationComponent::PerformSlamImpact()
{
	const FVector Origin = OwnerCharacter->GetActorLocation();
	const float DiveHeight = FMath::Max(0.f, SlamStartZ - Origin.Z);
	const float HeightBonus = SlamMaxHeightBonus * FMath::Clamp(DiveHeight / SlamHeightForMaxBonus, 0.f, 1.f);

	FCombatDamageSpec Spec = SlamDamage;
	Spec.Damage *= 1.f + HeightBonus;
	Spec.PoiseDamage *= 1.f + HeightBonus;
	UCombatLibrary::ApplyRadialDamage(this, Spec, Origin, SlamRadius, OwnerCharacter, OwnerCharacter);

	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		Feedback->PlayImpactEffect(SlamImpactEffect, Origin, FRotator::ZeroRotator);
		Feedback->AddCameraTrauma(0.35f + 0.3f * HeightBonus);
		Feedback->ApplyHitStop(OwnerCharacter, 0.08f);
	}
	if (const APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
	{
		if (AActionPlayerCameraManager* Camera = Cast<AActionPlayerCameraManager>(PC->PlayerCameraManager))
		{
			Camera->AddFOVKick(-6.f); // Brief zoom punch on impact.
		}
	}
	if (SlamLandMontage)
	{
		OwnerCharacter->PlayAnimMontage(SlamLandMontage);
	}
	EndSlam();
}

void ULevitationComponent::EndSlam()
{
	if (!bSlamming)
	{
		return;
	}
	bSlamming = false;
	OwnerCharacter->RemoveStateTag(ActionGameTags::State_Slamming);
	OwnerCharacter->RemoveStateTag(ActionGameTags::State_SuperArmor);
	RefreshTickEnabled();
}

void ULevitationComponent::RefreshTickEnabled()
{
	SetComponentTickEnabled(bLevitating || bSlamming);
}

void ULevitationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!OwnerCharacter)
	{
		return;
	}
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();

	if (bSlamming)
	{
		SlamElapsed += DeltaTime;
		Movement->Velocity = FVector(0.f, 0.f, -SlamDiveSpeed);
		if (SlamElapsed > MaxSlamDiveTime || !OwnerCharacter->IsAlive())
		{
			EndSlam();
		}
		return;
	}

	if (bLevitating)
	{
		const bool bOutOfEnergy = Energy && !Energy->Drain(EnergyDrainPerSecond * DeltaTime);
		if (bOutOfEnergy || !OwnerCharacter->CanAct() || !Movement->IsFalling())
		{
			StopLevitation();
			return;
		}
		Movement->Velocity.Z = FMath::FInterpTo(Movement->Velocity.Z, HoverVerticalSpeed, DeltaTime, HoverVerticalDamping);
	}
}
