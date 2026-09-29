#include "Abilities/TelekineticProp.h"
#include "ActionGameTags.h"
#include "Combat/CombatFeedbackSubsystem.h"
#include "Combat/CombatLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float ThrownTimeout = 3.f;
}

ATelekineticProp::ATelekineticProp()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->BodyInstance.bUseCCD = true; // Fast throws must not tunnel through thin enemies.
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// Placeholder shape so the prop works out of the box. Swap the mesh in a Blueprint subclass.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
		Mesh->SetRelativeScale3D(FVector(0.6f));
	}

	ImpactDamage.Damage = 40.f;
	ImpactDamage.PoiseDamage = 55.f;
	ImpactDamage.Reaction = EHitReaction::Knockback;
	ImpactDamage.KnockbackStrength = 900.f;
	ImpactDamage.LaunchStrength = 200.f;
	ImpactDamage.HitStop = 0.12f;
	ImpactDamage.CameraTrauma = 0.35f;
	ImpactDamage.PhysicalImpulse = 1200.f;
	ImpactDamage.DamageType = ActionGameTags::Damage_Kinetic;
}

void ATelekineticProp::BeginPlay()
{
	Super::BeginPlay();
	Mesh->OnComponentHit.AddDynamic(this, &ATelekineticProp::HandleHit);
}

void ATelekineticProp::SetHeldCollision(bool bHeld)
{
	// Held props ignore pawns so they never bump the player or block enemy paths.
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, bHeld ? ECR_Ignore : ECR_Block);
}

void ATelekineticProp::OnTelekinesisGrabbed(AActor* Grabber)
{
	State = ETelekineticPropState::Held;
	ThrowInstigator = Grabber;
	SpinAxis = FMath::VRand();
	Mesh->SetSimulatePhysics(true);
	Mesh->SetEnableGravity(false);
	SetHeldCollision(true);
	Mesh->WakeRigidBody();
}

void ATelekineticProp::TelekinesisMoveTo(const FVector& HoldLocation, float DeltaTime)
{
	// Spring toward the hold point by setting velocity directly: stable and frame-rate independent.
	const FVector Desired = (HoldLocation - GetActorLocation()) * FollowStiffness;
	Mesh->SetPhysicsLinearVelocity(Desired.GetClampedToMaxSize(MaxFollowSpeed));
	Mesh->SetPhysicsAngularVelocityInDegrees(SpinAxis * HoldSpinSpeed);
}

void ATelekineticProp::OnTelekinesisThrown(AActor* Thrower, const FVector& Velocity)
{
	State = ETelekineticPropState::Thrown;
	ThrowInstigator = Thrower;
	Mesh->SetEnableGravity(true);
	SetHeldCollision(false);
	Mesh->SetPhysicsLinearVelocity(Velocity);
	Mesh->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * 720.f);

	GetWorldTimerManager().SetTimer(ThrownTimeoutHandle, this, &ATelekineticProp::ReturnToIdle, ThrownTimeout, false);
}

void ATelekineticProp::OnTelekinesisReleased()
{
	ReturnToIdle();
}

void ATelekineticProp::ReturnToIdle()
{
	State = ETelekineticPropState::Idle;
	Mesh->SetEnableGravity(true);
	SetHeldCollision(false);
	GetWorldTimerManager().ClearTimer(ThrownTimeoutHandle);
}

void ATelekineticProp::HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (State != ETelekineticPropState::Thrown || OtherActor == ThrowInstigator)
	{
		return;
	}

	const FVector Velocity = GetVelocity();
	const float Speed = Velocity.Size();
	if (Speed < MinDamageSpeed)
	{
		ReturnToIdle();
		return;
	}

	if (OtherActor && UCombatLibrary::CanDamage(ThrowInstigator, OtherActor) && UCombatLibrary::IsAlive(OtherActor))
	{
		FCombatHit CombatHit;
		CombatHit.Spec = ImpactDamage;
		CombatHit.Instigator = ThrowInstigator;
		CombatHit.DamageCauser = this;
		CombatHit.Target = OtherActor;
		CombatHit.ImpactPoint = Hit.ImpactPoint;
		CombatHit.ImpactNormal = Hit.ImpactNormal;
		CombatHit.HitDirection = Velocity.GetSafeNormal();
		CombatHit.BoneName = Hit.BoneName;
		CombatHit.DamageMultiplier = FMath::Clamp(Speed / ReferenceSpeed, 0.5f, 1.5f);
		UCombatLibrary::ApplyDamage(CombatHit);
	}
	else if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		// Hitting the environment still deserves a thud.
		Feedback->AddCameraTraumaAtLocation(Hit.ImpactPoint, 0.15f, 1500.f);
	}

	if (bShatterOnImpact)
	{
		Shatter();
	}
	else
	{
		ReturnToIdle();
	}
}

void ATelekineticProp::Shatter()
{
	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		Feedback->PlayImpactEffect(ShatterEffect, GetActorLocation(), GetActorRotation());
	}
	Destroy();
}
