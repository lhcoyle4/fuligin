# FULIGIN HUD Design Brief
## Terminal Cockpit Aesthetic — FF7R / Caves of Qud Synthesis

*Prepared for 3 parallel coding agents: ui.h + ui.c, game.c render functions, and STYLE_GUIDE.md update.*
*Based on: FF7 Remake HUD analysis, Caves of Qud UI (Polat Yarisci, 2020), lore requirements.*

---

## Part 1 — Reference Analysis

### 1A. Final Fantasy VII Remake HUD

#### Panel Geometry
- **Primary shape: parallelogram / angled cut panels.** Every character stat block uses a trapezoid — left or right edge sliced at ~20–30° diagonal. The cut goes into the corner, not across the full height.
- Battle character cards (bottom of screen) use a **left-angled cut** on the left edge: the top-left corner is clipped diagonally ~20px in, creating a chevron-entry feel.
- HP / MP / ATB bars are housed inside these angled containers; the container itself clips the bar corners.
- **Thin 1px border lines** — no thick frames. The border is almost always the accent color at partial transparency, creating a glow rather than a solid frame.
- Separators between sections are **diagonal lines**, not horizontal, matching the panel angle.
- **No rounded corners anywhere in the battle HUD.** Menus use small rounding (~2px), combat UI uses zero.

#### Typography
- **All-caps throughout** for every player-facing label and value.
- Two tiers: a **small label** (~9–10px equivalent, dimmer, uppercase) and a **large value** (~18–22px, bright, same caps).
- Labels are left-aligned and positioned above or beside the value. Never below.
- Character names use a **wider, spaced-out uppercase** treatment. Think `C L O U D` with 2–3px letter spacing between each character.
- Numbers (HP values) are right-aligned within their column. The decimal alignment matters — the numeral column is always fixed-width.
- Font feel: **geometric sans-serif with sharp angles** — zero curves, all corners, slightly condensed. Not humanist. Think Eurostile, Handel Gothic, or Bebas Neue.

#### Color Usage
- **Background panels:** very dark near-black with a deep blue or purple cast. `~#0a0a14` to `#0d0f1c`.
- **HP bar:** bright cyan-white `#c8f0ff` or near-white with cyan tint. The bar itself is a clean teal.
- **MP bar:** blue-violet `#6060ff` to `#8080ff`.
- **ATB bars:** bright cyan `#00e5ff` or `#00cfff`. Partially charged shows dimmer teal track.
- **Limit Break bar:** amber/gold `#ffb800` to `#ffd040`.
- **Danger state (HP low):** bar shifts to cinnabar/red with pulsing glow.
- **Panel borders:** **cyan at partial alpha** (`#00b4d8` at ~180/255 alpha) for most panels. Purple-tinted borders for special states.
- **Active / selected:** gold highlight `#ffd040` appears on the selected character or action.
- **Enemy HP bars:** appear above enemies in the world, minimal style — thin bar, no label text, cinnabar fill.
- **Inactive / background elements:** desaturated, dim (~40–60% opacity of the normal color).

#### Progress Bar / Gauge Style
- **ATB gauge:** segmented into 2 discrete sections separated by a thin gap (2px). Each segment fills independently. The **segment divider is a vertical line at 50% of bar width** — when the first segment is full, it shows as a bright filled block; unfilled is a dark track.
- **HP bar:** solid fill, no segmentation. Leading-edge glow is subtle — a 2–3px bright stripe at the fill boundary.
- **Track color:** very dark, slightly blue-tinted `#1a1a2e` — not pure black.
- **Fill-to-track contrast:** high. The fill is bright and vivid; the track is very dark.
- **Bar height:** approximately 8–12px for HP/MP, 6–8px for ATB. Never thick chunky bars.
- **Border:** 1px hairline in the fill color at 50% alpha.
- **Glow:** the entire bar has a faint halo — achieved by drawing the bar 2px larger at very low alpha first.

#### HUD Element Shapes
- **Chevron markers (`>`, `>>`):** used to point at the active character / active menu item. The chevron is drawn with two diagonal lines meeting at a point — not a text character, a drawn shape.
- **Corner cuts:** top-left or top-right corner of panels is cut diagonally. This cut is consistently ~15–25px on both axes.
- **Dividers:** thin diagonal lines at the panel's base angle, separating sections within a panel.
- **Brackets:** used around icons and ability slots (`[ATK]`, `[MAT]`). The brackets are tight to the content — 3–4px gap on each side.
- **Reticle on active target:** a thin 4-line crosshair drawn in the world, not the HUD — cyan, 1px lines.
- **Status effect icons:** small square/diamond shapes, no rounding, 16x16px range.

#### Information Density
- **Very high.** 3 character cards visible simultaneously, each showing: name, HP value, HP max, HP bar, MP bar, ATB gauge (x2 segments), Limit bar, status icons.
- Data is **organized by character row** — one character per horizontal strip.
- Horizontal layout for gauges: label | bar | value. All on one line per stat.
- **Compressed vertically** — the entire character panel stack fits in the bottom ~20% of the screen.
- Ability menu floats above the panel, shows 4 abilities in a 2x2 grid inside its own angled panel.

#### Distinctive Motifs
- **Angled cut corners** (top-left clip). This is the single strongest visual signature.
- **Diagonal separator lines** within panels.
- **Chevron pointers** for active states.
- **Segmented ATB bars** — the dual-segment design.
- **Thin glowing borders** with no fill (borders are the decoration, not the shape).
- **Floating damage numbers** in the world, cinnabar/amber colors, fade and drift upward.
- **Vignette** on the screen edges during combat.

#### Overall Feel / Mood
Tactical, precise, engineered. Military-industrial. Cool-toned with surgical accents. Information-first — every pixel serves a purpose. The angled geometry reads as "advanced technology," the glow reads as "active/hot," the dark fill reads as "reliable, serious." Not playful. The UI is a cockpit readout.

---

### 1B. Caves of Qud UI (Polat Yarisci, 2020 Redesign)

#### Panel Geometry
- **Rectangular panels with no corner cuts.** The entire redesign is axis-aligned rectangles. No diagonals.
- **Border treatment:** thin 1px lines in the accent color. Sometimes a double-border: an outer dim line and an inner bright line 2px inside.
- **Inner panels** (sub-sections within a larger panel) use a **1px separator line** in a dimmer version of the border color, and a slightly different fill color to create depth.
- The **outer screen frame** in the classic ASCII mode uses a solid border of `█` characters — heavy single-line box-drawing. This translates to a 2px or 3px solid border in the redesign.
- **Panel padding:** 6–8px inner padding from border to content on all sides. Content is never flush with the edge.

#### Typography
- **Monospaced throughout.** Source Code Pro is the system font. The redesign keeps this monospace discipline.
- **ALL-CAPS for headers and labels.** Mixed case for content descriptions and lore text.
- **Prompt characters as decoration:** `>` prefixes active items. `[` and `]` bracket stat labels. `:` follows label names.
- **Fixed-width columns:** values are right-aligned in a consistent column position. The label occupies the left ~60% of the panel width; the value occupies the right ~40%, right-aligned.
- Label style: `HP:` or `[HP]` — the label is dimmer than the value. Two-tier brightness.
- **Header rows** use bright text with a separator line beneath (`──────────`).
- **Cursor indicator:** `>` in bright green precedes the selected line. Rest of the list dims to 60% alpha.

#### Color Usage
- **Background:** near-black, with a slight teal or green tint. `#040c0c` or `#060d0c`.
- **Primary accent / active elements:** bright green `#00c420` (dark green) and `#39ff14` (acid green for critical states). The 18-color system uses:
  - Bright green `g` = `(0, 196, 32)` — primary interactive color
  - Dark green `G` = `(0, 148, 3)` — secondary, background elements
  - Bright cyan `C` = `(0, 255, 255)` — used for highlighting special items
  - Yellow/gold `W` = `(255, 255, 0)` — warnings, key items
  - Dark red `r` = `(128, 0, 0)` / Bright red `R` = `(255, 0, 0)` — danger, enemy HP
  - White `Y` = `(255, 255, 255)` — primary text
  - Bright grey `y` = `(192, 192, 192)` — secondary text
  - Dark grey `K` = `(64, 64, 64)` — tertiary / inactive text
  - Black `k` = `(0, 0, 0)` — backgrounds, voids
- **Panel fill:** nearly black, slight warm tint at very low saturation. `#0a0c0a`.
- **Selected item:** inverted — bright green background `#00c420`, black text `#000000`. Or: full-brightness text + `>` cursor, rest dimmed.
- **Borders:** dim green `#003300` for outer frame, bright green `#00c420` for inner/active frame.

#### Progress Bar / Gauge Style
- **HP bar:** solid fill, green when healthy, red when low. Text value printed inside or beside the bar.
- The Qud bar is explicitly **text-based in ASCII mode**: `[████████░░░░]` — filled blocks vs empty blocks. Block characters `█` and `░`.
- **In the redesigned graphical mode:** a traditional fill bar, but with a **monospace aesthetic** — bar height matches line height exactly, and the bar is divided into **discrete block segments** corresponding to 10% increments. Each segment is 1 character wide.
- **Track:** dark fill `#001100`, fill color `#00c420`.
- **Bar border:** tight 1px in the fill color.
- **No leading-edge glow in Qud's aesthetic.** Clean, flat blocks.
- **Bar labels** are printed as text beside the bar: `HP  142/200` — label, then value/max.

#### HUD Element Shapes
- **Screen border:** full-screen border using box-drawing characters or equivalent 1px lines. The four corners have no decoration — just the corner joint of two lines.
- **Section headers** in panels: `─── SECTION NAME ───` — text surrounded by dash-lines. In graphical mode, this becomes a centered label with horizontal rules extending left and right.
- **Minimap:** small rectangular area, top-right corner. Filled with grid cells — 1 cell = 1 tile. Player position marked with `@` or a bright single pixel.
- **Ability bar at bottom:** rectangular blocks, each labeled `[A]`, `[B]`, etc. Square brackets are decorative.
- **Event log / message area:** right panel, bottom-anchored, scrollable text list. Separator line at the top of this panel.
- **Popup panels** (examine, loot): appear centered over the tile being inspected. Border style matches the main panels but with a title bar at the top.
- **Status effects:** listed as text names, comma-separated, color-coded by type.

#### Information Density
- **Very high, text-dense.** The classic Qud UI fits: character portrait icon, name, HP bar, thirst/hunger status, temp, carry weight, water drams, QN/MS/AV/DV/MA stats, date/time, zone name, active effects, missile weapon status, and abilities bar — all visible simultaneously without scrolling.
- **Two-column layout** for stats: label left, value right.
- **Top bar** carries non-critical ambient status in a single dense line: `T:25° Sated Quenched 63/318# 35$ Harvest Dawn 6th of Nivvun Ut  Joppa`.
- **Everything is dense, nothing wastes space.** No decorative whitespace. Padding is minimal (4–6px).

#### Distinctive Motifs
- **Box-drawing characters:** `┌─┐│└─┘`, `╔═╗║╚═╝` — used for panel borders. In graphical mode, simulated with SDL line draws.
- **Prompt cursor `>`** for active selection.
- **Square bracket labels `[STAT]`** for grouped data.
- **Section dividers with text embedded:** `─── LABEL ───`.
- **CRT scanlines overlay** on the main game screen (visual style doc confirms this).
- **Vignette on screen edges** (also confirmed in visual style doc).
- **Monospaced column alignment:** values are always in the same horizontal column, regardless of label length.
- **Text particle VFX** (optional setting) — floating text chunks from events.
- **Color-coded text** — every category of data has its own consistent color from the 18-color palette.

#### Overall Feel / Mood
Ancient terminal. Utilitarian archaeology. A machine that was never meant to be beautiful but has become beautiful through age and decay. Green phosphor on dark glass. Bureaucratic precision about chaotic events. The UI has been running for ten thousand years without maintenance and still works. Functional, archaeological, quietly eerie.

---

## Part 2 — FULIGIN HUD Design Rules

### Philosophy

FULIGIN's HUD is the cockpit of the AUTODYNE — a machine that predates recorded history, still operational, its screen technology from an age that no longer exists. The UI should feel like:

> *"A flight panel from an aeon-old warship, rebuilt by hand from recovered circuit-schematics, displaying data in formats that nobody alive can fully decode, but which still compel obedience."*

**Two aesthetic modes in synthesis:**
- **FF7R geometry:** angled cuts, glowing thin borders, segmented bars, tactical readout feel.
- **CoQ terminal:** green-on-dark, monospaced alignment, bracket/prompt characters, text density, CRT scanlines.

**The synthesis rule:** use FF7R *shapes and color structure* with CoQ *typographic language and character motifs*. The result is a terminal that looks like it was drawn by an engineer, not a UI designer — precise, character-dense, but with the deliberate angularity of military hardware.

---

### Color Palette — Authoritative Definitions

Use these hex values and SDL_Color constants exclusively. Never raw literals in draw code.

```c
/* =========================================================
 * FULIGIN HUD TERMINAL PALETTE
 * ========================================================= */

/* Backgrounds & surfaces */
#define HUD_VOID          ((SDL_Color){  5,   5,  12, 255})  /* Absolute void — clear color            */
#define HUD_PANEL_FILL    ((SDL_Color){ 10,  15,  28, 200})  /* Panel interior — deep navy              */
#define HUD_PANEL_DEEP    ((SDL_Color){  6,   8,  16, 220})  /* Sub-panel, inset fill — darker layer    */
#define HUD_TRACK         ((SDL_Color){ 18,  24,  38, 210})  /* Progress bar track / empty              */

/* Borders & structure */
#define HUD_BORDER_DIM    ((SDL_Color){  0,  90, 110,  90})  /* Outer glow pass — very low alpha teal   */
#define HUD_BORDER_MID    ((SDL_Color){  0, 150, 170, 130})  /* Inner glow pass — mid alpha             */
#define HUD_BORDER_MAIN   ((SDL_Color){  0, 180, 216, 200})  /* Main 1px border — Noctis teal #00b4d8   */
#define HUD_BORDER_ACTIVE ((SDL_Color){  0, 255, 255, 240})  /* Hot-active border — full cyan           */

/* Text */
#define HUD_TEXT_PRIMARY  ((SDL_Color){230, 235, 240, 255})  /* Main labels and values                  */
#define HUD_TEXT_DIM      ((SDL_Color){100, 115, 130, 200})  /* Secondary labels, inactive              */
#define HUD_TEXT_DARK     ((SDL_Color){ 45,  55,  70, 180})  /* Tertiary / background text              */
#define HUD_TEXT_CYAN     ((SDL_Color){  0, 210, 230, 255})  /* Active/selected text                    */
#define HUD_TEXT_GOLD     ((SDL_Color){255, 220,  80, 255})  /* Highlight, selected item, gold value    */

/* Functional colors — resource states */
#define HUD_GREEN         ((SDL_Color){ 57, 255,  20, 230})  /* Healthy / CHRONICLE / resources         */
#define HUD_GREEN_DIM     ((SDL_Color){ 20,  90,   8, 180})  /* Green bar track / dim background        */
#define HUD_AMBER         ((SDL_Color){255, 140,   0, 230})  /* Warning / fuel low / mid-danger         */
#define HUD_AMBER_DIM     ((SDL_Color){ 80,  42,   0, 160})  /* Amber track                             */
#define HUD_CINNABAR      ((SDL_Color){227,  66,  52, 240})  /* Critical / damage / enemy               */
#define HUD_CINNABAR_DIM  ((SDL_Color){ 80,  18,  12, 160})  /* Cinnabar track                          */
#define HUD_TEAL          ((SDL_Color){  0, 180, 216, 230})  /* HP / primary resource bar               */
#define HUD_TEAL_DIM      ((SDL_Color){  0,  50,  65, 160})  /* Teal track                              */
#define HUD_GOLD_BAR      ((SDL_Color){255, 200,  50, 230})  /* CHRONICLE (XP) / progression bar        */
#define HUD_PURPLE        ((SDL_Color){120,  80, 220, 220})  /* Deep zone / Abyss / special state       */
#define HUD_MAGENTA       ((SDL_Color){255,   0, 255, 220})  /* Enemy fire / Cacogen / chaos            */

/* CRT & atmosphere */
#define HUD_SCANLINE      ((SDL_Color){  0,   0,   0,  24})  /* Scanline overlay pass                   */
#define HUD_VIGNETTE      ((SDL_Color){  0,   0,   0, 180})  /* Screen-edge vignette                    */
#define HUD_PARTICLE      ((SDL_Color){  0, 200, 255,  28})  /* Ambient drift particle tint             */

/* Zone-indexed accent colors [HOME, INNER, DEEP, ABYSS] */
static const SDL_Color HUD_ZONE_ACCENT[4] = {
    {  0, 180, 216, 200},  /* HOME SPACE  — Noctis teal   #00b4d8 */
    {  0, 140, 255, 200},  /* INNER BELT  — cool azure            */
    {160,  80, 255, 200},  /* DEEP VOID   — void purple           */
    {227,  66,  52, 220},  /* THE ABYSS   — cinnabar red          */
};
```

---

### Typography Rules

FULIGIN uses a custom vector stroke font (rendered via `vf_draw_string`). Because it is a stroke font, not a bitmap grid, it is **not inherently monospaced** — but the layout must feel monospaced. Achieve this with explicit column-grid alignment.

#### Size Tiers

| Tier | vf_draw_string `size` param | Use case |
|---|---|---|
| Tiny | 7 | Tertiary labels, secondary values, scanline-dense info |
| Small | 9 | Standard HUD labels, stat names, bracket labels |
| Medium | 12 | Values for HP/fuel/score — the number the player reads |
| Large | 16 | SCORE, COMBO multiplier, prominent single values |
| XL | 22 | Title screen, GAME OVER, dramatic one-liners |

#### Column Grid Alignment

Define a column grid with 7px column width (matching a typical monospaced character cell at size 9). All text must snap to this grid:

```c
#define HUD_COL_W    7    /* Column width in pixels — monospace grid unit */
#define HUD_ROW_H   12    /* Row height in pixels — standard line spacing  */

/* Snap a float x to the nearest column boundary */
#define HUD_SNAP_COL(x)  ((int)((x) / HUD_COL_W) * HUD_COL_W)
```

When laying out a label-value pair in a panel of width `W`:
- Label occupies columns 0 through `LABEL_COLS - 1` (left-aligned).
- Value occupies columns `LABEL_COLS` through end (right-aligned within its column block).
- The split is typically at ~60% of panel width for stats, ~50% for narrow panels.

#### Typography Motifs (Required)

These characters must appear in the UI as literal drawn text (via vf_draw_string, not as SDL rect decorations):

| Motif | Usage |
|---|---|
| `>` | Active selection cursor — leftmost column, bright cyan |
| `[ ]` | Bracket around stat labels: `[HP]`, `[ATB]`, `[FUEL]` |
| `:` | Label separator: `HULL:`, `ZONE:` |
| `─` or `---` | Section dividers (drawn with SDL_RenderDrawLineF, not characters) |
| `>>>` | Warning indicator — triple chevron in amber, critical state |
| `//` | Comment-style separator in dense info areas |

#### ALL-CAPS Rule

**Every player-facing string is ALL-CAPS.** No exceptions. Lowercase is reserved for:
- Developer log messages (`SDL_Log`)
- Source code identifiers
- Doxygen comments

---

### Panel Geometry — Shapes and SDL Primitives

#### Primary Panel Type: Angled-Cut Rectangle

The signature shape borrowed from FF7R. A standard rectangle with the **top-left corner clipped diagonally**.

```
(x+cut, y)─────────────────(x+w, y)
    /                              |
   /  CUT = 12px on each axis     |
  /                                |
(x, y+cut)                    (x+w, y+h)
  |                                |
  └────────────────────────────────┘
```

Draw with `SDL_RenderDrawLinesF` as a 6-point polygon:

```c
void ui_panel_angled(SDL_Renderer *r, float x, float y, float w, float h,
                     float cut, SDL_Color border)
{
    /* Fill: use SDL_FRect for the main body, then fill the cut triangle */
    /* Border: draw the 6-point outline */
    SDL_FPoint pts[7] = {
        { x + cut,  y       },   /* top-left after cut */
        { x + w,    y       },   /* top-right */
        { x + w,    y + h   },   /* bottom-right */
        { x,        y + h   },   /* bottom-left */
        { x,        y + cut },   /* left edge after cut */
        { x + cut,  y       },   /* close to start */
        { x + cut,  y       },   /* duplicate for SDL_RenderDrawLinesF */
    };
    /* ... fill and border passes as in ui_panel() */
}
```

**Cut size by panel type:**
- Small stat panels (bottom HUD): `cut = 10`
- Mid-size info panels: `cut = 14`
- Large section panels: `cut = 18`
- Minimap panel: `cut = 0` (rectangular — the minimap is a special case)

#### Secondary Panel Type: Double-Border Rectangle (CoQ style)

For info-dense terminal panels. No angled cut. Two concentric rectangle borders:

```c
/* Outer border: HUD_BORDER_DIM at 2px outside the panel */
/* Inner border: HUD_BORDER_MAIN at 1px, flush with panel edge */
/* Fill: HUD_PANEL_FILL */
```

Use this for: message log, item lists, menu panels, popup examine dialogs.

#### Separator Lines

- **Horizontal rule within a panel:** single `SDL_RenderDrawLineF` at `HUD_BORDER_MID` color, alpha 80.
- **Header separator:** after the panel header text, draw a line from `x+4` to `x+w-4` at `y + HUD_ROW_H + 2`.
- **Section divider with embedded label:** draw the line in two segments, leaving a gap for the label text. Label is centered in the gap.

```
────────── LABEL ──────────
```

Implement as: draw left segment from `x+4` to `label_x - 4`; draw text; draw right segment from `label_x + label_w + 4` to `x+w-4`.

---

### Progress Bar — Segmented Terminal Style

The bar system combines FF7R's segmented ATB gauge with CoQ's block-discrete aesthetic.

#### Standard Bar (solid fill with glow)

```
[track──────────────────────]
[fill═══════════░░░░░░░░░░░░]   ← solid fill, then dark track
                ^
                leading-edge glow: 3px bright stripe
```

```c
/* Bar geometry */
bar_height = 8;           /* standard stat bars */
bar_height = 10;          /* primary resource (HP equivalent) */
bar_height = 6;           /* secondary resource (ATB, secondary gauges) */

/* Border: 1px in fill_color at alpha=100 */
/* Track: HUD_TRACK */
/* Fill: see color table below */
/* Leading-edge glow: fill_color at alpha=150, 3px wide, extends 1px above and below bar */
```

#### Segmented Bar (ATB style — for 2-segment resources)

Divide the bar into N segments with 2px gaps between each:

```
[seg1══════][seg2══════]    ← two segments, 2px gap between
```

```c
#define HUD_BAR_SEG_GAP  2    /* pixel gap between segments */

/* For a 2-segment bar of total width W:
   seg_w = (W - HUD_BAR_SEG_GAP) / 2;
   seg1 at x, seg2 at x + seg_w + HUD_BAR_SEG_GAP */
```

Use segmented bars for: CHRONICLE (XP — segments per level tier), weapon charge state.

#### Block-Discrete Bar (CoQ ASCII style — for secondary displays)

A text-style bar using repeated drawn rectangles, each 5px wide with 1px gap. Maximum 20 blocks for full bar. Visually: `▐▐▐▐▐▐▐▐░░░░░░░░`.

```c
#define HUD_BLOCK_W     5     /* width of each block */
#define HUD_BLOCK_GAP   1     /* gap between blocks */
#define HUD_BLOCK_MAX  20     /* maximum blocks in a full bar */

/* Draw N filled blocks + (MAX-N) dim empty blocks */
/* Filled block: fill_color, 4x bar_height rect */
/* Empty block: HUD_TRACK + 20 alpha, 4x bar_height rect */
```

Use block-discrete bars for: secondary energy, ammo, shield segments.

#### Color State Rules

```c
SDL_Color hud_bar_color(float pct) {
    if (pct > 0.60f)  return HUD_TEAL;      /* Healthy    — teal       */
    if (pct > 0.30f)  return HUD_AMBER;     /* Warning    — amber      */
    if (pct > 0.10f)  return HUD_CINNABAR;  /* Danger     — cinnabar   */
    return (SDL_Color){227, 40, 30, 255};   /* Critical   — deep red, add pulse */
}
```

Critical state (< 10%) adds a pulse to the bar glow:
```c
/* alpha = (Uint8)(base_alpha * (0.6f + 0.4f * sinf(game_time * 6.0f))) */
```

---

### HUD Layout — Zone Positions

Screen dimensions: 1280 × 960 (per `game.h` constants).

#### Fixed Zone Map

```
┌──────────────────────────────────────────────────────────────────┐
│ [TL PANEL — 240×90]              [TR PANEL — 160×90 MINIMAP]    │
│  SCORE / LEVEL / COMBO            Zone grid + AUTODYNE dot       │
│                                                                   │
│                  [ CENTER — transient overlays ]                  │
│                    COMBO POP / SCORE FLOAT                        │
│                    WARNING TEXT / PROXIMITY                       │
│                                                                   │
│ [BL PANEL — 240×80]              [BR PANEL — 200×80]            │
│  HULL / CHRONICLE / LIVES         FUEL / RESOURCES / ZONE        │
└──────────────────────────────────────────────────────────────────┘
```

#### Pixel Constants

```c
/* Margins from screen edge */
#define HUD_MARGIN_X     8     /* pixels from left/right edge */
#define HUD_MARGIN_Y     8     /* pixels from top/bottom edge */
#define HUD_PAD_INNER    6     /* interior padding — border to content */

/* Top-left panel (SCORE) */
#define HUD_TL_X         8
#define HUD_TL_Y         8
#define HUD_TL_W       240
#define HUD_TL_H        90
#define HUD_TL_CUT      12    /* angled cut size */

/* Top-right panel (MINIMAP) */
#define HUD_TR_W       160
#define HUD_TR_H       160
#define HUD_TR_X      (1280 - HUD_TR_W - HUD_MARGIN_X)  /* = 1112 */
#define HUD_TR_Y         8
#define HUD_TR_CUT       0    /* minimap is rectangular */

/* Bottom-left panel (HULL / CHRONICLE) */
#define HUD_BL_X         8
#define HUD_BL_H        90
#define HUD_BL_Y      (960 - HUD_BL_H - HUD_MARGIN_Y)   /* = 862 */
#define HUD_BL_W       240
#define HUD_BL_CUT      12

/* Bottom-right panel (FUEL / ZONE) */
#define HUD_BR_W       210
#define HUD_BR_H        90
#define HUD_BR_X      (1280 - HUD_BR_W - HUD_MARGIN_X)  /* = 1062 */
#define HUD_BR_Y      (960 - HUD_BR_H - HUD_MARGIN_Y)   /* = 862 */
#define HUD_BR_CUT      12

/* Center overlay anchor — offsets from screen center */
#define HUD_CENTER_X   640
#define HUD_CENTER_Y   480
```

---

### Panel Content Layouts

#### Top-Left Panel — SCORE / LEVEL / COMBO

```
┌────────────── [angled cut top-left] ──────────────┐
│ [SCORE]                                            │
│ > 000042800                   [size 16, gold]     │
│ ─────────────────────────────────────────────────  │
│ ZONE: HOME SPACE    LVL: 03   [size 9, dim text]  │
│ COMBO ×2.5          [amber if >1x, dim if 1x]     │
└────────────────────────────────────────────────────┘
```

Draw sequence:
1. `ui_panel_angled(r, HUD_TL_X, HUD_TL_Y, HUD_TL_W, HUD_TL_H, HUD_TL_CUT, HUD_ZONE_ACCENT[zone])`
2. `ui_scanlines(r, ...)` over the panel interior
3. Header label `[SCORE]` at size 9, `HUD_TEXT_DIM`, at `(TL_X + PAD, TL_Y + PAD)`
4. Score value at size 16, `HUD_TEXT_GOLD`, right-aligned in the panel
5. Separator line
6. Zone name + level in a single row, size 9, `HUD_TEXT_DIM`
7. Combo multiplier: size 9 if 1x (dim), size 12 + `HUD_AMBER` if >1x

#### Top-Right Panel — MINIMAP

```
┌────────────────────┐
│ [MINIMAP]          │  ← header label, dim cyan
├────────────────────┤  ← separator
│  · · · · · · · ·   │  ← zone grid (1 cell = ~6px square)
│  · · ◉ · · · · ·   │  ← AUTODYNE dot: bright cyan, 3px
│  · · · · · · · ·   │  ← enemies: dim cinnabar, 2px
│  · · · · · · · ·   │
└────────────────────┘
```

Draw sequence:
1. `ui_panel(r, HUD_TR_X, HUD_TR_Y, HUD_TR_W, HUD_TR_H, HUD_BORDER_MAIN)` (rectangular)
2. `ui_scanlines(r, ...)` over interior
3. Header `[MINIMAP]` at size 9
4. Grid cells: each is a `4×4` SDL_FRect with 1px gap. Empty = `HUD_PANEL_DEEP`. With object = object color.
5. Player position: bright cyan `3×3` rect, centered on player cell.

#### Bottom-Left Panel — HULL / CHRONICLE / LIVES

```
┌────────── [angled cut top-left] ──────────────────┐
│ [HULL]  ████████████████░░░░  142/200  [teal bar] │
│ [CHRON] ████████░░░░░░░░░░░░  3200XP   [gold bar] │
│ [LIVES] ◈ ◈ ◈ ·  ·            [cyan ships]       │
└────────────────────────────────────────────────────┘
```

Draw sequence (row layout, each row = `HUD_ROW_H = 12` pixels):
1. Panel at `(HUD_BL_X, HUD_BL_Y, HUD_BL_W, HUD_BL_H)` with cut
2. Row 1 — `y_base = HUD_BL_Y + PAD`:
   - Label `[HULL]` at size 9, `HUD_TEXT_DIM`, column 0
   - Bar: `ui_bar(r, x+54, y_base, 130, 8, hull, hull_max, hud_bar_color(hull/hull_max))`
   - Value `NNN/NNN` at size 7, `HUD_TEXT_PRIMARY`, right-aligned column
3. Row 2 — `y_base += HUD_ROW_H + 4`:
   - Label `[CHRON]`, bar with `HUD_GOLD_BAR`, value
4. Row 3 — `y_base += HUD_ROW_H + 4`:
   - Label `[LIVES]`, then draw N small ship glyphs (vector, 8×8px), dimmed for lost lives

**Bar x-start:** `HUD_BL_X + HUD_PAD_INNER + 54` (54px = 7-col label area + 2px gap in a 7px-col grid: 7 cols × 7px + 5px margin = 54)

#### Bottom-Right Panel — FUEL / ZONE / RESOURCES

```
┌────────────────────────── [angled cut top-left] ──┐
│ [FUEL]  ████████████░░░░░░  78%     [amber→green] │
│ [ZONE]  HOME SPACE         [zone name in color]   │
│ [RES]   ◆◆◆◆◆◆◆◆◆◆ 10     [green, resource count]│
└────────────────────────────────────────────────────┘
```

Same row-layout rules as bottom-left. Zone name color uses `HUD_ZONE_ACCENT[zone]`.

---

### Angled Panel Fill — SDL Implementation

Because SDL2 has no polygon fill primitive, fill the angled panel with two SDL_FRect calls:

```c
void ui_panel_angled_fill(SDL_Renderer *r,
                          float x, float y, float w, float h, float cut,
                          SDL_Color fill)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);

    /* Main body rect (avoids the cut corner) */
    SDL_FRect main = { x, y + cut, w, h - cut };
    SDL_RenderFillRectF(r, &main);

    /* Top strip (right side of cut) */
    SDL_FRect top = { x + cut, y, w - cut, cut };
    SDL_RenderFillRectF(r, &top);

    /* The cut triangle area is left unfilled (or fill with a
       slightly different shade for depth, then the border covers it) */
}
```

The diagonal border line (the cut edge) is drawn with:
```c
SDL_RenderDrawLineF(r, x + cut, y, x, y + cut);
```

---

### Glow / Border — Three-Pass System

All bordered panels use a three-pass border system. Parameters:

```c
#define HUD_GLOW_OUTER_SPREAD  2.0f
#define HUD_GLOW_INNER_SPREAD  1.0f
#define HUD_GLOW_OUTER_ALPHA   22    /* very transparent outer halo */
#define HUD_GLOW_INNER_ALPHA   65    /* semi-transparent inner ring */
/* main pass: border.a as-is (typically 180–240) */
```

For angled panels, all three passes must trace the 6-point polygon, not a rect.

```c
/* Pass 1: outer halo — expand polygon by OUTER_SPREAD on each axis */
/* Pass 2: inner glow — expand by INNER_SPREAD */
/* Pass 3: main border — exact polygon */
```

**Active state:** when a panel is "hot" (e.g., active character in combat), replace `HUD_BORDER_MAIN` with `HUD_BORDER_ACTIVE` and double the `GLOW_INNER_ALPHA` to 130.

---

### CRT Scanlines

Draw over every panel interior. Parameters:

```c
#define HUD_SCANLINE_STRIDE  4     /* every 4 pixels vertically */
#define HUD_SCANLINE_ALPHA  22     /* SDL_Color alpha for scanline */
#define HUD_SCANLINE_COLOR  (SDL_Color){0, 0, 0, HUD_SCANLINE_ALPHA}
```

Implementation — single loop with `SDL_RenderDrawLineF`:
```c
for (float sy = y; sy < y + h; sy += HUD_SCANLINE_STRIDE) {
    SDL_RenderDrawLineF(r, x, sy, x + w, sy);
}
```

**Screen-wide scanlines:** additionally draw scanlines across the full 1280×960 screen at `alpha=10` as a global atmosphere pass before any HUD elements.

---

### Vignette

Draw a screen-edge darkening gradient using layered transparent rects:

```c
void ui_vignette(SDL_Renderer *r) {
    /* 6 concentric rects from edges inward, increasing alpha toward center=0 */
    int depths[] = {0, 10, 16, 22, 30, 42};
    for (int i = 0; i < 6; i++) {
        int margin = i * 18;
        SDL_SetRenderDrawColor(r, 0, 0, 0, depths[5-i]);
        SDL_FRect vr = {margin, margin,
                        1280 - margin*2, 960 - margin*2};
        /* Draw border lines only, not fill */
        /* ... 4 border-rect lines ... */
    }
}
```

Or simpler: 4 gradient-bar rects along each screen edge, width 80px, alpha 0→120:
```c
/* Top: SDL_FRect {0, 0, 1280, 80} with vertical alpha gradient simulation */
/* (Achieved with ~8 stacked rects of increasing alpha, 10px each) */
```

---

### Terminal Motifs — Implementation Details

#### Selection Cursor `>`

Drawn as a vector chevron shape (not text character) 6px wide × 8px tall:
```c
void ui_cursor_chevron(SDL_Renderer *r, float x, float y, SDL_Color color) {
    /* > shape: two lines meeting at a point */
    SDL_RenderDrawLineF(r, x, y,     x + 6, y + 4);
    SDL_RenderDrawLineF(r, x, y + 8, x + 6, y + 4);
}
```

Draw this cursor at `x = HUD_BL_X + PAD - 8` for the selected row in a list.

#### Bracket Labels `[STAT]`

The brackets `[` and `]` are drawn as small L-shapes (not font characters), 3px tall, 2px wide:
```c
void ui_bracket_open(SDL_Renderer *r, float x, float y, float h, SDL_Color c) {
    SDL_RenderDrawLineF(r, x,   y,   x,   y+h);  /* vertical */
    SDL_RenderDrawLineF(r, x,   y,   x+2, y);    /* top arm */
    SDL_RenderDrawLineF(r, x,   y+h, x+2, y+h);  /* bottom arm */
}
```

For `[HULL]` at size 9 — the brackets are drawn as SDL primitives, the `HULL` text is `vf_draw_string`.

#### Section Divider

```c
void ui_section_divider(SDL_Renderer *r, float x, float y, float w,
                        const char *label, SDL_Color color) {
    float label_w = vf_measure_string(label, 9);  /* if this function exists */
    float label_x = x + (w - label_w) / 2.0f;    /* centered */

    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, 80);
    SDL_RenderDrawLineF(r, x + 4, y + 5, label_x - 4, y + 5);
    SDL_RenderDrawLineF(r, label_x + label_w + 4, y + 5, x + w - 4, y + 5);

    vf_draw_string(label, label_x, y, 9, color);
}
```

#### Corner Brackets (existing `ui_corner_brackets`)

Increase default size to `14px` arm length. Use on:
- Selected upgrade card
- Active/targeted enemy indicator (drawn in world space)
- Any interactive panel in menu state

Color: `HUD_BORDER_ACTIVE` (full cyan) for selected, `HUD_BORDER_MAIN` (dim teal) for available.

#### Triple Chevron Warning `>>>`

For critical states (hull < 10%, fuel critical):
```c
void ui_warning_chevrons(SDL_Renderer *r, float x, float y, SDL_Color color) {
    for (int i = 0; i < 3; i++) {
        ui_cursor_chevron(r, x + i * 8, y, color);
    }
}
```

Draw with pulsed alpha: `ui_pulse(HUD_AMBER, game_time, 2.0f, 0.5f)`.

---

### Ambient Atmosphere — Particle Drift

The existing `ui_particle_drift` is correct in principle. Update particle color to use `HUD_PARTICLE` (teal-cyan at alpha 28). Ensure particles render **before** all panels (lowest z-order in the render pipeline):

```
Render order:
  1. Clear screen (HUD_VOID)
  2. Game scene (ship, asteroids, bullets, etc.)
  3. ui_vignette()                  ← screen-edge darkening
  4. ui_scanlines() full-screen     ← global CRT pass
  5. ui_particle_drift()            ← ambient particles
  6. HUD panels (TL, TR, BL, BR)   ← all four zone panels
  7. Center overlays (transient)    ← combo, score floats
  8. Pause / menu overlay (if any)
```

---

### Menu Panels — Terminal Style

Menu overlays use the **double-border rectangular** style (no angled cut), with a heavier terminal feel:

```
╔══════════════════════════════╗   ← outer border: HUD_BORDER_MAIN, 2px
║                              ║
║ ─── RELIQUARY ───            ║   ← section header, section divider
║                              ║
║ > [UPGRADE A]   COST: 500    ║   ← selected item: full-bright, > cursor
║   [UPGRADE B]   COST: 750    ║   ← unselected: 60% alpha
║   [UPGRADE C]   COST: 1200   ║
║                              ║
╚══════════════════════════════╝
```

The double-box border is drawn as:
```c
/* Outer box */
SDL_SetRenderDrawColor(r, HUD_BORDER_MAIN.r, ..., 120);
draw_rect_outline(r, x-2, y-2, w+4, h+4);
/* Inner box (main border) */
SDL_SetRenderDrawColor(r, HUD_BORDER_MAIN.r, ..., 220);
draw_rect_outline(r, x, y, w, h);
```

**Menu item row height:** `HUD_ROW_H + 4 = 16px` per item.
**Selected item:** draw a filled rect at `HUD_PANEL_DEEP` alpha 100 behind the row. Text at full brightness. `>` cursor drawn left of the row.
**Unselected items:** text at `HUD_TEXT_DIM` (alpha 160).

---

### Transient / Center Overlays

These appear in `(HUD_CENTER_X, HUD_CENTER_Y)` ± offsets. They animate in and out.

#### Score Float

- Text size 14, `HUD_TEXT_GOLD`, alpha fades 255→0 over 1.5 seconds.
- Drifts upward 40px over lifetime.
- Draw with `SDL_BLENDMODE_ADD` for glow effect.

```c
/* Position: center_x - text_w/2, center_y - lifetime * 27 */
/* Alpha: (Uint8)(255 * (1.0f - age / 1.5f)) */
```

#### Combo Multiplier Pop

- Text `×2.5` at size 22, `HUD_AMBER`, scale-animates from 1.3→1.0 over 0.3 seconds.
- Stays on screen for 2.0 seconds total, then fades over 0.5 seconds.

#### Proximity Warning

- Text `>>> DANGER <<<` at size 12, `HUD_CINNABAR`, pulsed alpha at 4hz.
- Positioned at center-bottom: `(HUD_CENTER_X, HUD_CENTER_Y + 120)`.
- Only displayed when an enemy is within N pixels of player.

#### Zone Transition Banner

- Full-width text bar with `HUD_PANEL_FILL` background, border in `HUD_ZONE_ACCENT[new_zone]`.
- Text: `ENTERING [ZONE NAME]` at size 16, centered.
- Slides in from top over 0.5s, holds 2.0s, slides out over 0.5s.
- Position: `y = -40 + transition_t * 50` during slide-in.

---

### Existing `ui.h` / `ui.c` — Migration Notes

The current system (glassmorphism, rectangular panels) needs these additions:

1. **Add `ui_panel_angled()`** — the core new function. Signature:
   ```c
   void ui_panel_angled(SDL_Renderer *r, float x, float y, float w, float h,
                        float cut, SDL_Color border);
   ```

2. **Add `ui_bar_segmented()`** — for ATB/CHRONICLE segmented bars:
   ```c
   void ui_bar_segmented(SDL_Renderer *r, float x, float y, float w, float h,
                         float value, float maximum, int segments, SDL_Color fill);
   ```

3. **Add `ui_bar_block()`** — for CoQ-style discrete block bars:
   ```c
   void ui_bar_block(SDL_Renderer *r, float x, float y,
                     float value, float maximum, int max_blocks,
                     SDL_Color fill);
   ```

4. **Add `ui_cursor_chevron()`** — selection cursor.

5. **Add `ui_section_divider()`** — embedded label in a horizontal rule.

6. **Add `ui_warning_chevrons()`** — triple `>>>` indicator.

7. **Add `ui_vignette()`** — screen-edge darkening.

8. **Update color constants** in `ui.h` from the old palette to the new `HUD_*` constants defined in this brief. The semantic roles remain the same; hex values are refined for better contrast against the darker `#0a0f1c` background.

9. **Update `ui_panel_header()`** — add the section-divider line below the header using `ui_section_divider()` pattern (label centered in the rule).

---

### Visual Keyword Summary

Use these as test criteria when evaluating renders against this brief:

- **Tactical** — could a pilot read this under stress?
- **Terminal** — does it feel like a machine that predates modern UI conventions?
- **Monospaced** — are all values in consistent column positions?
- **Green-on-dark** — is the primary accent color landing as phosphor green / teal?
- **Angled** — is there at least one diagonal cut in every combat panel?
- **Dense** — is the information-to-pixel ratio high?
- **Glow** — do active elements emit light rather than merely having color?
- **Baroque decay** — does it feel ancient and slightly wrong, like a system that has been running too long?

---

## Part 3 — Quick Reference Card

### Colors (most-used)

| Role | Constant | Hex |
|---|---|---|
| Background void | `HUD_VOID` | `#05050c` |
| Panel fill | `HUD_PANEL_FILL` | `#0a0f1c` at alpha 200 |
| Main border | `HUD_BORDER_MAIN` | `#00b4d8` at alpha 200 |
| Active border | `HUD_BORDER_ACTIVE` | `#00ffff` at alpha 240 |
| Primary text | `HUD_TEXT_PRIMARY` | `#e6ebf0` |
| Dim text | `HUD_TEXT_DIM` | `#64737f` |
| Selected text | `HUD_TEXT_CYAN` | `#00d2e6` |
| Gold highlight | `HUD_TEXT_GOLD` | `#ffdc50` |
| Healthy bar | `HUD_TEAL` | `#00b4d8` |
| Warning bar | `HUD_AMBER` | `#ff8c00` |
| Danger bar | `HUD_CINNABAR` | `#e34234` |
| Resource/XP bar | `HUD_GREEN` | `#39ff14` |
| Bar track | `HUD_TRACK` | `#12182e` |
| Scanline | `HUD_SCANLINE` | `#000000` at alpha 24 |

### Size & Spacing

| Constant | Value | Purpose |
|---|---|---|
| `HUD_COL_W` | 7px | Monospace column unit |
| `HUD_ROW_H` | 12px | Standard text row height |
| `HUD_MARGIN_X/Y` | 8px | Screen edge margin |
| `HUD_PAD_INNER` | 6px | Panel border to content |
| Standard bar height | 8px | HP / fuel bars |
| Large bar height | 10px | Primary resource |
| Small bar height | 6px | Secondary / ATB |
| Angled cut (standard) | 12px | Panel corner clip |
| Corner bracket arm | 14px | Selection indicator |
| Scanline stride | 4px | Every 4th row |

### Draw Call Order Per Panel

1. Fill (`SDL_BLENDMODE_BLEND`, `HUD_PANEL_FILL`)
2. Scanlines over fill
3. Glow outer (+2px expand, alpha 22)
4. Glow inner (+1px expand, alpha 65)
5. Main border (1px, full alpha)
6. Header label + section divider rule
7. Content rows (bars, text)
8. Corner brackets (if selected)

---

*This brief is authoritative for tasks #27, #28, and #29. All three agents should treat the hex values, pixel constants, and draw call sequences as implementation contracts, not guidelines.*
