#include "Game/ActionGameMode.h"
#include "ActionGameSettings.h"
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

UClass* AActionGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	// Project Settings > Game > Action Game > Player Pawn Class wins, so no Blueprint game mode is required.
	if (UClass* ConfiguredClass = GetDefault<UActionGameSettings>()->PlayerPawnClass.LoadSynchronous())
	{
		return ConfiguredClass;
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
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

void AActionGameMode::Announce(FText Text, float Duration)
{
	AnnouncementText = Text;
	AnnouncementStartTime = GetWorld()->GetRealTimeSeconds();
	AnnouncementDuration = FMath::Max(0.1f, Duration);
}

FText AActionGameMode::GetAnnouncement(float& OutAlpha) const
{
	const float Elapsed = GetWorld()->GetRealTimeSeconds() - AnnouncementStartTime;
	if (Elapsed < 0.f || Elapsed > AnnouncementDuration)
	{
		OutAlpha = 0.f;
		return FText::GetEmpty();
	}
	// Quick fade in, hold, fade out over the last 30%.
	const float FadeIn = FMath::Clamp(Elapsed / 0.15f, 0.f, 1.f);
	const float FadeOut = FMath::Clamp((AnnouncementDuration - Elapsed) / (AnnouncementDuration * 0.3f), 0.f, 1.f);
	OutAlpha = FMath::Min(FadeIn, FadeOut);
	return AnnouncementText;
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
