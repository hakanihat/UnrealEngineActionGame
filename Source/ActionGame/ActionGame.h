#pragma once

#include "CoreMinimal.h"

ACTIONGAME_API DECLARE_LOG_CATEGORY_EXTERN(LogActionGame, Log, All);

/** Custom trace channel (see DefaultEngine.ini). Hits mesh physics bodies, ignored by capsules. */
#define ECC_Weapon ECC_GameTraceChannel1
