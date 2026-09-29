#pragma once

#include "CoreMinimal.h"
#include "Characters/ActionCharacterBase.h"
#include "ActionPlayerCharacter.generated.h"

class UActionInputConfig;
class UCameraComponent;
class UDodgeComponent;
class UEnergyComponent;
class UGunComponent;
class UKineticLaunchComponent;
class ULevitationComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class UTargetingComponent;
class UTelekinesisComponent;
struct FInputActionValue;

/**
 * The player. Owns the camera and translates input into context-sensitive actions:
 *  - Attack: blade light combo on the ground, air combo when airborne, dash attack after a
 *    dodge, and dual pistols while aiming.
 *  - Heavy: combo finisher on the ground, ground slam in the air.
 *  - Jump: jump, then hold in the air to levitate.
 *  - Telekinesis: grab/hold/throw. Kinetic Launch: throw yourself at the target.
 * Each ability lives in its own component; this class only decides *which* to use,
 * keeping abilities reusable and this class readable.
 */
UCLASS()
class ACTIONGAME_API AActionPlayerCharacter : public AActionCharacterBase
{
	GENERATED_BODY()

public:
	AActionPlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void NotifyControllerChanged() override;
	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintPure, Category = "Player")
	bool IsAiming() const { return bIsAiming; }

	UTargetingComponent* GetTargetingComponent() const { return TargetingComponent; }
	UEnergyComponent* GetEnergyComponent() const { return EnergyComponent; }
	UGunComponent* GetGunComponent() const { return GunComponent; }
	UTelekinesisComponent* GetTelekinesisComponent() const { return TelekinesisComponent; }
	ULevitationComponent* GetLevitationComponent() const { return LevitationComponent; }

protected:
	virtual void BeginPlay() override;
	virtual void HandleDeath(AActor* DeadActor, const FCombatHit& KillingHit) override;
	virtual void HandleInterrupted(EHitReaction Reaction, bool bStaggered) override;

	// --- Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BladeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GunMeshRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GunMeshLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTargetingComponent> TargetingComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UEnergyComponent> EnergyComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDodgeComponent> DodgeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<ULevitationComponent> LevitationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UKineticLaunchComponent> KineticLaunchComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UTelekinesisComponent> TelekinesisComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGunComponent> GunComponent;

	// --- Input ---

	/** Optional authored bindings. Left empty, default bindings are generated at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UActionInputConfig> InputConfig;

	// --- Weapon attachment ---

	UPROPERTY(EditAnywhere, Category = "Weapons")
	FName BladeAttachSocket = TEXT("hand_r");

	UPROPERTY(EditAnywhere, Category = "Weapons")
	FName LeftGunAttachSocket = TEXT("hand_l");

	UPROPERTY(EditAnywhere, Category = "Weapons")
	FName RightGunAttachSocket = TEXT("hand_r");

	/** Sockets on the blade mesh marking the base and tip of the cutting edge. */
	UPROPERTY(EditAnywhere, Category = "Weapons")
	FName BladeBaseSocket = TEXT("BladeBase");

	UPROPERTY(EditAnywhere, Category = "Weapons")
	FName BladeTipSocket = TEXT("BladeTip");

	/** Guns stay out this long after the last shot before the blade returns. */
	UPROPERTY(EditAnywhere, Category = "Weapons", meta = (ClampMin = "0"))
	float GunHolsterDelay = 0.6f;

	// --- Camera ---

	UPROPERTY(EditAnywhere, Category = "Camera")
	float DefaultArmLength = 420.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FVector DefaultSocketOffset = FVector(0.f, 55.f, 65.f);

	UPROPERTY(EditAnywhere, Category = "Camera")
	float DefaultFOV = 90.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float AimArmLength = 190.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	FVector AimSocketOffset = FVector(0.f, 70.f, 55.f);

	UPROPERTY(EditAnywhere, Category = "Camera")
	float AimFOV = 70.f;

	/** Camera pulls back while levitating to show the arena below. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float LevitateArmLengthBonus = 120.f;

	UPROPERTY(EditAnywhere, Category = "Camera", meta = (ClampMin = "0"))
	float CameraBlendSpeed = 10.f;

	UPROPERTY(EditAnywhere, Category = "Camera|Lock On", meta = (ClampMin = "0"))
	float LockOnRotationSpeed = 8.f;

	/** Look slightly down on locked targets so the player stays in frame. */
	UPROPERTY(EditAnywhere, Category = "Camera|Lock On")
	float LockOnPitchOffset = -12.f;

	/** Accumulated horizontal look input needed to switch lock-on targets. */
	UPROPERTY(EditAnywhere, Category = "Camera|Lock On", meta = (ClampMin = "0"))
	float TargetSwitchThreshold = 12.f;

	// --- Movement / gameplay ---

	UPROPERTY(EditAnywhere, Category = "Movement")
	float RunSpeed = 650.f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float AimWalkSpeed = 420.f;

	/** Energy refunded per landed blade hit: the blade fuels the powers. */
	UPROPERTY(EditAnywhere, Category = "Gameplay", meta = (ClampMin = "0"))
	float EnergyPerMeleeHit = 4.f;

	UPROPERTY(EditAnywhere, Category = "Gameplay", meta = (ClampMin = "0"))
	float AbilityTargetRange = 2000.f;

private:
	UActionInputConfig* GetInputConfig();
	bool CanUseAbilities() const;
	FVector GetMoveInputDirection() const;
	FVector GetCameraForward() const;

	void SetAiming(bool bNewAiming);
	void UpdateRotationMode();
	void UpdateCamera(float DeltaTime);
	void UpdateLockOnRotation(float DeltaTime);
	void UpdateWeaponVisibility();
	void UpdateLevitationHold();

	UFUNCTION()
	void HandleMeleeHitLanded(const FCombatHit& Hit, const FCombatDamageResult& Result);

	UFUNCTION()
	void HandleShotFired(int32 MuzzleIndex, bool bHitTarget);

	// Input handlers
	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpStarted();
	void OnJumpCompleted();
	void OnAttackStarted();
	void OnAttackCompleted();
	void OnHeavyAttack();
	void OnAimStarted();
	void OnAimCompleted();
	void OnTelekinesisStarted();
	void OnTelekinesisCompleted();
	void OnKineticLaunch();
	void OnDodge();
	void OnLockOn();

	/** The config actually in use (authored or generated). */
	UPROPERTY(Transient)
	TObjectPtr<UActionInputConfig> ActiveInputConfig;

	bool bIsAiming = false;
	bool bJumpHeld = false;
	float LastShotTime = -1000.f;
	float LastTargetSwitchTime = -1000.f;
	float TargetSwitchAccumulator = 0.f;
};
