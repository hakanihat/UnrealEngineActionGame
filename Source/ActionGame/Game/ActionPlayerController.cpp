#include "Game/ActionPlayerController.h"
#include "Camera/ActionPlayerCameraManager.h"

AActionPlayerController::AActionPlayerController()
{
	PlayerCameraManagerClass = AActionPlayerCameraManager::StaticClass();
}

void AActionPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}
