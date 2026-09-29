#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Combat/CombatTypes.h"
#include "ActionHUD.generated.h"

class AActionPlayerCharacter;

/**
 * Prototype HUD drawn with Canvas, so the demo needs no UMG assets.
 * Everything here is feedback that makes combat readable:
 *  - Crosshair that blooms with gun spread; hit marker on hits (red X on kills).
 *  - Player health / energy / ammo; boss bar with a delayed "chip" trail that shows damage dealt.
 *  - Floating health bars on recently damaged enemies; lock-on and telekinesis markers.
 *  - Red vignette flash when the player is hurt.
 * Replace with UMG widgets for production; the data sources stay the same.
 */
UCLASS()
class ACTIONGAME_API AActionHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	bool bShowControlsHint = true;

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0"))
	float HitMarkerDuration = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD", meta = (ClampMin = "0"))
	float EnemyBarDuration = 3.f;

private:
	void BindToPlayer(AActionPlayerCharacter* Player);

	UFUNCTION()
	void HandleDamageDealt(const FCombatHit& Hit, const FCombatDamageResult& Result);

	UFUNCTION()
	void HandlePlayerDamaged(const FCombatHit& Hit, const FCombatDamageResult& Result);

	void DrawCrosshair(const AActionPlayerCharacter* Player);
	void DrawPlayerStatus(const AActionPlayerCharacter* Player);
	void DrawBossBar(float DeltaTime);
	void DrawEnemyBars();
	void DrawWorldMarkers(const AActionPlayerCharacter* Player);
	void DrawDamageVignette(float DeltaTime);
	void DrawDemoState();
	void DrawControlsHint();

	void DrawBar(float X, float Y, float Width, float Height, float Percent, const FLinearColor& Color, float TrailPercent = -1.f);
	void DrawCenteredText(const FString& Text, float Y, const FLinearColor& Color, float Scale);
	bool ProjectToScreen(const FVector& World, FVector2D& OutScreen) const;

	TWeakObjectPtr<AActionPlayerCharacter> BoundPlayer;
	TMap<TWeakObjectPtr<AActor>, float> RecentlyHitEnemies;

	float UIScale = 1.f;
	float LastHitMarkerTime = -1000.f;
	bool bLastHitWasKill = false;
	float DamageFlash = 0.f;
	float BossTrailPercent = 1.f;
	float LastDrawTime = 0.f;
};
