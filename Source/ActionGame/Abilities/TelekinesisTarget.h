#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TelekinesisTarget.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UTelekinesisTarget : public UInterface
{
	GENERATED_BODY()
};

/**
 * Anything the telekinesis power can grab and throw. Each implementer decides how it moves
 * (physics props steer their rigid body, projectiles move kinematically), so the power itself
 * never needs to know what it is holding. Add the interface to make new things throwable.
 */
class ACTIONGAME_API ITelekinesisTarget
{
	GENERATED_BODY()

public:
	virtual bool CanBeGrabbed(const AActor* Grabber) const = 0;
	virtual void OnTelekinesisGrabbed(AActor* Grabber) = 0;

	/** Called every frame while held: move toward HoldLocation. */
	virtual void TelekinesisMoveTo(const FVector& HoldLocation, float DeltaTime) = 0;

	virtual void OnTelekinesisThrown(AActor* Thrower, const FVector& Velocity) = 0;

	/** Dropped without being thrown (grabber interrupted or died). */
	virtual void OnTelekinesisReleased() = 0;
};
