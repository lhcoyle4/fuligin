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
 * FULIGIN COLOR PALETTE — semantic constants
 *
 * Use by role, not by hue.  The semantic comment describes intent;
 * that is what governs which constant to reach for.
 * ========================================================= */

#define UI_BLACK       ((SDL_Color){  5,   5,  12, 255})  /* Fuligin void — absolute ground            */
#define UI_PANEL_FILL  ((SDL_Color){ 10,  15,  30, 185})  /* Frosted-glass panel fill                  */
#define UI_CYAN        ((SDL_Color){  0, 255, 255, 255})  /* Player / UI active / interactive borders  */
#define UI_CINNABAR    ((SDL_Color){227,  66,  52, 255})  /* Danger / enemy / damage                   */
#define UI_AMBER       ((SDL_Color){255, 140,   0, 255})  /* Warning / fuel low / mid-danger           */
#define UI_GREEN       ((SDL_Color){ 57, 255,  20, 255})  /* Resources / XP / Chronicle / pickups      */
#define UI_MAGENTA     ((SDL_Color){255,   0, 255, 255})  /* Enemy fire / Cacogen / chaos              */
#define UI_WHITE       ((SDL_Color){240, 240, 240, 255})  /* Primary UI text / labels                  */
#define UI_GREY        ((SDL_Color){ 74,  74,  90, 255})  /* Secondary surfaces / inactive             */
#define UI_STEEL       ((SDL_Color){ 26,  26,  46, 255})  /* Deep panel layer / sub-borders            */
#define UI_RUST        ((SDL_Color){178,  34,  34, 255})  /* Background zone glow / Photic Rust        */
#define UI_GOLD        ((SDL_Color){255, 220,  80, 255})  /* Selection highlight / Noctis keyword gold */

/* Zone-indexed border colors: [0] HOME, [1] INNER, [2] DEEP, [3] ABYSS */
extern const SDL_Color UI_ZONE_COLORS[4];

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
 * PROGRESS BAR
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

/* =========================================================
 * ATMOSPHERE
 * ========================================================= */

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
 * @param size     Length of each bracket arm in pixels (typically 12)
 */
void ui_corner_brackets(SDL_Renderer *r, float x, float y, float w, float h,
                         SDL_Color color, float size);

#endif /* UI_H */
