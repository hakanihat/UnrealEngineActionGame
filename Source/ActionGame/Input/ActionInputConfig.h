#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ActionInputConfig.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * All player input actions in one asset. Assign an authored asset on the player Blueprint to
 * rebind keys in the editor; if none is assigned, CreateDefault() builds the default
 * keyboard/mouse + gamepad scheme at runtime so the project is playable out of the box.
 */
UCLASS(BlueprintType)
class ACTIONGAME_API UActionInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Builds the default bindings (see README for the control scheme). */
	static UActionInputConfig* CreateDefault(UObject* Outer);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Move;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Look;

	/** Jump on the ground; hold in the air to levitate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Jump;

	/** Light attack, or fire while aiming. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Attack;

	/** Heavy attack / finisher, or ground slam in the air. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> HeavyAttack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Aim;

	/** Hold to grab, release to throw. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Telekinesis;

	/** Throw yourself at the target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> KineticLaunch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> Dodge;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LockOn;
};
