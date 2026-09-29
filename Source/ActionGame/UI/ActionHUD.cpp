#include "UI/ActionHUD.h"
#include "Abilities/EnergyComponent.h"
#include "Abilities/TelekinesisComponent.h"
#include "Characters/ActionCharacterBase.h"
#include "Characters/ActionPlayerCharacter.h"
#include "Combat/CombatLibrary.h"
#include "Combat/GunComponent.h"
#include "Combat/HealthComponent.h"
#include "Combat/TargetingComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Game/ActionGameMode.h"
#include "GameFramework/PlayerController.h"

namespace HUDColors
{
	const FLinearColor Background(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor Health(0.85f, 0.15f, 0.15f, 1.f);
	const FLinearColor Energy(0.25f, 0.65f, 1.f, 1.f);
	const FLinearColor Ammo(0.95f, 0.85f, 0.4f, 1.f);
	const FLinearColor Trail(1.f, 1.f, 1.f, 0.85f);
	const FLinearColor Crosshair(1.f, 1.f, 1.f, 0.9f);
	const FLinearColor Kill(1.f, 0.2f, 0.15f, 1.f);
	const FLinearColor LockOn(1.f, 0.35f, 0.2f, 1.f);
	const FLinearColor Telekinesis(0.45f, 0.8f, 1.f, 1.f);
}

void AActionHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	const float Now = GetWorld()->GetRealTimeSeconds();
	const float DeltaTime = FMath::Clamp(Now - LastDrawTime, 0.f, 0.1f);
	LastDrawTime = Now;
	UIScale = Canvas->ClipY / 1080.f;

	AActionPlayerCharacter* Player = Cast<AActionPlayerCharacter>(GetOwningPawn());
	if (Player != BoundPlayer.Get())
	{
		BindToPlayer(Player);
	}

	DrawDamageVignette(DeltaTime);
	DrawEnemyBars();
	if (Player)
	{
		DrawWorldMarkers(Player);
		DrawCrosshair(Player);
		DrawPlayerStatus(Player);
	}
	DrawBossBar(DeltaTime);
	DrawDemoState();
	if (bShowControlsHint)
	{
		DrawControlsHint();
	}
}

void AActionHUD::BindToPlayer(AActionPlayerCharacter* Player)
{
	if (AActionPlayerCharacter* Previous = BoundPlayer.Get())
	{
		Previous->OnDamageDealt.RemoveDynamic(this, &AActionHUD::HandleDamageDealt);
		Previous->GetHealthComponent()->OnDamageTaken.RemoveDynamic(this, &AActionHUD::HandlePlayerDamaged);
	}
	BoundPlayer = Player;
	if (Player)
	{
		Player->OnDamageDealt.AddUniqueDynamic(this, &AActionHUD::HandleDamageDealt);
		Player->GetHealthComponent()->OnDamageTaken.AddUniqueDynamic(this, &AActionHUD::HandlePlayerDamaged);
	}
}

void AActionHUD::HandleDamageDealt(const FCombatHit& Hit, const FCombatDamageResult& Result)
{
	LastHitMarkerTime = GetWorld()->GetRealTimeSeconds();
	bLastHitWasKill = Result.bKilled;
	if (Hit.Target)
	{
		RecentlyHitEnemies.Add(Hit.Target.Get(), GetWorld()->GetRealTimeSeconds());
	}
}

void AActionHUD::HandlePlayerDamaged(const FCombatHit& Hit, const FCombatDamageResult& Result)
{
	DamageFlash = 1.f;
}

// ---------------------------------------------------------------------------------------------
// Elements
// ---------------------------------------------------------------------------------------------

void AActionHUD::DrawCrosshair(const AActionPlayerCharacter* Player)
{
	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;

	if (Player->IsAiming())
	{
		// Four ticks pushed out by the current spread: players can *see* accuracy.
		const float Gap = (6.f + Player->GetGunComponent()->GetCurrentSpread() * 6.f) * UIScale;
		const float Len = 8.f * UIScale;
		DrawLine(CX - Gap - Len, CY, CX - Gap, CY, HUDColors::Crosshair, 2.f);
		DrawLine(CX + Gap, CY, CX + Gap + Len, CY, HUDColors::Crosshair, 2.f);
		DrawLine(CX, CY - Gap - Len, CX, CY - Gap, HUDColors::Crosshair, 2.f);
		DrawLine(CX, CY + Gap, CX, CY + Gap + Len, HUDColors::Crosshair, 2.f);
	}
	else
	{
		const float Dot = 3.f * UIScale;
		DrawRect(HUDColors::Crosshair, CX - Dot * 0.5f, CY - Dot * 0.5f, Dot, Dot);
	}

	const float SinceHit = GetWorld()->GetRealTimeSeconds() - LastHitMarkerTime;
	if (SinceHit < HitMarkerDuration)
	{
		const float Alpha = 1.f - SinceHit / HitMarkerDuration;
		FLinearColor Color = bLastHitWasKill ? HUDColors::Kill : FLinearColor::White;
		Color.A = Alpha;
		const float Inner = 7.f * UIScale;
		const float Outer = (bLastHitWasKill ? 18.f : 14.f) * UIScale;
		DrawLine(CX - Outer, CY - Outer, CX - Inner, CY - Inner, Color, 2.f);
		DrawLine(CX + Inner, CY - Inner, CX + Outer, CY - Outer, Color, 2.f);
		DrawLine(CX - Outer, CY + Outer, CX - Inner, CY + Inner, Color, 2.f);
		DrawLine(CX + Inner, CY + Inner, CX + Outer, CY + Outer, Color, 2.f);
	}
}

void AActionHUD::DrawPlayerStatus(const AActionPlayerCharacter* Player)
{
	const float X = 40.f * UIScale;
	const float Width = 360.f * UIScale;
	float Y = Canvas->ClipY - 110.f * UIScale;

	DrawBar(X, Y, Width, 16.f * UIScale, Player->GetHealthComponent()->GetHealthPercent(), HUDColors::Health);
	Y += 24.f * UIScale;
	DrawBar(X, Y, Width * 0.8f, 10.f * UIScale, Player->GetEnergyComponent()->GetEnergyPercent(), HUDColors::Energy);
	Y += 18.f * UIScale;

	const UGunComponent* Gun = Player->GetGunComponent();
	DrawBar(X, Y, Width * 0.5f, 6.f * UIScale, Gun->GetAmmo() / Gun->GetMaxAmmo(), HUDColors::Ammo);
}

void AActionHUD::DrawBossBar(float DeltaTime)
{
	const AActionGameMode* GameMode = GetWorld()->GetAuthGameMode<AActionGameMode>();
	const AActionCharacterBase* Boss = GameMode ? GameMode->GetActiveBoss() : nullptr;
	if (!Boss)
	{
		BossTrailPercent = 1.f;
		return;
	}

	const float Percent = Boss->GetHealthComponent()->GetHealthPercent();
	// The white trail lingers, then drains: big hits read as big chunks.
	BossTrailPercent = BossTrailPercent > Percent ? FMath::FInterpConstantTo(BossTrailPercent, Percent, DeltaTime, 0.35f) : Percent;

	const float Width = Canvas->ClipX * 0.5f;
	const float X = (Canvas->ClipX - Width) * 0.5f;
	const float Y = 60.f * UIScale;
	DrawCenteredText(GameMode->GetBossName().ToString(), Y - 32.f * UIScale, FLinearColor::White, 1.4f * UIScale);
	DrawBar(X, Y, Width, 14.f * UIScale, Percent, HUDColors::Health, BossTrailPercent);
}

void AActionHUD::DrawEnemyBars()
{
	const float Now = GetWorld()->GetRealTimeSeconds();
	const AActionGameMode* GameMode = GetWorld()->GetAuthGameMode<AActionGameMode>();
	const AActor* Boss = GameMode ? GameMode->GetActiveBoss() : nullptr;

	for (auto It = RecentlyHitEnemies.CreateIterator(); It; ++It)
	{
		AActor* Enemy = It.Key().Get();
		if (!Enemy || !UCombatLibrary::IsAlive(Enemy) || Now - It.Value() > EnemyBarDuration)
		{
			It.RemoveCurrent();
			continue;
		}
		if (Enemy == Boss)
		{
			continue; // The boss has its own bar.
		}
		FVector2D Screen;
		if (ProjectToScreen(Enemy->GetActorLocation() + FVector(0.f, 0.f, 120.f), Screen))
		{
			const float Width = 70.f * UIScale;
			DrawBar(Screen.X - Width * 0.5f, Screen.Y, Width, 6.f * UIScale,
				UCombatLibrary::GetHealthComponent(Enemy)->GetHealthPercent(), HUDColors::Health);
		}
	}
}

void AActionHUD::DrawWorldMarkers(const AActionPlayerCharacter* Player)
{
	FVector2D Screen;

	if (const AActor* Locked = Player->GetTargetingComponent()->GetLockedTarget())
	{
		if (ProjectToScreen(UCombatLibrary::GetTargetPoint(Locked), Screen))
		{
			const float S = 12.f * UIScale;
			DrawLine(Screen.X - S, Screen.Y, Screen.X, Screen.Y - S, HUDColors::LockOn, 2.f);
			DrawLine(Screen.X, Screen.Y - S, Screen.X + S, Screen.Y, HUDColors::LockOn, 2.f);
			DrawLine(Screen.X + S, Screen.Y, Screen.X, Screen.Y + S, HUDColors::LockOn, 2.f);
			DrawLine(Screen.X, Screen.Y + S, Screen.X - S, Screen.Y, HUDColors::LockOn, 2.f);
		}
	}

	if (const AActor* Grabbable = Player->GetTelekinesisComponent()->GetHighlightedTarget())
	{
		if (ProjectToScreen(Grabbable->GetActorLocation(), Screen))
		{
			const float S = 7.f * UIScale;
			DrawLine(Screen.X - S, Screen.Y - S, Screen.X + S, Screen.Y - S, HUDColors::Telekinesis, 1.5f);
			DrawLine(Screen.X + S, Screen.Y - S, Screen.X + S, Screen.Y + S, HUDColors::Telekinesis, 1.5f);
			DrawLine(Screen.X + S, Screen.Y + S, Screen.X - S, Screen.Y + S, HUDColors::Telekinesis, 1.5f);
			DrawLine(Screen.X - S, Screen.Y + S, Screen.X - S, Screen.Y - S, HUDColors::Telekinesis, 1.5f);
		}
	}
}

void AActionHUD::DrawDamageVignette(float DeltaTime)
{
	if (DamageFlash <= 0.f)
	{
		return;
	}
	DamageFlash = FMath::Max(0.f, DamageFlash - DeltaTime * 3.f);
	const float Edge = 60.f * UIScale;
	const FLinearColor Color(0.6f, 0.f, 0.f, 0.35f * DamageFlash);
	DrawRect(Color, 0.f, 0.f, Canvas->ClipX, Edge);
	DrawRect(Color, 0.f, Canvas->ClipY - Edge, Canvas->ClipX, Edge);
	DrawRect(Color, 0.f, Edge, Edge, Canvas->ClipY - Edge * 2.f);
	DrawRect(Color, Canvas->ClipX - Edge, Edge, Edge, Canvas->ClipY - Edge * 2.f);
}

void AActionHUD::DrawDemoState()
{
	const AActionGameMode* GameMode = GetWorld()->GetAuthGameMode<AActionGameMode>();
	if (!GameMode)
	{
		return;
	}
	float AnnouncementAlpha = 0.f;
	const FText Announcement = GameMode->GetAnnouncement(AnnouncementAlpha);
	if (AnnouncementAlpha > 0.f)
	{
		DrawCenteredText(Announcement.ToString(), Canvas->ClipY * 0.28f, FLinearColor(1.f, 1.f, 1.f, AnnouncementAlpha), 1.8f * UIScale);
	}

	switch (GameMode->GetDemoState())
	{
	case EDemoState::Victory:
		DrawCenteredText(TEXT("VICTORY"), Canvas->ClipY * 0.4f, FLinearColor(1.f, 0.85f, 0.4f), 3.f * UIScale);
		break;
	case EDemoState::Defeated:
		DrawCenteredText(TEXT("YOU DIED"), Canvas->ClipY * 0.4f, HUDColors::Kill, 3.f * UIScale);
		break;
	default:
		break;
	}
}

void AActionHUD::DrawControlsHint()
{
	static const TCHAR* Lines[] = {
		TEXT("LMB  Attack / Fire (aiming)"),
		TEXT("F    Heavy / Ground Slam (air)"),
		TEXT("RMB  Aim dual pistols"),
		TEXT("E    Telekinesis (hold, release to throw)"),
		TEXT("Q    Kinetic Launch (throw yourself)"),
		TEXT("Shift Dodge   Space Jump / hold to levitate"),
		TEXT("MMB / Tab  Lock on"),
	};
	UFont* Font = GEngine->GetSmallFont();
	float Y = Canvas->ClipY - 150.f * UIScale;
	for (const TCHAR* Line : Lines)
	{
		DrawText(Line, FLinearColor(1.f, 1.f, 1.f, 0.6f), Canvas->ClipX - 330.f * UIScale, Y, Font, UIScale);
		Y += 18.f * UIScale;
	}
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

void AActionHUD::DrawBar(float X, float Y, float Width, float Height, float Percent, const FLinearColor& Color, float TrailPercent)
{
	const float Pad = 2.f * UIScale;
	DrawRect(HUDColors::Background, X - Pad, Y - Pad, Width + Pad * 2.f, Height + Pad * 2.f);
	if (TrailPercent > Percent)
	{
		DrawRect(HUDColors::Trail, X, Y, Width * FMath::Clamp(TrailPercent, 0.f, 1.f), Height);
	}
	DrawRect(Color, X, Y, Width * FMath::Clamp(Percent, 0.f, 1.f), Height);
}

void AActionHUD::DrawCenteredText(const FString& Text, float Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GEngine->GetLargeFont();
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font, Scale);
	DrawText(Text, Color, (Canvas->ClipX - Width) * 0.5f, Y, Font, Scale);
}

bool AActionHUD::ProjectToScreen(const FVector& World, FVector2D& OutScreen) const
{
	if (!PlayerOwner)
	{
		return false;
	}
	FVector CameraLocation;
	FRotator CameraRotation;
	PlayerOwner->GetPlayerViewPoint(CameraLocation, CameraRotation);
	if (FVector::DotProduct(World - CameraLocation, CameraRotation.Vector()) <= 0.f)
	{
		return false; // Behind the camera.
	}
	const FVector Projected = Project(World);
	OutScreen = FVector2D(Projected.X, Projected.Y);
	return Projected.X >= 0.f && Projected.X <= Canvas->ClipX && Projected.Y >= 0.f && Projected.Y <= Canvas->ClipY;
}
