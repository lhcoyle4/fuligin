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
 * ui_panel_header and ui_section_divider, which render text via the vector
 * stroke font).
 *
 * FULIGIN — Gene Wolfe's Book of the New Sun / Jack Vance's Dying Earth
 */

#include "ui.h"
#include "vector_font.h"

#include <math.h>
#include <stdlib.h>   /* rand() for particle init */
#include <string.h>   /* strlen() for ui_section_divider label width estimate */
#include <stdio.h>    /* sprintf() for combo string formatting */
/* =========================================================
 * CONSTANTS
 * ========================================================= */

#define UI_SCANLINE_STRIDE  4     /* pixels between scanlines */
#define UI_SCANLINE_ALPHA   22    /* scanline darkness (0-255) */
#define UI_MAX_PARTICLES    64    /* hard cap on drift particles */

/* Glow spread widths for the border pass system */
#define UI_GLOW_OUTER_SPREAD  2.0f
#define UI_GLOW_INNER_SPREAD  1.0f
#define UI_GLOW_OUTER_ALPHA   22    /* very transparent outer halo (design brief §Glow) */
#define UI_GLOW_INNER_ALPHA   65    /* semi-transparent inner ring */

/* Screen bounds used by particle drift; matches SCREEN_WIDTH/HEIGHT in game.h */
#define UI_SCREEN_W  1280.0f
#define UI_SCREEN_H   960.0f

/* Block bar geometry */
#define UI_BLOCK_W    5     /* width of each block rectangle */
#define UI_BLOCK_GAP  1     /* 1px gap between blocks */
#define UI_BLOCK_H    8     /* fixed bar height for block bars */

/* Segmented bar gap */
#define UI_SEG_GAP    2     /* pixel gap between bar segments */

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
 * COLOR PALETTE — zone tables
 * ========================================================= */

/* HUD zone accent colors (new authoritative palette) */
const SDL_Color HUD_ZONE_ACCENT[5] = {
    {  0, 180, 216, 200},  /* HOME SPACE  — Noctis teal   #00b4d8 */
    {  0, 140, 255, 200},  /* INNER BELT  — cool azure            */
    {160,  80, 255, 200},  /* DEEP VOID   — void purple           */
    {227,  66,  52, 220},  /* THE ABYSS   — cinnabar red          */
    {220, 215, 190, 220},  /* DEEP DRIFT  — bone white, light bleached out */
};

/* Zone-indexed border colors (deprecated — use HUD_ZONE_ACCENT):
 *   0 = HOME SPACE (cyan), 1 = INNER BELT (blue), 2 = DEEP VOID (purple),
 *   3 = THE ABYSS (red), 4 = DEEP DRIFT (bone) */
const SDL_Color UI_ZONE_COLORS[5] = {
    {  80, 255, 255, 200 },   /* HOME SPACE  — URTH CYAN     */
    { 120, 180, 255, 200 },   /* INNER BELT  — cool blue     */
    { 180,  80, 255, 200 },   /* DEEP VOID   — void purple   */
    { 255,  80,  80, 200 },   /* THE ABYSS   — Cinnabar red  */
    { 220, 215, 190, 200 }    /* DEEP DRIFT  — bone white    */
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
 * @brief Draw an angled-cut panel with the top-left corner clipped diagonally.
 *        See ui.h for full documentation.
 */
void ui_panel_angled(SDL_Renderer *r, float x, float y, float w, float h,
                     float cut, SDL_Color border)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    /* ── Fill: two SDL_FRect calls to approximate the polygon ── */
    {
        SDL_Color fill = HUD_PANEL_FILL;
        set_color(r, fill);

        /* Main body (full width, below the cut row) */
        SDL_FRect main_body = { x, y + cut, w, h - cut };
        SDL_RenderFillRectF(r, &main_body);

        /* Top strip (right of the cut, above main body) */
        SDL_FRect top_strip = { x + cut, y, w - cut, cut };
        SDL_RenderFillRectF(r, &top_strip);
    }

    /* ── CRT scanlines over the filled interior ──────────── */
    ui_scanlines(r, x, y + cut, w, h - cut);
    ui_scanlines(r, x + cut, y, w - cut, cut);

    /* ── 6-point polygon outline — three border passes ───── */
    /* Points trace the angled polygon clockwise: */
    /*   (x+cut, y) → (x+w, y) → (x+w, y+h) → (x, y+h) → (x, y+cut) → close */

    /* Pass 1: outer halo (+OUTER_SPREAD px expand, very low alpha) */
    {
        float s = UI_GLOW_OUTER_SPREAD;
        SDL_Color glow = { border.r, border.g, border.b, UI_GLOW_OUTER_ALPHA };
        set_color(r, glow);
        SDL_FPoint pts[7] = {
            { x + cut - s,  y - s       },
            { x + w   + s,  y - s       },
            { x + w   + s,  y + h + s   },
            { x       - s,  y + h + s   },
            { x       - s,  y + cut - s },
            { x + cut - s,  y - s       },
            { x + cut - s,  y - s       },
        };
        SDL_RenderDrawLinesF(r, pts, 7);
    }

    /* Pass 2: inner glow (+INNER_SPREAD px, mid alpha) */
    {
        float s = UI_GLOW_INNER_SPREAD;
        SDL_Color glow = { border.r, border.g, border.b, UI_GLOW_INNER_ALPHA };
        set_color(r, glow);
        SDL_FPoint pts[7] = {
            { x + cut - s,  y - s       },
            { x + w   + s,  y - s       },
            { x + w   + s,  y + h + s   },
            { x       - s,  y + h + s   },
            { x       - s,  y + cut - s },
            { x + cut - s,  y - s       },
            { x + cut - s,  y - s       },
        };
        SDL_RenderDrawLinesF(r, pts, 7);
    }

    /* Pass 3: main 1px border at full border.a */
    {
        set_color(r, border);
        SDL_FPoint pts[7] = {
            { x + cut,  y       },
            { x + w,    y       },
            { x + w,    y + h   },
            { x,        y + h   },
            { x,        y + cut },
            { x + cut,  y       },
            { x + cut,  y       },
        };
        SDL_RenderDrawLinesF(r, pts, 7);

        /* Explicit diagonal cut edge */
        SDL_RenderDrawLineF(r, x + cut, y, x, y + cut);
    }
}

/**
 * @brief Draw a rectangular Qud/Noctis terminal panel.
 *        See ui.h for full documentation.
 *
 *  Visual recipe:
 *    1. Deep near-black interior fill (HUD_PANEL_DEEP).
 *    2. Subtle CRT scanlines over the interior.
 *    3. Dim continuous 1px outline in the accent color at low alpha — it
 *       defines the rectangle without dominating the eye.
 *    4. Four bright 8px L-shaped bracket-corner marks at the corners, drawn
 *       at the accent color's full alpha.  This is the Qud signature.
 *
 *  No diagonal cuts.  No multi-pass glow halo (the brackets carry the
 *  emphasis instead).  This is the unified primitive for ALL non-overlay UI
 *  per action-list items #1 and #29.
 */
void ui_panel_terminal(SDL_Renderer *r, float x, float y, float w, float h,
                       SDL_Color accent)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    /* ── Deep terminal-glass fill ───────────────────────── */
    {
        SDL_Color fill = HUD_PANEL_DEEP;
        set_color(r, fill);
        SDL_FRect fr = { x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f };
        SDL_RenderFillRectF(r, &fr);
    }

    /* ── CRT scanlines over interior ────────────────────── */
    ui_scanlines(r, x + 1.0f, y + 1.0f, w - 2.0f, h - 2.0f);

    /* ── Dim continuous border: defines the rectangle ───── */
    {
        SDL_Color dim = { accent.r, accent.g, accent.b, 70 };
        set_color(r, dim);
        draw_rect_outline(r, x, y, w, h);
    }

    /* ── Bracket-corner marks — the Qud signature ──────── */
    {
        const float A = 8.0f;       /* arm length per corner */
        set_color(r, accent);

        /* Top-left  ┌ */
        SDL_RenderDrawLineF(r, x,             y,             x + A,         y);
        SDL_RenderDrawLineF(r, x,             y,             x,             y + A);

        /* Top-right ┐ */
        SDL_RenderDrawLineF(r, x + w - A,     y,             x + w,         y);
        SDL_RenderDrawLineF(r, x + w,         y,             x + w,         y + A);

        /* Bottom-left  └ */
        SDL_RenderDrawLineF(r, x,             y + h - A,     x,             y + h);
        SDL_RenderDrawLineF(r, x,             y + h,         x + A,         y + h);

        /* Bottom-right ┘ */
        SDL_RenderDrawLineF(r, x + w,         y + h - A,     x + w,         y + h);
        SDL_RenderDrawLineF(r, x + w - A,     y + h,         x + w,         y + h);
    }
}

/**
 * @brief Draw a terminal-style menu panel with a centered title divider.
 *        See ui.h for full documentation.
 *
 *  Implementation: ui_panel_terminal() for the frame + ui_section_divider()
 *  for the `──── TITLE ────` header.  Per action-list item #29, this is the
 *  single primitive every menu overlay (pause, settings, reliquary, warp,
 *  high scores, game-over) should converge on.  The old FF7R angled-cut
 *  menu look is retired.
 */
void ui_panel_menu(SDL_Renderer *r, float x, float y, float w, float h,
                   const char *title, SDL_Color accent)
{
    ui_panel_terminal(r, x, y, w, h, accent);

    if (title && title[0]) {
        /* Title sits 8px below the top edge — clear of the corner brackets */
        ui_section_divider(r, x + 4.0f, y + 8.0f, w - 8.0f, title, accent);
    }
}

/**
 * @brief Draw a `[LABEL]  VALUE` row in the unified terminal style.
 *        See ui.h for full documentation.
 *
 *  The label is wrapped in literal `[` and `]` characters and rendered with
 *  the vector stroke font at the requested size.  The value is right-aligned
 *  inside the row by computing its pixel width with the same `strlen * 5`
 *  approximation ui_section_divider() uses (per-character width scales
 *  linearly with the requested font size relative to size 9).
 *
 *  This is the canonical row primitive for HUD readouts — score, fuel,
 *  hull, ammo, zone, level — and replaces ad-hoc vf_draw_string pairs
 *  scattered across game.c's render functions.
 */
void ui_text_label_value(SDL_Renderer *r, float x, float y, float w,
                         const char *label, const char *value,
                         SDL_Color label_c, SDL_Color value_c, int size)
{
    (void)r;  /* All drawing goes through vf_draw_string — renderer is global */

    if (!label) label = "";
    if (!value) value = "";

    /* Build "[LABEL]" into a small stack buffer */
    char lbuf[40];
    size_t n = 0;
    lbuf[n++] = '[';
    for (size_t i = 0; label[i] != '\0' && n < sizeof(lbuf) - 2; i++) {
        lbuf[n++] = label[i];
    }
    lbuf[n++] = ']';
    lbuf[n]   = '\0';

    vf_draw_string(lbuf, x, y, size, label_c);

    /* Estimate value pixel width — match ui_section_divider's convention:
     * ~5px per character at size 9, scaled linearly by size. */
    float char_w = (float)size * (5.0f / 9.0f);
    float val_w  = (float)strlen(value) * char_w;
    float val_x  = x + w - val_w;

    /* Defensive clamp: don't let the value overlap the label */
    float label_end = x + (float)n * char_w + 4.0f;
    if (val_x < label_end) val_x = label_end;

    vf_draw_string(value, val_x, y, size, value_c);
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
 * PROGRESS BARS
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

/**
 * @brief Draw an ATB-style segmented progress bar.
 *        See ui.h for full documentation.
 */
void ui_bar_segmented(SDL_Renderer *r, float x, float y, float w, float h,
                      float value, float maximum, int segments, SDL_Color fill)
{
    if (segments < 1) { segments = 1; }

    float pct = (maximum > 0.0f) ? (value / maximum) : 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    if (pct < 0.0f) pct = 0.0f;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    /* Segment width: total bar width minus (N-1) gaps, divided N ways */
    float total_gap = (float)(segments - 1) * (float)UI_SEG_GAP;
    float seg_w = (w - total_gap) / (float)segments;

    for (int i = 0; i < segments; i++) {
        float sx = x + (float)i * (seg_w + (float)UI_SEG_GAP);

        /* Value fraction that falls within this segment */
        float seg_start = (float)i       / (float)segments;
        float seg_end   = (float)(i + 1) / (float)segments;
        float seg_pct   = 0.0f;
        if (pct >= seg_end) {
            seg_pct = 1.0f;
        } else if (pct > seg_start) {
            seg_pct = (pct - seg_start) / (seg_end - seg_start);
        }

        /* ── Track ──────────────────────────────────────────── */
        {
            SDL_Color track = HUD_TRACK;
            set_color(r, track);
            SDL_FRect tr = { sx, y, seg_w, h };
            SDL_RenderFillRectF(r, &tr);
        }

        /* ── Fill ───────────────────────────────────────────── */
        if (seg_pct > 0.005f) {
            float fill_w = seg_w * seg_pct;
            set_color(r, fill);
            SDL_FRect fr = { sx, y, fill_w, h };
            SDL_RenderFillRectF(r, &fr);

            /* Leading-edge glow at fill boundary */
            {
                SDL_Color glow = { fill.r, fill.g, fill.b, 150 };
                set_color(r, glow);
                float edge_x = sx + fill_w - 2.0f;
                if (edge_x < sx) edge_x = sx;
                SDL_FRect eg = { edge_x, y - 1.0f, 3.0f, h + 2.0f };
                SDL_RenderFillRectF(r, &eg);
            }
        }

        /* ── Segment border at half fill alpha ──────────────── */
        {
            SDL_Color bord = { fill.r, fill.g, fill.b,
                               (Uint8)(fill.a / 2) };
            set_color(r, bord);
            draw_rect_outline(r, sx, y, seg_w, h);
        }

        /* ── Gap between segments (dark rect) ───────────────── */
        if (i < segments - 1) {
            SDL_Color gap_clr = { 0, 0, 0, 200 };
            set_color(r, gap_clr);
            SDL_FRect gap = { sx + seg_w, y, (float)UI_SEG_GAP, h };
            SDL_RenderFillRectF(r, &gap);
        }
    }
}

/**
 * @brief Draw a CoQ-style discrete block bar.
 *        See ui.h for full documentation.
 */
void ui_bar_block(SDL_Renderer *r, float x, float y,
                  float value, float maximum, int max_blocks,
                  SDL_Color fill)
{
    if (max_blocks < 1) { max_blocks = 1; }

    float pct = (maximum > 0.0f) ? (value / maximum) : 0.0f;
    if (pct > 1.0f) pct = 1.0f;
    if (pct < 0.0f) pct = 0.0f;

    int filled = (int)(pct * (float)max_blocks + 0.5f);
    if (filled > max_blocks) { filled = max_blocks; }

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < max_blocks; i++) {
        float bx = x + (float)i * (float)(UI_BLOCK_W + UI_BLOCK_GAP);

        if (i < filled) {
            /* Filled block */
            set_color(r, fill);
        } else {
            /* Empty block — HUD_TRACK at reduced alpha */
            SDL_Color empty = HUD_TRACK;
            empty.a = 20;
            set_color(r, empty);
        }

        SDL_FRect blk = { bx, y, (float)(UI_BLOCK_W - 1), (float)UI_BLOCK_H };
        SDL_RenderFillRectF(r, &blk);
    }
}

/**
 * @brief Draw a dynamic combo multiplier UI.
 *
 * Displays a dynamic combo multiplier that reads the player's kill chain state and
 * applies a random (dx, dy) jitter offset (screen shake) when the combo increases.
 */
void ui_combo_multiplier(SDL_Renderer *r, float x, float y, int combo_count, float combo_timer)
{
    static int s_last_combo = 0;
    static float s_jitter_x = 0.0f;
    static float s_jitter_y = 0.0f;
    static float s_jitter_time = 0.0f;

    if (combo_count > s_last_combo && combo_count > 1) {
        s_jitter_time = 0.2f; /* 200ms of shake */
        s_jitter_x = ((float)(rand() % 100) / 50.0f - 1.0f) * 6.0f;
        s_jitter_y = ((float)(rand() % 100) / 50.0f - 1.0f) * 6.0f;
    }
    s_last_combo = combo_count;

    float dx = 0.0f;
    float dy = 0.0f;

    if (s_jitter_time > 0.0f) {
        /* Approximate decay (since we don't have delta_time here, we just decrease per frame) */
        s_jitter_time -= 0.016f; 
        if (s_jitter_time < 0.0f) {
            s_jitter_time = 0.0f;
        } else {
            float env = s_jitter_time / 0.2f;
            dx = s_jitter_x * env;
            dy = s_jitter_y * env;
        }
    }

    if (combo_count > 1) {
        SDL_Color combo_col = HUD_AMBER; 
        if (combo_count >= 4) {
            combo_col = HUD_MAGENTA;
        } else if (combo_count >= 2) {
            combo_col = HUD_AMBER;
        }

        char text[32];
        sprintf(text, "COMBO x%d", combo_count);
        
        vf_draw_string(text, x + dx, y + dy, 12, combo_col);

        if (combo_timer > 0.0f) {
            /* Draw a small bar under it. Max combo time is 1.8f (COMBO_WINDOW) */
            ui_bar(r, x + dx, y + dy + 16.0f, 60.0f, 4.0f, combo_timer, 1.8f, combo_col);
        }
    } else {
        vf_draw_string("COMBO x1", x, y, 9, HUD_TEXT_DIM);
    }
}

/* =========================================================
 * TERMINAL MOTIFS
 * ========================================================= */

/**
 * @brief Draw a `>` selection chevron as two SDL lines.
 *        See ui.h for full documentation.
 */
void ui_cursor_chevron(SDL_Renderer *r, float x, float y, SDL_Color color)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    set_color(r, color);
    SDL_RenderDrawLineF(r, x,       y,     x + 6.0f, y + 4.0f);
    SDL_RenderDrawLineF(r, x + 6.0f, y + 4.0f, x,   y + 8.0f);
}

/**
 * @brief Draw a `─── LABEL ───` section divider with centered text.
 *        See ui.h for full documentation.
 */
void ui_section_divider(SDL_Renderer *r, float x, float y, float w,
                        const char *label, SDL_Color color)
{
    /* Estimate label pixel width — use vf_measure_string if available,
     * otherwise fall back to a 5px-per-character approximation. */
    float label_w;
#ifdef VF_MEASURE_STRING_AVAILABLE
    label_w = vf_measure_string(label, 9);
#else
    label_w = (float)strlen(label) * 5.0f;
#endif

    float label_x = x + (w - label_w) * 0.5f;
    float rule_y  = y + 5.0f;  /* vertical centre of the rule line */

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    /* Left rule */
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, 80);
    SDL_RenderDrawLineF(r, x + 4.0f, rule_y, label_x - 4.0f, rule_y);

    /* Label text */
    vf_draw_string(label, label_x, y, 9, color);

    /* Right rule */
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, 80);
    SDL_RenderDrawLineF(r, label_x + label_w + 4.0f, rule_y,
                           x + w - 4.0f, rule_y);
}

/**
 * @brief Draw three `>>>` warning chevrons spaced 8px apart.
 *        See ui.h for full documentation.
 */
void ui_warning_chevrons(SDL_Renderer *r, float x, float y, SDL_Color color)
{
    for (int i = 0; i < 3; i++) {
        ui_cursor_chevron(r, x + (float)i * 8.0f, y, color);
    }
}

/* =========================================================
 * ATMOSPHERE
 * ========================================================= */

/**
 * @brief Draw a screen-edge vignette using stacked alpha-blended rects.
 *        See ui.h for full documentation.
 */
void ui_vignette(SDL_Renderer *r)
{
    /* Alpha values for layers 0 (innermost) through 7 (outermost / darkest) */
    static const Uint8 depths[8] = { 0, 5, 10, 18, 28, 42, 60, 85 };

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < 8; i++) {
        SDL_SetRenderDrawColor(r, 0, 0, 0, depths[i]);

        /* Top edge — layer i is the i-th strip from the top */
        {
            SDL_FRect top = { 0.0f,
                              (float)i * 10.0f,
                              UI_SCREEN_W,
                              10.0f };
            SDL_RenderFillRectF(r, &top);
        }

        /* Bottom edge — layer i is the i-th strip from the bottom */
        {
            SDL_FRect bot = { 0.0f,
                              UI_SCREEN_H - (float)(i + 1) * 10.0f,
                              UI_SCREEN_W,
                              10.0f };
            SDL_RenderFillRectF(r, &bot);
        }

        /* Left edge — draw all 8 strips (each 10px wide) */
        {
            SDL_FRect lft = { (float)(7 - i) * 10.0f,
                              0.0f,
                              10.0f,
                              UI_SCREEN_H };
            SDL_RenderFillRectF(r, &lft);
        }

        /* Right edge */
        {
            SDL_FRect rgt = { UI_SCREEN_W - (float)(8 - i) * 10.0f,
                              0.0f,
                              10.0f,
                              UI_SCREEN_H };
            SDL_RenderFillRectF(r, &rgt);
        }
    }
}

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
        /* HUD_PARTICLE_CLR tint: {0, 200, 255, a} */
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
    if (pct > 0.30f) return HUD_GREEN;
    if (pct > 0.10f) return HUD_AMBER;
    return HUD_CINNABAR;
}

/**
 * @brief Return the zone accent color for zone index 0-4.
 *        0=HOME SPACE, 1=INNER BELT, 2=DEEP VOID, 3=THE ABYSS, 4=DEEP DRIFT.
 */
SDL_Color ui_zone_color(int zone)
{
    if (zone < 0) { zone = 0; }
    if (zone > 4) { zone = 4; }
    return HUD_ZONE_ACCENT[zone];
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

/**
 * @brief Draw a radar blip (small rectangle) for the minimap.
 *        See ui.h for full documentation.
 */
void ui_minimap_blip(SDL_Renderer *r, float x, float y, SDL_Color color, float size)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_FRect dot = { x - size * 0.5f, y - size * 0.5f, size, size };
    SDL_RenderFillRectF(r, &dot);
}
