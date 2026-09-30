#include "Characters/ActionCharacterBase.h"
#include "ActionGame.h"
#include "ActionGameTags.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Combat/HealthComponent.h"
#include "Combat/HitReactionComponent.h"
#include "Combat/MeleeComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
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

namespace
{
	/** First asset of Class whose name starts with Prefix (e.g. "SKM_Manny" also matches "SKM_Manny_Simple"). */
	FAssetData FindAssetByNamePrefix(const FTopLevelAssetPath& Class, const FString& Prefix)
	{
		TArray<FAssetData> Assets;
		IAssetRegistry::GetChecked().GetAssetsByClass(Class, Assets);

		const FAssetData* Best = nullptr;
		for (const FAssetData& Asset : Assets)
		{
			const FString Name = Asset.AssetName.ToString();
			if (Name.StartsWith(Prefix) && (!Best || Name.Len() < Best->AssetName.ToString().Len()))
			{
				Best = &Asset; // Shortest match = the plain variant.
			}
		}
		return Best ? *Best : FAssetData();
	}

	/** Finds the main locomotion Animation Blueprint made for Skeleton. */
	UClass* FindAnimClassForSkeleton(const USkeleton* Skeleton)
	{
		if (!Skeleton)
		{
			return nullptr;
		}
		TArray<FAssetData> Assets;
		IAssetRegistry::GetChecked().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("AnimBlueprint")), Assets);

		const FString SkeletonPath = Skeleton->GetPathName();
		const FAssetData* Best = nullptr;
		int32 BestScore = -1;
		for (const FAssetData& Asset : Assets)
		{
			FString TargetSkeleton;
			if (!Asset.GetTagValue(TEXT("TargetSkeleton"), TargetSkeleton) || !TargetSkeleton.Contains(SkeletonPath))
			{
				continue;
			}
			// Skip helper graphs (post-process, linked layers); prefer the template's main ABP.
			const FString Name = Asset.AssetName.ToString();
			if (Name.Contains(TEXT("PostProcess")) || Name.Contains(TEXT("Layer")))
			{
				continue;
			}
			const int32 Score = Name.Contains(TEXT("Unarmed")) ? 3 : (Name.Contains(TEXT("Manny")) || Name.Contains(TEXT("Quinn"))) ? 2 : 1;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = &Asset;
			}
		}
		return Best ? LoadObject<UClass>(nullptr, *(Best->GetObjectPathString() + TEXT("_C"))) : nullptr;
	}
}

void AActionCharacterBase::BeginPlay()
{
	// Before Super: components (hit reactions, melee) read the skeleton in their own BeginPlay.
	ApplyDefaultVisualsIfNeeded();

	Super::BeginPlay();

	HealthComponent->OnDeath.AddDynamic(this, &AActionCharacterBase::HandleDeath);
	HitReactionComponent->OnHitReaction.AddDynamic(this, &AActionCharacterBase::HandleInterrupted);
}

void AActionCharacterBase::ApplyDefaultVisualsIfNeeded()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp->GetSkeletalMeshAsset())
	{
		return; // A Blueprint already assigned a character: respect it.
	}

	// 1) Use the UE5 Mannequins if the Third Person content pack is in the project:
	//    Manny for the player, Quinn for enemies.
	const FString MeshPrefix = Team == ECombatTeam::Player ? TEXT("SKM_Manny") : TEXT("SKM_Quinn");
	const FAssetData MeshAsset = FindAssetByNamePrefix(USkeletalMesh::StaticClass()->GetClassPathName(), MeshPrefix);
	if (USkeletalMesh* Mannequin = Cast<USkeletalMesh>(MeshAsset.GetAsset()))
	{
		MeshComp->SetSkeletalMeshAsset(Mannequin);
		if (UClass* AnimClass = FindAnimClassForSkeleton(Mannequin->GetSkeleton()))
		{
			MeshComp->SetAnimInstanceClass(AnimClass);
		}
		return;
	}

	// 2) Otherwise show a coloured capsule so the game is still playable with zero art.
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!Cylinder)
	{
		return;
	}

	UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(this, TEXT("PlaceholderBody"));
	PlaceholderBody = Body;
	Body->SetStaticMesh(Cylinder);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetupAttachment(GetCapsuleComponent());
	Body->RegisterComponent();

	// The engine cylinder is 100 x 100 units; stretch it to the capsule.
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float Diameter = Capsule->GetUnscaledCapsuleRadius() * 2.f / 100.f;
	const float Height = Capsule->GetUnscaledCapsuleHalfHeight() * 2.f / 100.f;
	Body->SetRelativeScale3D(FVector(Diameter, Diameter, Height));
	Body->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(PlaceholderColor.R, PlaceholderColor.G, PlaceholderColor.B));
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

	// Placeholder bodies topple over along the killing blow (skeletal meshes ragdoll instead).
	if (PlaceholderBody)
	{
		PlaceholderBody->SetCollisionProfileName(TEXT("PhysicsActor"));
		PlaceholderBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		PlaceholderBody->SetSimulatePhysics(true);
		const FVector Kick = KillingHit.HitDirection.GetSafeNormal() * (500.f + KillingHit.Spec.KnockbackStrength)
			+ FVector(0.f, 0.f, 200.f + KillingHit.Spec.LaunchStrength);
		PlaceholderBody->AddImpulseAtLocation(Kick * PlaceholderBody->GetMass(), GetActorLocation() + FVector(0.f, 0.f, 60.f));
	}
}

void AActionCharacterBase::HandleInterrupted(EHitReaction Reaction, bool bStaggered)
{
	MeleeComponent->CancelAttack();
}
