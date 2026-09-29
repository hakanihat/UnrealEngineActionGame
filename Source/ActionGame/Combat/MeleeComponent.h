#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/MeleeTypes.h"
#include "MeleeComponent.generated.h"

class ACharacter;
class UAnimMontage;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeleeAttackStarted, EMeleeAttackKind, Kind);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeleeAttackEnded, bool, bInterrupted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMeleeHitLanded, const FCombatHit&, Hit, const FCombatDamageResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeleeHitWindowChanged, bool, bActive);

/**
 * Generic melee driver used by the player, enemies and bosses alike.
 *
 *  - Combos with input buffering: presses during an attack are remembered and executed as soon
 *    as the montage opens its combo window.
 *  - Attack magnetism: on attack start the attacker snaps to face the target and slides into
 *    range, so swings connect without pixel-perfect spacing (standard in character action games).
 *  - Swept traces: during the hit window, several points along the weapon are swept from their
 *    previous to current position every frame, so fast swings never tunnel through enemies.
 *    Each target can only be hit once per swing.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class ACTIONGAME_API UMeleeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeComponent();

	/**
	 * Player-style request: picks the next attack of the given kind from the moveset,
	 * or buffers it if an attack is already playing.
	 * @param Target         Optional target for magnetism.
	 * @param DesiredFacing  Direction to face when there is no target (e.g. stick direction).
	 */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	bool RequestAttack(EMeleeAttackKind Kind, AActor* Target, FVector DesiredFacing);

	/** AI-style request: plays a specific attack immediately. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	bool PerformAttack(const FMeleeAttack& Attack, AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Melee")
	void CancelAttack();

	UFUNCTION(BlueprintCallable, Category = "Melee")
	void ResetCombo();

	/** Air combos restart after landing. */
	void ResetAirCombo() { AirIndex = 0; }

	UFUNCTION(BlueprintPure, Category = "Melee")
	bool IsAttacking() const { return bIsAttacking; }

	UFUNCTION(BlueprintPure, Category = "Melee")
	bool IsHitWindowActive() const { return bHitWindowActive; }

	/** Attacks can be cancelled (e.g. into a dodge) at any time except during active frames. */
	UFUNCTION(BlueprintPure, Category = "Melee")
	bool CanBeCanceled() const { return !bHitWindowActive; }

	/** Sets what the weapon traces follow. Sockets are looked up on Component. */
	UFUNCTION(BlueprintCallable, Category = "Melee")
	void SetTraceSource(UPrimitiveComponent* Component, FName StartSocket, FName EndSocket);

	void SetMoveset(UMeleeMoveset* NewMoveset) { Moveset = NewMoveset; }

	// Called by anim notify states.
	void BeginHitWindow(FName StartSocketOverride, FName EndSocketOverride, float RadiusOverride);
	void EndHitWindow();
	void OpenComboWindow();
	void CloseComboWindow();

	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeAttackStarted OnAttackStarted;

	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeAttackEnded OnAttackEnded;

	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeHitLanded OnHitLanded;

	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FOnMeleeHitWindowChanged OnHitWindowChanged;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	TObjectPtr<UMeleeMoveset> Moveset;

	/** Radius of each swept sphere along the weapon. */
	UPROPERTY(EditAnywhere, Category = "Melee|Trace", meta = (ClampMin = "1"))
	float TraceRadius = 14.f;

	/** Number of sample points from weapon base to tip. More = more accurate arcs. */
	UPROPERTY(EditAnywhere, Category = "Melee|Trace", meta = (ClampMin = "2", ClampMax = "12"))
	int32 TraceSamples = 4;

	/** Weapon length used when the end socket is missing (extends along the owner's forward). */
	UPROPERTY(EditAnywhere, Category = "Melee|Trace", meta = (ClampMin = "0"))
	float MissingSocketReach = 90.f;

	/** Default sockets when no trace source is set (the owner's mesh is used). */
	UPROPERTY(EditAnywhere, Category = "Melee|Trace")
	FName DefaultStartSocket = TEXT("hand_r");

	UPROPERTY(EditAnywhere, Category = "Melee|Trace")
	FName DefaultEndSocket = NAME_None;

	/** Seconds to slide into range when magnetizing. Short enough to be invisible, long enough to not teleport. */
	UPROPERTY(EditAnywhere, Category = "Melee|Magnetism", meta = (ClampMin = "0.01"))
	float MagnetismDuration = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Melee|Debug")
	bool bDrawDebugTraces = false;

	// --- Fallback (no animation) ---

	/**
	 * When the requested attack has no montage (or there is no moveset), perform an instant
	 * sweep instead. Lets you tune hit feel, reactions and enemies before any animation exists.
	 */
	UPROPERTY(EditAnywhere, Category = "Melee|Fallback")
	bool bUseFallbackSwing = true;

	UPROPERTY(EditAnywhere, Category = "Melee|Fallback")
	FCombatDamageSpec FallbackDamage;

	UPROPERTY(EditAnywhere, Category = "Melee|Fallback", meta = (ClampMin = "0"))
	float FallbackReach = 200.f;

	/** Minimum time between fallback swings (the "attack speed"). */
	UPROPERTY(EditAnywhere, Category = "Melee|Fallback", meta = (ClampMin = "0.05"))
	float FallbackInterval = 0.3f;

private:
	const FMeleeAttack* SelectAttack(EMeleeAttackKind Kind);
	bool StartAttack(const FMeleeAttack& Attack, EMeleeAttackKind Kind, AActor* Target, const FVector& DesiredFacing);
	void FinishAttack(bool bInterrupted);
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, int32 EndedAttackId);

	void StartMagnetism(AActor* Target, const FVector& DesiredFacing);
	void TickMagnetism(float DeltaTime);

	void GatherSamplePoints(TArray<FVector>& OutPoints) const;
	void TickWeaponTrace();
	void ProcessHit(const FHitResult& HitResult, const FVector& SwingDirection);

	bool PerformFallbackSwing(EMeleeAttackKind Kind, AActor* Target, const FVector& DesiredFacing);
	void TryConsumeBufferedInput();
	void RestoreGravity();
	void RefreshTickEnabled();

	UPROPERTY()
	TObjectPtr<ACharacter> OwnerCharacter;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> TraceComponent;

	FName TraceStartSocket;
	FName TraceEndSocket;

	FMeleeAttack CurrentAttack;
	int32 AttackId = 0;
	int32 LightIndex = 0;
	int32 AirIndex = 0;
	bool bIsAttacking = false;
	bool bComboWindowOpen = false;
	bool bHitWindowActive = false;

	// Hit window state
	FName ActiveStartSocket;
	FName ActiveEndSocket;
	float ActiveTraceRadius = 0.f;
	TArray<FVector> PreviousSamplePoints;
	TSet<TWeakObjectPtr<AActor>> HitActors;

	// Input buffer
	bool bHasBufferedInput = false;
	EMeleeAttackKind BufferedKind = EMeleeAttackKind::Light;
	TWeakObjectPtr<AActor> BufferedTarget;
	FVector BufferedFacing = FVector::ZeroVector;
	float BufferedTime = 0.f;

	// Magnetism
	bool bMagnetizing = false;
	FVector MagnetStart = FVector::ZeroVector;
	FVector MagnetEnd = FVector::ZeroVector;
	float MagnetElapsed = 0.f;

	// Air hang
	float SavedGravityScale = -1.f;

	// Fallback swings
	float LastFallbackSwingTime = -1000.f;
	int32 FallbackComboCount = 0;
};
