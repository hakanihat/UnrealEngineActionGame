#include "Combat/HealthComponent.h"
#include "ActionGameTags.h"
#include "GameplayTagAssetInterface.h"

UHealthComponent::UHealthComponent()
{
	// Ticking is only switched on while poise needs to regenerate.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;
	Poise = MaxPoise;
}

bool UHealthComponent::IsOwnerTagged(const FGameplayTag& Tag) const
{
	const IGameplayTagAssetInterface* TagOwner = Cast<IGameplayTagAssetInterface>(GetOwner());
	return TagOwner && TagOwner->HasMatchingGameplayTag(Tag);
}

FCombatDamageResult UHealthComponent::ApplyDamage(const FCombatHit& Hit)
{
	FCombatDamageResult Result;
	if (!IsAlive())
	{
		return Result;
	}

	if (IsOwnerTagged(ActionGameTags::State_Invulnerable))
	{
		Result.bNegated = true;
		OnDamageNegated.Broadcast(Hit);
		return Result;
	}

	const bool bStaggered = IsOwnerTagged(ActionGameTags::State_Staggered);
	const float Multiplier = Hit.DamageMultiplier * (bStaggered ? StaggeredDamageMultiplier : 1.f);
	const float Damage = FMath::Max(0.f, Hit.Spec.Damage * Multiplier);

	const float Now = GetWorld()->GetTimeSeconds();
	LastDamageTime = Now;

	Result.bApplied = true;
	Result.DamageDealt = FMath::Min(Damage, Health);
	SetHealth(Health - Damage);
	Result.bKilled = !IsAlive();

	// Poise does not drain while already staggered, otherwise stagger could be chained forever.
	if (!Result.bKilled && !bStaggered && Hit.Spec.PoiseDamage > 0.f)
	{
		Poise = FMath::Max(0.f, Poise - Hit.Spec.PoiseDamage * Hit.DamageMultiplier);
		LastPoiseDamageTime = Now;
		Result.bPoiseBroken = Poise <= 0.f;
		SetComponentTickEnabled(!Result.bPoiseBroken);
	}

	OnDamageTaken.Broadcast(Hit, Result);

	if (Result.bPoiseBroken)
	{
		OnPoiseBroken.Broadcast(Hit);
	}
	if (Result.bKilled)
	{
		HandleDeath(Hit);
	}
	return Result;
}

void UHealthComponent::Heal(float Amount)
{
	if (IsAlive() && Amount > 0.f)
	{
		SetHealth(Health + Amount);
	}
}

void UHealthComponent::Kill(AActor* Instigator)
{
	if (!IsAlive())
	{
		return;
	}
	FCombatHit KillingHit;
	KillingHit.Instigator = Instigator;
	KillingHit.Target = GetOwner();
	KillingHit.ImpactPoint = GetOwner()->GetActorLocation();
	SetHealth(0.f);
	HandleDeath(KillingHit);
}

void UHealthComponent::ResetPoise()
{
	Poise = MaxPoise;
	SetComponentTickEnabled(false);
}

void UHealthComponent::SetHealth(float NewHealth)
{
	const float OldHealth = Health;
	Health = FMath::Clamp(NewHealth, 0.f, MaxHealth);
	if (!FMath::IsNearlyEqual(OldHealth, Health))
	{
		OnHealthChanged.Broadcast(this, Health, Health - OldHealth);
	}
}

void UHealthComponent::HandleDeath(const FCombatHit& KillingHit)
{
	SetComponentTickEnabled(false);
	OnDeath.Broadcast(GetOwner(), KillingHit);
}

void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsAlive() || IsOwnerTagged(ActionGameTags::State_Staggered))
	{
		return;
	}

	if (GetWorld()->GetTimeSeconds() - LastPoiseDamageTime >= PoiseRegenDelay)
	{
		Poise = FMath::Min(MaxPoise, Poise + PoiseRegenRate * DeltaTime);
		if (Poise >= MaxPoise)
		{
			SetComponentTickEnabled(false);
		}
	}
}
