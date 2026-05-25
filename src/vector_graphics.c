/*
 * vector_graphics.c
 *
 * Purpose : Retro vector-graphics renderer for FULIGIN.
 *           Renders line-based shapes to an off-screen SDL texture so that
 *           the phosphor persistence effect (fade-to-black between frames)
 *           accumulates naturally.  Additional post-effects include chromatic
 *           aberration, screen shake, monochrome mode, and a two-pass bloom.
 *
 * Author  : FULIGIN contributors
 * Date    : 2026-05-25
 */

#include "vector_graphics.h"
#include <math.h>
#include <stdlib.h>

/* ====== CONSTANTS ====== */

/* ITU-R BT.601 luminance coefficients used to convert RGB to grayscale. */
#define LUMA_R  0.299f
#define LUMA_G  0.587f
#define LUMA_B  0.114f

/*
 * Alpha value (0-255) used for the secondary bloom pass lines.
 * Kept well below full opacity so the halo stays subtle.
 */
#define BLOOM_SECONDARY_ALPHA  100

/* ====== MODULE STATE ====== */

static SDL_Renderer *vg_renderer          = NULL;
static SDL_Texture  *persistence_tex      = NULL;
static int           win_w, win_h;
static Vec2          vg_camera_offset     = {0.0f, 0.0f};
static float         vg_zoom             = 1.0f;
static float         vg_brightness       = 1.0f;
static int           vg_monochrome       = 0;
static float         vg_chromatic_aberration = 0.0f;

/*
 * Screen-shake displacement applied when blitting the off-screen buffer.
 * Kept in the module-state section so they are visible alongside the
 * other globals that affect rendering.
 */
static int shake_dx = 0;
static int shake_dy = 0;

/* ====== INITIALIZATION ====== */

/**
 * @brief Initialise the vector-graphics module.
 *
 * Creates the off-screen persistence texture.  Two pixel formats are tried in
 * order (ABGR8888 then RGBA8888) for broad driver compatibility.  If both
 * fail the phosphor effect is disabled and drawing falls back to the screen
 * directly.
 *
 * @param renderer  The SDL renderer to use for all subsequent draw calls.
 * @param width     Width of the render target in pixels.
 * @param height    Height of the render target in pixels.
 */
void vg_init(SDL_Renderer *renderer, int width, int height)
{
    vg_renderer = renderer;
    win_w = width;
    win_h = height;

    /* Try ABGR8888 first — friendlier to WebGL/little-endian. */
    persistence_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                        SDL_TEXTUREACCESS_TARGET, width, height);
    if (!persistence_tex) {
        SDL_Log("vg_init: ABGR8888 target texture failed (%s), trying RGBA8888",
                SDL_GetError());
        persistence_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_TARGET, width, height);
    }

    if (persistence_tex) {
        SDL_SetTextureBlendMode(persistence_tex, SDL_BLENDMODE_BLEND);
        SDL_Log("vg_init: persistence texture created OK (%dx%d)", width, height);
    } else {
        SDL_Log("vg_init: all texture formats failed (%s) — phosphor effect disabled",
                SDL_GetError());
    }
}

/* ====== DRAWING ====== */

/**
 * @brief Set the camera world-space offset applied to every draw call.
 *
 * The offset is subtracted from each transformed vertex so the camera
 * pans the world in the opposite direction.
 *
 * @param offset  World-space translation to subtract from vertex positions.
 */
void vg_set_camera(Vec2 offset)
{
    vg_camera_offset = offset;
}

/**
 * @brief Set the global zoom factor applied on top of each shape's own scale.
 *
 * @param zoom  Multiplicative zoom level (1.0 = 1:1, 2.0 = 2× magnification).
 */
void vg_set_zoom(float zoom)
{
    vg_zoom = zoom;
}

/**
 * @brief Set the brightness level from a discrete 0-4 range.
 *
 * Converts the integer level to the internal [0.0, 1.0] float used during
 * rendering.  Level 0 = off, level 4 = full brightness.
 *
 * @param level  Integer brightness in the range [0, 4].
 */
void vg_set_brightness(int level)
{
    vg_brightness = level / 4.0f;
}

/**
 * @brief Enable or disable monochrome (greyscale) rendering.
 *
 * When enabled every shape's colour is converted to luminance using the
 * ITU-R BT.601 coefficients (LUMA_R / LUMA_G / LUMA_B) before drawing.
 *
 * @param enable  Non-zero to enable monochrome mode; zero to restore colour.
 */
void vg_set_monochrome(int enable)
{
    vg_monochrome = enable;
}

/**
 * @brief Set the horizontal chromatic-aberration offset in pixels.
 *
 * When non-zero each line segment is drawn three times: the red channel
 * shifted left, the green channel centred, and the blue channel shifted
 * right, producing a colour-fringing effect reminiscent of a misaligned
 * CRT beam.  Pass 0.0f to disable.
 *
 * @param strength  Pixel offset applied to the red and blue channels.
 */
void vg_set_chromatic_aberration(float strength)
{
    vg_chromatic_aberration = strength;
}

/**
 * @brief Draw a shape transformed by position, rotation, and scale.
 *
 * Rotates and scales each line segment in the shape, then translates it to
 * world space before applying the camera offset.
 *
 * Additive blending (SDL_BLENDMODE_ADD) is intentional: overlapping bright
 * lines saturate towards white, mimicking the glow of phosphor vector
 * displays where beam energy accumulates on the screen surface.
 *
 * When chromatic aberration is disabled, a two-pass bloom is applied:
 *   Pass 1 — full-brightness line at the exact position.
 *   Pass 2 — half-brightness, low-alpha line shifted 1 pixel on each axis,
 *             creating a soft halo without a dedicated render target.
 *
 * @param shape  Shape definition containing line segments and base colour.
 * @param pos    World-space position for the shape's origin.
 * @param angle  Rotation in radians.
 * @param scale  Uniform scale factor applied before vg_zoom.
 */
void vg_draw_shape(Shape *shape, Vec2 pos, float angle, float scale)
{
    if (persistence_tex) {
        SDL_SetRenderTarget(vg_renderer, persistence_tex);
    }

    SDL_Color color = shape->color;
    if (vg_monochrome) {
        Uint8 gray = (Uint8)(LUMA_R * color.r + LUMA_G * color.g + LUMA_B * color.b);
        color.r = gray;
        color.g = gray;
        color.b = gray;
    }

    SDL_SetRenderDrawColor(vg_renderer, color.r, color.g, color.b, color.a);

    /*
     * Additive blending gives the vector-glow look: bright lines accumulate
     * luminance on the persistence buffer just as a real phosphor dot would.
     */
    SDL_SetRenderDrawBlendMode(vg_renderer, SDL_BLENDMODE_ADD);

    float s = sinf(angle);
    float c = cosf(angle);

    for (int i = 0; i < shape->line_count; i++) {
        Line l = shape->lines[i];

        /* Rotate, scale, translate to world space, then apply camera. */
        float x1 = (l.p1.x * c - l.p1.y * s) * scale * vg_zoom + pos.x - vg_camera_offset.x;
        float y1 = (l.p1.x * s + l.p1.y * c) * scale * vg_zoom + pos.y - vg_camera_offset.y;
        float x2 = (l.p2.x * c - l.p2.y * s) * scale * vg_zoom + pos.x - vg_camera_offset.x;
        float y2 = (l.p2.x * s + l.p2.y * c) * scale * vg_zoom + pos.y - vg_camera_offset.y;

        if (vg_chromatic_aberration > 0.0f) {
            /* Red channel shifted left. */
            SDL_SetRenderDrawColor(vg_renderer, color.r, 0, 0, color.a);
            SDL_RenderDrawLine(vg_renderer,
                               (int)(x1 - vg_chromatic_aberration), (int)y1,
                               (int)(x2 - vg_chromatic_aberration), (int)y2);

            /* Green channel centred (reference position). */
            SDL_SetRenderDrawColor(vg_renderer, 0, color.g, 0, color.a);
            SDL_RenderDrawLine(vg_renderer, (int)x1, (int)y1, (int)x2, (int)y2);

            /* Blue channel shifted right. */
            SDL_SetRenderDrawColor(vg_renderer, 0, 0, color.b, color.a);
            SDL_RenderDrawLine(vg_renderer,
                               (int)(x1 + vg_chromatic_aberration), (int)y1,
                               (int)(x2 + vg_chromatic_aberration), (int)y2);
        } else {
            /*
             * Two-pass bloom technique:
             *   Pass 1 — sharp primary line at full colour.
             *   Pass 2 — half-brightness halo shifted 1 px right and 1 px down,
             *             blended at BLOOM_SECONDARY_ALPHA to stay subtle.
             * Using additive blending both passes add luminance on top of the
             * persistence buffer, so the halo never darkens the image.
             */
            SDL_SetRenderDrawColor(vg_renderer, color.r, color.g, color.b, color.a);
            SDL_RenderDrawLine(vg_renderer, (int)x1, (int)y1, (int)x2, (int)y2);

            SDL_SetRenderDrawColor(vg_renderer,
                                   color.r / 2, color.g / 2, color.b / 2,
                                   BLOOM_SECONDARY_ALPHA);
            /* 1-pixel offsets — intentionally small to keep the halo tight. */
            SDL_RenderDrawLine(vg_renderer, (int)x1 + 1, (int)y1, (int)x2 + 1, (int)y2);
            SDL_RenderDrawLine(vg_renderer, (int)x1, (int)y1 + 1, (int)x2, (int)y2 + 1);

            SDL_SetRenderDrawColor(vg_renderer, color.r, color.g, color.b, color.a);
        }
    }

    if (persistence_tex) {
        SDL_SetRenderTarget(vg_renderer, NULL);
    }
}

/**
 * @brief Draw a motion trail for a shape using a ring-buffer of past poses.
 *
 * Walks backwards through the ring buffer from trail_head, computing an
 * exponentially decaying alpha for each historical pose so that older
 * positions appear progressively more transparent.  Exits early once the
 * computed alpha falls below one LSB (1/255).
 *
 * @param shape       Shape definition to render at each trail position.
 * @param trail_pos   Ring buffer of world-space positions, length trail_len.
 * @param trail_angle Ring buffer of rotation angles (radians), length trail_len.
 * @param trail_len   Number of slots in the ring buffers.
 * @param trail_head  Index of the most-recent entry in the ring buffers.
 * @param scale       Uniform scale applied to the shape at every trail step.
 * @param base_alpha  Alpha at step 0 (most recent), in [0.0, 1.0].
 * @param decay       Per-step exponential decay multiplier, in (0.0, 1.0).
 */
void vg_draw_shape_trail(Shape *shape, Vec2 *trail_pos, float *trail_angle,
                         int trail_len, int trail_head, float scale,
                         float base_alpha, float decay)
{
    if (persistence_tex) {
        SDL_SetRenderTarget(vg_renderer, persistence_tex);
    }
    SDL_SetRenderDrawBlendMode(vg_renderer, SDL_BLENDMODE_ADD);

    SDL_Color color = shape->color;
    if (vg_monochrome) {
        Uint8 gray = (Uint8)(LUMA_R * color.r + LUMA_G * color.g + LUMA_B * color.b);
        color.r = gray;
        color.g = gray;
        color.b = gray;
    }

    for (int step = 1; step < trail_len; step++) {
        float alpha = base_alpha * powf(decay, (float)step);
        if (alpha < 1.0f / 255.0f) {
            break; /* Too transparent to be visible — skip remaining steps. */
        }

        int idx = (trail_head - step + trail_len * 256) % trail_len;
        Vec2 pos = trail_pos[idx];
        float angle = trail_angle[idx];

        /* Skip uninitialised ring-buffer slots (default-zeroed positions). */
        if (pos.x == 0.0f && pos.y == 0.0f) {
            continue;
        }

        float s = sinf(angle);
        float c = cosf(angle);
        int a = (int)(alpha * 255.0f);
        if (a > 255) {
            a = 255;
        }

        for (int i = 0; i < shape->line_count; i++) {
            Line l = shape->lines[i];
            float x1 = (l.p1.x * c - l.p1.y * s) * scale * vg_zoom + pos.x - vg_camera_offset.x;
            float y1 = (l.p1.x * s + l.p1.y * c) * scale * vg_zoom + pos.y - vg_camera_offset.y;
            float x2 = (l.p2.x * c - l.p2.y * s) * scale * vg_zoom + pos.x - vg_camera_offset.x;
            float y2 = (l.p2.x * s + l.p2.y * c) * scale * vg_zoom + pos.y - vg_camera_offset.y;

            if (vg_chromatic_aberration > 0.0f) {
                SDL_SetRenderDrawColor(vg_renderer, color.r, 0, 0, (Uint8)a);
                SDL_RenderDrawLine(vg_renderer,
                                   (int)(x1 - vg_chromatic_aberration), (int)y1,
                                   (int)(x2 - vg_chromatic_aberration), (int)y2);

                SDL_SetRenderDrawColor(vg_renderer, 0, color.g, 0, (Uint8)a);
                SDL_RenderDrawLine(vg_renderer, (int)x1, (int)y1, (int)x2, (int)y2);

                SDL_SetRenderDrawColor(vg_renderer, 0, 0, color.b, (Uint8)a);
                SDL_RenderDrawLine(vg_renderer,
                                   (int)(x1 + vg_chromatic_aberration), (int)y1,
                                   (int)(x2 + vg_chromatic_aberration), (int)y2);
            } else {
                SDL_SetRenderDrawColor(vg_renderer, color.r, color.g, color.b, (Uint8)a);
                SDL_RenderDrawLine(vg_renderer, (int)x1, (int)y1, (int)x2, (int)y2);
            }
        }
    }

    if (persistence_tex) {
        SDL_SetRenderTarget(vg_renderer, NULL);
    }
}

/* ====== EFFECTS ====== */

/*
 * Phosphor decay algorithm used by vg_apply_persistence():
 *
 * Real phosphor screens do not go dark instantly after the electron beam
 * passes — the phosphor coating continues to glow and fades exponentially.
 * We simulate this by drawing a semi-transparent black rectangle over the
 * entire persistence texture each frame.  The alpha of that rectangle
 * controls how aggressively the image fades:
 *
 *   alpha = 0   → no fade (image persists forever)
 *   alpha = 255 → instant blackout (no persistence)
 *
 * Because the persistence texture uses SDL_BLENDMODE_BLEND, the rectangle
 * composites as: dst = src_alpha * black + (1 - src_alpha) * dst,
 * which reduces each pixel's luminance by the factor (1 - fade_amount)
 * every frame — the familiar exponential tail of real phosphor decay.
 */

/**
 * @brief Apply one step of phosphor persistence decay to the off-screen buffer.
 *
 * Draws a semi-transparent black rectangle over the persistence texture.
 * Higher fade_amount = faster decay (shorter phosphor tail).
 * If no persistence texture is available, clears the screen directly so
 * that the next frame starts clean.
 *
 * @param fade_amount  Normalised fade speed in [0.0, 1.0].  Mapped to an
 *                     8-bit alpha value: 0.0 = no fade, 1.0 = full clear.
 */
void vg_apply_persistence(float fade_amount)
{
    if (!persistence_tex) {
        /* Fallback: no offscreen texture — clear each frame so draws are fresh. */
        SDL_SetRenderDrawColor(vg_renderer, 0, 0, 0, 255);
        SDL_RenderClear(vg_renderer);
        return;
    }

    SDL_SetRenderTarget(vg_renderer, persistence_tex);
    SDL_SetRenderDrawBlendMode(vg_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(vg_renderer, 0, 0, 0, (Uint8)(fade_amount * 255));
    SDL_Rect rect = {0, 0, win_w, win_h};
    SDL_RenderFillRect(vg_renderer, &rect);
    SDL_SetRenderTarget(vg_renderer, NULL);
}

/**
 * @brief Clear the off-screen persistence buffer to opaque black.
 *
 * Use this at the start of a scene transition or whenever a hard cut is
 * needed instead of the gradual phosphor fade provided by
 * vg_apply_persistence().
 */
void vg_clear(void)
{
    if (persistence_tex) {
        SDL_SetRenderTarget(vg_renderer, persistence_tex);
        SDL_SetRenderDrawColor(vg_renderer, 0, 0, 0, 255);
        SDL_RenderClear(vg_renderer);
        SDL_SetRenderTarget(vg_renderer, NULL);
    } else {
        SDL_SetRenderDrawColor(vg_renderer, 0, 0, 0, 255);
        SDL_RenderClear(vg_renderer);
    }
}

/**
 * @brief Set the screen-shake displacement for the next vg_present() call.
 *
 * The displacement is applied as a pixel offset when blitting the
 * persistence texture to the screen.  Call vg_set_shake(0, 0) to cancel.
 *
 * @param dx  Horizontal displacement in pixels.
 * @param dy  Vertical displacement in pixels.
 */
void vg_set_shake(int dx, int dy)
{
    shake_dx = dx;
    shake_dy = dy;
}

/* ====== PRESENTATION ====== */

/**
 * @brief Blit the off-screen buffer to the display and present the frame.
 *
 * If screen-shake is active the persistence texture is blitted at the
 * current (shake_dx, shake_dy) offset; otherwise it fills the window
 * exactly.  After blitting, SDL_RenderPresent() is called to flip buffers.
 *
 * If no persistence texture was created (hardware fallback) drawing went
 * directly to the screen, so only the present call is needed.
 */
void vg_present(void)
{
    if (persistence_tex) {
        /* Clear the screen before blitting so shake offsets leave black bars. */
        SDL_SetRenderDrawColor(vg_renderer, 0, 0, 0, 255);
        SDL_RenderClear(vg_renderer);

        if (shake_dx != 0 || shake_dy != 0) {
            SDL_Rect dest = {shake_dx, shake_dy, win_w, win_h};
            SDL_RenderCopy(vg_renderer, persistence_tex, NULL, &dest);
        } else {
            SDL_RenderCopy(vg_renderer, persistence_tex, NULL, NULL);
        }
    }
    /* If no persistence_tex, drawing went directly to the screen — just present. */
    SDL_RenderPresent(vg_renderer);
}
