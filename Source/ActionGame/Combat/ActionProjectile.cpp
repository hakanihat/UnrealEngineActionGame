#include "Combat/ActionProjectile.h"
#include "ActionGame.h"
#include "ActionGameTags.h"
#include "Combat/CombatFeedbackSubsystem.h"
#include "Combat/CombatLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "UObject/ConstructorHelpers.h"

AActionProjectile::AActionProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(18.f);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetGenerateOverlapEvents(true);
	RootComponent = Collision;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Collision);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(SphereMesh.Object);
		VisualMesh->SetRelativeScale3D(FVector(0.3f));
	}

	Trail = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	Trail->SetupAttachment(Collision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 0.f;
	Movement->MaxSpeed = 0.f; // No limit, so reflected projectiles can go faster.
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bAutoActivate = false;

	Damage.Damage = 12.f;
	Damage.PoiseDamage = 15.f;
	Damage.Reaction = EHitReaction::Flinch;
	Damage.KnockbackStrength = 250.f;
	Damage.HitStop = 0.05f;
	Damage.CameraTrauma = 0.25f;
	Damage.DamageType = ActionGameTags::Damage_Energy;
}

void AActionProjectile::BeginPlay()
{
	Super::BeginPlay();
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AActionProjectile::HandleOverlap);
	Movement->OnProjectileStop.AddDynamic(this, &AActionProjectile::HandleStop);
	SetLifeSpan(MaxLifetime);
}

void AActionProjectile::Launch(AActor* InInstigator, const FVector& Direction, AActor* HomingTarget)
{
	OwnerInstigator = InInstigator;
	Collision->IgnoreActorWhenMoving(InInstigator, true);

	Movement->Velocity = Direction.GetSafeNormal() * Speed;
	if (HomingTarget && HomingAcceleration > 0.f)
	{
		Movement->bIsHomingProjectile = true;
		Movement->HomingTargetComponent = HomingTarget->GetRootComponent();
		Movement->HomingAccelerationMagnitude = HomingAcceleration;
	}
	Movement->Activate(true);
	bFlying = true;
}

void AActionProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bFlying || !OtherActor || OtherActor == OwnerInstigator || !UCombatLibrary::CanDamage(OwnerInstigator, OtherActor))
	{
		return;
	}
	const FVector Location = bFromSweep ? FVector(SweepResult.ImpactPoint) : GetActorLocation();
	Explode(Location, -GetVelocity().GetSafeNormal(), OtherActor);
}

void AActionProjectile::HandleStop(const FHitResult& ImpactResult)
{
	if (bFlying)
	{
		Explode(ImpactResult.ImpactPoint, ImpactResult.ImpactNormal, nullptr);
	}
}

void AActionProjectile::Explode(const FVector& Location, const FVector& Normal, AActor* DirectHitActor)
{
	bFlying = false;
	const float Multiplier = bReflected ? ReflectDamageMultiplier : 1.f;

	if (ExplosionRadius > 0.f)
	{
		FCombatDamageSpec AreaSpec = Damage;
		AreaSpec.Damage *= Multiplier;
		AreaSpec.PoiseDamage *= Multiplier;
		UCombatLibrary::ApplyRadialDamage(this, AreaSpec, Location, ExplosionRadius, OwnerInstigator, this);
	}
	else if (DirectHitActor)
	{
		FCombatHit Hit;
		Hit.Spec = Damage;
		Hit.Instigator = OwnerInstigator;
		Hit.DamageCauser = this;
		Hit.Target = DirectHitActor;
		Hit.ImpactPoint = Location;
		Hit.ImpactNormal = Normal;
		Hit.HitDirection = GetVelocity().GetSafeNormal();
		Hit.DamageMultiplier = Multiplier;
		UCombatLibrary::ApplyDamage(Hit);
	}

	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		Feedback->PlayImpactEffect(ExplosionEffect, Location, Normal.Rotation());
	}
	Destroy();
}

bool AActionProjectile::CanBeGrabbed(const AActor* Grabber) const
{
	// Only enemy shots can be caught, and only while in flight.
	return bCanBeCaught && bFlying && !bHeld && UCombatLibrary::CanDamage(OwnerInstigator, Grabber);
}

void AActionProjectile::OnTelekinesisGrabbed(AActor* Grabber)
{
	bHeld = true;
	bFlying = false;
	Movement->StopMovementImmediately();
	Movement->Deactivate();
	Movement->bIsHomingProjectile = false;
	SetLifeSpan(0.f); // Held projectiles never expire in the player's "hand".
}

void AActionProjectile::TelekinesisMoveTo(const FVector& HoldLocation, float DeltaTime)
{
	SetActorLocation(FMath::VInterpTo(GetActorLocation(), HoldLocation, DeltaTime, 12.f));
}

void AActionProjectile::OnTelekinesisThrown(AActor* Thrower, const FVector& Velocity)
{
	bHeld = false;
	bReflected = true;
	OwnerInstigator = Thrower;
	Collision->ClearMoveIgnoreActors();
	Collision->IgnoreActorWhenMoving(Thrower, true);
	Movement->Velocity = Velocity;
	Movement->Activate(true);
	bFlying = true;
	SetLifeSpan(MaxLifetime);
}

void AActionProjectile::OnTelekinesisReleased()
{
	Destroy(); // A dropped energy bolt simply fizzles out.
}
