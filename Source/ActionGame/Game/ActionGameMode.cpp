#include "Game/ActionGameMode.h"
#include "Characters/ActionCharacterBase.h"
#include "Characters/ActionPlayerCharacter.h"
#include "Combat/CombatFeedbackSubsystem.h"
#include "Combat/HealthComponent.h"
#include "Game/ActionPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/ActionHUD.h"

AActionGameMode::AActionGameMode()
{
	DefaultPawnClass = AActionPlayerCharacter::StaticClass();
	PlayerControllerClass = AActionPlayerController::StaticClass();
	HUDClass = AActionHUD::StaticClass();
}

void AActionGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);

	if (const AActionCharacterBase* Player = Cast<AActionCharacterBase>(PlayerPawn))
	{
		if (UHealthComponent* Health = Player->GetHealthComponent())
		{
			Health->OnDeath.AddUniqueDynamic(this, &AActionGameMode::HandlePlayerDeath);
		}
	}
}

void AActionGameMode::RegisterBoss(AActionCharacterBase* Boss, FText DisplayName)
{
	if (!Boss || ActiveBoss.Get() == Boss)
	{
		return;
	}
	ActiveBoss = Boss;
	BossName = DisplayName;
	if (UHealthComponent* Health = Boss->GetHealthComponent())
	{
		Health->OnDeath.AddUniqueDynamic(this, &AActionGameMode::HandleBossDeath);
	}
}

void AActionGameMode::HandlePlayerDeath(AActor* DeadActor, const FCombatHit& KillingHit)
{
	if (DemoState != EDemoState::Playing)
	{
		return;
	}
	SetDemoState(EDemoState::Defeated);
	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		Feedback->PlaySlowMotion(0.3f, 1.f, 0.5f);
	}
	GetWorldTimerManager().SetTimer(RestartTimer, this, &AActionGameMode::RestartCurrentLevel, RestartDelay, false);
}

void AActionGameMode::HandleBossDeath(AActor* DeadActor, const FCombatHit& KillingHit)
{
	if (DemoState != EDemoState::Playing)
	{
		return;
	}
	SetDemoState(EDemoState::Victory);

	// The final blow is the most important moment of the demo: let it breathe.
	if (UCombatFeedbackSubsystem* Feedback = UCombatFeedbackSubsystem::Get(this))
	{
		Feedback->PlaySlowMotion(BossKillTimeScale, BossKillSlowMoDuration, 1.f);
		Feedback->AddCameraTrauma(0.6f);
	}
}

void AActionGameMode::SetDemoState(EDemoState NewState)
{
	if (DemoState != NewState)
	{
		DemoState = NewState;
		OnDemoStateChanged.Broadcast(NewState);
	}
}

void AActionGameMode::RestartCurrentLevel()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}
