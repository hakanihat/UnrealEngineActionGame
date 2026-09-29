#include "Input/ActionInputConfig.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, Name);
		Action->ValueType = ValueType;
		return Action;
	}

	/** Maps a digital key onto one direction of a 2D axis (WASD -> Move). */
	void MapDirectionalKey(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bSwizzleToY, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bSwizzleToY)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
	}

	void MapStick(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bInvertY, const FVector& Scale, bool bScaleByDeltaTime)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(Context));
		if (bInvertY)
		{
			UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(Context);
			Negate->bX = false;
			Negate->bZ = false;
			Mapping.Modifiers.Add(Negate);
		}
		if (!Scale.Equals(FVector::OneVector))
		{
			UInputModifierScalar* Scalar = NewObject<UInputModifierScalar>(Context);
			Scalar->Scalar = Scale;
			Mapping.Modifiers.Add(Scalar);
		}
		if (bScaleByDeltaTime)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierScaleByDeltaTime>(Context));
		}
	}
}

UActionInputConfig* UActionInputConfig::CreateDefault(UObject* Outer)
{
	UActionInputConfig* Config = NewObject<UActionInputConfig>(Outer, TEXT("DefaultInputConfig"));

	Config->Move = MakeAction(Config, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	Config->Look = MakeAction(Config, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	Config->Jump = MakeAction(Config, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	Config->Attack = MakeAction(Config, TEXT("IA_Attack"), EInputActionValueType::Boolean);
	Config->HeavyAttack = MakeAction(Config, TEXT("IA_HeavyAttack"), EInputActionValueType::Boolean);
	Config->Aim = MakeAction(Config, TEXT("IA_Aim"), EInputActionValueType::Boolean);
	Config->Telekinesis = MakeAction(Config, TEXT("IA_Telekinesis"), EInputActionValueType::Boolean);
	Config->KineticLaunch = MakeAction(Config, TEXT("IA_KineticLaunch"), EInputActionValueType::Boolean);
	Config->Dodge = MakeAction(Config, TEXT("IA_Dodge"), EInputActionValueType::Boolean);
	Config->LockOn = MakeAction(Config, TEXT("IA_LockOn"), EInputActionValueType::Boolean);

	UInputMappingContext* Context = NewObject<UInputMappingContext>(Config, TEXT("IMC_Default"));
	Config->MappingContext = Context;

	// --- Keyboard & mouse ---
	MapDirectionalKey(Context, Config->Move, EKeys::W, true, false);
	MapDirectionalKey(Context, Config->Move, EKeys::S, true, true);
	MapDirectionalKey(Context, Config->Move, EKeys::A, false, true);
	MapDirectionalKey(Context, Config->Move, EKeys::D, false, false);

	{
		FEnhancedActionKeyMapping& MouseLook = Context->MapKey(Config->Look, EKeys::Mouse2D);
		UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(Context);
		NegateY->bX = false;
		NegateY->bZ = false;
		MouseLook.Modifiers.Add(NegateY);
	}

	Context->MapKey(Config->Jump, EKeys::SpaceBar);
	Context->MapKey(Config->Attack, EKeys::LeftMouseButton);
	Context->MapKey(Config->HeavyAttack, EKeys::F);
	Context->MapKey(Config->Aim, EKeys::RightMouseButton);
	Context->MapKey(Config->Telekinesis, EKeys::E);
	Context->MapKey(Config->KineticLaunch, EKeys::Q);
	Context->MapKey(Config->Dodge, EKeys::LeftShift);
	Context->MapKey(Config->LockOn, EKeys::MiddleMouseButton);
	Context->MapKey(Config->LockOn, EKeys::Tab);

	// --- Gamepad ---
	MapStick(Context, Config->Move, EKeys::Gamepad_Left2D, false, FVector::OneVector, false);
	// Stick look is a rate (degrees/second), so scale it by delta time.
	MapStick(Context, Config->Look, EKeys::Gamepad_Right2D, true, FVector(70.f, 45.f, 1.f), true);

	Context->MapKey(Config->Jump, EKeys::Gamepad_FaceButton_Bottom);
	Context->MapKey(Config->Attack, EKeys::Gamepad_FaceButton_Left);
	Context->MapKey(Config->HeavyAttack, EKeys::Gamepad_FaceButton_Top);
	Context->MapKey(Config->Dodge, EKeys::Gamepad_FaceButton_Right);
	Context->MapKey(Config->Aim, EKeys::Gamepad_LeftTrigger);
	Context->MapKey(Config->Attack, EKeys::Gamepad_RightTrigger);
	Context->MapKey(Config->Telekinesis, EKeys::Gamepad_RightShoulder);
	Context->MapKey(Config->KineticLaunch, EKeys::Gamepad_LeftShoulder);
	Context->MapKey(Config->LockOn, EKeys::Gamepad_RightThumbstick);

	return Config;
}
