#include "Characters/RangedEnemyCharacter.h"
#include "ActionGameTags.h"
#include "Combat/ActionProjectile.h"
#include "Combat/HealthComponent.h"

ARangedEnemyCharacter::ARangedEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlaceholderColor = FLinearColor(0.95f, 0.5f, 0.1f);
	PreferredRange = 1100.f;
	StrafeSpeed = 320.f;
	ChaseSpeed = 450.f;
	HealthComponent->SetMaxHealth(60.f);
	HealthComponent->SetMaxPoise(30.f);

	Attacks.Reset();

	FEnemyAttack Volley;
	Volley.Name = TEXT("Volley");
	Volley.MinRange = 450.f;
	Volley.MaxRange = 2200.f;
	Volley.Cooldown = 2.5f;
	Volley.Weight = 3.f;
	Volley.FallbackWindup = 0.7f;
	Volley.FallbackRecovery = 0.5f;
	Volley.ProjectileClass = AActionProjectile::StaticClass();
	Volley.ProjectileCount = 2;
	Volley.ProjectileSpread = 12.f;
	Attacks.Add(Volley);

	// Close-range answer: a shove that restores the gunner's preferred distance.
	FEnemyAttack Shove;
	Shove.Name = TEXT("Shove");
	Shove.MaxRange = 220.f;
	Shove.Cooldown = 3.f;
	Shove.Weight = 1.f;
	Shove.FallbackWindup = 0.45f;
	Shove.FallbackRecovery = 0.5f;
	Shove.Attack.Damage.Damage = 6.f;
	Shove.Attack.Damage.PoiseDamage = 10.f;
	Shove.Attack.Damage.Reaction = EHitReaction::Knockback;
	Shove.Attack.Damage.KnockbackStrength = 1000.f;
	Shove.Attack.Damage.HitStop = 0.06f;
	Shove.Attack.Damage.CameraTrauma = 0.25f;
	Shove.Attack.Damage.DamageType = ActionGameTags::Damage_Blunt;
	Attacks.Add(Shove);
}
