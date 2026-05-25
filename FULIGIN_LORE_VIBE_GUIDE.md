# FULIGIN — Lore & Vibe Guide
*The authoritative design reference for all visual, naming, and UI decisions.*
*Version 1.0 — Last-Day Revision*

---

## 1. Creative Vision Statement

FULIGIN is an arcade space shooter set at the end of everything. The sun has gone red and swollen, shedding its last light across a void choked with the wreckage of dead civilizations. You are a salvager — a Cugel — threading your Autodyne through orbital graveyards, chasing holy relics and Chronicle orbs while the dying heartbeat of the star pounds beneath every session. The aesthetic is a deliberate collision: the deep, baroque melancholy of Gene Wolfe's dying Urth and the grotesque picaresque of Jack Vance's Dying Earth, rendered through the pure geometric light of a 1979 vector arcade cabinet. Neon lasers slice through cold ash. Wireframe silhouettes glow cyan against absolute black. Ancient terminology — Reliquary, Autarch, Lictor, Cacogen — appears in crisp, jagged ALL-CAPS vector type as if stenciled onto the hull of something very old. The game is fast, ruthless, and momentum-driven, but everything it touches — every name, every color, every UI panel — carries the weight of deep time. This is not a future. It is an aftermath.

---

## 2. Inspirations & References

### Gene Wolfe — *The Book of the New Sun*

The primary mythological substrate of FULIGIN. Wolfe's tetralogy is set on a far-future Urth whose sun is dying — not dramatically, but with geological slowness, the way a civilization forgets its own history. The key borrowings:

- **The dying sun as ever-present dread.** The sun is never dramatic in Wolfe; it is simply old, dim, and red. FULIGIN inherits this register: Solar Rust is a background condition, not a boss. It beats. It dims. It does not explain itself.
- **Deep time and ruined civilizations.** Wolfe's Urth is layered — each epoch buried under the next, with relics of the Commonwealth surfacing into the hands of people who no longer understand what they are. FULIGIN translates this directly: Reliquaries are ancient objects repurposed as upgrades. Their original function is unknown. They work anyway.
- **The guild vocabulary.** Wolfe names his factions with archaic precision: Torturers, Seekers, Fuliginous Cloaks. FULIGIN adopts this clinical, single-word naming logic. Lictors are executioners. Ascians cast no shadows. The Autarch is the absolute, the final score ceiling, the thing that cannot be surpassed.
- **The color fuligin itself.** In Wolfe, fuligin is the color worn by the guild of torturers — darker than black, absorbing all light. It names this game's background, its void, its total visual ground.
- **Urth** as the name of the dying world-grid rendered in vector lines: Urthgrid.

### Jack Vance — *The Dying Earth* series

Vance provides the tone beneath the mythology: dark humor, picaresque momentum, grotesque beauty, and the figure of the clever rogue who survives a dying world through wit and audacity rather than virtue.

- **Cugel the Clever** is FULIGIN's archetype for the player character. Not a hero. A survivor. A chancer. Cugel's Run is the name for a high-risk, high-reward salvage loop precisely because Cugel always takes the gamble and usually regrets it.
- **The Dying Earth aesthetic** is Vance's specific brand of entropy: colorful, strange, not quite tragic. The world is ending but the magicians are still bickering. FULIGIN adopts this — the void is lethal but also bizarre. Cacogens are not merely enemies; they are genuinely strange entities, off-worlders whose logic is alien.
- **Picaresque structure.** Each run is a picaresque episode — the Autodyne sets out, something goes wrong, resources are scraped together, and the whole thing ends badly or well depending on how cleverly the player navigated the chaos. Chronicle is the ledger. The Autarch is the score you cannot top.

### Caves of Qud

Qud provides the surreal ecological density and the psychedelic lore register. Key influences:

- **Vivid color against ruined landscape.** Qud's most powerful visual move is saturated color — acid greens, strange blues — against a landscape of ash and decay. FULIGIN uses the same principle: ACID GREEN resource readouts and URTH CYAN wireframes against FULIGIN BLACK.
- **Lore density and naming.** Qud names everything with specificity and strangeness: Chitinous, Ichorous, Rusted. FULIGIN adopts this vocabulary directly — Ichorous Rounds, Ossified Hull, Chondrite Glow.
- **Mutant ecology as enemy design logic.** Cacogens are FULIGIN's equivalent of Qud's mutant factions: each type has a specific and bizarre signature. They are not generic enemies. They are strange.

### Classic Arcade Vector Games — *Asteroids* (1979), *Tempest*, *Star Wars Arcade*

The visual grammar of FULIGIN is geometric light. No textures. No gradients in gameplay elements. Pure lines, pure glow, pure black. The influences:

- **Asteroids (1979):** The core loop — momentum-based ship, drifting rocks, orbiting threats — is FULIGIN's mechanical skeleton. The phosphor persistence of vector CRTs is emulated in the Vectrex-glow decay system.
- **Tempest:** The sense of geometric threat approaching from a void. Enemy patterns that feel like abstract geometry with lethal intent.
- **Star Wars Arcade:** The wire-grid as a world-map. FULIGIN's Urthgrid is directly descended from the Death Star trench rendered in vector lines.

The rule: **gameplay graphics are always line art.** No filled polygons in combat. Wireframe ships, wireframe asteroids, wireframe enemies. The glow is additive. The background is absolute black.

### Synthwave / Decaywave

The audio-visual envelope of FULIGIN sits between synthwave's neon romanticism and a darker subgenre — Decaywave — defined by VHS degradation, analog warmth, and the specific melancholy of things that are running down.

- Neon against black as a fundamental truth, not a style choice.
- Phosphor decay: the way vector lines fade slowly after being drawn, like a CRT cooling.
- Scanline shimmer as a UI texture.
- The emotional register is not nostalgia — it is elegy. Something beautiful, in the act of ending.

### Glassmorphism UI

Panel and HUD elements use a frosted-glass aesthetic: semi-transparent dark surfaces with luminous, single-pixel borders. The effect implies depth — as if the UI panels are physical objects floating above the gameplay plane — without compromising the minimalist, geometric clarity of the underlying style.

---

## 3. Color System

All colors must be used with semantic consistency. Never use a color outside its designated role without a documented reason.

| Name | Hex | Role |
|---|---|---|
| FULIGIN BLACK | `#000000` / `#050505` | The absolute void. Game background. All non-UI space. |
| URTH CYAN | `#00FFFF` | Player ship wireframe. Player bullets. Primary interactive UI. Active selections. |
| CINNABAR | `#E34234` | Dying sun glow. Danger states. Enemy highlights. Asteroid cores. Explosion cores. |
| SOLAR AMBER | `#FF8C00` | Warnings. Fuel indicators. Mid-danger states. Shop prompts. |
| ACID GREEN | `#39FF14` | Resource readouts. XP / Chronicle bar. Pickup orbs. Active buffs. |
| CACOGEN MAGENTA | `#FF00FF` | Enemy fire. Hostile entity wireframes. Chaos states. UFO signatures. |
| PHOTIC RUST | `#B22222` | Background zone-depth glow. Warm star radiation. Zone-danger tinting. |
| GHOST WHITE | `#F0F0F0` | Primary UI text. Labels. Score readouts. All non-interactive copy. |
| CHONDRITE GREY | `#4A4A5A` | Secondary UI surfaces. Glassmorphism panel fill. Inactive states. |
| VOID STEEL | `#1A1A2E` | Panel borders at low contrast. Sub-panel backgrounds. Deep UI depth layer. |

### Usage Rules

**FULIGIN BLACK** is the ground of everything. Never put any filled background behind gameplay elements — only pure black. The only exception is UI panels, which use glassmorphism fills (CHONDRITE GREY at low opacity).

**URTH CYAN** is the player's color. If something glows cyan, it belongs to the player or is navigable by the player. Interactive UI elements use cyan on hover. The Urthgrid lines are cyan. Player bullets are cyan.

**CINNABAR** is the color of danger and the dying sun. Enemy health indicators, collision warnings, and the Solar Rust background glow all use CINNABAR variants. Explosion cores pulse from CINNABAR to SOLAR AMBER as they expand.

**SOLAR AMBER** is the warning tier below CINNABAR. Fuel below 30% switches from ACID GREEN to SOLAR AMBER. At below 15%, it switches again to CINNABAR. Ammo warnings follow the same ramp.

**ACID GREEN** is the resource and growth color. Chronicle orbs are ACID GREEN. The XP bar is ACID GREEN. All pickup effects pulse ACID GREEN. This is the color of things the player gains.

**CACOGEN MAGENTA** is the enemy fire color. All projectiles not belonging to the player are CACOGEN MAGENTA. UFO ship wireframes use CACOGEN MAGENTA as their primary line color.

**PHOTIC RUST** is a background-only color — the ambient radiation of the dying star washing across deep-void zones. It appears as subtle screen-edge vignetting and zone-depth fog, never as a foreground element.

**GHOST WHITE** is for text only. Never use white for graphics elements — in a glowing additive environment, pure white washes out. Use URTH CYAN for interactive graphics, GHOST WHITE for readable labels.

---

## 4. Typography Rules

### Typeface Philosophy

All text in FULIGIN uses a vector stroke font — the same geometric, jagged, no-fill line-art system used for ship and asteroid graphics. Letters are drawn as connected line segments, not as bitmap or bezier glyphs. This means:

- Text has the same phosphor glow as everything else — it is rendered through the same additive blending pipeline.
- Text is never anti-aliased. Edges are hard. Glow is soft. These are different things.
- Text is NEVER soft, blurred, or rendered with drop-shadows. Glow only. Additive blend only.

### Case

All game text is **ALL CAPS**. This applies to:
- All HUD elements (SCORE, FUEL, AMMO, CHRONICLE, LEVEL)
- All upgrade names (ICHOROUS ROUNDS, VOID RICOCHET, OSSIFIED HULL)
- All zone and location names (HOME STATION, SCRAP FIELDS, VOID REACHES, IRON SHOALS, DEEP DRIFT)
- All menu items and prompts
- All resource labels

The only exceptions are lore-flavor text fragments (extended descriptions in menus and upgrade tooltips), which may use sentence case for readability.

### Hierarchy

| Level | Use | Style |
|---|---|---|
| TITLE | Game title, level announce | Large, wide letter-spacing, full URTH CYAN glow |
| SECTION HEADER | Zone names, menu headers | Medium, moderate spacing, GHOST WHITE |
| BODY | Upgrade descriptions, tooltip text | Small, tight, GHOST WHITE |
| DATA LABEL | HUD values, resource counts | Small, minimal spacing, color-coded by resource type |
| FLASH | Score float, event announce | Animated, scales up and fades, color varies by event type |

### Arcade Resolution

Target resolution: **1280 x 960** (4:3 ratio, the arcade standard). All typography sizing decisions are made for this resolution. Text must be legible at arm's length from a monitor — favor larger sizes and more spacing over density.

---

## 5. Lore & World-Building Reference

### The Setting: The Latter-Day

The game takes place during the Latter-Day — the final geological epoch of a cosmos that has run down. The sun, called **Solar Rust** or **the Shorn Star**, is a swollen red cinder: still warm, still radiating, but dimming on a timescale of millennia. Its light is the color of old iron. The void between objects is absolute — black and cold and littered with the detritus of dead civilizations.

There is no dramatic apocalypse. The Latter-Day has been ending for so long that ending is simply the condition of existence. Civilizations rise, accomplish something, decay, and leave their wreckage in orbit. The orbital graveyards are so dense that navigation requires constant evasion. This is, in game terms, the asteroid field — but every rock is also a fragment of something that once meant something.

### Zones

The game world is organized into radial zones centered on the **Home Station** — a rotating octagonal structure at coordinate origin (0, 0). As the player ventures outward, zone danger and resource density increase.

| Zone | In-Game Name | Distance | Character |
|---|---|---|---|
| 0 | HOME STATION | Origin | Safe harbor. Passive fuel regen. Friendly drones. Shop access. |
| 1 | SCRAP FIELDS | ~2000u | Light debris. Introductory threat density. Void Steel and Autodyne Frags. |
| 2 | IRON SHOALS | ~3500u | Dense rock fields. Shielded asteroids appear. Hex Modules available. |
| 3 | VOID REACHES | ~1800u far arc | UFO activity increases. Remnant threats revealed. Isotopes and contraband. |
| 4 | DEEP DRIFT | ~3000u far arc | Maximum threat. Cacogen incursions. Rarest Reliquary drops. |

**Warp Loci** are fixed navigation beacons at each zone — the player warps between them using the Autodyne's transit system. Each beacon has a name that signals its character: SCRAP FIELDS is scavenger territory, IRON SHOALS is dense and treacherous, VOID REACHES is haunted, DEEP DRIFT is where things get truly strange.

### Entities

**The Autodyne**
The player's ship. The name denotes a self-powered, self-directed drive system — a vessel that runs on its own harvested energy. The Autodyne is a wireframe silhouette in URTH CYAN: a triangle with vector thrust lines. It is never filled. It is never solid. It exists as pure geometric intention in the void.

**Cacogens**
Bizarre off-worlders of unknown origin. In FULIGIN, Cacogens are the UFO faction — the small and large saucers that enter from screen edges to harass the player. Small Cacogens are precise, targeted hunters. Large Cacogens are chaotic, spraying magenta fire in wide patterns. Both emit the characteristic UFO oscillation tone — their vessel signature, a sound unlike any natural phenomenon in the dying cosmos. The word comes from Wolfe's Urth: off-world entities of uncertain purpose and unsettling design.

**Ascians**
The voiceless antagonists. In Wolfe, Ascians are people from a totalitarian state who can only speak in approved phrases — they have no individual voice. In FULIGIN, Ascians are a class of enemy that appears without warning or announcement, moving in strict geometric patterns, never deviating. They are the game's most mechanical threat — no chaos, no randomness, pure ruthless geometry. Future enemy type reserved for high-zone encounters.

**Cugels**
Trickster rogues. The player archetype. Also the name for any NPC in the game world who operates on the margins — scavengers, traders, questionable allies. Cugel's Run is the name for a high-risk salvage sweep of a deep zone without warping back to Home Station — maximum resource density, maximum danger, no safety net.

**Lictors**
Executioners. The rank-and-file of the dying system's enforcement apparatus. In game terms, Lictors are elite Cacogen variants that appear at high difficulty — faster, more accurate, specifically targeting the player rather than patrolling. When a Lictor arrives, the beat accelerates.

**The Autarch**
The absolute ceiling. In Wolfe, the Autarch is the supreme ruler of Urth — not a tyrant, but a singular authority. In FULIGIN, the Autarch is the top score on the high score table — the reigning sovereign of the run history. To surpass the Autarch is the game's only genuine objective. The Autarch does not appear in the game world; the Autarch is the score itself, the ghost of the best run ever made, the thing that defines how well you played.

**Remnants**
The hidden threats. Remnants are entities that do not appear on standard readouts — ghost-hostile asteroids, cloaked hazards, environmental dangers invisible without the PALE SIGHT Reliquary (Reveal hidden Remnant threats). In lore terms, Remnants are the residue of civilizations so old they have become indistinguishable from the environment. They are not alive. They are not dead. They are simply still operating on their original instructions.

### Reliquaries

Reliquaries are the upgrade system. In lore terms, a Reliquary is an ancient holy object recovered from the debris fields — a relic of a prior epoch, function unknown, power still active. In game terms, Reliquaries are the passive upgrades the player selects between waves.

Each Reliquary has:
- A formal name in archaic terminology (the Reliquary's designation as found on its casing)
- A brief functional description (what it actually does to the Autodyne's systems)
- An implied origin that no one in the Latter-Day remembers

Reliquaries are **never** described as technology. They are described as objects. The player does not install them — the player carries them.

### Resource Glossary

The economy of the void runs on salvage. Resources are gathered from destroyed asteroids and zone exploration:

| Resource | Lore Description | Primary Use |
|---|---|---|
| Void Steel | Structural alloy of unknown composition, harder than any extant metallurgy | Shields, crafting, structural trades |
| Autodyne Frags | Drive-system fragments from wrecked vessels of familiar type | Ammo conversion |
| Hex Modules | Crystalline computation substrates from pre-collapse information networks | Rare crafts, advanced upgrades |
| Isotopes | Radioactive cores from dead reactor housings, still warm | Speed upgrades, phase systems |
| Shield Caps | Capacitors salvaged from ancient planetary defense grids | Shield repairs |
| Alien Flora | Biological material of off-world ecosystem origin, still viable | Magnet radius, biological trades |
| Biomatter | Organic substrate of indeterminate origin, traded by weight | Synthesis trades |
| Contraband | Objects whose nature is not discussed | High-value, high-risk economy |
| Medicinals | Chemical compounds from pre-collapse pharmacopeias | Health restoration |
| Coolant | Thermal regulation fluid from ancient engine housings | Engine trades |

### The Beat System

The Beat is the dying heartbeat of Solar Rust. As the session progresses, the beat interval shortens — the two-tone pulse accelerates toward its own limit. In Asteroids this was a simple tension mechanic. In FULIGIN it is the metaphysics: the sun is beating faster as it dies, and the player is synchronized to that rhythm whether they want to be or not.

Mechanically: the beat timer starts at approximately 1.0 second and compresses toward 0.25 seconds as the asteroid count falls and the wave nears its end. The beat is also the pace-setter for Lictor spawn timing and UFO acceleration states. Every session ends in the same way: the beat becomes a continuous tone. Then silence.

### Chronicle

Chronicle is XP — the accumulated record of the player's actions in the void. In lore terms, the Autodyne's chronicle system maintains an account of every rock destroyed, every Cacogen driven off, every Reliquary recovered. This chronicle feeds back into the vessel's capability systems, unlocking Reliquary selection events and increasing the player's operational ceiling.

Chronicle orbs are ACID GREEN — they drift from destroyed asteroids and are drawn to the Autodyne by the ORB MAGNET Reliquary. The Chronicle bar is the green XP bar at the top of the HUD. When it fills, a Reliquary selection screen appears. The CHRONICLE BOOST Reliquary increases Chronicle harvest rate by 50%; this is, in lore terms, an improvement to the Autodyne's record-keeping systems.

---

## 6. UI Design Directives

These rules govern the dcimgui-based UI overhaul and all future interface work.

### Panel Style: Glassmorphism

All UI panels — menus, HUD containers, upgrade screens, shop interfaces — use a glassmorphism treatment:

- **Background fill:** CHONDRITE GREY (`#4A4A5A`) at 30-40% opacity. Semi-transparent. The gameplay is visible through the panel.
- **Border:** Single-pixel luminous line in URTH CYAN. No gradient. No thickness variation. Just the line, glowing.
- **No drop shadows.** Depth is achieved through opacity and border glow, not by casting shadow onto the layer below.
- **No rounded corners on tactical/combat elements.** The HUD, ammo counter, fuel bar, and zone indicator all use hard 90-degree corners. Soft corners (4-6px radius) are permitted only on main menu panels and the upgrade selection screen — elements the player engages while not under fire.

### Borders and Lines

Single-pixel glowing lines are the grammar of all UI structure. Rules:

- All borders are exactly 1 pixel wide in base rendering.
- Glow is achieved through additive blending of 2-3 additional passes at decreasing opacity and increasing radius. This creates the phosphor bloom effect without thickening the line itself.
- Never use thick borders as a design element. Thickness reads as heaviness; FULIGIN UI is weightless.

### Buttons

- At rest: GHOST WHITE label, CHONDRITE GREY border.
- On hover/selection: border and label switch to URTH CYAN with full glow.
- On confirm: brief full-white flash, then return to selected state.
- No button fill at rest. No background color. The button exists as its border and its label only.

### Animations

- **Particle drift:** UI backgrounds carry a slow-drifting particle field in CHONDRITE GREY at very low opacity — the visual equivalent of dead dust in a slow current.
- **Scanline shimmer:** Periodic horizontal scan-line sweeps at 1-pixel height, top to bottom. Opacity: 5-10%. Interval: 3-5 seconds. This is the CRT reference, kept subtle.
- **Phosphor decay:** All vector lines — ships, bullets, asteroids, UI elements — leave a brief luminance trail that decays over approximately 0.1-0.2 seconds. This is the Vectrex-glow system already implemented in the renderer. It applies to UI line elements as well.
- **Score floats:** GHOST WHITE text that scales up from the point of event and fades over approximately 1.5 seconds. Color-coded: URTH CYAN for Chronicle gain, CINNABAR for damage taken, ACID GREEN for resource pickup.

### Color Coding in HUD

| HUD Element | Color |
|---|---|
| Score / Chronicle label | URTH CYAN |
| Chronicle bar fill | ACID GREEN |
| Fuel full | ACID GREEN |
| Fuel warning (30%) | SOLAR AMBER |
| Fuel danger (15%) | CINNABAR |
| Ammo count | URTH CYAN |
| Shield active pulse | URTH CYAN |
| Resource count labels | GHOST WHITE labels, color-coded values |
| Zone indicator | GHOST WHITE text, PHOTIC RUST zone-depth tint |
| Beat pulse visual sync | CINNABAR flash at beat peak |

### HUD Density Principle

Every HUD element earns its position by being consulted at least once per minute of active play. Information the player never checks during gameplay belongs in the Shop or pause menu — not in the active HUD. When in doubt, remove it from the HUD. The void should be visible. The game should be felt, not read.

Active HUD contains: Score, Fuel, Ammo, Chronicle bar, active Reliquary count, resource summary, zone indicator. Everything else is on-demand.

---

## 7. Audio Identity

The audio system is fully procedural — no external WAV files. All sound effects are synthesized in memory using sine waves, frequency modulation, and white noise. This is a design choice, not a limitation. FULIGIN sounds the way a vector game sounds: clean, synthesized tones, not samples.

### Laser / Bullet Sounds: Neon Embers

The player's laser is the neon ember — a short, clean frequency decay that cuts through void silence like a beam of light through ash. High-frequency attack, rapid decay. Not explosive. Not meaty. Precise. The sound of something cutting, not destroying.

### The Beat: Dying Heartbeat of Solar Rust

The two-tone heartbeat (SFX_BEAT1 / SFX_BEAT2) is the session's pulse. At maximum interval it is slow and ominous — the sound of something vast and old, still alive but barely. As it accelerates toward wave completion it becomes urgent, then frantic. The beat never stops during active play. It is the only audio element that is always present. It is the sun. It is running down.

### UFO / Cacogen Vessel Signatures

The UFO oscillation tone is the Cacogen signature: an alien modulation pattern that announces the presence of off-worlders. Large Cacogen vessels have a lower, slower oscillation. Small Cacogen hunters have a higher, more erratic pattern. The distinction in tone tells experienced players which threat has arrived before they see it. The sound is not threatening — it is simply wrong. It does not belong in this system.

### Ambient: Urth's Last Radiation

The ambient sonic environment of FULIGIN is near-silence punctuated by the beat and the sounds of destruction. The void does not hum. When ambient texture is needed — in menus, between waves — it is the sound of very low-level electromagnetic radiation: the noise floor of a universe running down. Not music. Not atmosphere. The actual sound of entropy.

### Explosion Hierarchy

- **Small** (single asteroid fragment): short, mid-frequency crackle.
- **Large** (full asteroid, Cacogen vessel): low-frequency boom with high-frequency debris scatter.
- **Player death:** silence, then a long, slow decay tone. The absence of the beat is more disturbing than any sound that could replace it.

---

## 8. Naming Conventions for Future Content

### The Test

Every new name in FULIGIN — enemy type, Reliquary, zone, UI element, resource — must answer one question: **does it sound like it belongs on a ruined object recovered from a dead civilization?**

If yes, use it. If it sounds like a feature in a space strategy game — discard it.

### Source Materials to Draw From

- **Gene Wolfe vocabulary:** Torturer guild terminology (Carnifex, Journeyman, Seeker), Urth geography (Nessus, the Citadel, the Autarch's court), Old Urthish compounds. Favor words that sound like they were technical once and have since become ceremonial.
- **Vance flavor:** Grotesque compound nouns, absurdist specificity, the names of spells that describe effects with baroque precision. Not "fireball" — "The Excellent Prismatic Spray."
- **Caves of Qud register:** Material-derived naming (Chondrite, Isotopic, Ichorous), biological specificity, the sense that the world is named by people who understand materials better than abstractions.
- **Archaic English and Latin roots:** Single-syllable punchy Anglo-Saxon words, or long Latinate compounds. The middle register — two-syllable casual words — is where generic sci-fi lives. Avoid it.

### Approved Vocabulary Bank

`Fuliginous` `Autodynic` `Ichorous` `Photic` `Chondrite` `Voidward` `Reliquary` `Terminus` `Remnant` `Ascian` `Cacogenic` `Ossified` `Triskelion` `Ether` `Nocturne` `Cinnabar` `Phlogiston` `Lacunae` `Umbral` `Gravitic` `Shorn` `Corroded` `Temporal` `Resonant` `Singularity` `Carnifex` `Liminant` `Penultimate` `Vect` `Gyre` `Ichthic` `Dissolution` `Autarch` `Null-Photic` `Crepuscular` `Void-Wrought`

### Vocabulary to Avoid

`Plasma` `Energy` (prefer: Photic, Ether, Isotopic) `Shield` (prefer: Shroud, Carapace, Ossified Hull) `Upgrade` (prefer: Reliquary) `Alien` as standalone adjective (prefer: Cacogenic, Off-world, Chthonic) `Power` as a noun (prefer: Drive, Resonance, Fury) `Laser` in lore text (acceptable in HUD shorthand only)

### Naming Templates

**Reliquaries:** `[Material or Origin] + [Function or Effect]`
Examples: *Ichorous Rounds, Ossified Hull, Triskelion Burst, Ether Shroud, Chondrite Glow, Pale Sight, Solar Fury*

**Zones:** Single noun or noun phrase evoking geography and danger — never generic.
Examples: *Scrap Fields, Iron Shoals, Void Reaches, Deep Drift, Terminus Run, Gravegrid, Nocturne Vect*

**Enemy variants:** Single archaic noun used as a designation, no modifier required.
Examples: *Lictor, Ascian, Cacogen, Remnant, Carnifex*

**Resources:** Material compound or salvage term grounded in physical specificity.
Examples: *Void Steel, Hex Modules, Autodyne Frags, Isotopes, Biomatter, Alien Flora, Shield Caps*

**Upgrade HUD labels:** Two words maximum, ALL CAPS, compound-specific over generic.
Examples: *PALE SIGHT, SOLAR FURY, CORRUPTION ROUNDS, TEMPORAL FOLD, FISSION SHOT, VOID RICOCHET, ETHER SHROUD*

### Final Check

Read the name aloud. If it could appear in a Gene Wolfe novel, a Vance short story, or a Caves of Qud item description — it is correct. If it sounds like it belongs in a 2018 space strategy game — it does not belong here.

---

*End of FULIGIN Lore & Vibe Guide v1.0*
*"The color darker than black."*
