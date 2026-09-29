#include "Combat/CombatFeedbackConfig.h"

const FImpactEffect& UCombatFeedbackConfig::GetImpactEffect(const FGameplayTag& DamageType) const
{
	if (const FImpactEffect* Found = ImpactEffects.Find(DamageType))
	{
		return *Found;
	}
	return DefaultImpact;
}
