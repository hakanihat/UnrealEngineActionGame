#include "Game/HealthOrb.h"
#include "Combat/CombatLibrary.h"
#include "Combat/HealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float OrbGravity = -1800.f;
	constexpr float OrbLifetime = 20.f;
}

AHealthOrb::AHealthOrb()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(10.f);
	// Only collides with level geometry, so the orb can land on floors while falling.
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	RootComponent = Collision;

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Collision);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->CastShadow = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		VisualMesh->SetStaticMesh(SphereMesh.Object);
		VisualMesh->SetRelativeScale3D(FVector(0.15f));
	}
}

void AHealthOrb::BeginPlay()
{
	Super::BeginPlay();
	SetLifeSpan(OrbLifetime);

	// Pop out in a random direction so several orbs fan out nicely.
	const FVector2D Random = FMath::RandPointInCircle(1.f);
	Velocity = FVector(Random.X * 300.f, Random.Y * 300.f, FMath::FRandRange(450.f, 650.f));
}

void AHealthOrb::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Age += DeltaTime;

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	UHealthComponent* PlayerHealth = UCombatLibrary::GetHealthComponent(Player);
	const FVector Location = GetActorLocation();

	const bool bCanMagnet = Player && PlayerHealth && PlayerHealth->IsAlive() && Age >= MagnetDelay
		&& FVector::Dist(Player->GetActorLocation(), Location) <= MagnetRadius;

	if (bCanMagnet)
	{
		const FVector ToPlayer = Player->GetActorLocation() - Location;
		if (ToPlayer.Size() <= PickupRadius)
		{
			PlayerHealth->Heal(HealAmount);
			if (PickupSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, PickupSound, Location);
			}
			Destroy();
			return;
		}
		// Accelerate toward the player while damping sideways drift, so the orb curves in.
		Velocity += ToPlayer.GetSafeNormal() * MagnetAcceleration * DeltaTime;
		Velocity = FMath::VInterpTo(Velocity, ToPlayer.GetSafeNormal() * Velocity.Size(), DeltaTime, 4.f);
	}
	else
	{
		Velocity.Z += OrbGravity * DeltaTime;
		Velocity.X = FMath::FInterpTo(Velocity.X, 0.f, DeltaTime, 2.f);
		Velocity.Y = FMath::FInterpTo(Velocity.Y, 0.f, DeltaTime, 2.f);
	}

	FHitResult Hit;
	SetActorLocation(Location + Velocity * DeltaTime, !bCanMagnet, &Hit);
	if (Hit.bBlockingHit)
	{
		Velocity = FVector::ZeroVector; // Settle on the floor until the player comes close.
	}
}
