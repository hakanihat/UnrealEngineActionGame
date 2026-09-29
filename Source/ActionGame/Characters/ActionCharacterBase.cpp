#include "Characters/ActionCharacterBase.h"
#include "ActionGame.h"
#include "ActionGameTags.h"
#include "Combat/HealthComponent.h"
#include "Combat/HitReactionComponent.h"
#include "Combat/MeleeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"

AActionCharacterBase::AActionCharacterBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("Health"));
	HitReactionComponent = CreateDefaultSubobject<UHitReactionComponent>(TEXT("HitReaction"));
	MeleeComponent = CreateDefaultSubobject<UMeleeComponent>(TEXT("Melee"));
	PhysicalAnimationComponent = CreateDefaultSubobject<UPhysicalAnimationComponent>(TEXT("PhysicalAnimation"));

	// Capsules ignore the Weapon channel so bullets and blades hit the mesh's physics bodies,
	// which gives us the exact bone that was struck (headshots, location-aware flinches).
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);

	USkeletalMeshComponent* MeshComp = GetMesh();
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); // Needed for partial ragdoll hit reactions.
	MeshComp->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);
	MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);     // Simulated limbs never fight capsules.
	MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	MeshComp->SetGenerateOverlapEvents(false);

	// Snappy, responsive movement baseline for an action game. Tune per character in Blueprints.
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 720.f, 0.f);
	Movement->MaxAcceleration = 2400.f;
	Movement->BrakingDecelerationWalking = 2400.f;
	Movement->GroundFriction = 8.f;
	Movement->GravityScale = 1.75f;   // Heavier gravity = less floaty jumps.
	Movement->JumpZVelocity = 800.f;
	Movement->AirControl = 0.45f;

	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

void AActionCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	HealthComponent->OnDeath.AddDynamic(this, &AActionCharacterBase::HandleDeath);
	HitReactionComponent->OnHitReaction.AddDynamic(this, &AActionCharacterBase::HandleInterrupted);
}

FVector AActionCharacterBase::GetTargetPoint() const
{
	const USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp && !TargetPointBone.IsNone() && MeshComp->GetBoneIndex(TargetPointBone) != INDEX_NONE)
	{
		return MeshComp->GetSocketLocation(TargetPointBone);
	}
	return GetActorLocation();
}

void AActionCharacterBase::NotifyDamageDealt(const FCombatHit& Hit, const FCombatDamageResult& Result)
{
	OnDamageDealt.Broadcast(Hit, Result);
}

void AActionCharacterBase::AddStateTag(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return;
	}
	int32& Count = StateTagCounts.FindOrAdd(Tag);
	if (++Count == 1)
	{
		ActiveStateTags.AddTag(Tag);
		OnStateTagChanged.Broadcast(Tag, true);
	}
}

void AActionCharacterBase::RemoveStateTag(FGameplayTag Tag)
{
	int32* Count = StateTagCounts.Find(Tag);
	if (!Count)
	{
		return;
	}
	if (--(*Count) <= 0)
	{
		StateTagCounts.Remove(Tag);
		ActiveStateTags.RemoveTag(Tag);
		OnStateTagChanged.Broadcast(Tag, false);
	}
}

bool AActionCharacterBase::IsAlive() const
{
	return HealthComponent && HealthComponent->IsAlive();
}

bool AActionCharacterBase::CanAct() const
{
	static const FGameplayTagContainer BlockingTags = FGameplayTagContainer::CreateFromArray(TArray<FGameplayTag>{
		ActionGameTags::State_Dead,
		ActionGameTags::State_HitStun,
		ActionGameTags::State_Staggered,
		ActionGameTags::State_KnockedDown });

	return IsAlive() && !ActiveStateTags.HasAnyExact(BlockingTags);
}

void AActionCharacterBase::HandleDeath(AActor* DeadActor, const FCombatHit& KillingHit)
{
	AddStateTag(ActionGameTags::State_Dead);
	MeleeComponent->CancelAttack();
	GetCharacterMovement()->DisableMovement();
}

void AActionCharacterBase::HandleInterrupted(EHitReaction Reaction, bool bStaggered)
{
	MeleeComponent->CancelAttack();
}
