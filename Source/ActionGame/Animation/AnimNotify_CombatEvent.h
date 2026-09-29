#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "AnimNotify_CombatEvent.generated.h"

/**
 * Fires a gameplay event on the owning AActionCharacterBase at an exact animation frame.
 * One notify class covers telegraphs, projectile spawns, area impacts, charges... so new
 * attack behaviours need a new tag and a handler, not a new notify class.
 */
UCLASS(meta = (DisplayName = "Combat Event"))
class ACTIONGAME_API UAnimNotify_CombatEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	UPROPERTY(EditAnywhere, Category = "Event", meta = (Categories = "Event"))
	FGameplayTag EventTag;
};
