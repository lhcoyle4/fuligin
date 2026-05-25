# FULIGIN Action List & Consolidated Roadmap

This document merges the foundational features from Claude (previously in `todo.md`) with the new stylistic guidelines from Antigravity. The UI approach synthesizes the *Caves of Qud* terminal aesthetic with the *Final Fantasy VII Remake* (FF7R) geometry and layout, using the *Noctis* color palette.

---

## 1. HUD & Cockpit UI Overhaul
**Style Rule:** *Primarily Caves of Qud terminal aesthetic + Noctis colors/fonts, secondarily FF7R geometry.*

- [ ] **Angled Viewport Panels (FF7R Motif):** Update `ui_panel_angled` to use a top-left angled cut (12px diagonal), but draw borders using the Noctis cyan/teal palette (`#00b4d8`).
- [ ] **Terminal Typography (CoQ Motif):** Use strict monospace-aligned column grids for all HUD elements. All text must be ALL-CAPS. Use bracket prompts `[HP]`, `[FUEL]` and the chevron `>` for active targets.
- [ ] **Progress Bars (Synthesis):**
  - Use **Segmented Bars** (FF7R) for primary resources (ATB/Chronicle) with a 2px gap between segments.
  - Use **Discrete Blocks** (CoQ) for secondary gauges (`▐▐▐▐░░░░`), rendered in Noctis Acid Green or Cyan.
- [ ] **CRT Atmosphere:** Apply screen-edge vignettes and scanline overlays across the entire HUD.
- [ ] **Zone Transition Banners:** Slide in `>>> ENTERING [ZONE] <<<` overlays at the top of the screen on warping, using terminal monospaced alignment and bright Noctis colors.
- [ ] **Proximity Danger Alert:** Pulse a flashing `>>> DANGER <<<` alert at bottom-center when enemies are close (Cinnabar/Red).

---

## 2. Menu Systems & Settings UI
**Style Rule:** *Primarily FF7R menu geometry/layout, secondarily Caves of Qud Noctis colors/fonts.*

- [ ] **Comprehensive Settings Tree:** Implement multi-tab settings UI tree (Video, Audio, Gameplay, Controls).
  - Use FF7R's stacked diagonal-cut panels for the menu containers.
  - Use CoQ's monospaced font, Noctis terminal green/teal text colors, and `>` cursors for list selection.
- [ ] **Menu Layout:** High information density. Selected items get an inverted background (Noctis teal background, dark text), while unselected items dim to 60% alpha.
- [ ] **Dynamic Range & Toggles:** Add support for dynamic range, mute on focus loss, screen shake scaling, and deadzones within the new menu style.

---

## 3. Procedural World & Rogue-like Mechanics
- [ ] **Procedural World Generation & Seeding:**
  - Integrate `WorldGenParams` in `include/game.h`.
  - Support custom generation seeds for predictable sector maps.
  - Procedurally place structures, debris shoals, and warp loci.
- [ ] **The Identification & Salvage Loop:**
  - Unidentified Reliquaries drop with encrypted headers (`RELIC ENC-X902`).
  - Volatile Fuel Isotopes risk overheating vs. restoring double capacity.
  - Analysis Terminals at safe structures to identify relics safely.
- [ ] **Reactor Core & Fuel Clock:**
  - Sub-System Fuel Consumption: Equipped Reliquaries drain the reactor.
  - Emergency Drift Mode: At 0% fuel, systems shut down, drifting begins, and hull integrity depletes.
  - Solar Flare Cycles: Periodic radiation forces coolant/shield consumption.
- [ ] **Cargo Slots & Inventory Weight:**
  - Grid-Based Cargo Bay: Limited space for salvage, shield caps, and Reliquaries.
  - Weight & Inertia: Heavy cargo physically reduces ship rotation/thrust responsiveness.
- [ ] **Plating Durability & Calibration:**
  - Ablative Plating Wear: Hits permanently degrade maximum armor value.
  - Calibration Codes: Chips compiled to boost weapon damage (`+1 CAL`) or armor deflection.
  - Void-Rust Anomalies: Gas clouds that slowly erode hull integrity.

---

## 4. Lore, Enemies & Retro-Arcade Aesthetics
- [ ] **High-Zone Specialized Enemies:**
  - *Ascians:* Voiceless interceptors patrolling in rigid geometric trajectories.
  - *Lictors:* Elite, aggressive interceptor saucers pursuing the Autodyne at high speeds.
  - *Rust-Weaver Drones:* Spit corrosive green acid bypassing shields.
  - *Scavenger Probes:* Tractor-beam collected Void Steel and warp away.
  - *EMP Sentinels:* Pulse freezing electromagnetic fields, locking steering/thrust.
- [ ] **Status Malfunctions:**
  - *Sensor Static (Blindness):* Solar radiation disables minimap and reticles.
  - *Tox-Gas Hallucinations:* Space sickness scrambles visual outlines (e.g. rendering asteroids as Cacogens).
  - *Reverse Drift (Confusion):* Thruster vectors reversed.
- [ ] **Visual & Audio Features:**
  - *Phos-Green Engine Dust Trails:* Engine interacts with floating cosmic dust.
  - *Cugel-9 Board Computer:* Depressed TTS robot voice printing status reports on HUD.
  - *Star-Shadow Radiation Shielding:* Steer into asteroid shadows to avoid solar flares.
  - *Vector Stress-Cracking:* Asteroids show wireframe cracks as they take damage.
  - *Chronicle Chord Harmonics:* Gathering orbs plays sequential procedural FM-synth notes.
  - *Glitch Tearing Crit-Hits:* Crits cause screen tear and audio screech.
  - *Emergency Heat Venting:* Spray superheated steam forward as reverse thrust.
  - *Warp-Drive Singularity Exit:* Custom exit effects (black hole collapse, CRT power-down).
  - *Asteroid Bowling:* Launch asteroids into others for a `"STRIKE!"` combo.

---

## 5. Interactions & Lore Entities
- [ ] **The Rusty Flagon Tavern Beacon:**
  - Docking station to trade Contraband, buy coordinates, or play vector roulette.
- [ ] **Gravitational Slingshot Boosts:**
  - Orbit black holes or planetoids for zero-fuel speed boosts.
- [ ] **The Physical Bane-Whip:**
  - Kinetic vector tail tethered to ship; smashes rocks on swerve.
- [ ] **Pet Shield Drone Chatter:**
  - Tiny, low-contrast text dialogues floating over drones (`"SHIELD AT 24% (HELP)"`).
