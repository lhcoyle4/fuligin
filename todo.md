# FULIGIN TODO Roadmap

This document outlines the current developmental milestones for FULIGIN, alongside a roadmap of features inspired by the design principles of the classic roguelike *Rogue*, adapted to our 1979 vector-style space shooter.

---

## ─── CURRENT TASKS (FULIGIN BASE) ───

- [ ] **Phase 2 HUD & Cockpit UI Overhaul**
  - [ ] Implement angled viewport panel outlines (`ui_panel_angled`)
  - [ ] Integrate segmented bars for Chronicle/XP and shields (`ui_bar_segmented`)
  - [ ] Render discrete retro blocks for hull and core metrics (`ui_bar_block`)
  - [ ] Apply screen-edge vignettes and scanline overlays
- [ ] **Procedural World Generation & Seeding**
  - [ ] Integrate backing `WorldGenParams` struct in `include/game.h`
  - [ ] Support custom generation seeds for predictable sector maps
  - [ ] Procedurally place structures, debris shoals, and warp loci
- [ ] **Comprehensive Settings System**
  - [ ] Implement multi-tab settings UI tree (Video, Audio, Gameplay, Controls)
  - [ ] Support dynamic range, mute on focus loss, screen shake scaling, and deadzones
- [ ] **Zone Transition Banners**
  - [ ] Slide in/out `>>> ENTERING [ZONE] <<<` overlays at the top of the screen on warping
- [ ] **Proximity Danger Alert**
  - [ ] Pulse a flashing `>>> DANGER <<<` alert at bottom-center when enemies are close
- [ ] **Remnant Debris (Hidden Hazards)**
  - [ ] Code invisible spatial hazards and ghost asteroids requiring the `PALE SIGHT` upgrade
- [ ] **High-Zone specialized Enemies**
  - [ ] *Ascians:* Voiceless interceptors patrolling in rigid geometric trajectories
  - [ ] *Lictors:* Elite, aggressive interceptor saucers that pursue the Autodyne at high speeds
- [ ] **Cacogen UFO Auditory Signature**
  - [ ] Synthesize procedural oscillating hums in `src/audio.c` for active saucers

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
- [ ] **Rust-Weaver Drones:** Spits corrosive green spit that bypasses shields to degrade hull plating.
- [ ] **Scavenger Probes (Leprechauns):** Do not attack directly; instead, they tractor-beam your collected Void Steel and attempt to warp away.
- [ ] **EMP Sentinels (Floating Eyes):** Pulse a freezing electromagnetic field, locking ship steering and thrust for a brief duration.

### 6. Status Malfunctions (Blindness & Hallucination)
- [ ] **Sensor Static (Blindness):** Sensor arrays are damaged by solar radiation, removing the minimap, targeting reticles, and flashing HUD data.
- [ ] **Tox-Gas Hallucinations:** Space sickness or reactor leaks scramble visual outlines, turning wireframes into shifting geometric patterns (e.g., rendering simple asteroids as elite Cacogens).
- [ ] **Reverse Drift (Confusion):** Thruster vector direction is reversed or randomized.
