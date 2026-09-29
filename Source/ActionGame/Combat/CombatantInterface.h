#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Combat/CombatTypes.h"
#include "CombatantInterface.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UCombatant : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything that takes part in combat. Lets combat code ask "which team are you?" and
 * "where should I aim?" without knowing about concrete character classes.
 */
class ACTIONGAME_API ICombatant
{
	GENERATED_BODY()

public:
	virtual ECombatTeam GetCombatTeam() const = 0;

	/** World-space point that aim assist, lock-on and homing should target (center mass). */
	virtual FVector GetTargetPoint() const = 0;

	/** Called on the instigator after one of its hits was applied (hit markers, resource gain...). */
	virtual void NotifyDamageDealt(const FCombatHit& Hit, const FCombatDamageResult& Result) {}
};
