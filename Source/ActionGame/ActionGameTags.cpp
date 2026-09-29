#include "ActionGameTags.h"

namespace ActionGameTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Character has died.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_HitStun, "State.HitStun", "Interrupted by a hit; cannot act.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Staggered, "State.Staggered", "Poise broken; long vulnerable window.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_KnockedDown, "State.KnockedDown", "Launched or knocked to the ground.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "Ignores incoming damage (i-frames).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_SuperArmor, "State.SuperArmor", "Takes damage but is never interrupted.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Attacking, "State.Attacking", "Performing a melee attack.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dodging, "State.Dodging", "Performing an evade.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Aiming, "State.Aiming", "Aiming guns.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Levitating, "State.Levitating", "Hovering in the air.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Slamming, "State.Slamming", "Diving into a ground slam.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_KineticLaunch, "State.KineticLaunch", "Self-thrown through the air.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Blade, "Damage.Blade", "Sword / slash damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Bullet, "Damage.Bullet", "Hitscan gun damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Kinetic, "Damage.Kinetic", "Telekinetic impacts (thrown objects, self launch).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Slam, "Damage.Slam", "Ground slam shockwave.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Blunt, "Damage.Blunt", "Enemy punches / heavy blunt weapons.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Energy, "Damage.Energy", "Enemy energy projectiles and blasts.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Telegraph, "Event.Telegraph", "Attack wind-up warning (flash + sound).");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_FireProjectile, "Event.FireProjectile", "Spawn the current attack's projectiles.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_AreaImpact, "Event.AreaImpact", "Apply the current attack's area damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_ChargeStart, "Event.ChargeStart", "Begin a charging dash toward the target.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_ChargeEnd, "Event.ChargeEnd", "End a charging dash.");
}
