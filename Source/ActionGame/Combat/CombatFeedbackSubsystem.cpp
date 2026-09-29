#include "Combat/CombatFeedbackSubsystem.h"
#include "ActionGameSettings.h"
#include "Camera/ActionPlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

UCombatFeedbackSubsystem* UCombatFeedbackSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UCombatFeedbackSubsystem>() : nullptr;
}

void UCombatFeedbackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Config = GetDefault<UActionGameSettings>()->FeedbackConfig.LoadSynchronous();
	if (!Config)
	{
		// Class defaults give sensible hitstop/trauma numbers even before any asset is authored.
		Config = GetMutableDefault<UCombatFeedbackConfig>();
	}
}

void UCombatFeedbackSubsystem::Deinitialize()
{
	for (const TPair<TWeakObjectPtr<AActor>, float>& Entry : HitStopTimers)
	{
		RestoreActorTime(Entry.Key.Get());
	}
	HitStopTimers.Reset();
	Super::Deinitialize();
}

bool UCombatFeedbackSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UCombatFeedbackSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCombatFeedbackSubsystem, STATGROUP_Tickables);
}

void UCombatFeedbackSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Feedback timers run on real time so they behave the same during slow motion.
	const float RealDeltaTime = GetWorld()->DeltaRealTimeSeconds;
	TickHitStop(RealDeltaTime);
	TickSlowMotion(RealDeltaTime);
}

void UCombatFeedbackSubsystem::HandleHit(const FCombatHit& Hit, const FCombatDamageResult& Result)
{
	if (!Result.bApplied)
	{
		return;
	}

	const bool bBigHit = Result.bKilled || Result.bPoiseBroken;

	// --- Hitstop ---
	float HitStop = Hit.Spec.HitStop + (bBigHit ? Config->BigHitStopBonus : 0.f);
	HitStop = FMath::Min(HitStop, Config->MaxHitStop);
	if (HitStop > 0.f)
	{
		ApplyHitStop(Hit.Target, HitStop);

		// Only freeze the attacker for contact hits. Freezing a shooter for every bullet feels sluggish.
		if (Hit.DamageCauser && Hit.DamageCauser == Hit.Instigator)
		{
			ApplyHitStop(Hit.Instigator, HitStop);
		}
	}

	// --- Camera ---
	if (IsLocalPlayer(Hit.Instigator) || IsLocalPlayer(Hit.Target))
	{
		AddCameraTrauma(Hit.Spec.CameraTrauma + (bBigHit ? Config->BigHitTraumaBonus : 0.f));
	}

	// --- Impact VFX / SFX (layered: base, critical, poise break, kill) ---
	const FRotator ImpactRotation = Hit.ImpactNormal.Rotation();
	PlayImpactEffect(Config->GetImpactEffect(Hit.Spec.DamageType), Hit.ImpactPoint, ImpactRotation);
	if (Hit.bCritical)
	{
		PlayImpactEffect(Config->CriticalImpact, Hit.ImpactPoint, ImpactRotation);
	}
	if (Result.bPoiseBroken)
	{
		PlayImpactEffect(Config->PoiseBreakImpact, Hit.ImpactPoint, ImpactRotation);
	}
	if (Result.bKilled)
	{
		PlayImpactEffect(Config->KillImpact, Hit.ImpactPoint, ImpactRotation);
	}
}

void UCombatFeedbackSubsystem::ApplyHitStop(AActor* Actor, float Duration)
{
	if (!Actor || Duration <= 0.f)
	{
		return;
	}
	float& Remaining = HitStopTimers.FindOrAdd(Actor);
	Remaining = FMath::Max(Remaining, Duration);
	Actor->CustomTimeDilation = Config->HitStopTimeScale;
}

void UCombatFeedbackSubsystem::RestoreActorTime(AActor* Actor)
{
	if (Actor)
	{
		Actor->CustomTimeDilation = 1.f;
	}
}

void UCombatFeedbackSubsystem::TickHitStop(float RealDeltaTime)
{
	for (auto It = HitStopTimers.CreateIterator(); It; ++It)
	{
		AActor* Actor = It.Key().Get();
		It.Value() -= RealDeltaTime;
		if (!Actor || It.Value() <= 0.f)
		{
			RestoreActorTime(Actor);
			It.RemoveCurrent();
		}
	}
}

void UCombatFeedbackSubsystem::PlaySlowMotion(float TimeScale, float Duration, float BlendOutTime)
{
	TimeScale = FMath::Clamp(TimeScale, 0.01f, 1.f);
	if (bSlowMoActive && TimeScale > SlowMoScale && SlowMoRemaining > 0.f)
	{
		return; // A stronger slow motion is already running.
	}

	SlowMoScale = TimeScale;
	SlowMoRemaining = Duration;
	SlowMoBlendOutTime = FMath::Max(0.f, BlendOutTime);
	SlowMoBlendElapsed = 0.f;
	bSlowMoActive = true;
	UGameplayStatics::SetGlobalTimeDilation(this, SlowMoScale);
}

void UCombatFeedbackSubsystem::TickSlowMotion(float RealDeltaTime)
{
	if (!bSlowMoActive)
	{
		return;
	}

	if (SlowMoRemaining > 0.f)
	{
		SlowMoRemaining -= RealDeltaTime;
		return;
	}

	SlowMoBlendElapsed += RealDeltaTime;
	const float Alpha = SlowMoBlendOutTime > 0.f ? FMath::Clamp(SlowMoBlendElapsed / SlowMoBlendOutTime, 0.f, 1.f) : 1.f;
	// Ease-in curve: time "catches up" smoothly instead of snapping back.
	const float Dilation = FMath::InterpEaseIn(SlowMoScale, 1.f, Alpha, 2.f);
	UGameplayStatics::SetGlobalTimeDilation(this, Dilation);

	if (Alpha >= 1.f)
	{
		bSlowMoActive = false;
	}
}

void UCombatFeedbackSubsystem::AddCameraTrauma(float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (AActionPlayerCameraManager* Camera = PC ? Cast<AActionPlayerCameraManager>(PC->PlayerCameraManager) : nullptr)
	{
		Camera->AddTrauma(Amount);
	}
}

void UCombatFeedbackSubsystem::AddCameraTraumaAtLocation(FVector Location, float Amount, float Radius)
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || Radius <= 0.f)
	{
		return;
	}
	const float Distance = FVector::Dist(Pawn->GetActorLocation(), Location);
	const float Falloff = 1.f - FMath::Clamp(Distance / Radius, 0.f, 1.f);
	AddCameraTrauma(Amount * Falloff);
}

void UCombatFeedbackSubsystem::PlayImpactEffect(const FImpactEffect& Effect, FVector Location, FRotator Rotation)
{
	if (Effect.Niagara)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Effect.Niagara, Location, Rotation, FVector(Effect.Scale),
			true, true, ENCPoolMethod::AutoRelease);
	}
	if (Effect.Sound)
	{
		const float Variance = Config->SoundPitchVariance;
		UGameplayStatics::PlaySoundAtLocation(this, Effect.Sound, Location, Rotation, 1.f, 1.f + FMath::FRandRange(-Variance, Variance));
	}
}

bool UCombatFeedbackSubsystem::IsLocalPlayer(const AActor* Actor) const
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->IsLocallyControlled() && Pawn->IsPlayerControlled();
}
