# FULIGIN TODO Roadmap

This document outlines the current developmental milestones for FULIGIN, alongside a roadmap of features inspired by the design principles of the classic roguelike *Rogue*, adapted to our 1979 vector-style space shooter.

---

## ─── CURRENT TASKS (FULIGIN BASE) ───

- [ ] **Phase 2 HUD & Cockpit UI Overhaul**
  - [ ] Implement angled viewport panel outlines (`ui_panel_angled`)
  - [ ] Integrate segmented bars for Chronicle/XP and shields (`ui_bar_segmented`)
  - [ ] Render discrete retro blocks for hull and core metrics (`ui_bar_block`)
  - [x] Apply screen-edge vignettes and scanline overlays
- [ ] **Procedural World Generation & Seeding**
  - [ ] Integrate backing `WorldGenParams` struct in `include/game.h`
  - [ ] Support custom generation seeds for predictable sector maps
  - [ ] Procedurally place structures, debris shoals, and warp loci
- [ ] **Comprehensive Settings System**
  - [ ] Implement multi-tab settings UI tree (Video, Audio, Gameplay, Controls)
  - [ ] Support dynamic range, mute on focus loss, screen shake scaling, and deadzones
- [x] **Zone Transition Banners**
  - [x] Slide in/out `>>> ENTERING [ZONE] <<<` overlays at the top of the screen on warping
- [x] **Proximity Danger Alert**
  - [x] Pulse a flashing `>>> DANGER <<<` alert at bottom-center when enemies are close
- [ ] **Remnant Debris (Hidden Hazards)**
  - [ ] Code invisible spatial hazards and ghost asteroids requiring the `PALE SIGHT` upgrade
- [ ] **High-Zone specialized Enemies**
  - [x] *Ascians:* Voiceless interceptors patrolling in rigid geometric trajectories
  - [ ] *Lictors:* Elite, aggressive interceptor saucers that pursue the Autodyne at high speeds
- [x] **Cacogen UFO Auditory Signature**
  - [x] Synthesize procedural oscillating hums in `src/audio.c` for active saucers

---

## ─── ROGUE-INSPIRED SYSTEMS ROADMAP ───

### 1. The Identification & Salvage Loop (Scrolls & Potions)
In *Rogue*, items are found unidentified. In the cold void, salvaging must feel just as unpredictable:
- [ ] **Unidentified Reliquaries:** Dropped relics from dead civilizations spawn with encrypted headers (e.g. `RELIC ENC-X902`).
  - [ ] Must be hot-wired or loaded into the ship's computer to identify.
  - [ ] Running unidentified code might grant a massive shield output boost, or temporarily disable engine trust (system crash).
- [ ] **Volatile Fuel Isotopes:** Harvested fuel pods can be unrefined.
  - [ ] Injecting unrefined fuel might restore double capacity (stable isotope) or overheat the thrusters and damage the hull (thermal spike).
- [ ] **Analysis Terminals:** Safe structures (like Home Station) can identify and calibrate relics safely for a price.

### 2. Reactor Core & Fuel Clock (Hunger & Starvation)
Rogue uses food to force momentum. In FULIGIN, the ship itself is the clock:
- [ ] **Sub-System Fuel Consumption:** Every equipped Reliquary and weapon increases the background reactor drain.
- [ ] **Emergency Drift Mode:** When fuel hits 0%, systems shut down. The ship drifts out of control, shields fail, and life support begins depleting hull integrity directly.
- [ ] **Solar Flare Cycles:** Star zones that pulse radiation, forcing the player to consume extra shields/coolant to stay in the sector.

### 3. Cargo Slots & Inventory Weight
- [ ] **Grid-Based Cargo Bay:** Space in the Autodyne cargo bay is strictly limited.
  - [ ] Players must decide between carrying raw salvage (Void Steel, Autodyne Frags), consumable Shield Caps, or slotting new Reliquaries.
- [ ] **Weight & Inertia:** Heavy cargo physically affects ship physics, reducing rotation speeds and thruster responsiveness.

### 4. Plating Durability & Calibration (Rust & Enchantment)
- [ ] **Ablative Plating Wear:** Micro-meteoroids and projectile hits permanently degrade maximum armor value.
- [ ] **Calibration Codes:** Found memory chips (equivalent to Scrolls of Enchant Armor/Weapon) can be compiled to boost weapon damage (`+1 CAL`) or armor deflection.
- [ ] **Void-Rust Anomalies:** Radioactive gas clouds that slowly dissolve ship hull integrity and erode weapon accuracy.

### 5. Tactical Enemy AI Archetypes (Monsters)
Adapt Rogue's classic monster behaviors to space entities:
- [x] **Rust-Weaver Drones:** Spits corrosive green spit that bypasses shields to degrade hull plating. *(Integrated into `src/game.c` — module spawns in Zone 2+; corrosive spit bypasses Ether Shroud and Phase Shift; see action list item 23 for full wiring details.)*
- [ ] **Scavenger Probes (Leprechauns):** Do not attack directly; instead, they tractor-beam your collected Void Steel and attempt to warp away.
- [ ] **EMP Sentinels (Floating Eyes):** Pulse a freezing electromagnetic field, locking ship steering and thrust for a brief duration.

### 6. Status Malfunctions (Blindness & Hallucination)
- [ ] **Sensor Static (Blindness):** Sensor arrays are damaged by solar radiation, removing the minimap, targeting reticles, and flashing HUD data.
- [ ] **Tox-Gas Hallucinations:** Space sickness or reactor leaks scramble visual outlines, turning wireframes into shifting geometric patterns (e.g., rendering simple asteroids as elite Cacogens).
- [ ] **Reverse Drift (Confusion):** Thruster vector direction is reversed or randomized.

---

## ─── LORE & RETRO-ARCADE FEATURE DESIGN ───

### 1. Phos-Green Engine Dust Trails (Visual/Flashy)
- [x] **Engine Particulate Drift:** Make the player's engine exhaust interact with floating cosmic dust. Moving at high speeds sucks in ambient particles, creating a brilliant glowing vector dust-trail proportional to ship velocity.

### 2. "Cugel-9" Sarcastic Board Computer (Lore/Silly)
- [ ] **Melancholic Text & Speech Logs:** A virtual intelligence terminal named `CUGEL-9` that prints status reports on the HUD and uses text-to-speech to speak them out loud.
  - [ ] Speech must utilize a sad, depressed robot voice (inspired by Marvin from *Hitchhiker's Guide to the Galaxy*).
  - [ ] E.g., when taking collision damage: `"STRUCTURAL INTEGRITY COMPROMISED. COMPENSATING FOR PILOT INEPTITUDE."`
  - [ ] E.g., when missing several shots in a row: `"DISCHARGING BULLETS DIRECTLY INTO VACUUM. ASTEROIDS ARE UNIMPRESSED."`

### 3. Star-Shadow Radiation Shielding (Tactical/Mechanic)
- [ ] **Dying Sun Solar Flares:** The swollen star in the center of the zone periodically releases a Cinnabar-colored solar pulse.
  - [ ] The player must steer the Autodyne into the physical shadow cast by large asteroids (rendering a dynamic vector shadow line) to shield themselves from system damage.

### 4. Vector Stress-Cracking (Visual/Flashy)
- [x] **Structural Strain Outlines:** High-value metal and crystal asteroids display glowing wireframe stress cracks as they take damage, giving immediate visual feedback before they shatter.

### 5. Chronicle Chord Harmonics (Audio/Flashy)
- [x] **Musical Orb Gathering:** Vacuuming up multiple Chronicle Orbs in rapid succession plays sequential notes on a procedural FM-synth scale (e.g. pentatonic or minor chord arpeggios), rewarding combo chains with harmonious music.

### 6. Glitch Tearing Crit-Hits (Visual/Flashy)
- [x] **Matrix Disruption Frame-Hold:** Landing a critical strike or detonating a large rift causes the vector lines to briefly duplicate, shake, and tear across the horizontal scanlines, paired with a short white-noise audio screech.

### 7. Emergency Heat Venting (Mechanic/Tactical)
- [x] **Coolant Blowback:** Rapid firing overheats the Autodyne weapons. The player can trigger an emergency vent that sprays superheated steam (dense vector line particles) forward, acting as an instant reverse thrust but consuming fuel.

### 8. Warp-Drive Singularity Exit (Visual/Flashy)
- [x] **Warp Decal Configurations:** Unlock custom exit effects when jumping between loci. E.g., standard vector expansion, black-hole collapse (screen lines collapsing to a single pixel before exploding), or a static CRT screen-power-down flash. *(Default vector-expansion + CRT-bloom-flash variant implemented on warp arrival; black-hole-collapse variant and the unlock progression deferred.)*

### 9. Dynamic CRT Glass Curvature (Visual/Aesthetic)
- [ ] **Arcade Cabinet Curvature:** Add a toggleable pixel shader simulating the physical curved faceplate of a 1970s CRT vector arcade monitor, with subtle glass reflections on the corners.

### 10. The Rusty Flagon Tavern Beacon (Lore/Interactable)
- [ ] **Scavenger Hideout:** A rare neutral space station in the Scrap Fields.
  - [ ] Docking lets the player trade forbidden Contraband resources, purchase secret coordinates, or play a quick vector-rendered roulette mini-game to double their Void Steel.

### 11. Gravitational Slingshot Boosts (Physics/Tactical)
- [x] **Gravity Assist Trajectories:** Allows the player to enter a close orbit around massive black-hole rifts or heavy planetoids, gaining a high-velocity speed boost (catapult effect) for zero fuel cost.

### 12. The Physical Bane-Whip (Weapon/Flashy)
- [ ] **Kinetic Vector Whip:** An upgrade that tethers an elastic, segmented vector tail to the rear of the ship.
  - [ ] Swerving or rotating at high speed whips the tail around, physically smashing smaller asteroids or deflecting incoming Cacogen fire.

### 13. Pet Shield Drone Chatter (Silly/Visual)
- [x] **Drone Dialogues:** Orbital Shield Drones have tiny, low-contrast text dialogues floating over them.
  - [x] E.g., `"COORDINATING..."`, `"SHIELD AT 24% (HELP)"`, `"TARGET DETECTED: BIG ROCK"`.

### 14. Asteroid Bowling Combo chains (Silly/Fun)
- [x] **Kinetic Launching:** Using heavy projectiles or gravity displacements to launch smaller asteroids into larger ones. Splintering a chain of rocks by bowling them into each other grants a special `"STRIKE!"` score float and multiplier.
