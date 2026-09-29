#include "Characters/ActionPlayerCharacter.h"
#include "Abilities/DodgeComponent.h"
#include "Abilities/EnergyComponent.h"
#include "Abilities/KineticLaunchComponent.h"
#include "Abilities/LevitationComponent.h"
#include "Abilities/TelekinesisComponent.h"
#include "ActionGameTags.h"
#include "Camera/CameraComponent.h"
#include "Combat/GunComponent.h"
#include "Combat/HitReactionComponent.h"
#include "Combat/MeleeComponent.h"
#include "Combat/TargetingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Input/ActionInputConfig.h"
#include "InputActionValue.h"
#include "UObject/ConstructorHelpers.h"

AActionPlayerCharacter::AActionPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	Team = ECombatTeam::Player;

	GetCapsuleComponent()->InitCapsuleSize(35.f, 90.f);
	GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -90.f), FRotator(0.f, -90.f, 0.f));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = DefaultArmLength;
	CameraBoom->SocketOffset = DefaultSocketOffset;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;       // Slight lag makes fast movement read as speed.
	CameraBoom->CameraLagSpeed = 14.f;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->FieldOfView = DefaultFOV;

	// Placeholder weapon shapes so the prototype is readable; replace the meshes in the Blueprint.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	auto SetupWeaponMesh = [this](UStaticMeshComponent* Weapon, FName Socket)
	{
		Weapon->SetupAttachment(GetMesh(), Socket);
		Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Weapon->SetGenerateOverlapEvents(false);
		Weapon->CastShadow = true;
	};

	BladeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeMesh"));
	SetupWeaponMesh(BladeMesh, BladeAttachSocket);
	if (CylinderMesh.Succeeded())
	{
		BladeMesh->SetStaticMesh(CylinderMesh.Object);
		BladeMesh->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.9f));
		BladeMesh->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	}

	GunMeshRight = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMeshRight"));
	SetupWeaponMesh(GunMeshRight, RightGunAttachSocket);
	GunMeshLeft = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMeshLeft"));
	SetupWeaponMesh(GunMeshLeft, LeftGunAttachSocket);
	if (CubeMesh.Succeeded())
	{
		for (UStaticMeshComponent* Gun : { GunMeshRight.Get(), GunMeshLeft.Get() })
		{
			Gun->SetStaticMesh(CubeMesh.Object);
			Gun->SetRelativeScale3D(FVector(0.25f, 0.06f, 0.12f));
		}
	}

	TargetingComponent = CreateDefaultSubobject<UTargetingComponent>(TEXT("Targeting"));
	EnergyComponent = CreateDefaultSubobject<UEnergyComponent>(TEXT("Energy"));
	DodgeComponent = CreateDefaultSubobject<UDodgeComponent>(TEXT("Dodge"));
	LevitationComponent = CreateDefaultSubobject<ULevitationComponent>(TEXT("Levitation"));
	KineticLaunchComponent = CreateDefaultSubobject<UKineticLaunchComponent>(TEXT("KineticLaunch"));
	TelekinesisComponent = CreateDefaultSubobject<UTelekinesisComponent>(TEXT("Telekinesis"));
	GunComponent = CreateDefaultSubobject<UGunComponent>(TEXT("Gun"));

	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;

	// Player-friendly hit rules: brief mercy i-frames after being interrupted, and no corpse cleanup.
	HitReactionComponent->SetPostHitInvulnerability(0.5f);
	HitReactionComponent->SetCorpseLifeSpan(0.f);
}

void AActionPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	MeleeComponent->SetTraceSource(BladeMesh, BladeBaseSocket, BladeTipSocket);
	MeleeComponent->OnHitLanded.AddDynamic(this, &AActionPlayerCharacter::HandleMeleeHitLanded);

	GunComponent->SetMuzzles({ GunMeshRight.Get(), GunMeshLeft.Get() });
	GunComponent->OnShotFired.AddDynamic(this, &AActionPlayerCharacter::HandleShotFired);

	UpdateRotationMode();
	UpdateWeaponVisibility();
}

// ---------------------------------------------------------------------------------------------
// Input setup
// ---------------------------------------------------------------------------------------------

UActionInputConfig* AActionPlayerCharacter::GetInputConfig()
{
	if (!ActiveInputConfig)
	{
		ActiveInputConfig = InputConfig ? InputConfig.Get() : UActionInputConfig::CreateDefault(this);
	}
	return ActiveInputConfig;
}

void AActionPlayerCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	const APlayerController* PC = Cast<APlayerController>(Controller);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = PC && PC->GetLocalPlayer()
		? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()) : nullptr;
	if (Subsystem)
	{
		Subsystem->AddMappingContext(GetInputConfig()->MappingContext, 0);
	}
}

void AActionPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	const UActionInputConfig* Config = GetInputConfig();
	if (!Input || !Config)
	{
		return;
	}

	Input->BindAction(Config->Move, ETriggerEvent::Triggered, this, &AActionPlayerCharacter::OnMove);
	Input->BindAction(Config->Look, ETriggerEvent::Triggered, this, &AActionPlayerCharacter::OnLook);
	Input->BindAction(Config->Jump, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnJumpStarted);
	Input->BindAction(Config->Jump, ETriggerEvent::Completed, this, &AActionPlayerCharacter::OnJumpCompleted);
	Input->BindAction(Config->Attack, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnAttackStarted);
	Input->BindAction(Config->Attack, ETriggerEvent::Completed, this, &AActionPlayerCharacter::OnAttackCompleted);
	Input->BindAction(Config->HeavyAttack, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnHeavyAttack);
	Input->BindAction(Config->Aim, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnAimStarted);
	Input->BindAction(Config->Aim, ETriggerEvent::Completed, this, &AActionPlayerCharacter::OnAimCompleted);
	Input->BindAction(Config->Telekinesis, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnTelekinesisStarted);
	Input->BindAction(Config->Telekinesis, ETriggerEvent::Completed, this, &AActionPlayerCharacter::OnTelekinesisCompleted);
	Input->BindAction(Config->KineticLaunch, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnKineticLaunch);
	Input->BindAction(Config->Dodge, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnDodge);
	Input->BindAction(Config->LockOn, ETriggerEvent::Started, this, &AActionPlayerCharacter::OnLockOn);
}

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

bool AActionPlayerCharacter::CanUseAbilities() const
{
	return CanAct() && !KineticLaunchComponent->IsLaunching() && !LevitationComponent->IsSlamming();
}

FVector AActionPlayerCharacter::GetMoveInputDirection() const
{
	return GetLastMovementInputVector().GetSafeNormal2D();
}

FVector AActionPlayerCharacter::GetCameraForward() const
{
	return Controller ? Controller->GetControlRotation().Vector() : GetActorForwardVector();
}

// ---------------------------------------------------------------------------------------------
// Input handlers
// ---------------------------------------------------------------------------------------------

void AActionPlayerCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller || !CanAct())
	{
		return;
	}
	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Axis.X);
}

void AActionPlayerCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();

	if (TargetingComponent->IsLockedOn())
	{
		// While locked, the camera is driven by the target. Horizontal look input accumulates
		// (and decays in Tick); a deliberate flick past the threshold switches targets.
		// Accumulating makes mouse flicks and stick pushes behave the same.
		TargetSwitchAccumulator += Axis.X;
		const float Now = GetWorld()->GetTimeSeconds();
		if (FMath::Abs(TargetSwitchAccumulator) >= TargetSwitchThreshold && Now - LastTargetSwitchTime > 0.3f)
		{
			TargetingComponent->SwitchTarget(TargetSwitchAccumulator);
			TargetSwitchAccumulator = 0.f;
			LastTargetSwitchTime = Now;
		}
		return;
	}

	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AActionPlayerCharacter::OnJumpStarted()
{
	bJumpHeld = true;
	if (!CanUseAbilities())
	{
		return;
	}
	if (GetCharacterMovement()->IsFalling())
	{
		LevitationComponent->StartLevitation();
	}
	else if (!MeleeComponent->IsAttacking() || MeleeComponent->CanBeCanceled())
	{
		MeleeComponent->CancelAttack();
		Jump();
	}
}

void AActionPlayerCharacter::OnJumpCompleted()
{
	bJumpHeld = false;
	StopJumping();
	LevitationComponent->StopLevitation();
}

void AActionPlayerCharacter::OnAttackStarted()
{
	if (!CanUseAbilities())
	{
		return;
	}
	if (bIsAiming)
	{
		GunComponent->StartFiring();
		return;
	}

	EMeleeAttackKind Kind = EMeleeAttackKind::Light;
	if (GetCharacterMovement()->IsFalling())
	{
		Kind = EMeleeAttackKind::Air;
	}
	else if (DodgeComponent->IsInDashAttackWindow())
	{
		Kind = EMeleeAttackKind::Dash;
	}

	const FVector InputDirection = GetMoveInputDirection();
	AActor* Target = TargetingComponent->GetMeleeTarget(InputDirection);
	const FVector Facing = InputDirection.IsNearlyZero() ? GetCameraForward() : InputDirection;
	MeleeComponent->RequestAttack(Kind, Target, Facing);
}

void AActionPlayerCharacter::OnAttackCompleted()
{
	GunComponent->StopFiring();
}

void AActionPlayerCharacter::OnHeavyAttack()
{
	if (!CanUseAbilities())
	{
		return;
	}
	if (GetCharacterMovement()->IsFalling())
	{
		LevitationComponent->TryGroundSlam();
		return;
	}
	const FVector InputDirection = GetMoveInputDirection();
	AActor* Target = TargetingComponent->GetMeleeTarget(InputDirection);
	const FVector Facing = InputDirection.IsNearlyZero() ? GetCameraForward() : InputDirection;
	MeleeComponent->RequestAttack(EMeleeAttackKind::Heavy, Target, Facing);
}

void AActionPlayerCharacter::OnAimStarted()
{
	SetAiming(true);
}

void AActionPlayerCharacter::OnAimCompleted()
{
	SetAiming(false);
}

void AActionPlayerCharacter::OnTelekinesisStarted()
{
	if (CanUseAbilities())
	{
		TelekinesisComponent->BeginGrab();
	}
}

void AActionPlayerCharacter::OnTelekinesisCompleted()
{
	TelekinesisComponent->RequestThrow(TargetingComponent->GetAimTarget(AbilityTargetRange));
}

void AActionPlayerCharacter::OnKineticLaunch()
{
	if (CanUseAbilities())
	{
		KineticLaunchComponent->TryLaunch(TargetingComponent->GetAimTarget(AbilityTargetRange), GetCameraForward());
	}
}

void AActionPlayerCharacter::OnDodge()
{
	if (CanUseAbilities())
	{
		LevitationComponent->StopLevitation();
		DodgeComponent->TryDodge(GetMoveInputDirection());
	}
}

void AActionPlayerCharacter::OnLockOn()
{
	TargetingComponent->ToggleLockOn();
	UpdateRotationMode();
}

// ---------------------------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------------------------

void AActionPlayerCharacter::SetAiming(bool bNewAiming)
{
	if (bIsAiming == bNewAiming)
	{
		return;
	}
	bIsAiming = bNewAiming;

	if (bIsAiming)
	{
		AddStateTag(ActionGameTags::State_Aiming);
		if (MeleeComponent->CanBeCanceled())
		{
			MeleeComponent->CancelAttack();
		}
	}
	else
	{
		RemoveStateTag(ActionGameTags::State_Aiming);
		GunComponent->StopFiring();
	}

	GetCharacterMovement()->MaxWalkSpeed = bIsAiming ? AimWalkSpeed : RunSpeed;
	UpdateRotationMode();
}

void AActionPlayerCharacter::UpdateRotationMode()
{
	// Aiming or locked on: strafe and face the camera/target. Otherwise: turn toward movement.
	const bool bStrafe = bIsAiming || TargetingComponent->IsLockedOn();
	bUseControllerRotationYaw = bStrafe;
	GetCharacterMovement()->bOrientRotationToMovement = !bStrafe;
}

void AActionPlayerCharacter::HandleMeleeHitLanded(const FCombatHit& Hit, const FCombatDamageResult& Result)
{
	EnergyComponent->AddEnergy(EnergyPerMeleeHit);
}

void AActionPlayerCharacter::HandleShotFired(int32 MuzzleIndex, bool bHitTarget)
{
	LastShotTime = GetWorld()->GetTimeSeconds();
}

void AActionPlayerCharacter::HandleInterrupted(EHitReaction Reaction, bool bStaggered)
{
	Super::HandleInterrupted(Reaction, bStaggered);
	GunComponent->StopFiring();
	TelekinesisComponent->Release();
	LevitationComponent->StopLevitation();
}

void AActionPlayerCharacter::HandleDeath(AActor* DeadActor, const FCombatHit& KillingHit)
{
	Super::HandleDeath(DeadActor, KillingHit);
	SetAiming(false);
	GunComponent->StopFiring();
	TelekinesisComponent->Release();
	LevitationComponent->StopLevitation();
	TargetingComponent->ClearLockOn();

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		DisableInput(PC);
	}
}

void AActionPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	DodgeComponent->ResetAirDodge();
	MeleeComponent->ResetAirCombo();
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void AActionPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TargetSwitchAccumulator = FMath::FInterpTo(TargetSwitchAccumulator, 0.f, DeltaTime, 6.f);
	UpdateLevitationHold();
	UpdateLockOnRotation(DeltaTime);
	UpdateCamera(DeltaTime);
	UpdateWeaponVisibility();

	// Lock can break on its own (target died, out of range): keep the movement mode in sync.
	const bool bShouldStrafe = bIsAiming || TargetingComponent->IsLockedOn();
	if (bUseControllerRotationYaw != bShouldStrafe)
	{
		UpdateRotationMode();
	}
}

void AActionPlayerCharacter::UpdateLevitationHold()
{
	// Holding jump through the apex of a jump starts levitation (Control-style).
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (bJumpHeld && Movement->IsFalling() && Movement->Velocity.Z <= 0.f && !LevitationComponent->IsLevitating() && CanUseAbilities())
	{
		LevitationComponent->StartLevitation();
	}
}

void AActionPlayerCharacter::UpdateLockOnRotation(float DeltaTime)
{
	const AActor* Target = TargetingComponent->GetLockedTarget();
	if (!Target || !Controller)
	{
		return;
	}
	const FVector CameraLocation = FollowCamera->GetComponentLocation();
	FRotator Desired = (Target->GetActorLocation() - CameraLocation).Rotation();
	Desired.Pitch += LockOnPitchOffset;
	Desired.Roll = 0.f;
	Controller->SetControlRotation(FMath::RInterpTo(Controller->GetControlRotation(), Desired, DeltaTime, LockOnRotationSpeed));
}

void AActionPlayerCharacter::UpdateCamera(float DeltaTime)
{
	const bool bLevitating = LevitationComponent->IsLevitating();
	const float TargetArm = (bIsAiming ? AimArmLength : DefaultArmLength) + (bLevitating && !bIsAiming ? LevitateArmLengthBonus : 0.f);
	const FVector TargetOffset = bIsAiming ? AimSocketOffset : DefaultSocketOffset;
	const float TargetFOV = bIsAiming ? AimFOV : DefaultFOV;

	CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArm, DeltaTime, CameraBlendSpeed);
	CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetOffset, DeltaTime, CameraBlendSpeed);
	FollowCamera->SetFieldOfView(FMath::FInterpTo(FollowCamera->FieldOfView, TargetFOV, DeltaTime, CameraBlendSpeed));
}

void AActionPlayerCharacter::UpdateWeaponVisibility()
{
	// Guns materialize while aiming (like Control's shifting Service Weapon); otherwise the blade is drawn.
	const bool bGunsOut = bIsAiming || GetWorld()->GetTimeSeconds() - LastShotTime < GunHolsterDelay;
	if (GunMeshRight->IsVisible() != bGunsOut)
	{
		GunMeshRight->SetVisibility(bGunsOut);
		GunMeshLeft->SetVisibility(bGunsOut);
		BladeMesh->SetVisibility(!bGunsOut);
	}
}
