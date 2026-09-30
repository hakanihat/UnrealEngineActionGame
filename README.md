# Action Game Demo (Unreal Engine 5, C++)

A third-person action combat demo inspired by **Control** and **Control Resonant**:

- **Blade combat**: combos with input buffering, branching heavy finishers, air combos, dash attacks, attack magnetism.
- **Dual pistols**: alternating hitscan fire with spread bloom, recoil and headshots. Ammo regenerates.
- **Telekinesis**: rip objects (or chunks of the floor) loose, hold them, and throw them at enemies. You can also catch enemy projectiles and throw them back.
- **Throw yourself**: *Kinetic Launch* hurls you into an enemy, then bounces you into the air.
- **Levitation and ground slam**: hover over the arena, then dive into a shockwave that launches enemies.
- **Enemies that react**: directional hit reactions, procedural physics flinches on the struck bone, poise and stagger, knockback, launches, air juggles and ragdoll deaths.
- **Demo flow**: three waves of melee grunts and ranged gunners, then a **three-phase boss**.

Everything is written in C++ and data-driven. **The demo is playable before you import any art.** Characters appear as colored capsules, attacks use timed fallbacks, and the HUD is drawn in code.

---

## 1. Quick start (no art needed)

**Requirements:**
- Unreal Engine **5.8** (the project is set to 5.8; for another version, right-click `ActionGame.uproject` → *Switch Unreal Engine version*)
- Visual Studio 2022 with the **"Game development with C++"** workload

**Steps:**
1. Right-click `ActionGame.uproject` and choose **Generate Visual Studio project files**.
   If that menu entry is missing, run `UnrealVersionSelector.exe` once from `C:\Program Files (x86)\Epic Games\Launcher\Engine\Binaries\Win64\`.
2. Double-click `ActionGame.uproject` and click **Yes** to build.
3. In the editor, choose **File > New Level > Basic**. That level has a floor, lights and a Player Start.
4. From the **Place Actors** panel, search for **Demo Arena Director** and drop it on the floor a few meters in front of the Player Start.
5. Optional but recommended: add a **Nav Mesh Bounds Volume** covering the floor, so enemies path around props. Without it they walk straight at you.
6. **Save the level** (Ctrl+S). Restart-on-death reloads the saved map.
7. Press **Play**. Walk into the arena and fight.

**Colors:** you are the blue capsule, grunts are red, and gunners are orange. The large red capsule is the boss.

### Real animated characters (one click)
In the editor's **Content Browser**, click **Add → Add Feature or Content Pack → Blueprint → Third Person → Add to Project**.
The game detects the UE5 Mannequins automatically: you play as **Manny**, and enemies are **Quinn** (the boss is a scaled-up Quinn). They come with run, jump and idle animations, physics flinches on hit, and ragdoll deaths. You don't need to create any Blueprints.

> **If the build fails:** the error popup doesn't say why. The real compiler errors are in
> `%LOCALAPPDATA%\UnrealBuildTool\Log.txt`, or in Visual Studio's Output window if you build from `ActionGame.sln`.

### Moving the code into a fresh Unreal project (alternative setup)
1. **Create an empty project:** in the Unreal Project Browser choose **Games → Blank**, select **C++**, and name it exactly **`ActionGame`**. Put it outside this repository folder, e.g. `D:\UnrealProjects`.
   This step compiles an empty project, so it also proves your Unreal + Visual Studio setup works.
2. **Close the new project:** exit both the Unreal Editor and Visual Studio.
3. **Copy the code over:** double-click **`CopyToNewProject.bat`** in this repository folder, drag the new project's folder into the window, and press Enter.
   It copies the code, docs and settings, and deletes the new project's old build files so Unreal recompiles.
4. **Open and rebuild:** open the new project from the **Epic Games Launcher → Library → My Projects** and click **Yes** to rebuild.
5. **Updating later:** after each `git pull` in this folder, run `CopyToNewProject.bat` again.

## 2. Controls

| Action | Keyboard / Mouse | Gamepad |
|---|---|---|
| Move / Look | WASD / Mouse | Left / Right stick |
| Light attack (combo, air combo, dash attack after dodge) | LMB | X / Square |
| Heavy attack / finisher (in air: **Ground Slam**) | F | Y / Triangle |
| Aim dual pistols (LMB fires while aiming) | RMB | LT (RT fires) |
| Telekinesis: hold to grab, release to throw | E | RB |
| Kinetic Launch (throw yourself at the target) | Q | LB |
| Dodge / air dash (perfect dodge = slow-mo) | Left Shift | B / Circle |
| Jump, hold in air to **levitate** | Space | A / Cross |
| Lock on (flick the mouse to switch target) | MMB or Tab | R3 |

## 3. Project structure

```
Source/ActionGame/
├── ActionGameTags.*            Native gameplay tags (states, damage types, anim events)
├── ActionGameSettings.h        Project Settings > Game > Action Game
├── Combat/
│   ├── CombatTypes.h           FCombatDamageSpec / FCombatHit / FCombatDamageResult
│   ├── CombatLibrary.*         The single damage pipeline (team rules + feedback)
│   ├── CombatFeedbackSubsystem Hitstop, slow motion, camera trauma, impact VFX/SFX
│   ├── CombatFeedbackConfig    Data asset with all "juice" tuning
│   ├── HealthComponent         Health, poise (stagger meter), i-frames, death
│   ├── HitReactionComponent    Flash, physical flinch, montages, knockback, stagger, ragdoll
│   ├── MeleeComponent / MeleeTypes   Combos, buffering, magnetism, swept traces
│   ├── GunComponent            Dual pistols
│   ├── TargetingComponent      Lock-on, melee and aim assist
│   └── ActionProjectile        Dodgeable, catchable enemy projectile
├── Abilities/                  Energy, Dodge, Levitation (+slam), KineticLaunch, Telekinesis, props
├── Characters/                 ActionCharacterBase, Player, Enemy (grunt), RangedEnemy (gunner), Boss
├── AI/                         EnemyAIController (state machine), AttackTokenSubsystem
├── Animation/                  ActionAnimInstance, Hit Window / Combo Window / Combat Event notifies
├── Camera/                     ActionPlayerCameraManager (trauma shake, FOV kicks)
├── Game/                       GameMode, PlayerController, ArenaEncounter, DemoArenaDirector, HealthOrb
├── Input/                      ActionInputConfig (runtime default bindings)
└── UI/                         ActionHUD (Canvas HUD)
```

### Architecture in one paragraph
Every ability is its own **component** with a single responsibility. The player character only decides *which* ability an input means in the current context. Characters share one **reference-counted state tag set** (`State.Attacking`, `State.Invulnerable`, ...), so systems coordinate without knowing about each other. **All damage flows through `UCombatLibrary::ApplyDamage`**, which enforces team rules and triggers feedback, so every hit feels consistent. Tuning lives in **data**: movesets, enemy attacks, boss phases and the feedback config are all editable without touching code.

## 4. Next steps: making it look good
See **[Docs/ASSET_SETUP.md](Docs/ASSET_SETUP.md)** for step-by-step instructions to:
- use the UE5 Mannequin with a Blueprint player and enemies;
- author attack montages with the Hit Window / Combo Window / Combat Event notifies;
- add the `HitFlash` / `TelegraphGlow` material parameters;
- create the Combat Feedback data asset with impact VFX and sounds.

See **[Docs/COMBAT_DESIGN.md](Docs/COMBAT_DESIGN.md)** for the design reasoning behind each system and the numbers to tune.
