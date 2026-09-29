#include "Combat/CombatLibrary.h"
#include "Combat/CombatantInterface.h"
#include "Combat/CombatFeedbackSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

FCombatDamageResult UCombatLibrary::ApplyDamage(const FCombatHit& Hit)
{
	AActor* Target = Hit.Target;
	if (!Target || Target == Hit.Instigator || !CanDamage(Hit.Instigator, Target))
	{
		return FCombatDamageResult();
	}

	UHealthComponent* Health = GetHealthComponent(Target);
	if (!Health)
	{
		return FCombatDamageResult();
	}

	const FCombatDamageResult Result = Health->ApplyDamage(Hit);

	if (Result.bApplied)
	{
		if (ICombatant* InstigatorCombatant = Cast<ICombatant>(Hit.Instigator))
		{
			InstigatorCombatant->NotifyDamageDealt(Hit, Result);
		}
	}

	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(Target))
	{
		Feedback->HandleHit(Hit, Result);
	}
	return Result;
}

int32 UCombatLibrary::ApplyRadialDamage(const UObject* WorldContextObject, const FCombatDamageSpec& Spec, FVector Origin, float Radius,
	AActor* Instigator, AActor* DamageCauser, float MinFalloff)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World || Radius <= 0.f)
	{
		return 0;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RadialDamage), false, Instigator);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	TSet<AActor*> Processed;
	int32 NumDamaged = 0;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		UPrimitiveComponent* Component = Overlap.GetComponent();

		// Loose physics objects get shoved even though they cannot take damage.
		if (Component && Component->IsSimulatingPhysics() && !GetHealthComponent(Actor))
		{
			Component->AddRadialImpulse(Origin, Radius, Spec.KnockbackStrength + Spec.LaunchStrength, RIF_Linear, true);
		}

		if (!Actor || Processed.Contains(Actor))
		{
			continue;
		}
		Processed.Add(Actor);

		if (!CanDamage(Instigator, Actor) || !IsAlive(Actor))
		{
			continue;
		}

		const FVector TargetPoint = GetTargetPoint(Actor);
		const float Distance = FVector::Dist(Origin, TargetPoint);
		const float Falloff = FMath::Lerp(1.f, MinFalloff, FMath::Clamp(Distance / Radius, 0.f, 1.f));

		FCombatHit Hit;
		Hit.Spec = Spec;
		Hit.Instigator = Instigator;
		Hit.DamageCauser = DamageCauser;
		Hit.Target = Actor;
		Hit.ImpactPoint = TargetPoint;
		Hit.HitDirection = (TargetPoint - Origin).GetSafeNormal2D();
		Hit.ImpactNormal = -Hit.HitDirection;
		Hit.DamageMultiplier = Falloff;

		if (ApplyDamage(Hit).bApplied)
		{
			++NumDamaged;
		}
	}
	return NumDamaged;
}

ECombatTeam UCombatLibrary::GetTeam(const AActor* Actor)
{
	const ICombatant* Combatant = Cast<ICombatant>(Actor);
	return Combatant ? Combatant->GetCombatTeam() : ECombatTeam::Neutral;
}

bool UCombatLibrary::CanDamage(const AActor* Instigator, const AActor* Target)
{
	if (!Target || Instigator == Target)
	{
		return false;
	}
	const ECombatTeam TargetTeam = GetTeam(Target);
	return TargetTeam == ECombatTeam::Neutral || GetTeam(Instigator) != TargetTeam;
}

UHealthComponent* UCombatLibrary::GetHealthComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
}

bool UCombatLibrary::IsAlive(const AActor* Actor)
{
	const UHealthComponent* Health = GetHealthComponent(Actor);
	return Health && Health->IsAlive();
}

FVector UCombatLibrary::GetTargetPoint(const AActor* Actor)
{
	if (const ICombatant* Combatant = Cast<ICombatant>(Actor))
	{
		return Combatant->GetTargetPoint();
	}
	if (Actor)
	{
		FVector Origin, Extent;
		Actor->GetActorBounds(true, Origin, Extent);
		return Origin;
	}
	return FVector::ZeroVector;
}
