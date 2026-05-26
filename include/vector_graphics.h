/*
 * vector_graphics.h
 *
 * Purpose : Public interface for the FULIGIN vector-graphics renderer.
 *           Declare types and functions used to draw retro line-based shapes
 *           with phosphor persistence, chromatic aberration, screen shake,
 *           monochrome mode, and bloom effects.
 *
 * Author  : FULIGIN contributors
 * Date    : 2026-05-25
 */

#ifndef VECTOR_GRAPHICS_H
#define VECTOR_GRAPHICS_H

#include <SDL2/SDL.h>

/* ====== TYPES ====== */

/** @brief A 2-D point or vector in world space (floating-point). */
typedef struct { float x, y; } Vec2;

/** @brief A single line segment defined by two endpoints. */
typedef struct { Vec2 p1, p2; } Line;

/**
 * @brief A renderable shape: a collection of line segments with a base colour.
 *
 * All lines share the same colour; per-line colouring is not supported.
 * The lines array must remain valid for the lifetime of any draw call that
 * references this Shape.
 */
typedef struct {
    Line     *lines;
    int       line_count;
    SDL_Color color;
} Shape;

/* ====== INITIALIZATION ====== */

/**
 * @brief Initialise the vector-graphics renderer.
 *
 * Must be called once before any other vg_* function.  Creates the
 * off-screen SDL texture used for phosphor persistence.
 *
 * @param renderer  Active SDL renderer.
 * @param width     Render-target width in pixels.
 * @param height    Render-target height in pixels.
 */
void vg_init(SDL_Renderer *renderer, int width, int height);

/* ====== DRAWING ====== */

/**
 * @brief Draw a shape in world space with the given pose.
 *
 * @param shape  Shape to draw.
 * @param pos    World-space origin of the shape.
 * @param angle  Rotation in radians.
 * @param scale  Uniform scale applied before the global zoom.
 */
void vg_draw_shape(Shape *shape, Vec2 pos, float angle, float scale);

/**
 * @brief Draw a fading motion trail for a shape using a ring buffer of poses.
 *
 * @param shape       Shape definition to replicate at each historical pose.
 * @param trail_pos   Ring buffer of world-space positions, length trail_len.
 * @param trail_angle Ring buffer of rotation angles (radians), length trail_len.
 * @param trail_len   Number of slots in the ring buffers.
 * @param trail_head  Index of the most-recent entry in the ring buffers.
 * @param scale       Uniform scale applied at every trail step.
 * @param base_alpha  Starting alpha at step 0 (most recent), in [0.0, 1.0].
 * @param decay       Per-step exponential decay factor, in (0.0, 1.0).
 */
void vg_draw_shape_trail(Shape *shape, Vec2 *trail_pos, float *trail_angle,
                         int trail_len, int trail_head, float scale,
                         float base_alpha, float decay);

/* ====== EFFECTS ====== */

/**
 * @brief Apply one frame of phosphor persistence decay.
 *
 * Composites a semi-transparent black layer over the off-screen buffer.
 * Call once per frame before drawing new geometry.
 *
 * @param fade_amount  Normalised decay speed in [0.0, 1.0].
 *                     0.0 = no fade (infinite persistence).
 *                     1.0 = instant full clear (no persistence).
 */
void vg_apply_persistence(float fade_amount);

/**
 * @brief Hard-clear the off-screen persistence buffer to opaque black.
 *
 * Use for scene transitions or any situation requiring an instant cut
 * rather than the gradual phosphor decay of vg_apply_persistence().
 */
void vg_clear(void);

/**
 * @brief Set the screen-shake pixel offset for the next vg_present() call.
 *
 * Pass (0, 0) to cancel shake.
 *
 * @param dx  Horizontal shake displacement in pixels.
 * @param dy  Vertical shake displacement in pixels.
 */
void vg_set_shake(int dx, int dy);

/* ====== CAMERA AND GLOBAL SETTINGS ====== */

/**
 * @brief Set the world-space camera offset subtracted from every vertex.
 *
 * @param offset  Camera position in world space.
 */
void vg_set_camera(Vec2 offset);

/**
 * @brief Set the global zoom factor.
 *
 * @param zoom  Magnification level (1.0 = no zoom).
 */
void vg_set_zoom(float zoom);

/**
 * @brief Set the global brightness from a discrete level.
 *
 * Internally maps the integer to a [0.0, 1.0] float.
 * Level 0 = off; level 4 = full brightness.
 *
 * @param level  Integer brightness level in the range [0, 4].
 *               Note: this is an int, not a float.
 */
void vg_set_brightness(int level);

/**
 * @brief Enable or disable monochrome (greyscale) rendering.
 *
 * When enabled, shape colours are converted to luminance using ITU-R BT.601
 * weights before drawing.
 *
 * @param enable  Non-zero to enable; zero to restore colour.
 */
void vg_set_monochrome(int enable);

/**
 * @brief Set the horizontal chromatic-aberration offset in pixels.
 *
 * Splits each line into three colour-channel passes (red left, green centre,
 * blue right) to simulate CRT beam misalignment.
 * Pass 0.0f to disable the effect.
 *
 * @param strength  Pixel offset applied to the red and blue channels.
 */
void vg_set_chromatic_aberration(float strength);

/* ====== PRESENTATION ====== */

/**
 * @brief Blit the off-screen buffer to the display and flip.
 *
 * Applies the current screen-shake offset (if any) and calls
 * SDL_RenderPresent().  Must be called once at the end of each frame.
 */
void vg_present(void);

void vg_draw_hyperjump_flash(float intensity);

#endif /* VECTOR_GRAPHICS_H */
