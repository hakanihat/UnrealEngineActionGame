#include "Abilities/DodgeComponent.h"
#include "Abilities/EnergyComponent.h"
#include "ActionGameTags.h"
#include "Characters/ActionCharacterBase.h"
#include "Combat/CombatFeedbackSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Combat/MeleeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

UDodgeComponent::UDodgeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDodgeComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<AActionCharacterBase>(GetOwner());
	if (OwnerCharacter && OwnerCharacter->GetHealthComponent())
	{
		OwnerCharacter->GetHealthComponent()->OnDamageNegated.AddDynamic(this, &UDodgeComponent::HandleDamageNegated);
	}
}

bool UDodgeComponent::TryDodge(FVector WorldDirection)
{
	if (!OwnerCharacter || bIsDodging || !OwnerCharacter->CanAct())
	{
		return false;
	}
	if (GetWorld()->GetTimeSeconds() - LastDodgeEndTime < Cooldown)
	{
		return false;
	}

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	const bool bInAir = Movement->IsFalling();
	if (bInAir && (!bAllowAirDodge || bAirDodgeUsed))
	{
		return false;
	}

	UMeleeComponent* Melee = OwnerCharacter->GetMeleeComponent();
	if (Melee && Melee->IsAttacking())
	{
		if (!Melee->CanBeCanceled())
		{
			return false; // Active frames are the one committed part of an attack.
		}
		Melee->CancelAttack();
	}

	DodgeDirection = WorldDirection.GetSafeNormal2D();
	if (DodgeDirection.IsNearlyZero())
	{
		DodgeDirection = -OwnerCharacter->GetActorForwardVector(); // Backstep.
	}

	// Free-roaming characters turn into the dodge; strafing characters (aiming/locked) dodge directionally.
	UAnimMontage* Montage = nullptr;
	if (Movement->bOrientRotationToMovement && !WorldDirection.IsNearlyZero())
	{
		OwnerCharacter->SetActorRotation(DodgeDirection.Rotation());
		Montage = DodgeMontages.Pick(EHitDirection::Front);
	}
	else
	{
		// ComputeHitDirection expects the direction a force travels; a dodge "toward" X is a force from -X.
		Montage = DodgeMontages.Pick(UHitReactionComponent::ComputeHitDirection(OwnerCharacter, -DodgeDirection));
	}
	if (Montage)
	{
		OwnerCharacter->PlayAnimMontage(Montage);
	}

	if (bInAir)
	{
		bAirDodgeUsed = true;
		Movement->Velocity.Z = 0.f;
	}

	bIsDodging = true;
	bInvulnerable = true;
	bPerfectDodgeTriggered = false;
	DodgeElapsed = 0.f;
	OwnerCharacter->AddStateTag(ActionGameTags::State_Dodging);
	OwnerCharacter->AddStateTag(ActionGameTags::State_Invulnerable);
	SetComponentTickEnabled(true);
	return true;
}

void UDodgeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bIsDodging || !OwnerCharacter)
	{
		SetComponentTickEnabled(false);
		return;
	}

	DodgeElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(DodgeElapsed / DodgeDuration, 0.f, 1.f);

	// Linear ease-out: speed = 2*(1 - t) integrates to exactly DodgeDistance over the dodge.
	const float Speed = (DodgeDistance / DodgeDuration) * 2.f * (1.f - Alpha);
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	Movement->Velocity.X = DodgeDirection.X * Speed;
	Movement->Velocity.Y = DodgeDirection.Y * Speed;
	if (Movement->IsFalling())
	{
		Movement->Velocity.Z = 0.f; // Air dash stays level.
	}

	if (bInvulnerable && DodgeElapsed >= InvulnerabilityTime)
	{
		EndInvulnerability();
	}
	if (Alpha >= 1.f || !OwnerCharacter->IsAlive())
	{
		EndDodge();
	}
}

void UDodgeComponent::EndInvulnerability()
{
	if (bInvulnerable && OwnerCharacter)
	{
		bInvulnerable = false;
		OwnerCharacter->RemoveStateTag(ActionGameTags::State_Invulnerable);
	}
}

void UDodgeComponent::EndDodge()
{
	EndInvulnerability();
	bIsDodging = false;
	LastDodgeEndTime = GetWorld()->GetTimeSeconds();
	OwnerCharacter->RemoveStateTag(ActionGameTags::State_Dodging);
	SetComponentTickEnabled(false);
}

bool UDodgeComponent::IsInDashAttackWindow() const
{
	return bIsDodging || GetWorld()->GetTimeSeconds() - LastDodgeEndTime <= DashAttackWindow;
}

void UDodgeComponent::HandleDamageNegated(const FCombatHit& Hit)
{
	if (!bIsDodging || bPerfectDodgeTriggered || DodgeElapsed > PerfectDodgeWindow)
	{
		return;
	}
	bPerfectDodgeTriggered = true;

	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		Feedback->PlaySlowMotion(PerfectDodgeTimeScale, PerfectDodgeSlowMoDuration, 0.25f);
		Feedback->AddCameraTrauma(0.2f);
	}
	if (UEnergyComponent* Energy = GetOwner()->FindComponentByClass<UEnergyComponent>())
	{
		Energy->AddEnergy(PerfectDodgeEnergyReward);
	}
	OnPerfectDodge.Broadcast();
}
