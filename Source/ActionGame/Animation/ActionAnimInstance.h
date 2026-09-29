#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "ActionAnimInstance.generated.h"

class AActionCharacterBase;

/**
 * Base class for character Animation Blueprints (reparent your ABP to this).
 * Gathers everything the anim graph needs once per frame, so the graph itself stays a pure
 * "read variables, blend poses" graph with no Blueprint logic.
 */
UCLASS()
class ACTIONGAME_API UActionAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float GroundSpeed = 0.f;

	/** Movement direction relative to facing, -180..180 (for strafe blend spaces). */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float VerticalSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bShouldMove = false;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsFalling = false;

	/** Procedural lean into turns (degrees). Plug into a roll on the spine or an additive lean pose. */
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float LeanAngle = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Aim")
	float AimPitch = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Aim")
	float AimYaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsAiming = false;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsLevitating = false;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsDodging = false;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsAttacking = false;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsStunned = false;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsDead = false;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion")
	float MaxLeanAngle = 15.f;

	/** Lean degrees per degree/second of turn rate. */
	UPROPERTY(EditDefaultsOnly, Category = "Locomotion")
	float LeanFactor = 0.035f;

	UPROPERTY(EditDefaultsOnly, Category = "Locomotion")
	float LeanInterpSpeed = 6.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<AActionCharacterBase> Character;

	float PreviousYaw = 0.f;
};
