#include "Abilities/KineticLaunchComponent.h"
#include "Abilities/EnergyComponent.h"
#include "ActionGameTags.h"
#include "Camera/ActionPlayerCameraManager.h"
#include "Characters/ActionCharacterBase.h"
#include "Combat/CombatLibrary.h"
#include "Combat/MeleeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

UKineticLaunchComponent::UKineticLaunchComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	ImpactDamage.Damage = 25.f;
	ImpactDamage.PoiseDamage = 45.f;
	ImpactDamage.Reaction = EHitReaction::Knockback;
	ImpactDamage.KnockbackStrength = 1100.f;
	ImpactDamage.LaunchStrength = 250.f;
	ImpactDamage.HitStop = 0.1f;
	ImpactDamage.CameraTrauma = 0.4f;
	ImpactDamage.PhysicalImpulse = 1000.f;
	ImpactDamage.DamageType = ActionGameTags::Damage_Kinetic;
}

void UKineticLaunchComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerCharacter = Cast<AActionCharacterBase>(GetOwner());
}

bool UKineticLaunchComponent::TryLaunch(AActor* Target, FVector AimDirection)
{
	if (!OwnerCharacter || bLaunching || !OwnerCharacter->CanAct())
	{
		return false;
	}
	if (Target && FVector::Dist(Target->GetActorLocation(), OwnerCharacter->GetActorLocation()) > MaxRange)
	{
		Target = nullptr;
	}
	UEnergyComponent* Energy = GetOwner()->FindComponentByClass<UEnergyComponent>();
	if (Energy && !Energy->TryConsume(EnergyCost))
	{
		return false;
	}
	if (UMeleeComponent* Melee = OwnerCharacter->GetMeleeComponent())
	{
		Melee->CancelAttack();
	}

	LaunchTarget = Target;
	FixedDestination = OwnerCharacter->GetActorLocation() + AimDirection.GetSafeNormal() * MaxRange;
	LastLocation = OwnerCharacter->GetActorLocation();
	Elapsed = 0.f;
	MaxDuration = (MaxRange / LaunchSpeed) * 1.25f;
	bLaunching = true;

	// Flying mode: no gravity and no ground friction while we steer the velocity ourselves.
	OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	OwnerCharacter->AddStateTag(ActionGameTags::State_KineticLaunch);
	OwnerCharacter->AddStateTag(ActionGameTags::State_Invulnerable);

	if (LaunchMontage)
	{
		OwnerCharacter->PlayAnimMontage(LaunchMontage);
	}
	if (const APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
	{
		if (AActionPlayerCameraManager* Camera = Cast<AActionPlayerCameraManager>(PC->PlayerCameraManager))
		{
			Camera->AddFOVKick(LaunchFOVKick); // Speed sensation.
		}
	}

	SetComponentTickEnabled(true);
	return true;
}

FVector UKineticLaunchComponent::GetDestination() const
{
	const AActor* Target = LaunchTarget.Get();
	return Target && UCombatLibrary::IsAlive(Target) ? UCombatLibrary::GetTargetPoint(Target) : FixedDestination;
}

void UKineticLaunchComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bLaunching || !OwnerCharacter)
	{
		SetComponentTickEnabled(false);
		return;
	}

	Elapsed += DeltaTime;
	const FVector Location = OwnerCharacter->GetActorLocation();
	const FVector ToDestination = GetDestination() - Location;
	const float Distance = ToDestination.Size();
	const bool bHasTarget = LaunchTarget.IsValid() && UCombatLibrary::IsAlive(LaunchTarget.Get());

	if (bHasTarget && Distance <= ImpactDistance)
	{
		Impact();
		return;
	}

	// Blocked by geometry: we moved far less than expected since last frame.
	const float Moved = FVector::Dist(Location, LastLocation);
	const bool bBlocked = Elapsed > 0.1f && DeltaTime > KINDA_SMALL_NUMBER && Moved < LaunchSpeed * DeltaTime * 0.2f;
	const bool bArrived = !bHasTarget && Distance <= LaunchSpeed * DeltaTime;

	if (bBlocked || bArrived || Elapsed >= MaxDuration || !OwnerCharacter->IsAlive())
	{
		EndLaunch(true);
		return;
	}

	LastLocation = Location;
	const FVector Direction = ToDestination.GetSafeNormal();
	OwnerCharacter->GetCharacterMovement()->Velocity = Direction * LaunchSpeed;
	OwnerCharacter->SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
}

void UKineticLaunchComponent::Impact()
{
	AActor* Target = LaunchTarget.Get();
	const FVector Direction = (GetDestination() - OwnerCharacter->GetActorLocation()).GetSafeNormal();

	FCombatHit Hit;
	Hit.Spec = ImpactDamage;
	Hit.Instigator = OwnerCharacter;
	Hit.DamageCauser = OwnerCharacter;
	Hit.Target = Target;
	Hit.ImpactPoint = UCombatLibrary::GetTargetPoint(Target) - Direction * 40.f;
	Hit.ImpactNormal = -Direction;
	Hit.HitDirection = Direction;
	UCombatLibrary::ApplyDamage(Hit);

	EndLaunch(false);

	// Bounce off: up and back, ready for a follow-up.
	const FVector Back = -Direction.GetSafeNormal2D() * BounceBackSpeed;
	OwnerCharacter->LaunchCharacter(FVector(Back.X, Back.Y, BounceUpSpeed), true, true);
	if (ImpactMontage)
	{
		OwnerCharacter->PlayAnimMontage(ImpactMontage);
	}
}

void UKineticLaunchComponent::EndLaunch(bool bKeepMomentum)
{
	bLaunching = false;
	SetComponentTickEnabled(false);

	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	Movement->Velocity = bKeepMomentum ? Movement->Velocity * MomentumKept : FVector::ZeroVector;
	Movement->SetMovementMode(MOVE_Falling);

	OwnerCharacter->RemoveStateTag(ActionGameTags::State_KineticLaunch);
	OwnerCharacter->RemoveStateTag(ActionGameTags::State_Invulnerable);
}
