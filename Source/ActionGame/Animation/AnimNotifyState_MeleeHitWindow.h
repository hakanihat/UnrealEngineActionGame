#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_MeleeHitWindow.generated.h"

/**
 * Marks the active frames of a melee attack. Place it over the frames where the weapon
 * should deal damage. Optional overrides let a single montage trace a kick or a punch
 * instead of the equipped weapon.
 */
UCLASS(meta = (DisplayName = "Melee Hit Window"))
class ACTIONGAME_API UAnimNotifyState_MeleeHitWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override { return TEXT("Hit Window"); }

	/** Leave empty to use the melee component's trace source (the weapon). */
	UPROPERTY(EditAnywhere, Category = "Trace")
	FName StartSocketOverride = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Trace")
	FName EndSocketOverride = NAME_None;

	/** 0 = use the component default. */
	UPROPERTY(EditAnywhere, Category = "Trace", meta = (ClampMin = "0"))
	float RadiusOverride = 0.f;
};
