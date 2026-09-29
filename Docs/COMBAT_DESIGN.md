# Combat Design Notes

This document explains **why** each system works the way it does, and which numbers to turn when tuning.
The goal of the demo is *combat feel*: every input should respond instantly, every hit should look and feel like it landed, and every enemy reaction should tell the player what just happened.

---

## 1. Design pillars

1. **Responsiveness over commitment (for the player).** Inputs are buffered and dodges can cancel attack recovery. The player should never feel stuck in an animation. The one committed part of an attack is its active frames.
2. **Every hit is felt.** Hitstop, camera trauma, a hit flash, a physical flinch and a sound play on every hit, all routed through one pipeline so nothing is forgotten.
3. **Enemies are readable.** Wind-ups glow before attacks, enemies commit to a direction when they swing, only a few attack at once, and every attack is followed by a recovery window you can punish.
4. **Tools have roles.** The blade deals damage and restores energy. Bullets chip away at poise from range. Telekinesis and slams break poise and control crowds. Mixing tools is rewarded, and no single tool wins alone.
5. **Aggression is rewarded.** Health orbs drop from kills (Control's approach), melee hits refill energy, and perfect dodges grant slow-mo and energy.

---

## 2. The hit pipeline (`UCombatLibrary::ApplyDamage`)

Every damage source builds an `FCombatHit` and calls `ApplyDamage`. That call:

1. checks team rules;
2. applies health and poise damage (`UHealthComponent`);
3. notifies the attacker (hit markers, energy gain);
4. runs the **feedback subsystem**:
   - **Hitstop:** the attacker and victim freeze for 40–120 ms by scaling their own `CustomTimeDilation`. It's per-actor, not global, so the world keeps moving and the game never feels laggy. Ranged hits only freeze the victim, because freezing the shooter feels sluggish.
   - **Camera trauma:** added only when the local player is involved. Shake grows with trauma², so small hits stay subtle and big hits feel violent.
   - **Layered impact VFX and SFX:** a base effect per damage type, plus extra layers for criticals, poise breaks and kills. Pitch is randomized so repeated hits don't sound robotic.

Then the victim's **HitReactionComponent** plays its layers:

| Layer | When | Why |
|---|---|---|
| Hit flash | Every hit | Instant, unambiguous confirmation |
| Physical flinch | Every hit | The struck bone chain briefly simulates physics and is kicked in the hit direction, then blends back to the animation. Every hit looks different and location-aware (a head hit snaps the head back), even with no hit animations. |
| Interrupt montage + stun | Hit reaction ≥ victim's `InterruptThreshold` | Grunts flinch from anything, while bosses need big hits. The same attack reads differently per enemy. |
| Knockback / launch / air-juggle lift | Interrupting hits | Movement sells force. Airborne victims get a small lift on each hit, so juggles work. |
| Stagger | Poise reaches 0 | A long punish window where the victim takes 1.5× damage. This is the counter to super armor. |
| Ragdoll | Death | Starts *after* hitstop ends ("freeze, then snap") and inherits the killing blow's momentum |

**Key trick:** physics can't be slowed per actor, so physical flinches and ragdolls are **queued until the victim's hitstop ends**. The victim freezes on impact, then snaps away. This one ordering detail makes hits feel dramatically heavier.

---

## 3. Blade

- **Combos:** `LightCombo[]` plays in order. `HeavyFinishers[N]` is used after N light hits, so Light-Light-Heavy is a different move from Light-Heavy. Two buttons give a lot of variety.
- **Input buffering:** presses during an attack are remembered for `InputBufferTime` (0.35 s) and fire when the montage's **Combo Window** opens. Presses older than that are dropped, which prevents "ghost" attacks.
- **Attack magnetism:** on attack start, the player snaps to face the target and slides into range over 0.12 s. Spacing forgiveness is standard in character action games. Players judge whether they hit what they meant to, not how precisely they aimed.
- **Swept traces:** several points along the blade are swept from last frame's position to this frame's, so fast swings never pass through enemies at low frame rates. Each enemy is hit once per swing.
- **Air combat:** air attacks can lower gravity (`AirGravityScale`) so the player hangs in the air. Combined with the air-juggle lift on enemies, this gives DMC-style juggles.
- **Energy loop:** each landed blade hit refunds energy (`EnergyPerMeleeHit`), so the blade fuels your powers.

## 4. Dual pistols

- The shot aims from the camera, then traces from the muzzle to the aim point. Cover right in front of the gun still blocks, so you never shoot through a wall you're hugging.
- The guns alternate muzzles. Spread blooms per shot and recovers quickly, and the crosshair visibly expands, so players can read accuracy.
- **Ammo regenerates** after a short pause, like Control's Service Weapon. Guns are always available but never replace the blade.
- Bullets use `Reaction = None`: they chip health and poise and twitch the victim physically, but never interrupt. That's their role: soften enemies up from range, set up staggers, and let the blade or telekinesis cash them in. Headshots crit for ×2.

## 5. Telekinesis ("Launch")

- The object closest to the crosshair is highlighted on the HUD before you press, so you always know what you'll grab.
- **Tap** to grab and throw in one motion. **Hold** to keep the object floating over your shoulder.
- If nothing is in range, a chunk of debris is ripped from the floor, so the power is never "dead".
- Throws are aim-assisted toward the target, with velocity lead so moving enemies are hit.
- Damage scales with impact speed, and the object shatters on impact.
- **Enemy projectiles are catchable** and deal ×2 damage when thrown back. It's a high-skill, high-reward counter to gunners.

## 6. Mobility

- **Dodge:** a fast ease-out burst with 0.25 s of i-frames and one air dash per jump. It cancels attack recovery.
  **Perfect dodge:** if a hit is negated during the first 0.18 s of the dodge, you get slow-mo (0.25× for 0.6 s) and +25 energy. This rewards reading telegraphs over mashing.
- **Levitation:** hold jump in the air, or keep holding through the jump's apex, to hover. Gravity is removed and vertical speed eases into a slow drift. It drains energy, and the camera pulls back so you can see the arena.
- **Ground slam:** heavy attack in the air. You dive with super armor, and on impact a radial shockwave launches enemies. Damage grows with dive height, which rewards gaining altitude first.
- **Kinetic Launch:** the player is thrown at the aimed enemy, homing, invulnerable in flight, and hits with a heavy kinetic impact before bouncing up. It's a gap closer, an escape, and an air-combo starter in one.

## 7. Enemies & AI

- **Attack tokens:** at most 2 melee and 2 ranged enemies may attack at once (Project Settings). The rest circle at their preferred range. This is the Batman: Arkham / DOOM technique, and it keeps crowds readable and fair.
- **Telegraph:** a glow (`TelegraphGlow` material parameter) plus optional VFX/SFX before each attack.
- **Commit:** enemies track the player during the wind-up but stop turning once the swing's active frames start, so a well-timed sidestep always works.
- **Recovery:** every attack is followed by 0.5–1.1 s of recovery. That's your guaranteed punish window.
- **Archetypes:**
  - **Grunt** (melee): flinches from everything and is the satisfying fodder.
  - **Gunner**: keeps range, fires slow volleys, and shoves you away if crowded.
  - **Boss**: armored, so only poise breaks and knockdowns interrupt it.

## 8. Boss: "The Hollow Warden"

| Phase | Health | Changes |
|---|---|---|
| Awakened | 100% | Cleave, telegraphed Slam (super armor, long recovery), 3-shot Volley |
| Enraged | ≤60% | ×1.25 speed, faster wind-ups, 5-shot volley, bigger slam, **2 grunt reinforcements** |
| Desperate | ≤30% | ×1.5 speed, 7-shot **homing** volley, largest slam |

- **Phase transitions** are dramatic beats: the boss becomes invulnerable and roars, a shockwave pushes the player back, and brief slow-mo plays. The transition also gives the player a moment to breathe.
- **The main loop:** chip poise with bullets, blade and slams, until the boss **staggers** for 2.5 s, taking 1.5× damage, and you unload.
- **The boss kill** triggers a 2.5 s slow-motion finish, then the victory banner.

## 9. Tuning cheat-sheet

| Feel problem | Turn this |
|---|---|
| Hits feel weak | `HitStop` (0.06–0.12), `CameraTrauma`, `PhysicalImpulse` on the attack's damage spec |
| Game feels "laggy" | Lower `HitStop` or `MaxHitStop` in the feedback config |
| Combos feel unresponsive | Open the Combo Window earlier, raise `InputBufferTime` |
| Attacks whiff | `MagnetismRange` / `MagnetismStopDistance`, `TraceRadius` |
| Enemies overwhelm you | `MaxSimultaneous*Attackers`, enemy `RecoveryTime*`, attack `Cooldown` |
| Boss too tanky | Boss `MaxPoise` (stagger frequency) before `MaxHealth` |
| Levitation too strong | `EnergyDrainPerSecond` |
