# Asset Setup Guide: from capsules to a good-looking demo

The code runs with zero assets. This guide replaces the placeholders step by step. Each step is independent, so you can stop at any point and still have a working game.

Paths below assume you keep your content under `Content/ActionGame/`.

---

## Step 1: Get a character and base animations (5 min)

1. **Content Browser → Add → Add Feature or Content Pack → Third Person (Blueprint) → Add to Project.**
   This gives you `SKM_Manny` / `SKM_Quinn`, their physics assets, `ABP_Manny`, and locomotion animations.
2. The C++ defaults (bone names, mesh offset of −90 Z and −90 yaw) already match the UE5 Mannequin.

## Step 2: Player Blueprint

1. **Create the Blueprint:** right-click → Blueprint Class → search **ActionPlayerCharacter** → name it `BP_Player`.
2. **Set the mesh:** on the **Mesh** component, set Skeletal Mesh = `SKM_Manny` and Anim Class = `ABP_Manny` (for now; see Step 7).
3. **Use it as the player:** Project Settings → Game → **Action Game** → **Player Pawn Class** = `BP_Player`.
   You don't need a game-mode Blueprint.
4. **Check it:** press Play. You now have Manny with real locomotion, the physical flinches when hit, and a ragdoll on death.

## Step 3: Enemy Blueprints

| Blueprint | Parent class | Suggested mesh |
|---|---|---|
| `BP_Grunt` | EnemyCharacter | SKM_Quinn |
| `BP_Gunner` | RangedEnemyCharacter | SKM_Quinn (different material) |
| `BP_Boss` | BossCharacter | SKM_Manny (it is scaled ×1.4 automatically) |

Select the **Demo Arena Director** in your level and set its Grunt / Gunner / Boss classes to these Blueprints.

## Step 4: Hit flash and telegraph glow (materials)

The code pulses two scalar parameters on every material of the character's mesh:

| Parameter | Driven by | Meaning |
|---|---|---|
| `HitFlash` (0–1) | HitReactionComponent | White flash on every hit |
| `TelegraphGlow` (0–1) | EnemyCharacter | Warning glow before an enemy attacks |

To add them:
1. Duplicate the mannequin's material, e.g. `MI_Manny_01` → its parent material. Then add two **Scalar Parameters** named exactly `HitFlash` and `TelegraphGlow`.
2. Change the emissive to: `Emissive = Existing + HitFlash * (1,1,1) * 20 + TelegraphGlow * (1, 0.1, 0.05) * 40`.
3. Assign the new material to your Blueprints.

## Step 5: Weapons

**Blade:**
1. In `BP_Player`, set **BladeMesh** to any sword static mesh. It's attached to `hand_r`; adjust its relative transform in the viewport until it sits in the palm.
2. Open the sword mesh → **Socket Manager** → add `BladeBase` at the hilt and `BladeTip` at the point. The swept damage traces run between them.

**Guns:**
1. Set **GunMeshRight** / **GunMeshLeft** to pistol meshes.
2. Add a socket named `Muzzle` at the barrel end.

Guns only appear while aiming; the blade is visible the rest of the time.

## Step 6: Attack animations (montages)

**Free sources:**
- **Paragon characters** on Fab, e.g. Greystone, Kwang or Aurora. They include polished sword combos, hit reactions and dodges.
- Epic's **Game Animation Sample**.
- **Lyra**, for pistol poses.
- **Mixamo**; retarget it to Manny with an IK Retargeter.

For each attack animation:
1. **Create the montage:** right-click the animation → Create → **Anim Montage**. It uses `DefaultSlot`, which `ABP_Manny` already plays.
2. **Enable root motion** on the source animation if the attack should lunge forward.
3. **Add notifies:** right-click the Notifies track → Add Notify State / Add Notify:
   - **Melee Hit Window**: over the active frames, when the blade is actually cutting.
   - **Combo Window**: from just after the hit until the end of recovery. Earlier = faster combos.
   - **Combat Event** with tag `Event.Telegraph` (enemies only): about 0.4 s before the hit.
   - Enemy specials: `Event.FireProjectile`, `Event.AreaImpact`, `Event.ChargeStart` / `Event.ChargeEnd`.

**Player moveset:**
1. Create a Data Asset of class **MeleeMoveset**, e.g. `DA_PlayerMoveset`.
2. Fill in the attacks:
   - `LightCombo`: 3–4 attacks.
   - `HeavyFinishers`: index N is used after N light hits.
   - `AirCombo`: set `AirGravityScale` ≈ 0.1 so the air combo hangs.
   - `DashAttack`.
3. Assign it on `BP_Player` → **Melee** component → **Moveset**.
Any attack without a montage keeps using the built-in fallback swing, so you can migrate one attack at a time.

**Enemy attacks:** in `BP_Grunt` → **Enemy | Attacks**, set each attack's `Attack.Montage`. Timing then comes from the montage instead of `Fallback Windup`.

## Step 7: Animation Blueprint using the C++ base

1. **Create it:** make a new Anim Blueprint for the Manny skeleton. In **Class Settings**, set Parent Class = **ActionAnimInstance**.
   It exposes `GroundSpeed`, `Direction`, `bShouldMove`, `bIsFalling`, `bIsLevitating`, `bIsAiming`, `bIsDodging`, `bIsAttacking`, `bIsStunned`, `AimPitch`, `AimYaw` and `LeanAngle`.
2. **Recommended graph:**
   - A locomotion state machine: Idle / Jog on `GroundSpeed`, Falling, and a Levitate loop on `bIsLevitating`.
   - **Layered blend per bone** from `spine_01`, blending a dual-pistol aim pose when `bIsAiming`, plus an **Aim Offset** on `AimPitch` / `AimYaw`.
   - A **Transform (Modify) Bone** on `spine_01` rolling by `LeanAngle`. This procedural lean into turns adds a lot of weight.
   - Finish with the `DefaultSlot` node, so montages play on top.
3. **Assign it:** set this ABP as the Anim Class on your character Blueprints.

## Step 8: Hit reaction and ability animations

**Hit reactions:** on each character's **HitReaction** component, fill in:
- `LightHitMontages` / `HeavyHitMontages`: Front / Back / Left / Right.
- `StaggerMontage` and `KnockdownMontage`.

Missing entries are fine; the stun still happens and the physical flinch still plays.

**Abilities (optional):**
- **Dodge:** `DodgeMontages` (use in-place animations, since the code moves the character).
- **Levitation:** `SlamDiveMontage`, `SlamLandMontage`.
- **KineticLaunch:** `LaunchMontage`, `ImpactMontage`.
- **Telekinesis:** `GrabMontage`, `ThrowMontage`.
- **Boss phases:** `TransitionMontage` (roar).

## Step 9: Impact effects and sounds

1. **Create the asset:** a Data Asset of class **CombatFeedbackConfig**, e.g. `DA_CombatFeedback`.
2. **Fill in `ImpactEffects`** per damage tag:
   - `Damage.Blade` → slash sparks + flesh hit sound
   - `Damage.Bullet` → small spark + impact tick
   - `Damage.Kinetic` → debris burst + heavy thud
   - `Damage.Slam` → shockwave ring + boom
3. **Add the extra layers:** `CriticalImpact`, `PoiseBreakImpact` (a big "shield break" sound is very satisfying), and `KillImpact`.
4. **Assign it:** Project Settings → Game → Action Game → **Feedback Config**.

**Also add:**
- **Guns:** `MuzzleFlash`, `Tracer` (a beam Niagara system with a Vector user parameter `BeamEnd`), `FireSound`.
- **Enemies:** `TelegraphEffect` (sound + flash).
- **Props:** `ShatterEffect`.

## Step 10: Hand-built arenas (instead of the Demo Director)

1. **Place the encounter:** add an **Arena Encounter** actor and scale its trigger box over the arena.
2. **Add spawn points:** place **Target Point** actors and reference them in each wave's `Spawns`.
3. **Seal the arena (optional):** add wall or door meshes to `Blockers`. They're hidden until the fight starts, then removed when it's won.
4. **Boss fight:** use a separate encounter whose wave contains `BP_Boss`.
5. **Navigation:** always add a **Nav Mesh Bounds Volume**.
