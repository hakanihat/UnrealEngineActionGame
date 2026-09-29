#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "ActionPlayerCameraManager.generated.h"

/**
 * Procedural camera feedback that needs no camera-shake assets:
 *  - Trauma shake: callers add "trauma" (0..1) which decays over time. The actual shake uses
 *    trauma squared, so small hits feel subtle while big hits feel violent, and it is driven by
 *    Perlin noise so it is smooth rather than jittery.
 *  - FOV kick: a short field-of-view punch for dashes, slams and heavy impacts.
 */
UCLASS()
class ACTIONGAME_API AActionPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Camera|Feedback")
	void AddTrauma(float Amount);

	/** Adds a temporary FOV offset in degrees (positive widens) that springs back to zero. */
	UFUNCTION(BlueprintCallable, Category = "Camera|Feedback")
	void AddFOVKick(float Degrees);

	UFUNCTION(BlueprintPure, Category = "Camera|Feedback")
	float GetTrauma() const { return Trauma; }

protected:
	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;

	/** Trauma lost per second. */
	UPROPERTY(EditAnywhere, Category = "Camera|Shake", meta = (ClampMin = "0"))
	float TraumaDecayRate = 1.6f;

	UPROPERTY(EditAnywhere, Category = "Camera|Shake")
	FRotator MaxShakeRotation = FRotator(2.5f, 2.5f, 3.5f);

	/** Max positional jitter in cm. */
	UPROPERTY(EditAnywhere, Category = "Camera|Shake", meta = (ClampMin = "0"))
	float MaxShakeOffset = 4.f;

	/** Noise speed. Higher values feel more "violent", lower values more "wobbly". */
	UPROPERTY(EditAnywhere, Category = "Camera|Shake", meta = (ClampMin = "0"))
	float ShakeFrequency = 22.f;

	/** How fast the FOV kick springs back. */
	UPROPERTY(EditAnywhere, Category = "Camera|FOV", meta = (ClampMin = "0"))
	float FOVKickRecoverySpeed = 8.f;

	UPROPERTY(EditAnywhere, Category = "Camera|FOV", meta = (ClampMin = "0"))
	float MaxFOVKick = 20.f;

private:
	float Trauma = 0.f;
	float FOVKick = 0.f;
	float NoiseTime = 0.f;
};
