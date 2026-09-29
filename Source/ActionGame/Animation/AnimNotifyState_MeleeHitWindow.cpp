#include "Animation/AnimNotifyState_MeleeHitWindow.h"
#include "Combat/MeleeComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace
{
	UMeleeComponent* FindMelee(const USkeletalMeshComponent* MeshComp)
	{
		const AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		return Owner ? Owner->FindComponentByClass<UMeleeComponent>() : nullptr;
	}
}

void UAnimNotifyState_MeleeHitWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (UMeleeComponent* Melee = FindMelee(MeshComp))
	{
		Melee->BeginHitWindow(StartSocketOverride, EndSocketOverride, RadiusOverride);
	}
}

void UAnimNotifyState_MeleeHitWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (UMeleeComponent* Melee = FindMelee(MeshComp))
	{
		Melee->EndHitWindow();
	}
}
