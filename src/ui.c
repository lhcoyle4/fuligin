/*
 * ui.c — FULIGIN Glassmorphism UI Primitives
 *
 * Implements the frosted-glass panel system, progress bars, CRT scanlines,
 * ambient particle drift, and color utilities described in ui.h.
 *
 * All drawing uses the SDL2 2D renderer.  No shader or GPU-specific paths
 * are used — the glow effect is achieved by layered alpha-blended rectangles.
 *
 * This module has NO knowledge of game state.  All values are passed as
 * arguments.  It only depends on SDL2, math.h, and vector_font.h (for
 * ui_panel_header, which renders text via the vector stroke font).
 *
 * FULIGIN — Gene Wolfe's Book of the New Sun / Jack Vance's Dying Earth
 */

#include "ui.h"
#include "vector_font.h"

#include <math.h>
#include <stdlib.h>   /* rand() for particle init */

/* =========================================================
 * CONSTANTS
 * ========================================================= */

#define UI_SCANLINE_STRIDE  4     /* pixels between scanlines */
#define UI_SCANLINE_ALPHA   22    /* scanline darkness (0-255) */
#define UI_MAX_PARTICLES    64    /* hard cap on drift particles */

/* Glow spread widths for the border pass system */
#define UI_GLOW_OUTER_SPREAD  2.0f
#define UI_GLOW_INNER_SPREAD  1.0f
#define UI_GLOW_OUTER_ALPHA   28    /* fraction of border.a for outer pass */
#define UI_GLOW_INNER_ALPHA   70    /* fraction of border.a for inner pass */

/* Screen bounds used by particle drift; matches SCREEN_WIDTH/HEIGHT in game.h */
#define UI_SCREEN_W  1280.0f
#define UI_SCREEN_H   960.0f

/* =========================================================
 * MODULE-SCOPE STATE
 * ========================================================= */

/* Particle drift positions — initialized once, then animated via game_time */
typedef struct {
    float x;      /* base X position (screen pixels) */
    float y;      /* base Y position (screen pixels) */
    float phase;  /* per-particle phase offset for oscillation */
    float amp_x;  /* oscillation amplitude on X axis */
    float amp_y;  /* oscillation amplitude on Y axis */
    float speed;  /* time multiplier for oscillation */
    float alpha;  /* base alpha (0–255) */
} Particle;

static Particle s_particles[UI_MAX_PARTICLES];
static int      s_particles_ready = 0;

/* =========================================================
 * COLOR PALETTE — zone table
 * ========================================================= */

/* Zone-indexed border colors matching the lore zones:
 *   0 = HOME SPACE (cyan), 1 = INNER BELT (blue), 2 = DEEP VOID (purple),
 *   3 = THE ABYSS (red) */
const SDL_Color UI_ZONE_COLORS[4] = {
    {  80, 255, 255, 200 },   /* HOME SPACE  — URTH CYAN     */
    { 120, 180, 255, 200 },   /* INNER BELT  — cool blue     */
    { 180,  80, 255, 200 },   /* DEEP VOID   — void purple   */
    { 255,  80,  80, 200 }    /* THE ABYSS   — Cinnabar red  */
};

/* =========================================================
 * INTERNAL HELPERS
 * ========================================================= */

/**
 * @brief Draw a non-filled rectangle outline using SDL_RenderDrawLinesF.
 *        SDL_RenderDrawRectF is available but using explicit lines lets us
 *        control exact pixel placement without half-pixel rounding artifacts.
 */
static void draw_rect_outline(SDL_Renderer *r,
                              float x, float y, float w, float h)
{
    /* 5 points: top-left → top-right → bottom-right → bottom-left → top-left */
    SDL_FPoint pts[5] = {
        { x,     y     },
        { x + w, y     },
        { x + w, y + h },
        { x,     y + h },
        { x,     y     }
    };
    SDL_RenderDrawLinesF(r, pts, 5);
}

/**
 * @brief Set renderer draw color from an SDL_Color struct.
 *        Convenience wrapper to reduce verbosity in draw calls.
 */
static void set_color(SDL_Renderer *r, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

/* =========================================================
 * PANEL PRIMITIVES
 * ========================================================= */

/**
 * @brief Draw a frosted-glass panel with a two-pass glowing border.
 *        See ui.h for full documentation.
 */
void ui_panel(SDL_Renderer *r, float x, float y, float w, float h,
              SDL_Color border)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    /* ── Frosted glass fill ─────────────────────────────── */
    {
        SDL_Color fill = { 10, 15, 30, 185 };
        set_color(r, fill);
        SDL_FRect fr = { x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f };
        SDL_RenderFillRectF(r, &fr);
    }

    /* ── CRT scanlines over fill ────────────────────────── */
    ui_scanlines(r, x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f);

    /* ── Border: outer glow pass (+2px spread, very low alpha) ── */
    {
        SDL_Color glow = { border.r, border.g, border.b, UI_GLOW_OUTER_ALPHA };
        set_color(r, glow);
        float s = UI_GLOW_OUTER_SPREAD;
        draw_rect_outline(r, x - s, y - s, w + s * 2.0f, h + s * 2.0f);
    }

    /* ── Border: inner glow pass (+1px spread, mid alpha) ── */
    {
        SDL_Color glow = { border.r, border.g, border.b, UI_GLOW_INNER_ALPHA };
        set_color(r, glow);
        float s = UI_GLOW_INNER_SPREAD;
        draw_rect_outline(r, x - s, y - s, w + s * 2.0f, h + s * 2.0f);
    }

    /* ── Border: main 1px hairline at full alpha ──────────── */
    set_color(r, border);
    draw_rect_outline(r, x, y, w, h);
}

/**
 * @brief Draw a small section-header label inside a panel with a separator.
 *        See ui.h for full documentation.
 */
void ui_panel_header(SDL_Renderer *r, float x, float y, float w,
                     const char *label, SDL_Color color)
{
    /* Label text via vector stroke font — size 9, 6px indent from panel edge */
    vf_draw_string(label, x + 6.0f, y, 9, color);

    /* Dim hairline separator below label */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, 45);
    SDL_RenderDrawLineF(r, x + 1.0f, y + 12.0f, x + w - 1.0f, y + 12.0f);
}

/**
 * @brief Draw horizontal CRT scanlines over a rectangular region.
 *        See ui.h for full documentation.
 */
void ui_scanlines(SDL_Renderer *r, float x, float y, float w, float h)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, UI_SCANLINE_ALPHA);
    for (float ly = y; ly < y + h; ly += UI_SCANLINE_STRIDE) {
        SDL_RenderDrawLineF(r, x, ly, x + w, ly);
    }
}

/* =========================================================
 * PROGRESS BAR
 * ========================================================= */

/**
 * @brief Draw a progress bar with track, fill, and leading-edge glow.
 *        See ui.h for full documentation.
 */
void ui_bar(SDL_Renderer *r, float x, float y, float w, float h,
            float value, float maximum, SDL_Color fill)
{
    float pct = (maximum > 0.0f) ? (value / maximum) : 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    if (pct < 0.0f) pct = 0.0f;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    /* ── Track (dim dark background) ──────────────────────── */
    {
        SDL_Color track = { 25, 30, 45, 200 };
        set_color(r, track);
        SDL_FRect tr = { x, y, w, h };
        SDL_RenderFillRectF(r, &tr);
    }

    /* ── Fill ─────────────────────────────────────────────── */
    if (pct > 0.005f) {
        float fill_w = w * pct;
        set_color(r, fill);
        SDL_FRect fr = { x, y, fill_w, h };
        SDL_RenderFillRectF(r, &fr);

        /* Leading-edge glow — narrow bright stripe at fill boundary */
        {
            SDL_Color glow = { fill.r, fill.g, fill.b, 130 };
            set_color(r, glow);
            float edge_x = x + fill_w - 2.0f;
            if (edge_x < x) edge_x = x;
            SDL_FRect eg = { edge_x, y - 1.0f, 4.0f, h + 2.0f };
            SDL_RenderFillRectF(r, &eg);
        }
    }

    /* ── Outer border at 50% fill alpha ────────────────────── */
    {
        SDL_Color bord = { fill.r, fill.g, fill.b, 110 };
        set_color(r, bord);
        draw_rect_outline(r, x, y, w, h);
    }
}

/* =========================================================
 * ATMOSPHERE — PARTICLE DRIFT
 * ========================================================= */

/**
 * @brief Initialize particle table with pseudo-random stable positions.
 *        Called lazily on the first ui_particle_drift() call.
 */
static void init_particles(void)
{
    /*
     * Use a fixed seed so the particle field looks the same every run.
     * The oscillation phase offsets create sufficient variety.
     */
    unsigned int seed = 0xDECADED;  /* fixed seed — deterministic particle field */
    for (int i = 0; i < UI_MAX_PARTICLES; i++) {
        /* LCG to keep it deterministic without requiring <stdlib.h> state */
        seed = seed * 1664525u + 1013904223u;
        float rx = (float)(seed & 0xFFF) / 4095.0f;
        seed = seed * 1664525u + 1013904223u;
        float ry = (float)(seed & 0xFFF) / 4095.0f;
        seed = seed * 1664525u + 1013904223u;
        float rp = (float)(seed & 0xFF) / 255.0f;
        seed = seed * 1664525u + 1013904223u;
        float ra = (float)(seed & 0xFF) / 255.0f;
        seed = seed * 1664525u + 1013904223u;
        float rs = (float)(seed & 0xFF) / 255.0f;

        s_particles[i].x     = rx * UI_SCREEN_W;
        s_particles[i].y     = ry * UI_SCREEN_H;
        s_particles[i].phase = rp * 6.2832f;   /* 0 .. 2π */
        s_particles[i].amp_x = 15.0f + ra * 35.0f;
        s_particles[i].amp_y = 10.0f + ra * 25.0f;
        s_particles[i].speed = 0.15f + rs * 0.35f;
        s_particles[i].alpha = 18.0f + ra * 38.0f;
    }
    s_particles_ready = 1;
}

/**
 * @brief Draw ambient particle drift.  See ui.h for full documentation.
 */
void ui_particle_drift(SDL_Renderer *r, float game_time, int count)
{
    if (!s_particles_ready) { init_particles(); }
    if (count > UI_MAX_PARTICLES) { count = UI_MAX_PARTICLES; }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < count; i++) {
        const Particle *p = &s_particles[i];
        float t  = game_time * p->speed + p->phase;
        float px = p->x + sinf(t)             * p->amp_x;
        float py = p->y + cosf(t * 0.7f + p->phase) * p->amp_y;

        /* Wrap within screen */
        while (px <  0.0f)       { px += UI_SCREEN_W; }
        while (px >= UI_SCREEN_W) { px -= UI_SCREEN_W; }
        while (py <  0.0f)       { py += UI_SCREEN_H; }
        while (py >= UI_SCREEN_H) { py -= UI_SCREEN_H; }

        /* Pulse alpha with slow sine wave */
        float pulse = 0.6f + 0.4f * sinf(game_time * 0.8f + p->phase);
        Uint8 a     = (Uint8)(p->alpha * pulse);
        SDL_SetRenderDrawColor(r, 0, 200, 255, a);
        SDL_RenderDrawPointF(r, px, py);
    }
}

/* =========================================================
 * COLOR UTILITIES
 * ========================================================= */

/**
 * @brief Return a fuel-state color based on percentage.
 */
SDL_Color ui_fuel_color(float pct)
{
    if (pct > 0.30f) return (SDL_Color){ 57, 255,  20, 220 };  /* UI_GREEN  */
    if (pct > 0.10f) return (SDL_Color){255, 140,   0, 220 };  /* UI_AMBER  */
    return               (SDL_Color){227,  66,  52, 240 };  /* UI_CINNABAR */
}

/**
 * @brief Return the zone accent color for zone index 0-3.
 */
SDL_Color ui_zone_color(int zone)
{
    if (zone < 0) { zone = 0; }
    if (zone > 3) { zone = 3; }
    return UI_ZONE_COLORS[zone];
}

/**
 * @brief Return a color with pulsing alpha.
 */
SDL_Color ui_pulse(SDL_Color base, float t, float hz, float depth)
{
    float wave = sinf(t * hz * 6.2832f) * 0.5f + 0.5f;   /* 0..1 */
    float modulated = (float)base.a * (1.0f - depth + depth * wave);
    if (modulated > 255.0f) { modulated = 255.0f; }
    if (modulated <   0.0f) { modulated =   0.0f; }
    return (SDL_Color){ base.r, base.g, base.b, (Uint8)modulated };
}

/**
 * @brief Draw corner-bracket selection decoration around a bounding box.
 *        Used to highlight the selected upgrade card without filling it.
 */
void ui_corner_brackets(SDL_Renderer *r, float x, float y, float w, float h,
                         SDL_Color color, float size)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_color(r, color);

    /* Clamp arm to avoid overlap on very small boxes */
    float s = size;
    if (s > w * 0.4f) { s = w * 0.4f; }
    if (s > h * 0.4f) { s = h * 0.4f; }

    float x1 = x + w, y1 = y + h;

    /* Top-left */
    SDL_RenderDrawLineF(r, x,  y,  x + s, y);
    SDL_RenderDrawLineF(r, x,  y,  x,  y + s);
    /* Top-right */
    SDL_RenderDrawLineF(r, x1 - s, y,  x1, y);
    SDL_RenderDrawLineF(r, x1, y,  x1, y + s);
    /* Bottom-left */
    SDL_RenderDrawLineF(r, x,  y1 - s, x,  y1);
    SDL_RenderDrawLineF(r, x,  y1, x + s, y1);
    /* Bottom-right */
    SDL_RenderDrawLineF(r, x1, y1 - s, x1, y1);
    SDL_RenderDrawLineF(r, x1 - s, y1, x1, y1);
}
