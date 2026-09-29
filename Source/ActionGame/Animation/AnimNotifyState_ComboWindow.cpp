#include "Animation/AnimNotifyState_ComboWindow.h"
#include "Combat/MeleeComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotifyState_ComboWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	const AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UMeleeComponent* Melee = Owner ? Owner->FindComponentByClass<UMeleeComponent>() : nullptr)
	{
		Melee->OpenComboWindow();
	}
}

void UAnimNotifyState_ComboWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	const AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	if (UMeleeComponent* Melee = Owner ? Owner->FindComponentByClass<UMeleeComponent>() : nullptr)
	{
		Melee->CloseComboWindow();
	}
}
