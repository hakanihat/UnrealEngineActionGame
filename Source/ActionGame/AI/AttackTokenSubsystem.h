#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AttackTokenSubsystem.generated.h"

UENUM(BlueprintType)
enum class EAttackTokenType : uint8
{
	Melee,
	Ranged
};

/**
 * Limits how many enemies may attack the player at the same time (the "attack token" pattern
 * used by Batman: Arkham and DOOM). Enemies without a token circle and wait their turn.
 * Fights stay readable and fair, and the player feels powerful against crowds instead of
 * being stun-locked from every side.
 */
UCLASS()
class ACTIONGAME_API UAttackTokenSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UAttackTokenSubsystem* Get(const UObject* WorldContextObject);

	/** Grants a token if one is free (or already held). */
	bool RequestToken(AActor* Requester, EAttackTokenType Type);

	void ReleaseToken(AActor* Holder);

	bool HasToken(const AActor* Actor) const;

private:
	int32 GetMaxTokens(EAttackTokenType Type) const;
	void RemoveStaleHolders();

	TMap<TWeakObjectPtr<AActor>, EAttackTokenType> Holders;
};
