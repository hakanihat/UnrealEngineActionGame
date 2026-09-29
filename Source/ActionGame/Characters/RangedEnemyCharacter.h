#pragma once

#include "CoreMinimal.h"
#include "Characters/EnemyCharacter.h"
#include "RangedEnemyCharacter.generated.h"

/**
 * "Gunner" archetype: keeps its distance and fires slow, dodgeable volleys, and shoves the
 * player away when crowded. Its projectiles can be caught with telekinesis and thrown back.
 * Mixing gunners with melee grunts forces the player to prioritise and to move, which is
 * what makes the gun and telekinesis tools matter.
 */
UCLASS()
class ACTIONGAME_API ARangedEnemyCharacter : public AEnemyCharacter
{
	GENERATED_BODY()

public:
	ARangedEnemyCharacter(const FObjectInitializer& ObjectInitializer);
};
