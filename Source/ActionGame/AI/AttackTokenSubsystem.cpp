#include "AI/AttackTokenSubsystem.h"
#include "ActionGameSettings.h"
#include "Combat/CombatLibrary.h"
#include "Engine/World.h"

UAttackTokenSubsystem* UAttackTokenSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UAttackTokenSubsystem>() : nullptr;
}

int32 UAttackTokenSubsystem::GetMaxTokens(EAttackTokenType Type) const
{
	const UActionGameSettings* Settings = GetDefault<UActionGameSettings>();
	return Type == EAttackTokenType::Melee ? Settings->MaxSimultaneousMeleeAttackers : Settings->MaxSimultaneousRangedAttackers;
}

void UAttackTokenSubsystem::RemoveStaleHolders()
{
	for (auto It = Holders.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !UCombatLibrary::IsAlive(It.Key().Get()))
		{
			It.RemoveCurrent();
		}
	}
}

bool UAttackTokenSubsystem::RequestToken(AActor* Requester, EAttackTokenType Type)
{
	if (!Requester)
	{
		return false;
	}
	if (Holders.Contains(Requester))
	{
		return true;
	}

	RemoveStaleHolders();
	int32 InUse = 0;
	for (const TPair<TWeakObjectPtr<AActor>, EAttackTokenType>& Holder : Holders)
	{
		InUse += Holder.Value == Type ? 1 : 0;
	}
	if (InUse >= GetMaxTokens(Type))
	{
		return false;
	}
	Holders.Add(Requester, Type);
	return true;
}

void UAttackTokenSubsystem::ReleaseToken(AActor* Holder)
{
	Holders.Remove(Holder);
}

bool UAttackTokenSubsystem::HasToken(const AActor* Actor) const
{
	for (const TPair<TWeakObjectPtr<AActor>, EAttackTokenType>& Holder : Holders)
	{
		if (Holder.Key.Get() == Actor)
		{
			return true;
		}
	}
	return false;
}
