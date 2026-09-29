#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ActionGameSettings.generated.h"

class APawn;
class UCombatFeedbackConfig;

/** Project-wide settings, editable in Project Settings > Game > Action Game. */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Action Game"))
class ACTIONGAME_API UActionGameSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	/** Player Blueprint to spawn (e.g. BP_Player). Empty = the bare C++ player with a placeholder body. */
	UPROPERTY(Config, EditAnywhere, Category = "Game")
	TSoftClassPtr<APawn> PlayerPawnClass;

	/** Global impact/hitstop/camera tuning used by the CombatFeedbackSubsystem. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	TSoftObjectPtr<UCombatFeedbackConfig> FeedbackConfig;

	/** Maximum number of normal enemies allowed to attack the player at the same time. */
	UPROPERTY(Config, EditAnywhere, Category = "AI", meta = (ClampMin = "1"))
	int32 MaxSimultaneousMeleeAttackers = 2;

	/** Maximum number of normal enemies allowed to shoot at the player at the same time. */
	UPROPERTY(Config, EditAnywhere, Category = "AI", meta = (ClampMin = "1"))
	int32 MaxSimultaneousRangedAttackers = 2;
};
