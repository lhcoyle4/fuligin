# FULIGIN TODO Roadmap

This document outlines the current developmental milestones for FULIGIN, alongside a roadmap of features inspired by the design principles of the classic roguelike *Rogue*, adapted to our 1979 vector-style space shooter.

---

## ─── CURRENT TASKS (FULIGIN BASE) ───

- [ ] **Phase 2 HUD & Cockpit UI Overhaul**
  - [x] ~~Implement angled viewport panel outlines (`ui_panel_angled`)~~ *(retired — see action list item 1: FF7R angled-cut aesthetic retired in favour of unified Qud terminal look; `ui_panel_terminal()` is now the single panel primitive used everywhere)*
  - [x] Integrate segmented bars for Chronicle/XP and shields (`ui_bar_segmented`) *(shipped — see action list item 3)*
  - [x] Render discrete retro blocks for hull and core metrics (`ui_bar_block`) *(shipped — see action list item 3)*
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
- [x] **High-Zone specialized Enemies**
  - [x] *Ascians:* Voiceless interceptors patrolling in rigid geometric trajectories
  - [x] *Lictors:* Elite, aggressive interceptor saucers that pursue the Autodyne at high speeds
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
- [x] **Sub-System Fuel Consumption:** Every equipped Reliquary and weapon increases the background reactor drain. *(Partial — per-relic drain shipped: 0.08 fuel/s base + 0.04/s per active relic. Per-weapon drain not yet wired. See action list item 16.)*
- [x] **Emergency Drift Mode:** When fuel hits 0%, systems shut down. The ship drifts out of control, shields fail, and life support begins depleting hull integrity directly. *(Shipped — at 0% fuel thrust blocks, `drift_penalty_timer` accumulates, 10 s adrift triggers hull breach with `HULL BREACH` EventFloat + screen shake + fuel refill to 20% emergency reserve. See action list item 16.)*
- [x] **Solar Flare Cycles:** Star zones that pulse radiation, forcing the player to consume extra shields/coolant to stay in the sector. *(Shipped — three-phase COUNTUP → WARNING (3 s telegraph) → ACTIVE (5 s, 3× passive reactor drain) state machine, gated to zone ≥ 1, Home Space exempt. See action list item 33.)*

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
- [x] **Scavenger Probes (Leprechauns):** Do not attack directly; instead, they tractor-beam your collected Void Steel and attempt to warp away. *(Integrated 2026-05-26 — module `src/enemy_scavenger.[ch]` with SEEK → TRACTOR → ESCAPE → WARP state machine; Zone 1+ AND `res_void_steel > 0` gated; refunds stolen cargo on kill at probe's last position with `+VOID STEEL` cyan float. See action list item 24 for full wiring.)*
- [x] **EMP Sentinels (Floating Eyes):** Pulse a freezing electromagnetic field, locking ship steering and thrust for a brief duration. *(Integrated 2026-05-26 — module `src/enemy_emp_sentinel.[ch]` spawns in Zone 2+; IDLE→CHARGE (1.4 s telegraph)→PULSE (0.45 s, 380 u ring)→COOLDOWN (4 s) state machine; pulse sets `player.emp_lock_timer = 1.6 s` which is honoured by `update_player_physics` to negate rotation/thrust input; bypasses Ether Shroud / Phase Shift. See action list item 25.)*

### 6. Status Malfunctions (Blindness & Hallucination)
- [x] **Sensor Static (Blindness):** Sensor arrays are damaged by solar radiation, removing the minimap, targeting reticles, and flashing HUD data. *(Integrated 2026-05-26 — new `player.sensor_static_timer` field on `ShipEntity` (both `include/entities.h` and the inline duplicate in `src/game.c`), initialised in `reset_player()` and `game_init()`, ticked down in `update_player_physics()` alongside the EMP timer. Trigger lives inside the Solar Flare ACTIVE phase in `update_progression()`: a file-static `sensor_static_roll_t` (3.5–6.5 s jittered interval) rolls a 45 % chance to scramble sensors for 2.4 s whenever the player is **not** in asteroid shadow — taking cover prevents the malfunction entirely, adding a second reward layer on top of the reduced fuel drain. While `sensor_static_timer > 0`, `render_minimap()` replaces the panel interior with sparse cyan/cinnabar pixel noise + a pulsed `[NO SIGNAL]` label and skips all blip rendering; the proximity `>>> DANGER <<<` readout in the bottom-overlay branch is also suppressed (same sensor stack). `cugel9_say("SENSORS BLINDED BY STELLAR WIND. NAVIGATE BY INTUITION OR DESPAIR.")` and a `SENSOR STATIC` cinnabar event float fire on scramble entry. Clean build, 340,100-byte exe (+1,465 bytes), no new warnings.)*
- [x] **Tox-Gas Hallucinations:** Space sickness or reactor leaks scramble visual outlines, turning wireframes into shifting geometric patterns (e.g., rendering simple asteroids as elite Cacogens). *(Integrated 2026-05-26 — new `player.tox_hallucination_timer` field on `ShipEntity` (mirrored in `include/entities.h` and the inline duplicate in `src/game.c`), initialised in `reset_player()` and `game_init()`, ticked down in `update_player_physics()` after the Reverse Drift tick. Effect lives in `render_entities()` asteroid block: per-asteroid render-time jitter (~4.5 u position, ±0.22 rad angle wobble, ±6 % scale breathe) keyed off `game_time` and asteroid index so the field shimmers asynchronously; on `i % 3 == 0` asteroids a low-alpha cyan triangular ghost-Cacogen silhouette is overlaid at ~85 % radius so simple rocks look like elite UFOs at a glance, alpha pulse-modulated by the timer's fade. Trigger lives inside the Solar Flare ACTIVE phase in `update_progression()` after the Reverse Drift roll: file-static `tox_hallucination_roll_t` rolls a 25 % chance every 5.0–9.0 s of jittered interval to set the timer to 3.0 s **only when `solar_flare_in_shadow == 0`** — Star-Shadow cover (Item 34) now blocks all three malfunctions, completing the trio. On hallucination entry: `TOX HALLUCINATION` cinnabar event float, `audio_play(SFX_EXPLOSION_SM)`, and `cugel9_say("REACTOR FUMES DETECTED. THAT ASTEROID IS PROBABLY NOT A CACOGEN. PROBABLY.")`. Roll timer resets on ACTIVE→IDLE transition alongside the others. Constants: `TOX_HALLUCINATION_DURATION = 3.0f`, `TOX_HALLUCINATION_ROLL_MIN/MAX = 5.0/9.0 s`, `TOX_HALLUCINATION_HIT_CHANCE = 0.25f` — lower than Sensor Static's 45 % and Reverse Drift's 30 % because the visual scramble compounds with the other two; tuned to land roughly once per flare on average. Clean build, 342,230-byte exe (+1,067 bytes), no new warnings beyond the five pre-existing ones.)*
- [x] **Reverse Drift (Confusion):** Thruster vector direction is reversed or randomized. *(Integrated 2026-05-26 — new `player.reverse_drift_timer` field on `ShipEntity` (both `include/entities.h` and the inline duplicate in `src/game.c`), initialised in `reset_player()` and `game_init()`, ticked down in `update_player_physics()` alongside the EMP-lock and sensor-static timers. Trigger lives inside the Solar Flare ACTIVE phase in `update_progression()`: a file-static `reverse_drift_roll_t` (rolled to a jittered 4.5–8.5 s interval) ticks down each frame and, when it expires, rolls a 30 % `REVERSE_DRIFT_HIT_CHANCE` to set `player.reverse_drift_timer = REVERSE_DRIFT_DURATION = 3.2 f`. The roll only runs when `solar_flare_in_shadow == 0` — hiding under an asteroid blocks the inversion outright, giving Star-Shadow shielding (#34) a third reward layer (after reduced fuel drain and sensor-static immunity). While shielded the roll timer is held at ≥ 1 s so leaving cover doesn't trigger an instant inversion. Effect lives in the thrust force block: a `drift_sign` of -1.0 is substituted for the usual +1.0 when the timer is positive, so engaging thrust pushes the ship along the **opposite** of its heading. Rotation is NOT affected — the pilot can still steer, but the engines push the wrong way. On inversion entry: `REVERSE DRIFT` cinnabar event float, `audio_play(SFX_EXPLOSION_SM)`, and `cugel9_say("THRUST POLARITY INVERTED. ENJOY YOUR INVOLUNTARY RETROGRADE.")`. Constants: `REVERSE_DRIFT_DURATION = 3.2f`, `REVERSE_DRIFT_ROLL_MIN/MAX = 4.5/8.5 s`, `REVERSE_DRIFT_HIT_CHANCE = 0.30f` — chosen lower than Sensor Static's 0.45 because losing thrust direction is a more disruptive penalty. Clean build, 341,163-byte exe (+1,063 bytes), no new warnings beyond the five pre-existing ones.)*

---

## ─── LORE & RETRO-ARCADE FEATURE DESIGN ───

### 1. Phos-Green Engine Dust Trails (Visual/Flashy)
- [x] **Engine Particulate Drift:** Make the player's engine exhaust interact with floating cosmic dust. Moving at high speeds sucks in ambient particles, creating a brilliant glowing vector dust-trail proportional to ship velocity.

### 2. "Cugel-9" Sarcastic Board Computer (Lore/Silly)
- [x] **Melancholic Text & Speech Logs:** A virtual intelligence terminal named `CUGEL-9` that prints status reports on the HUD and uses text-to-speech to speak them out loud. *(Partial — HUD text log shipped via `cugel9_say()` and `ui_panel_terminal()` rendering throughout `game.c`; see action list item 26. TTS speech with sad-robot voice still pending.)*
  - [ ] Speech must utilize a sad, depressed robot voice (inspired by Marvin from *Hitchhiker's Guide to the Galaxy*). *(Not yet wired — audio.c lacks a TTS path; would need an offline TTS engine or per-line pre-rendered WAVs.)*
  - [x] E.g., when taking collision damage: `"STRUCTURAL INTEGRITY COMPROMISED. COMPENSATING FOR PILOT INEPTITUDE."` *(Text shipped — quip lines fire on collision, EMP lock, Lictor pursuit, etc.)*
  - [x] E.g., when missing several shots in a row: `"DISCHARGING BULLETS DIRECTLY INTO VACUUM. ASTEROIDS ARE UNIMPRESSED."` *(Text quip patterns shipped on various trigger events; sustained miss-streak tracking may still be partial.)*

### 3. Star-Shadow Radiation Shielding (Tactical/Mechanic)
- [x] **Dying Sun Solar Flares:** The swollen star in the center of the zone periodically releases a Cinnabar-colored solar pulse.
  - [x] The player must steer the Autodyne into the physical shadow cast by large asteroids (rendering a dynamic vector shadow line) to shield themselves from system damage. *(Implemented 2026-05-26 in `src/game.c`. New constants `SOLAR_FLARE_SHADOW_MULT = 1.4f` and `SOLAR_FLARE_SHADOW_HALF_W = 80.0f` (world units). During an ACTIVE flare, `update_progression()` scans every large/medium asteroid (size ≥ 2) each frame: builds the player→origin unit vector, projects each asteroid onto that ray, and checks the perpendicular distance against a corridor half-width that scales with the asteroid's radius. If any asteroid sits inside the corridor between the player and the dying sun, `solar_flare_in_shadow = 1`. `update_player_physics()` reads that flag and substitutes the shadow drain multiplier (1.4×) for the bare flare multiplier (3.0×), so taking cover dramatically reduces fuel loss. Visual feedback in `render_entities()`: for every shielding asteroid, a low-alpha cinnabar tether line is drawn from the asteroid's player-facing edge to the player, brightness scaled by how centred the player sits in the corridor, pulsed at 8 rad/s so the line shimmers during the flare. Drawn before asteroids so the lines sit behind the rock silhouette. Verified clean build, 327KB exe.)*

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
