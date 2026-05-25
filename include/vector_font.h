/*
 * vector_font.h
 *
 * Purpose: Public interface for the vector/stroke font system.
 *          Characters are defined as sets of line segments in a normalized
 *          [0.0, 1.0] coordinate space and rendered via the vector graphics
 *          layer.
 *
 * Author:  <placeholder>
 * Date:    2026-05-25
 */

#ifndef VECTOR_FONT_H
#define VECTOR_FONT_H

#include "vector_graphics.h"

/**
 * @brief Initializes the vector font system and registers all character
 *        glyphs.  Safe to call multiple times; work is only performed on the
 *        first call.
 */
void vf_init(void);

/**
 * @brief Draws a single character centered at (x, y).
 *        Character glyph coordinates are normalized to the [0.0, 1.0] space
 *        and scaled by the provided scale factor before rendering.
 *        Unknown characters render as a fallback square; spaces are skipped.
 *
 * @param c     Character to draw (case-insensitive).
 * @param x     X coordinate of the character's top-left cell corner.
 * @param y     Y coordinate of the character's top-left cell corner.
 * @param scale Uniform scale applied to the glyph (pixels per unit).
 * @param color Stroke color.
 */
void vf_draw_char(char c, float x, float y, float scale, SDL_Color color);

/**
 * @brief Draws a null-terminated string left-aligned at (x, y).
 *        Each character cell advances by (scale * FONT_CHAR_SPACING).
 *        Character glyph coordinates are normalized to the [0.0, 1.0] space.
 *
 * @param str   Null-terminated string to draw.
 * @param x     X coordinate of the left edge of the first character.
 * @param y     Y coordinate of the top edge of the characters.
 * @param scale Uniform scale applied to each glyph.
 * @param color Stroke color.
 */
void vf_draw_string(const char *str, float x, float y, float scale, SDL_Color color);

/**
 * @brief Draws a null-terminated string horizontally centered on x.
 *        Total string width is derived from character count, scale, and the
 *        internal FONT_CHAR_SPACING constant.
 *        Character glyph coordinates are normalized to the [0.0, 1.0] space.
 *
 * @param str   Null-terminated string to draw.
 * @param x     X coordinate that the string will be centered upon.
 * @param y     Y coordinate of the top edge of the characters.
 * @param scale Uniform scale applied to each glyph.
 * @param color Stroke color.
 */
void vf_draw_string_centered(const char *str, float x, float y, float scale, SDL_Color color);

#endif /* VECTOR_FONT_H */
