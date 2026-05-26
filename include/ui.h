/*
 * ui.h — FULIGIN Glassmorphism UI Primitives
 *
 * Pure SDL2 drawing utilities for the FULIGIN HUD and menu system.
 * All functions operate in screen-space pixel coordinates.
 * Game state is NOT accessed here — all values are passed as arguments.
 *
 * Visual language: frosted-glass panels (semi-transparent dark fill +
 * single-pixel glowing border), CRT scanlines, ambient particle drift,
 * and progress bars with additive-blend edge glow.
 *
 * Color semantics follow STYLE_GUIDE.md §"FULIGIN Color Palette".
 * Always use the named constants; never raw {r,g,b,a} literals in draw calls.
 *
 * FULIGIN — Gene Wolfe's Book of the New Sun / Jack Vance's Dying Earth
 */

#ifndef UI_H
#define UI_H

#include <SDL2/SDL.h>

/* =========================================================
 * FULIGIN HUD TERMINAL PALETTE — authoritative color constants
 *
 * Use by role, not by hue.  The semantic comment describes intent;
 * that is what governs which constant to reach for.
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
#define HUD_SCANLINE_CLR  ((SDL_Color){  0,   0,   0,  24})  /* Scanline overlay pass                   */
#define HUD_VIGNETTE_CLR  ((SDL_Color){  0,   0,   0, 180})  /* Screen-edge vignette                    */
#define HUD_PARTICLE_CLR  ((SDL_Color){  0, 200, 255,  28})  /* Ambient drift particle tint             */

/* Grid layout constants */
#define HUD_COL_W    7    /* Column width in pixels — monospace grid unit */
#define HUD_ROW_H   12    /* Row height in pixels — standard line spacing  */
#define HUD_MARGIN_X 8    /* Pixels from left/right screen edge            */
#define HUD_MARGIN_Y 8    /* Pixels from top/bottom screen edge            */
#define HUD_PAD_INNER 6   /* Interior padding — border to content          */

/* Snap a float x to the nearest column boundary */
#define HUD_SNAP_COL(x)  ((int)((x) / HUD_COL_W) * HUD_COL_W)

/* Panel zone positions */
#define HUD_TL_X     8
#define HUD_TL_Y     8
#define HUD_TL_W   240
#define HUD_TL_H    90
#define HUD_TL_CUT  12

#define HUD_TR_W   160
#define HUD_TR_H   160
#define HUD_TR_X   (1280 - HUD_TR_W - HUD_MARGIN_X)
#define HUD_TR_Y     8
#define HUD_TR_CUT   0

#define HUD_BL_X     8
#define HUD_BL_H    90
#define HUD_BL_Y   (960 - HUD_BL_H - HUD_MARGIN_Y)
#define HUD_BL_W   240
#define HUD_BL_CUT  12

#define HUD_BR_W   210
#define HUD_BR_H    90
#define HUD_BR_X   (1280 - HUD_BR_W - HUD_MARGIN_X)
#define HUD_BR_Y   (960 - HUD_BR_H - HUD_MARGIN_Y)
#define HUD_BR_CUT  12

/* Zone-indexed accent colors [HOME, INNER, DEEP, ABYSS, DRIFT] */
extern const SDL_Color HUD_ZONE_ACCENT[5];

/* =========================================================
 * DEPRECATED ALIASES — old UI_* names for backwards compatibility.
 * Do not use in new code.  Will be removed after game.c is migrated.
 * ========================================================= */
#define UI_BLACK       HUD_VOID
#define UI_PANEL_FILL  HUD_PANEL_FILL
#define UI_CYAN        HUD_BORDER_ACTIVE
#define UI_CINNABAR    HUD_CINNABAR
#define UI_AMBER       HUD_AMBER
#define UI_GREEN       HUD_GREEN
#define UI_MAGENTA     HUD_MAGENTA
#define UI_WHITE       HUD_TEXT_PRIMARY
#define UI_GREY        ((SDL_Color){ 74,  74,  90, 255})
#define UI_STEEL       HUD_PANEL_DEEP
#define UI_RUST        ((SDL_Color){178,  34,  34, 255})
#define UI_GOLD        HUD_TEXT_GOLD

/* Zone-indexed border colors (deprecated — use HUD_ZONE_ACCENT) */
extern const SDL_Color UI_ZONE_COLORS[5];

/* =========================================================
 * PANEL PRIMITIVES
 * ========================================================= */

/**
 * @brief Draw a frosted-glass panel with a two-pass glowing border.
 *
 * The panel fill is a semi-transparent near-black with a blue tint.
 * The border is drawn as:
 *   - an outer glow pass (low alpha, +2px spread)
 *   - an inner glow pass (mid alpha, +1px spread)
 *   - the main 1px hairline at full border alpha
 *
 * @param r       SDL renderer
 * @param x,y     Top-left screen pixel
 * @param w,h     Panel dimensions in pixels
 * @param border  Glow / border color — use a palette constant
 */
void ui_panel(SDL_Renderer *r, float x, float y, float w, float h,
              SDL_Color border);

/**
 * @brief Draw an angled-cut panel with the top-left corner clipped diagonally.
 *
 * Implements the FF7R-style parallelogram panel.  The cut clips a triangle
 * of size cut×cut from the top-left corner, leaving a 6-point polygon.
 * Fill is approximated with two SDL_FRect calls (main body + top strip).
 * The border uses a three-pass glow system tracing the full 6-point outline.
 * CRT scanlines are drawn over the interior after filling.
 *
 * @param r       SDL renderer
 * @param x,y     Top-left bounding-box origin
 * @param w,h     Panel bounding-box size in pixels
 * @param cut     Diagonal cut size on each axis (typically 10–18px)
 * @param border  Border/glow color — use HUD_BORDER_MAIN or a zone accent
 */
void ui_panel_angled(SDL_Renderer *r, float x, float y, float w, float h,
                     float cut, SDL_Color border);

/**
 * @brief Draw a rectangular Qud/Noctis terminal-style panel.
 *
 * This is the unified primitive for ALL in-game UI per action-list items
 * #1 and #29 (the FF7R angled-cut aesthetic is retired in favour of a single
 * terminal look across HUD, menus, overlays, and dialogs).
 *
 * Geometry:
 *   - Axis-aligned rectangle, no diagonal cuts.
 *   - Deep near-black fill (HUD_PANEL_DEEP) for terminal contrast.
 *   - Dim continuous border (accent at low alpha) — visible but quiet.
 *   - Bright bracket-corner marks at the four corners (the Qud signature):
 *     8px L-shapes drawn in the accent color at full alpha.
 *   - Subtle CRT scanlines over the interior at very low alpha.
 *
 * Use ui_panel_menu() instead when you need a centered title divider on top.
 *
 * @param r       SDL renderer
 * @param x,y     Top-left screen pixel
 * @param w,h     Panel dimensions in pixels
 * @param accent  Primary accent color — bracket corners + title.  Pick a zone
 *                accent (HUD_ZONE_ACCENT[zone]) or HUD_BORDER_ACTIVE for hot.
 */
void ui_panel_terminal(SDL_Renderer *r, float x, float y, float w, float h,
                       SDL_Color accent);

/**
 * @brief Draw a terminal-style menu panel with a centered title header.
 *
 * Wraps ui_panel_terminal() and adds a `──── TITLE ────` centered divider
 * at the top using ui_section_divider().  Use for full-screen menu overlays
 * (pause, settings, reliquary, warp, high scores) and for in-world floating
 * dialogs that need a visible heading.
 *
 * Per action-list item #29: this REPLACES the retired FF7R-style angled
 * menu panel.  All menus across the game should converge on this primitive.
 *
 * @param r       SDL renderer
 * @param x,y     Top-left screen pixel
 * @param w,h     Panel dimensions in pixels (h must be >= 28px for header)
 * @param title   ALL-CAPS title string (NULL or "" suppresses the header)
 * @param accent  Primary accent color — borders, brackets, title text
 */
void ui_panel_menu(SDL_Renderer *r, float x, float y, float w, float h,
                   const char *title, SDL_Color accent);

/**
 * @brief Draw a `[LABEL: VALUE]` row in the unified terminal style.
 *
 * Renders three elements on one row at y:
 *   - "[" `label` "]" left-aligned at column `label_col` (HUD_TEXT_DIM).
 *   - ":" separator (optional, embedded in the label).
 *   - `value` text right-aligned at column `value_col_end` (color `value_col`).
 *
 * The brackets are drawn as text (not vector L-shapes) so they sit on the
 * vector-font baseline correctly.  Use HUD_COL_W for column alignment.
 *
 * Example call for a HUD row at (x, y, w):
 *   ui_text_label_value(r, x, y, w, "HULL", "142/200",
 *                       HUD_TEXT_DIM, HUD_TEXT_PRIMARY, 9);
 *
 * @param r        SDL renderer
 * @param x,y      Row origin (top-left)
 * @param w        Row width — the value is right-aligned within this
 * @param label    ALL-CAPS label string (rendered as `[LABEL]`)
 * @param value    Value string (typically numeric, ALL-CAPS for text)
 * @param label_c  Color for the bracketed label (typically HUD_TEXT_DIM)
 * @param value_c  Color for the value (typically HUD_TEXT_PRIMARY or accent)
 * @param size     Vector-font size in pixels (typically 9)
 */
void ui_text_label_value(SDL_Renderer *r, float x, float y, float w,
                         const char *label, const char *value,
                         SDL_Color label_c, SDL_Color value_c, int size);

/**
 * @brief Draw a small ALL-CAPS section label at the top of a panel.
 *        Renders a dim hairline separator beneath the text.
 *
 * @param r      SDL renderer
 * @param x      Left edge of the panel (label indented by 6px)
 * @param y      Top of the label row
 * @param w      Panel width (governs separator length)
 * @param label  Text string; should be ALL-CAPS per FULIGIN typography rules
 * @param color  Label text color
 */
void ui_panel_header(SDL_Renderer *r, float x, float y, float w,
                     const char *label, SDL_Color color);

/**
 * @brief Draw horizontal CRT scanlines over a rectangular region.
 *        Adds subtle depth texture to frosted-glass backgrounds.
 *        Lines are drawn every 4px at very low alpha (black, ~22/255).
 */
void ui_scanlines(SDL_Renderer *r, float x, float y, float w, float h);

/* =========================================================
 * PROGRESS BARS
 * ========================================================= */

/**
 * @brief Draw a filled progress bar: dim track, colored fill, edge glow.
 *
 * The bar has a dark track for the empty portion, the fill color for the
 * filled portion, and a bright 3px glow at the fill's leading edge.
 * The outer border is drawn at 50% alpha of the fill color.
 *
 * @param r       SDL renderer
 * @param x,y     Top-left of the bar
 * @param w,h     Width and height (h typically 8–14px)
 * @param value   Current value
 * @param maximum Full-bar value
 * @param fill    Fill color — use ui_fuel_color() or a palette constant
 */
void ui_bar(SDL_Renderer *r, float x, float y, float w, float h,
            float value, float maximum, SDL_Color fill);

/**
 * @brief Draw an ATB-style segmented progress bar divided into N equal sections.
 *
 * The total bar width is divided into segments separated by 2px gaps.
 * Each segment draws its own track, fill fraction, and leading-edge glow.
 * Segments fill left-to-right across segments (first segment fills before
 * the second begins to fill).
 *
 * @param r        SDL renderer
 * @param x,y      Top-left of the bar
 * @param w,h      Total width and height of the bar (segments share this)
 * @param value    Current value
 * @param maximum  Full-bar value
 * @param segments Number of equal segments (typically 2–4)
 * @param fill     Fill color — use a HUD_* palette constant
 */
void ui_bar_segmented(SDL_Renderer *r, float x, float y, float w, float h,
                      float value, float maximum, int segments, SDL_Color fill);

/**
 * @brief Draw a CoQ-style discrete block bar.
 *
 * Renders a row of fixed-width block rectangles.  Filled blocks use the
 * fill color; empty blocks use HUD_TRACK at reduced alpha.  Block width
 * is 5px with a 1px gap; bar height is always 8px.
 *
 * @param r          SDL renderer
 * @param x,y        Top-left of the first block
 * @param value      Current value
 * @param maximum    Full-bar value
 * @param max_blocks Maximum number of blocks to render
 * @param fill       Filled-block color — use a HUD_* palette constant
 */
void ui_bar_block(SDL_Renderer *r, float x, float y,
                  float value, float maximum, int max_blocks,
                  SDL_Color fill);

/**
 * @brief Draw a dynamic combo multiplier UI.
 *
 * Displays a dynamic combo multiplier that reads the player's kill chain state and
 * applies a random (dx, dy) jitter offset (screen shake) when the combo increases.
 *
 * @param r           SDL renderer
 * @param x,y         Top-left origin of the text
 * @param combo_count Current combo multiplier value
 * @param combo_timer Time remaining on the combo
 */
void ui_combo_multiplier(SDL_Renderer *r, float x, float y, int combo_count, float combo_timer);


/* =========================================================
 * TERMINAL MOTIFS
 * ========================================================= */

/**
 * @brief Draw a `>` selection chevron as two SDL lines.
 *
 * The chevron is 6px wide and 8px tall, drawn as a vector shape
 * rather than a text character.  Use at the left of an active list row.
 *
 * @param r      SDL renderer
 * @param x,y    Top-left origin of the chevron bounding box
 * @param color  Chevron color — typically HUD_TEXT_CYAN or HUD_AMBER
 */
void ui_cursor_chevron(SDL_Renderer *r, float x, float y, SDL_Color color);

/**
 * @brief Draw a `─── LABEL ───` section divider with centered text.
 *
 * The label is centered horizontally within [x, x+w].  Horizontal rules
 * are drawn on each side at reduced alpha.  Label text is rendered via
 * the vector stroke font at size 9.
 *
 * @param r      SDL renderer
 * @param x      Left edge of the divider span
 * @param y      Vertical position (text baseline anchor)
 * @param w      Total width of the divider span
 * @param label  ALL-CAPS label string to embed
 * @param color  Rule and label color
 */
void ui_section_divider(SDL_Renderer *r, float x, float y, float w,
                        const char *label, SDL_Color color);

/**
 * @brief Draw three `>>>` warning chevrons spaced 8px apart.
 *
 * Used for critical-state indicators (hull < 10%, fuel critical).
 * Callers should pass a pulsed color via ui_pulse() for animation.
 *
 * @param r      SDL renderer
 * @param x,y    Origin of the first chevron
 * @param color  Chevron color — typically pulsed HUD_AMBER or HUD_CINNABAR
 */
void ui_warning_chevrons(SDL_Renderer *r, float x, float y, SDL_Color color);

/* =========================================================
 * ATMOSPHERE
 * ========================================================= */

/**
 * @brief Draw a screen-edge vignette using 8 stacked alpha-blended rects.
 *
 * Darkens the top, bottom, left, and right edges of the 1280×960 screen.
 * Alpha increases toward the edge.  Call before HUD panels so vignette
 * renders beneath them.
 *
 * @param r  SDL renderer
 */
void ui_vignette(SDL_Renderer *r);

/**
 * @brief Draw a layer of slowly-drifting ambient particles over the screen.
 *        Call once per frame before all other UI elements for a background
 *        atmosphere layer.  Particles are stable across frames — positions
 *        are driven by sinusoidal oscillation of game_time.
 *
 * @param r         SDL renderer
 * @param game_time Accumulated game time in seconds; drives animation
 * @param count     Number of particles to draw (clamped to 64)
 */
void ui_particle_drift(SDL_Renderer *r, float game_time, int count);

/* =========================================================
 * COLOR UTILITIES
 * ========================================================= */

/**
 * @brief Return a semantically-correct fuel/resource bar color.
 *
 * @param pct  Percentage as 0.0–1.0
 * @return UI_GREEN (>30%), UI_AMBER (10–30%), or UI_CINNABAR (<10%)
 */
SDL_Color ui_fuel_color(float pct);

/**
 * @brief Return the border/accent color for a given zone index.
 *
 * @param zone  0 = HOME SPACE, 1 = INNER BELT, 2 = DEEP VOID, 3 = THE ABYSS
 */
SDL_Color ui_zone_color(int zone);

/**
 * @brief Return a color whose alpha is pulsed by a sine wave.
 *
 * @param base   Base color with its nominal alpha
 * @param t      Game time in seconds
 * @param hz     Pulse frequency in cycles per second
 * @param depth  Fractional modulation depth (0 = no pulse, 1 = full off/on)
 */
SDL_Color ui_pulse(SDL_Color base, float t, float hz, float depth);

/**
 * @brief Draw a bright corner-bracket decoration around a selected item.
 *        Used for the upgrade-card selection highlight.
 *
 * @param r        SDL renderer
 * @param x,y,w,h  Bounding box of the item
 * @param color    Bracket color
 * @param size     Length of each bracket arm in pixels (typically 14)
 */
void ui_corner_brackets(SDL_Renderer *r, float x, float y, float w, float h,
                         SDL_Color color, float size);

#endif /* UI_H */

/**
 * @brief Draw a radar blip (small rectangle) for the minimap.
 *
 * @param r      SDL renderer
 * @param x,y    Center of the blip
 * @param color  Blip color
 * @param size   Width and height of the blip
 */
void ui_minimap_blip(SDL_Renderer *r, float x, float y, SDL_Color color, float size);
