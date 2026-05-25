# FULIGIN — Settings Menu and World Generation Specification

*Design specification for the settings menu system and procedural world generation.*
*Implementation target: `settings.c` / `settings.h`, `worldgen.c` / `worldgen.h`, integration in `game.c`.*

---

## Part 1 — Settings Menu System

### Overview

The settings menu is a full-screen overlay activated from the title screen and the pause menu. It uses the **double-border terminal** panel style documented in STYLE_GUIDE.md §"Settings Menu UI Style". All input to the settings menu must be routed through the existing input system — settings navigation must not poll SDL_GetKeyboardState directly; it must consume events from the same `game_handle_input` pipeline, gated on `STATE_SETTINGS`.

Settings are persisted to disk in a plain-text key=value file (`fuligin_settings.cfg`) loaded at startup and written on clean exit and any time the player confirms a change. Invalid or missing keys fall back to their defaults silently.

### Data Model

Settings are stored in a single global `GameSettings` struct declared in `settings.h` and defined in `settings.c`:

```c
typedef struct {
    /* VIDEO */
    int  display_mode;       /* 0=Windowed, 1=Fullscreen, 2=Borderless */
    int  resolution_index;   /* index into RESOLUTIONS[] table          */
    int  vsync;              /* 0=off, 1=on                             */
    int  refresh_rate;       /* 0=monitor default, otherwise Hz value   */
    int  aspect_ratio;       /* 0=16:9, 1=4:3, 2=21:9                  */

    /* GRAPHICS */
    int  glow_intensity;     /* 0–4 preset levels (settings_glow)       */
    int  particle_count;     /* 0, 16, 32, 48, or 64                    */
    int  scanlines;          /* 0=off, 1=on                             */
    int  vignette;           /* 0=off, 1=on                             */
    int  motion_blur;        /* 0=off, 1=on                             */
    int  screen_shake;       /* 0=off, 1=on  (settings_screen_shake)   */

    /* AUDIO */
    int  vol_master;         /* 0–100                                   */
    int  vol_music;          /* 0–100                                   */
    int  vol_sfx;            /* 0–100                                   */
    int  vol_ui;             /* 0–100                                   */
    int  streamer_mode;      /* 1=mute music, keep SFX                  */

    /* CONTROLS */
    int  mouse_aim;          /* 0=off, 1=on  (settings_mouse_aim)       */
    int  mouse_sensitivity;  /* 1–10, maps to a float multiplier        */
    int  controller_deadzone;/* 0–30 (percent)                          */
    /* keybinds stored separately in KeyBindings struct (see CONTROLS)  */

    /* HUD */
    int  show_fps;           /* 0=off, 1=on                             */
    int  show_minimap;       /* 0=off, 1=on                             */
    int  crosshair_style;    /* 0=None, 1=Dot, 2=Cross, 3=Chevron       */
    int  hud_scale;          /* 0=75%, 1=100%, 2=125%, 3=150%           */
    int  show_combo;         /* 0=off, 1=on                             */
    int  show_zone_name;     /* 0=off, 1=on                             */

    /* ACCESSIBILITY */
    int  colorblind_mode;    /* 0=None,1=Deuteranopia,2=Protanopia,
                                3=Tritanopia                            */
    int  high_contrast;      /* 0=off, 1=on                             */
    int  font_size_bump;     /* 0, +2, or +4 added to all size tiers   */
    int  reduce_motion;      /* 1=disable particles + screen shake      */

    /* GAMEPLAY */
    int  starting_lives;     /* 3, 5, 7, or -1 (unlimited)             */
    int  difficulty;         /* 0=AUTODYNE(easy), 1=STANDARD, 2=CACOGEN*/
    int  auto_fire;          /* 0=off, 1=on                             */
    int  aim_assist;         /* 0=off, 1=on                             */

    /* WORLD */
    uint32_t world_seed;     /* 0 = generate new random seed each run   */
    int  asteroid_density_idx;/* 0=0.5x, 1=1x, 2=1.5x, 3=2x           */
    int  enemy_density_idx;  /* same scale                              */
    int  zone_sharpness;     /* 0=Gradual, 1=Sharp                     */
    int  loot_multiplier_idx;/* 0=0.5x, 1=1x, 2=2x                    */
    int  starting_zone;      /* 0=HOME, -1=RANDOM                      */

    /* SYSTEM */
    int  show_intro;         /* 0=off, 1=on                             */
    /* language: English only for now — no field required              */
} GameSettings;

extern GameSettings g_settings;
```

The `g_settings` global is the single source of truth. All subsystems read directly from it; do not cache settings values locally in module-scope variables unless profiling proves the read cost matters.

#### Existing Settings Variables

The following globals already exist in `game.c` and must be aliased or replaced:

| Old variable | New field | Action |
|---|---|---|
| `settings_glow` | `g_settings.glow_intensity` | Replace with field read |
| `settings_mouse_aim` | `g_settings.mouse_aim` | Replace with field read |
| `settings_screen_shake` | `g_settings.screen_shake` | Replace with field read |

Do not leave both the old variable and the new field alive simultaneously. Migrate in one commit; remove the old variables entirely.

---

### Menu Structure

The menu has a top-level category bar and one page per category. Navigation:
- Left/Right (or L-stick horizontal): cycle category tab. Page resets to row 0 on tab change.
- Up/Down (or L-stick vertical): move cursor within the current page.
- Confirm (Enter / A): activate the selected control (toggle, open sub-page, begin rebind).
- Cancel (Escape / B): close sub-page if open; otherwise return to calling state.
- The cursor wraps at top and bottom of each page.
- Section header rows are skipped by the cursor — they are not selectable.

#### Category Order

1. VIDEO
2. GRAPHICS
3. AUDIO
4. CONTROLS
5. HUD
6. ACCESSIBILITY
7. GAMEPLAY
8. WORLD
9. SYSTEM

---

### Category Pages

#### 1. VIDEO

| Row | Label | Control type | Values |
|---|---|---|---|
| — | `─── DISPLAY ───` | Section header | — |
| 1 | `DISPLAY MODE` | Cycle | WINDOWED / FULLSCREEN / BORDERLESS |
| 2 | `RESOLUTION` | Cycle | List of supported resolutions (see table below) |
| 3 | `V-SYNC` | Toggle | ON / OFF |
| 4 | `REFRESH RATE` | Cycle | MONITOR DEFAULT / 60 / 120 / 144 / 165 / 240 Hz |
| 5 | `ASPECT RATIO` | Cycle | 16:9 / 4:3 / 21:9 |

Resolution list (ordered, widest first):

```c
static const char *RESOLUTIONS[] = {
    "3840×2160", "2560×1440", "1920×1080",
    "1600×900",  "1280×720",  "1024×768"
};
```

The internal reference resolution remains 1280×960. All HUD position constants assume this logical resolution; SDL scaling handles the rest.

Applying a display mode or resolution change requires an SDL window recreation. Display a `[APPLYING...]` flash using pulsed `HUD_AMBER` text for 0.5 seconds before the change takes effect.

#### 2. GRAPHICS

| Row | Label | Control type | Values |
|---|---|---|---|
| — | `─── VISUAL EFFECTS ───` | Section header | — |
| 1 | `GLOW INTENSITY` | Cycle | 0 / 1 / 2 / 3 / 4 |
| 2 | `PARTICLE COUNT` | Cycle | NONE / 16 / 32 / 48 / 64 |
| 3 | `SCANLINES` | Toggle | ON / OFF |
| 4 | `VIGNETTE` | Toggle | ON / OFF |
| 5 | `MOTION BLUR` | Toggle | ON / OFF |
| 6 | `SCREEN SHAKE` | Toggle | ON / OFF |

`GLOW INTENSITY` maps to `g_settings.glow_intensity` and is passed as-is to the existing glow rendering path. Preset 0 disables all glow passes. Preset 4 doubles `HUD_GLOW_INNER_ALPHA` and `HUD_GLOW_OUTER_ALPHA`.

`PARTICLE COUNT` maps directly to the `count` argument of `ui_particle_drift()`. Setting 0 skips the call entirely.

#### 3. AUDIO

| Row | Label | Control type | Values |
|---|---|---|---|
| — | `─── VOLUME ───` | Section header | — |
| 1 | `MASTER VOLUME` | Slider | 0–100 |
| 2 | `MUSIC VOLUME` | Slider | 0–100 |
| 3 | `SFX VOLUME` | Slider | 0–100 |
| 4 | `UI VOLUME` | Slider | 0–100 |
| — | `─── OPTIONS ───` | Section header | — |
| 5 | `STREAMER MODE` | Toggle | ON / OFF |

Slider granularity: 5 units per left/right input step. Sliders display their current value as a number to the right of the bar.

`STREAMER MODE` ON sets `vol_music` rendering to 0 without changing the stored `vol_music` value. The stored value is restored when Streamer Mode is turned OFF.

All volume values map to `MIX_MAX_VOLUME` (128) via: `mix_vol = (int)(setting_vol / 100.0f * MIX_MAX_VOLUME)`. Apply immediately on change via `Mix_VolumeMusic`, `Mix_Volume`, etc.

#### 4. CONTROLS

| Row | Label | Control type | Values |
|---|---|---|---|
| — | `─── AIM ───` | Section header | — |
| 1 | `MOUSE AIM` | Toggle | ON / OFF |
| 2 | `MOUSE SENSITIVITY` | Slider | 1–10 |
| 3 | `CONTROLLER DEADZONE` | Slider | 0–30 (percent) |
| — | `─── BINDINGS ───` | Section header | — |
| 4 | `KEYBINDS` | Sub-page button | `> CONFIGURE` |

`MOUSE SENSITIVITY` maps to a float multiplier: `sensitivity = setting / 5.0f` (so 5 = 1.0x, 1 = 0.2x, 10 = 2.0x).

`CONTROLLER DEADZONE` applies to both axes of the left stick. Value is converted: `deadzone_f = setting / 100.0f`.

##### Keybinds Sub-Page

Entering `KEYBINDS` opens a sub-page listing all rebindable actions. Each row shows:

```
> [ACTION NAME]      [PRIMARY]      [SECONDARY]
```

Actions correspond to `KB_*` enum values defined in `input.h`. Only PRIMARY bindings are editable in this release; SECONDARY column displays `---` and is non-interactive.

When a row is selected and confirmed, the row enters **awaiting input** state:
- Primary binding cell displays `[...]` in pulsed `HUD_AMBER`.
- The next non-modifier keypress or gamepad button press is recorded as the new binding.
- Escape cancels the rebind without change.
- If the new key conflicts with another action, display `[CONFLICT]` in `HUD_CINNABAR` for 1.5 seconds, then revert.

| Action | Default key | KB_* enum |
|---|---|---|
| THRUST | W / Up Arrow | `KB_THRUST` |
| ROTATE LEFT | A / Left Arrow | `KB_ROTATE_LEFT` |
| ROTATE RIGHT | D / Right Arrow | `KB_ROTATE_RIGHT` |
| FIRE | Space | `KB_FIRE` |
| DRIFT BRAKE | S / Down Arrow | `KB_BRAKE` |
| WARP | Tab | `KB_WARP` |
| SALVAGE | E | `KB_SALVAGE` |
| PAUSE | Escape | `KB_PAUSE` (not rebindable) |

`KB_PAUSE` is locked and displayed with `HUD_TEXT_DARK` — visually dimmed, cursor skips it.

#### 5. HUD

| Row | Label | Control type | Values |
|---|---|---|---|
| — | `─── DISPLAY ───` | Section header | — |
| 1 | `SHOW FPS` | Toggle | ON / OFF |
| 2 | `SHOW MINIMAP` | Toggle | ON / OFF |
| 3 | `CROSSHAIR STYLE` | Cycle | NONE / DOT / CROSS / CHEVRON |
| 4 | `HUD SCALE` | Cycle | 75% / 100% / 125% / 150% |
| — | `─── OVERLAYS ───` | Section header | — |
| 5 | `SHOW COMBO` | Toggle | ON / OFF |
| 6 | `SHOW ZONE NAME` | Toggle | ON / OFF |

`HUD SCALE` applies a float multiplier to all HUD position and dimension constants at render time. The multiplier values are `{0.75f, 1.0f, 1.25f, 1.5f}` indexed by `hud_scale`. The constants `HUD_COL_W`, `HUD_ROW_H`, etc. remain unchanged; the multiplier is applied in the HUD draw functions.

`CROSSHAIR STYLE` controls what is drawn at the mouse cursor / aim reticle position during gameplay:
- `NONE`: no crosshair drawn.
- `DOT`: single 3×3 pixel rect at `HUD_TEXT_CYAN`.
- `CROSS`: four 6px SDL lines at cardinal directions, 1px gap at center, `HUD_TEXT_CYAN`.
- `CHEVRON`: `ui_cursor_chevron()` rotated to face the nearest enemy; defaults to pointing right.

#### 6. ACCESSIBILITY

| Row | Label | Control type | Values |
|---|---|---|---|
| — | `─── VISION ───` | Section header | — |
| 1 | `COLORBLIND MODE` | Cycle | NONE / DEUTERANOPIA / PROTANOPIA / TRITANOPIA |
| 2 | `HIGH CONTRAST` | Toggle | ON / OFF |
| — | `─── INTERFACE ───` | Section header | — |
| 3 | `UI FONT SIZE` | Cycle | DEFAULT / +2 / +4 |
| 4 | `REDUCE MOTION` | Toggle | ON / OFF |

`COLORBLIND MODE` remaps the functional bar colors at the SDL level using SDL_SetTextureColorMod on the render target, or by substituting palette constants in the draw path. The remapping is applied in `ui_fuel_color()` and `ui_zone_color()` based on `g_settings.colorblind_mode`. Specific remap tables TBD in accessibility implementation pass.

`HIGH CONTRAST` increases all border alpha values by 30 and sets text colors to white (`HUD_TEXT_PRIMARY` at alpha 255 for all text tiers, including dim).

`UI FONT SIZE` adds `g_settings.font_size_bump` (0, 2, or 4) to every `size` argument passed to `vf_draw_string` in HUD and menu code. All draw sites must use a helper:
```c
static inline int ui_font_size(int base) {
    return base + g_settings.font_size_bump;
}
```

`REDUCE MOTION` set to ON: forces `g_settings.particle_count = 0` and `g_settings.screen_shake = 0` for the session. It does not overwrite the stored values of those fields — when Reduce Motion is turned OFF, the original values are restored.

#### 7. GAMEPLAY

| Row | Label | Control type | Values |
|---|---|---|---|
| — | `─── DIFFICULTY ───` | Section header | — |
| 1 | `DIFFICULTY` | Cycle | AUTODYNE / STANDARD / CACOGEN |
| 2 | `STARTING LIVES` | Cycle | 3 / 5 / 7 / UNLIMITED |
| — | `─── COMBAT ───` | Section header | — |
| 3 | `AUTO-FIRE` | Toggle | ON / OFF |
| 4 | `AIM ASSIST` | Toggle | ON / OFF |

`DIFFICULTY` uses FULIGIN lore terminology for the three tiers. The underlying values map to a `DifficultyLevel` enum:

```c
typedef enum {
    DIFFICULTY_AUTODYNE  = 0,  /* Easy — reduced enemy aggression, more drops  */
    DIFFICULTY_STANDARD  = 1,  /* Normal                                        */
    DIFFICULTY_CACOGEN   = 2   /* Hard — increased density, reduced drop rates  */
} DifficultyLevel;
```

`STARTING LIVES` value -1 represents unlimited. Displayed as `UNLIMITED` in the UI. Internal: `INT_MAX` lives counter that the decrement logic checks against.

`AUTO-FIRE` holds the fire button down automatically when the cursor is active. Interval is the same as the manual fire repeat rate.

`AIM ASSIST` snaps the mouse aim direction toward the nearest enemy within a 60px radius when the player fires. Applies only when `mouse_aim = 1`.

#### 8. WORLD

| Row | Label | Control type | Values |
|---|---|---|---|
| — | `─── GENERATION ───` | Section header | — |
| 1 | `WORLD SEED` | Numeric input | 0 = RANDOM |
| 2 | `ASTEROID DENSITY` | Cycle | 0.5× / 1× / 1.5× / 2× |
| 3 | `ENEMY DENSITY` | Cycle | 0.5× / 1× / 1.5× / 2× |
| — | `─── ZONES ───` | Section header | — |
| 4 | `ZONE BOUNDARY` | Cycle | GRADUAL / SHARP |
| 5 | `LOOT MULTIPLIER` | Cycle | 0.5× / 1× / 2× |
| 6 | `STARTING ZONE` | Cycle | HOME / RANDOM |

`WORLD SEED` is the only numeric text input in the settings menu. When selected:
- The row enters edit mode; the value field becomes a digit cursor.
- Left/Right moves cursor within the value; Up/Down increments/decrements digit at cursor.
- Confirm accepts the value; Cancel discards the edit.
- Value 0 displayed as `RANDOM` (a new seed is generated each run).
- Maximum value: 4294967295 (UINT32_MAX).

The seed entered here populates `g_settings.world_seed` and is copied into `WorldGenParams.seed` at run start.

#### 9. SYSTEM

| Row | Label | Control type | Values |
|---|---|---|---|
| — | `─── OPTIONS ───` | Section header | — |
| 1 | `SHOW INTRO` | Toggle | ON / OFF |
| 2 | `LANGUAGE` | Display only | `ENGLISH` |
| — | `─── MAINTENANCE ───` | Section header | — |
| 3 | `CREDITS` | Button | Opens credits screen |
| 4 | `RESET TO DEFAULTS` | Button | Confirmation required |
| 5 | `DELETE SAVE` | Button | Confirmation required |

`LANGUAGE` is display-only in this release. The row is navigable but confirm produces no action. It renders in `HUD_TEXT_DIM` to signal non-interactivity.

`RESET TO DEFAULTS` opens a confirmation prompt:

```
╔══════════════════════════════╗
║  RESET ALL SETTINGS?         ║
║                              ║
║  > CONFIRM                   ║
║    CANCEL                    ║
╚══════════════════════════════╝
```

Prompt uses the same double-border terminal style. `CONFIRM` is drawn in `HUD_CINNABAR`; `CANCEL` in `HUD_TEXT_DIM`.

`DELETE SAVE` opens the same two-choice confirmation pattern. On confirm: delete `fuligin_save.dat` and `fuligin_settings.cfg`, then return to title screen with all defaults loaded.

---

### Persistence Format

`fuligin_settings.cfg` is a plain ASCII file. One `key=value` pair per line. Lines beginning with `#` are comments. Unknown keys are silently ignored on load.

```
# FULIGIN settings — do not hand-edit
display_mode=0
resolution_index=2
vsync=1
glow_intensity=2
vol_master=80
vol_music=70
vol_sfx=80
vol_ui=60
mouse_aim=1
world_seed=0
difficulty=1
```

`settings_save()` writes the full struct to disk. `settings_load()` reads it; any key not present in the file leaves the corresponding field at its default value.

---

## Part 2 — World Generation Specification

### Overview

Every run of FULIGIN uses a 32-bit seed to deterministically generate the world. The same seed always produces the same asteroid positions, enemy timing, resource locations, and warp points. Players can enter a seed in the WORLD settings page to replay or share specific runs.

The seed and generation parameters are collected into a `WorldGenParams` struct at run start. This struct is passed to `worldgen_init()`, which populates the world state. It is not mutated after initialization.

### WorldGenParams Struct

Declare in `game.h`:

```c
typedef struct {
    uint32_t seed;              /* 0 = generate a new random seed each run        */
    float    asteroid_density;  /* multiplier: 0.5, 1.0, 1.5, or 2.0             */
    float    enemy_density;     /* multiplier: same scale                          */
    float    loot_multiplier;   /* multiplier: 0.5, 1.0, or 2.0                  */
    int      zone_sharpness;    /* 0 = gradual zone transitions, 1 = sharp        */
    int      starting_zone;     /* 0 = HOME SPACE, -1 = random zone               */
} WorldGenParams;
```

The `WorldGenParams` struct is separate from `GameSettings`. `settings_to_worldgen()` (in `settings.c`) constructs a `WorldGenParams` from the current `g_settings` before each run:

```c
WorldGenParams settings_to_worldgen(const GameSettings *s) {
    static const float density_table[] = { 0.5f, 1.0f, 1.5f, 2.0f };
    static const float loot_table[]    = { 0.5f, 1.0f, 2.0f };

    WorldGenParams p;
    p.seed             = s->world_seed;
    p.asteroid_density = density_table[s->asteroid_density_idx];
    p.enemy_density    = density_table[s->enemy_density_idx];
    p.loot_multiplier  = loot_table[s->loot_multiplier_idx];
    p.zone_sharpness   = s->zone_sharpness;
    p.starting_zone    = s->starting_zone;
    return p;
}
```

If `seed == 0`, `worldgen_init()` generates a seed via `SDL_GetTicks() ^ (uint32_t)time(NULL)` and stores it back in the `WorldGenParams` so it can be displayed to the player at the end of the run.

### Seed Display

After a run ends (game over or victory), the post-run screen displays:

```
WORLD SEED: 2847391042
[ENTER THIS SEED IN WORLD SETTINGS TO REPLAY THIS RUN]
```

Seed display uses `HUD_TEXT_GOLD` at size 12. The instruction line uses `HUD_TEXT_DIM` at size 9.

---

### Zone Structure

The world is divided into four named zones arranged radially from the player's starting position. Zone 0 is the center; Zone 3 is the outermost ring. The player must traverse each zone in order to reach the next, though backtracking into earlier zones is always possible.

| Index | Name | Density | Enemy presence | Resource availability | Accent color |
|---|---|---|---|---|---|
| 0 | HOME SPACE | Low | Rare | Common — fuel, ammo | `HUD_TEAL` (`#00b4d8`) |
| 1 | INNER BELT | Medium | Sporadic | Moderate — mixed drops | `HUD_BORDER_ACTIVE` (`#00ffff`) |
| 2 | DEEP VOID | High | Frequent | Scarce — rare high-value | `HUD_PURPLE` (`#7850dc`) |
| 3 | THE ABYSS | Extreme | Boss-tier | Legendary drops only | `HUD_CINNABAR` (`#e34234`) |

`zone_sharpness` controls the transition between zones:
- `0` (GRADUAL): density ramps linearly over a 20% overlap band at zone boundaries. Enemy types also transition gradually.
- `1` (SHARP): zone boundary is a hard line. Density and enemy type change immediately at the zone edge.

---

### Seed-Driven Generation Details

The generator uses a simple linear congruential sequence seeded from `WorldGenParams.seed`. All randomness within a run is derived from this one source, consumed in a deterministic order. Do not call `rand()` anywhere in the world generation path — use the seeded LCG functions exposed by `worldgen.h`.

```c
/* worldgen.h — LCG interface */
void   wg_seed(uint32_t seed);
uint32_t wg_rand(void);               /* raw 32-bit value                  */
float  wg_randf(void);                /* uniform float [0.0, 1.0)          */
float  wg_randf_range(float lo, float hi); /* uniform float [lo, hi)       */
int    wg_randi_range(int lo, int hi);     /* uniform int [lo, hi]         */
```

The generation pass runs in this fixed order. Consuming random values out of this order changes the world and breaks seed reproducibility:

1. **Zone asteroid fields** — for each zone 0–3, generate `(base_count × density_multiplier)` asteroid positions.
2. **Warp points** — one per zone, placed at a seed-determined angle and distance from zone center.
3. **Fuel depots** — one per zone, placed at a position separate from the warp point.
4. **Shop encounters** — one per zone, position determined by seed.
5. **Enemy wave seeds** — one seed value per zone stored and used by the combat system at runtime to reproduce enemy wave timing and type selection.
6. **Resource drop seeds** — one seed value per zone passed to the loot table at drop time.
7. **Boss spawn position** — at the Zone 2→3 boundary, one position.

---

### Guaranteed Elements Per Zone

These elements are present in every run regardless of seed or density settings. Generation must place them before distributing random content:

| Element | Count per zone | Placement rule |
|---|---|---|
| Fuel depot | 1 | At least 200px from any warp point |
| Warp point | 1 | Connects to the next zone (Zone 3 warp point leads to post-run screen) |
| Shop encounter | 1 | At least 300px from the fuel depot |
| Boss encounter | 1 (Zone 2→3 boundary only) | At the midpoint between Zone 2 center and Zone 3 entry |

The boss encounter at the Zone 2→3 boundary is always present. It is not influenced by `enemy_density`. Defeating the boss unlocks the Zone 3 warp point.

---

### Asteroid Count Formula

Base counts before density multiplier:

| Zone | Base asteroid count |
|---|---|
| HOME SPACE | 24 |
| INNER BELT | 40 |
| DEEP VOID | 64 |
| THE ABYSS | 96 |

Final count: `(int)(base_count * params.asteroid_density)`. Minimum 6 per zone regardless of multiplier.

---

### Enemy Density and Wave Timing

Enemy wave frequency scales with `enemy_density`. The base wave interval is defined per zone:

| Zone | Base wave interval (seconds) |
|---|---|
| HOME SPACE | 45.0 |
| INNER BELT | 30.0 |
| DEEP VOID | 18.0 |
| THE ABYSS | 10.0 |

Actual interval: `base_interval / params.enemy_density`. Minimum interval 5.0 seconds.

Enemy type tables per zone (lore-authentic names must match `EnemyType` enum values in `game.h`):

| Zone | Possible enemy types |
|---|---|
| HOME SPACE | `ENEMY_VECTOR_STALKER` |
| INNER BELT | `ENEMY_VECTOR_STALKER`, `ENEMY_BOUNDARY_WEAVER` |
| DEEP VOID | `ENEMY_BOUNDARY_WEAVER`, `ENEMY_EYE_OF_THE_VOID`, `ENEMY_ELDRITCH_TENDRIL` |
| THE ABYSS | `ENEMY_EYE_OF_THE_VOID`, `ENEMY_ELDRITCH_TENDRIL`, `ENEMY_DAEMON_SIGIL` |

Enemy type selection within a zone uses the per-zone enemy wave seed via `wg_randi_range()`.

---

### Loot Table Scaling

`loot_multiplier` scales the probability weight of drop events, not the drop count per event. Base drop probabilities (for STANDARD / 1.0× loot):

| Resource | Base drop chance per asteroid destroyed |
|---|---|
| Fuel canister | 8% |
| Ammo pack | 12% |
| RELIQUARY (upgrade) token | 3% |
| CHRONICLE (XP) orb | 20% |

Final chance: `base_chance * params.loot_multiplier`, clamped to 60% maximum per resource type. Probabilities are evaluated independently — multiple drops from one asteroid are possible.

Guaranteed drops from the fuel depot and shop encounters are not subject to loot multiplier scaling.

---

### worldgen_init Signature

Declared in `worldgen.h`:

```c
/**
 * @brief Initialize the world state for a new run.
 *
 * Applies the given generation parameters, seeds the LCG, places all
 * guaranteed elements, then populates asteroid fields, warp points, shops,
 * and enemy wave seeds for each zone.
 *
 * If params->seed == 0, a new seed is generated and written back into
 * params->seed before generation proceeds.
 *
 * @param params  Generation parameters. Must not be NULL.
 * @return        0 on success, -1 on allocation failure.
 */
int worldgen_init(WorldGenParams *params);
```

`worldgen_init` is called once at the start of each run, after `settings_to_worldgen()` has populated the `WorldGenParams`. It must not be called during gameplay.

---

*This specification is authoritative for implementation of `settings.c`, `settings.h`, `worldgen.c`, `worldgen.h`, and all callers in `game.c`. Treat field names, enum values, function signatures, and pixel constants as implementation contracts. Do not deviate from the `WorldGenParams` struct layout without updating this document and all call sites.*
