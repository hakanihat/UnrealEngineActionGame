#include "Abilities/EnergyComponent.h"

UEnergyComponent::UEnergyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEnergyComponent::BeginPlay()
{
	Super::BeginPlay();
	SetEnergy(MaxEnergy);
}

bool UEnergyComponent::TryConsume(float Amount)
{
	if (Energy < Amount)
	{
		return false;
	}
	SetEnergy(Energy - Amount);
	LastSpendTime = GetWorld()->GetTimeSeconds();
	return true;
}

bool UEnergyComponent::Drain(float Amount)
{
	SetEnergy(Energy - Amount);
	LastSpendTime = GetWorld()->GetTimeSeconds();
	return Energy > 0.f;
}

void UEnergyComponent::AddEnergy(float Amount)
{
	SetEnergy(Energy + Amount);
}

void UEnergyComponent::SetEnergy(float NewEnergy)
{
	const float Clamped = FMath::Clamp(NewEnergy, 0.f, MaxEnergy);
	if (!FMath::IsNearlyEqual(Clamped, Energy))
	{
		Energy = Clamped;
		OnEnergyChanged.Broadcast(Energy, MaxEnergy);
	}
}

void UEnergyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (Energy < MaxEnergy && GetWorld()->GetTimeSeconds() - LastSpendTime >= RegenDelay)
	{
		SetEnergy(Energy + RegenRate * DeltaTime);
	}
}
