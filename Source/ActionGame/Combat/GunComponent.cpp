#include "Combat/GunComponent.h"
#include "ActionGame.h"
#include "ActionGameTags.h"
#include "Combat/CombatFeedbackSubsystem.h"
#include "Combat/CombatLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

UGunComponent::UGunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	BulletDamage.Damage = 6.f;
	BulletDamage.PoiseDamage = 4.f;
	BulletDamage.Reaction = EHitReaction::None;
	BulletDamage.HitStop = 0.f;
	BulletDamage.CameraTrauma = 0.f;
	BulletDamage.PhysicalImpulse = 250.f;
	BulletDamage.DamageType = ActionGameTags::Damage_Bullet;
}

void UGunComponent::BeginPlay()
{
	Super::BeginPlay();
	Ammo = MaxAmmo;
	CurrentSpread = BaseSpread;
}

void UGunComponent::SetMuzzles(const TArray<USceneComponent*>& InMuzzles)
{
	Muzzles.Reset();
	for (USceneComponent* Muzzle : InMuzzles)
	{
		Muzzles.Add(Muzzle);
	}
}

void UGunComponent::StartFiring()
{
	bWantsToFire = true;
}

void UGunComponent::StopFiring()
{
	bWantsToFire = false;
}

void UGunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ShotCooldown = FMath::Max(0.f, ShotCooldown - DeltaTime);
	CurrentSpread = FMath::Max(BaseSpread, CurrentSpread - SpreadRecovery * DeltaTime);

	const float Now = GetWorld()->GetTimeSeconds();
	if (!bWantsToFire && Now - LastShotTime >= AmmoRegenDelay)
	{
		Ammo = FMath::Min(MaxAmmo, Ammo + AmmoRegenRate * DeltaTime);
	}

	// Loop handles very high fire rates at low frame rates without losing shots.
	while (bWantsToFire && ShotCooldown <= 0.f)
	{
		if (Ammo < 1.f)
		{
			if (EmptySound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, EmptySound, GetOwner()->GetActorLocation());
			}
			ShotCooldown = 0.25f;
			break;
		}
		FireShot();
		ShotCooldown += 1.f / FireRate;
	}
}

FVector UGunComponent::GetMuzzleLocation(int32 Index) const
{
	if (Muzzles.IsValidIndex(Index) && Muzzles[Index])
	{
		const USceneComponent* Muzzle = Muzzles[Index];
		return Muzzle->DoesSocketExist(MuzzleSocket) ? Muzzle->GetSocketLocation(MuzzleSocket) : Muzzle->GetComponentLocation();
	}
	// No gun meshes yet: shoot from chest height.
	return GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 40.f + FVector(0.f, 0.f, 50.f);
}

void UGunComponent::FireShot()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}

	Ammo -= 1.f;
	LastShotTime = GetWorld()->GetTimeSeconds();

	const int32 MuzzleIndex = NextMuzzle;
	NextMuzzle = Muzzles.Num() > 1 ? (NextMuzzle + 1) % Muzzles.Num() : 0;

	// 1) Find what the crosshair is pointing at (with spread).
	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ShotDirection = FMath::VRandCone(ViewRotation.Vector(), FMath::DegreesToRadians(CurrentSpread));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GunTrace), false, GetOwner());
	Params.bReturnPhysicalMaterial = true;

	FHitResult ViewHit;
	const FVector ViewEnd = ViewLocation + ShotDirection * Range;
	const FVector AimPoint = GetWorld()->LineTraceSingleByChannel(ViewHit, ViewLocation, ViewEnd, ECC_Weapon, Params)
		? FVector(ViewHit.ImpactPoint) : ViewEnd;

	// 2) Trace from the muzzle to that point, so cover right in front of the gun still blocks.
	const FVector MuzzleLocation = GetMuzzleLocation(MuzzleIndex);
	const FVector MuzzleToAim = (AimPoint - MuzzleLocation).GetSafeNormal();
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, MuzzleLocation, MuzzleLocation + MuzzleToAim * Range, ECC_Weapon, Params);
	const FVector EndPoint = bHit ? FVector(Hit.ImpactPoint) : MuzzleLocation + MuzzleToAim * Range;

	bool bHitCharacter = false;
	if (bHit)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor && UCombatLibrary::GetHealthComponent(HitActor))
		{
			FCombatHit CombatHit;
			CombatHit.Spec = BulletDamage;
			CombatHit.Instigator = GetOwner();
			CombatHit.DamageCauser = nullptr; // Not a contact hit: the shooter must never be hit-stopped.
			CombatHit.Target = HitActor;
			CombatHit.ImpactPoint = Hit.ImpactPoint;
			CombatHit.ImpactNormal = Hit.ImpactNormal;
			CombatHit.HitDirection = MuzzleToAim;
			CombatHit.BoneName = Hit.BoneName;
			CombatHit.bCritical = CriticalBones.Contains(Hit.BoneName);
			CombatHit.DamageMultiplier = CombatHit.bCritical ? CriticalMultiplier : 1.f;

			bHitCharacter = UCombatLibrary::ApplyDamage(CombatHit).bApplied;
		}
		else if (UPrimitiveComponent* HitComponent = Hit.GetComponent(); HitComponent && HitComponent->IsSimulatingPhysics())
		{
			// Shooting loose objects moves them: cheap, and makes the world feel reactive.
			HitComponent->AddImpulseAtLocation(MuzzleToAim * BulletDamage.PhysicalImpulse, Hit.ImpactPoint, Hit.BoneName);
		}
	}

	CurrentSpread = FMath::Min(MaxSpread, CurrentSpread + SpreadPerShot);
	ApplyRecoil();
	PlayShotEffects(MuzzleLocation, EndPoint, bHit ? &Hit : nullptr, bHitCharacter);
	OnShotFired.Broadcast(MuzzleIndex, bHitCharacter);
}

void UGunComponent::ApplyRecoil()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}
	// Negative pitch input looks up with the default (non-inverted) setup.
	Pawn->AddControllerPitchInput(-RecoilPitch);
	Pawn->AddControllerYawInput(FMath::FRandRange(-RecoilYawJitter, RecoilYawJitter));

	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		Feedback->AddCameraTrauma(CameraTraumaPerShot);
	}
}

void UGunComponent::PlayShotEffects(const FVector& MuzzleLocation, const FVector& EndPoint, const FHitResult* Hit, bool bHitCharacter)
{
	const FRotator ShotRotation = (EndPoint - MuzzleLocation).Rotation();

	if (MuzzleFlash)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MuzzleFlash, MuzzleLocation, ShotRotation, FVector(1.f), true, true, ENCPoolMethod::AutoRelease);
	}
	if (Tracer)
	{
		if (UNiagaraComponent* TracerComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Tracer, MuzzleLocation, ShotRotation,
			FVector(1.f), true, true, ENCPoolMethod::AutoRelease))
		{
			TracerComponent->SetVariableVec3(TracerEndParameter, EndPoint);
		}
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation, 1.f, FMath::FRandRange(0.95f, 1.05f));
	}
	// Character impacts are handled by the combat feedback pipeline.
	if (Hit && !bHitCharacter)
	{
		if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
		{
			Feedback->PlayImpactEffect(WorldImpact, Hit->ImpactPoint, Hit->ImpactNormal.Rotation());
		}
	}
}
