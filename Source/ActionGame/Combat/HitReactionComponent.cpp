#include "Combat/HitReactionComponent.h"
#include "ActionGameTags.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/ActionCharacterBase.h"
#include "Combat/HealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

UAnimMontage* FDirectionalMontages::Pick(EHitDirection Direction) const
{
	UAnimMontage* Chosen = nullptr;
	switch (Direction)
	{
	case EHitDirection::Back:  Chosen = Back; break;
	case EHitDirection::Left:  Chosen = Left; break;
	case EHitDirection::Right: Chosen = Right; break;
	default: break;
	}
	return Chosen ? Chosen : Front.Get();
}

UHitReactionComponent::UHitReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Soft motors: strong enough to pull limbs back to the pose, loose enough to visibly flinch.
	PhysicalAnimationProfile.bIsLocalSimulation = true;
	PhysicalAnimationProfile.OrientationStrength = 1000.f;
	PhysicalAnimationProfile.AngularVelocityStrength = 100.f;
	PhysicalAnimationProfile.PositionStrength = 0.f;
	PhysicalAnimationProfile.VelocityStrength = 0.f;
}

void UHitReactionComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerCharacter = Cast<ACharacter>(GetOwner());
	Mesh = OwnerCharacter ? OwnerCharacter->GetMesh() : nullptr;
	PhysicalAnimation = GetOwner()->FindComponentByClass<UPhysicalAnimationComponent>();

	if (PhysicalAnimation && Mesh)
	{
		PhysicalAnimation->SetSkeletalMeshComponent(Mesh);
		if (Mesh->GetBoneIndex(PhysicalAnimationRootBone) != INDEX_NONE)
		{
			PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(PhysicalAnimationRootBone, PhysicalAnimationProfile, false);
		}
	}

	if (UHealthComponent* Health = GetOwner()->FindComponentByClass<UHealthComponent>())
	{
		Health->OnDamageTaken.AddDynamic(this, &UHitReactionComponent::HandleDamageTaken);
		Health->OnDeath.AddDynamic(this, &UHitReactionComponent::HandleDeath);
	}
}

EHitDirection UHitReactionComponent::ComputeHitDirection(const AActor* Victim, const FVector& HitDirection)
{
	if (!Victim)
	{
		return EHitDirection::Front;
	}
	// Direction pointing back toward the attacker, in the victim's local space.
	const FVector ToAttacker = Victim->GetActorTransform().InverseTransformVectorNoScale(-HitDirection);
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(ToAttacker.Y, ToAttacker.X));

	if (FMath::Abs(Angle) <= 45.f)  return EHitDirection::Front;
	if (FMath::Abs(Angle) >= 135.f) return EHitDirection::Back;
	return Angle > 0.f ? EHitDirection::Right : EHitDirection::Left;
}

void UHitReactionComponent::HandleDamageTaken(const FCombatHit& Hit, const FCombatDamageResult& Result)
{
	if (Result.bKilled || !OwnerCharacter)
	{
		return; // Death is handled separately (ragdoll).
	}

	// Layer 1 + 2: every hit flashes and physically twitches, even if it doesn't interrupt.
	FlashAlpha = 1.f;
	QueuePhysicalHit(Hit);

	const EHitReaction Reaction = ResolveReaction(Hit, Result);
	if (Reaction == EHitReaction::None && !Result.bPoiseBroken)
	{
		RefreshTickEnabled();
		return;
	}

	// Layers 3-5: interrupt, displace, stagger.
	ApplyDisplacement(Hit, Reaction);
	PlayReaction(Hit, Reaction, Result.bPoiseBroken);
	GrantMercyInvulnerability();
	OnHitReaction.Broadcast(Reaction, Result.bPoiseBroken);
	RefreshTickEnabled();
}

EHitReaction UHitReactionComponent::ResolveReaction(const FCombatHit& Hit, const FCombatDamageResult& Result) const
{
	const AActionCharacterBase* ActionOwner = Cast<AActionCharacterBase>(GetOwner());

	// Breaking poise is the counter to super armor, so it always wins.
	if (Result.bPoiseBroken)
	{
		return EHitReaction::Heavy;
	}
	if (ActionOwner && ActionOwner->HasStateTag(ActionGameTags::State_SuperArmor))
	{
		return EHitReaction::None;
	}
	if (Hit.Spec.Reaction < InterruptThreshold)
	{
		return EHitReaction::None;
	}

	// While staggered or knocked down, only launches/knockdowns override, so a combo
	// into a staggered enemy doesn't cancel the stagger it was meant to punish.
	const bool bLongStun = ActiveStunTag == ActionGameTags::State_Staggered || ActiveStunTag == ActionGameTags::State_KnockedDown;
	if (bLongStun && Hit.Spec.Reaction < EHitReaction::Launch)
	{
		return EHitReaction::None;
	}
	return Hit.Spec.Reaction;
}

void UHitReactionComponent::ApplyDisplacement(const FCombatHit& Hit, EHitReaction Reaction)
{
	UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement();
	const float Scale = 1.f - KnockbackResistance;
	if (!Movement || Scale <= 0.f)
	{
		return;
	}

	const FVector Horizontal = Hit.HitDirection.GetSafeNormal2D() * Hit.Spec.KnockbackStrength * Scale;
	float Vertical = Hit.Spec.LaunchStrength * Scale;

	// Air juggle: any interrupting hit on an airborne target keeps it afloat.
	if (Movement->IsFalling() && Reaction != EHitReaction::Launch)
	{
		Vertical = FMath::Max(Vertical, AirJuggleLift);
	}

	if (!Horizontal.IsNearlyZero() || Vertical > 0.f)
	{
		OwnerCharacter->LaunchCharacter(FVector(Horizontal.X, Horizontal.Y, Vertical), true, Vertical > 0.f);
	}
}

void UHitReactionComponent::PlayReaction(const FCombatHit& Hit, EHitReaction Reaction, bool bStagger)
{
	const EHitDirection Direction = ComputeHitDirection(GetOwner(), Hit.HitDirection);

	UAnimMontage* Montage = nullptr;
	FGameplayTag StunTag = ActionGameTags::State_HitStun;
	float Duration = FlinchStunTime;

	if (bStagger)
	{
		Montage = StaggerMontage;
		StunTag = ActionGameTags::State_Staggered;
		Duration = StaggerTime;
	}
	else
	{
		switch (Reaction)
		{
		case EHitReaction::Flinch:
			Montage = LightHitMontages.Pick(Direction);
			Duration = FlinchStunTime;
			break;
		case EHitReaction::Heavy:
		case EHitReaction::Knockback:
			Montage = HeavyHitMontages.Pick(Direction);
			Duration = HeavyStunTime;
			break;
		case EHitReaction::Launch:
		case EHitReaction::Knockdown:
			Montage = KnockdownMontage;
			StunTag = ActionGameTags::State_KnockedDown;
			Duration = KnockdownTime;
			break;
		default:
			break;
		}
	}

	if (Montage)
	{
		const float Length = OwnerCharacter->PlayAnimMontage(Montage);
		// Stagger keeps its designed duration; other reactions last as long as their animation.
		if (!bStagger && Length > 0.f)
		{
			Duration = Length;
		}
	}
	else if (UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr)
	{
		// No reaction animation authored yet: still interrupt whatever the owner was doing.
		AnimInstance->Montage_Stop(0.1f);
	}

	BeginStun(StunTag, Duration);
}

void UHitReactionComponent::BeginStun(const FGameplayTag& StunTag, float Duration)
{
	AActionCharacterBase* ActionOwner = Cast<AActionCharacterBase>(GetOwner());
	if (!ActionOwner)
	{
		return;
	}

	if (ActiveStunTag.IsValid())
	{
		ActionOwner->RemoveStateTag(ActiveStunTag);
	}
	ActiveStunTag = StunTag;
	ActionOwner->AddStateTag(ActiveStunTag);

	GetWorld()->GetTimerManager().SetTimer(StunTimer, this, &UHitReactionComponent::EndStun, FMath::Max(Duration, 0.05f), false);
}

void UHitReactionComponent::EndStun()
{
	AActionCharacterBase* ActionOwner = Cast<AActionCharacterBase>(GetOwner());
	if (!ActionOwner || !ActiveStunTag.IsValid())
	{
		return;
	}

	const bool bWasStaggered = ActiveStunTag == ActionGameTags::State_Staggered;
	ActionOwner->RemoveStateTag(ActiveStunTag);
	ActiveStunTag = FGameplayTag();

	if (bWasStaggered)
	{
		if (UHealthComponent* Health = ActionOwner->GetHealthComponent())
		{
			Health->ResetPoise();
		}
	}
	OnRecovered.Broadcast();
}

void UHitReactionComponent::GrantMercyInvulnerability()
{
	AActionCharacterBase* ActionOwner = Cast<AActionCharacterBase>(GetOwner());
	if (PostHitInvulnerability <= 0.f || !ActionOwner || bHasMercyInvulnerability)
	{
		return;
	}

	bHasMercyInvulnerability = true;
	ActionOwner->AddStateTag(ActionGameTags::State_Invulnerable);

	TWeakObjectPtr<UHitReactionComponent> WeakThis(this);
	GetWorld()->GetTimerManager().SetTimer(MercyTimer, [WeakThis]()
	{
		if (UHitReactionComponent* Self = WeakThis.Get())
		{
			if (AActionCharacterBase* Owner = Cast<AActionCharacterBase>(Self->GetOwner()))
			{
				Owner->RemoveStateTag(ActionGameTags::State_Invulnerable);
			}
			Self->bHasMercyInvulnerability = false;
		}
	}, PostHitInvulnerability, false);
}

// ---------------------------------------------------------------------------------------------
// Physical reactions
// ---------------------------------------------------------------------------------------------

void UHitReactionComponent::QueuePhysicalHit(const FCombatHit& Hit)
{
	if (!bEnablePhysicalHits || !Mesh || !Mesh->GetPhysicsAsset() || Hit.Spec.PhysicalImpulse <= 0.f)
	{
		return;
	}

	FName Bone = Hit.BoneName;
	if (Bone.IsNone() || Mesh->GetBoneIndex(Bone) == INDEX_NONE)
	{
		Bone = Mesh->FindClosestBone(Hit.ImpactPoint, nullptr, 0.f, true);
	}

	FPendingImpulse Pending;
	Pending.Bone = Bone;
	Pending.Impulse = Hit.HitDirection.GetSafeNormal() * Hit.Spec.PhysicalImpulse;
	PendingImpulses.Add(Pending);
}

void UHitReactionComponent::ApplyPhysicalHit(const FPendingImpulse& Pending)
{
	const FName Root = FindReactionRoot(Pending.Bone);
	const FName BodyBone = FindBoneWithBody(Pending.Bone);
	if (Root.IsNone() || BodyBone.IsNone())
	{
		return;
	}

	Mesh->SetAllBodiesBelowSimulatePhysics(Root, true, true);

	FActivePhysicalHit* Active = ActivePhysicalHits.FindByPredicate([Root](const FActivePhysicalHit& Entry) { return Entry.RootBone == Root; });
	if (!Active)
	{
		Active = &ActivePhysicalHits.AddDefaulted_GetRef();
		Active->RootBone = Root;
	}
	Active->Weight = 1.f;
	Mesh->SetAllBodiesBelowPhysicsBlendWeight(Root, PhysicalBlendWeight, false, true);
	Mesh->AddImpulse(Pending.Impulse, BodyBone, true);
}

void UHitReactionComponent::TickPhysicalHits(float DeltaTime)
{
	for (int32 Index = ActivePhysicalHits.Num() - 1; Index >= 0; --Index)
	{
		FActivePhysicalHit& Entry = ActivePhysicalHits[Index];
		Entry.Weight -= DeltaTime / PhysicalBlendOutTime;

		if (Entry.Weight <= 0.f)
		{
			Mesh->SetAllBodiesBelowSimulatePhysics(Entry.RootBone, false, true);
			ActivePhysicalHits.RemoveAtSwap(Index);
		}
		else
		{
			// Ease-out: the limb snaps quickly and settles gently back into the animation.
			const float Blend = PhysicalBlendWeight * Entry.Weight * Entry.Weight;
			Mesh->SetAllBodiesBelowPhysicsBlendWeight(Entry.RootBone, Blend, false, true);
		}
	}
}

FName UHitReactionComponent::FindReactionRoot(FName Bone) const
{
	for (FName Current = Bone; !Current.IsNone(); Current = Mesh->GetParentBone(Current))
	{
		if (PhysicalReactionRoots.Contains(Current))
		{
			return Current;
		}
	}
	// Hits on the pelvis/root: flinch the upper body.
	for (const FName& Root : PhysicalReactionRoots)
	{
		if (Mesh->GetBoneIndex(Root) != INDEX_NONE)
		{
			return Root;
		}
	}
	return NAME_None;
}

FName UHitReactionComponent::FindBoneWithBody(FName Bone) const
{
	for (FName Current = Bone; !Current.IsNone(); Current = Mesh->GetParentBone(Current))
	{
		if (Mesh->GetBodyInstance(Current))
		{
			return Current;
		}
	}
	return NAME_None;
}

// ---------------------------------------------------------------------------------------------
// Death
// ---------------------------------------------------------------------------------------------

void UHitReactionComponent::HandleDeath(AActor* DeadActor, const FCombatHit& KillingHit)
{
	GetWorld()->GetTimerManager().ClearTimer(StunTimer);
	if (!OwnerCharacter || !Mesh)
	{
		return;
	}

	const FVector Knockback = KillingHit.HitDirection.GetSafeNormal() * (DeathKickSpeed + KillingHit.Spec.KnockbackStrength);
	RagdollVelocity = OwnerCharacter->GetVelocity() + Knockback + FVector(0.f, 0.f, KillingHit.Spec.LaunchStrength * 0.5f);
	RagdollKickBone = KillingHit.BoneName;
	bRagdollPending = true;
	PendingImpulses.Reset();

	if (UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (CorpseLifeSpan > 0.f)
	{
		OwnerCharacter->SetLifeSpan(CorpseLifeSpan);
	}
	RefreshTickEnabled();
}

void UHitReactionComponent::StartRagdoll()
{
	bRagdollPending = false;
	ActivePhysicalHits.Reset();

	if (PhysicalAnimation)
	{
		PhysicalAnimation->ApplyPhysicalAnimationStrengthMultiplyer(0.f);
	}
	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.f);
	}

	Mesh->SetCollisionProfileName(TEXT("Ragdoll"));
	Mesh->SetAllBodiesSimulatePhysics(true);
	Mesh->SetAllBodiesPhysicsBlendWeight(1.f);
	Mesh->WakeAllRigidBodies();
	Mesh->SetAllPhysicsLinearVelocity(RagdollVelocity, false);

	if (!RagdollKickBone.IsNone() && Mesh->GetBodyInstance(RagdollKickBone))
	{
		Mesh->AddImpulse(RagdollVelocity * 0.35f, RagdollKickBone, true);
	}
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

bool UHitReactionComponent::IsOwnerFrozen() const
{
	return GetOwner()->CustomTimeDilation < 0.99f;
}

void UHitReactionComponent::RefreshTickEnabled()
{
	const bool bNeedsTick = FlashAlpha > 0.f || ActivePhysicalHits.Num() > 0 || PendingImpulses.Num() > 0 || bRagdollPending;
	SetComponentTickEnabled(bNeedsTick);
}

void UHitReactionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Mesh)
	{
		SetComponentTickEnabled(false);
		return;
	}

	// Physics isn't slowed by per-actor time dilation, so hold impulses until hitstop ends:
	// the victim freezes on impact, then snaps. This is what sells the hit.
	if (!IsOwnerFrozen())
	{
		if (bRagdollPending)
		{
			StartRagdoll();
		}
		for (const FPendingImpulse& Pending : PendingImpulses)
		{
			ApplyPhysicalHit(Pending);
		}
		PendingImpulses.Reset();
	}

	TickPhysicalHits(DeltaTime);

	if (FlashAlpha > 0.f)
	{
		FlashAlpha = FMath::Max(0.f, FlashAlpha - DeltaTime / FlashDuration);
		Mesh->SetScalarParameterValueOnMaterials(FlashParameterName, FlashAlpha);
	}

	RefreshTickEnabled();
}
