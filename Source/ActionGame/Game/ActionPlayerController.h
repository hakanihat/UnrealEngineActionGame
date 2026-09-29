#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ActionPlayerController.generated.h"

/** Installs the procedural-feedback camera manager and captures the mouse for gameplay. */
UCLASS()
class ACTIONGAME_API AActionPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AActionPlayerController();

protected:
	virtual void BeginPlay() override;
};
