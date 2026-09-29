#include "Animation/AnimNotify_CombatEvent.h"
#include "Characters/ActionCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_CombatEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (AActionCharacterBase* Character = MeshComp ? Cast<AActionCharacterBase>(MeshComp->GetOwner()) : nullptr)
	{
		Character->HandleCombatEvent(EventTag);
	}
}

FString UAnimNotify_CombatEvent::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? EventTag.ToString() : TEXT("Combat Event");
}
