/* fuligin/include/game.h
 * Public API for the FULIGIN game module.
 * Declares the game lifecycle functions called from main.c.
 *
 * Author: [author]
 * Date: 2025
 */

#ifndef GAME_H
#define GAME_H

#include <SDL2/SDL.h>

/* Canonical game resolution. All rendering and layout targets these dimensions. */
#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT  960

/**
 * @brief Called once after SDL window/renderer initialisation.
 *
 * Loads high scores from disk and initialises all game state — entities,
 * timers, RNG seed, and the starting screen. Must be called before any
 * other game_* function.
 */
void game_init(void);

/**
 * @brief Dispatches an SDL event to the appropriate handler for the current
 *        game state (menu, playing, paused, game-over, etc.).
 *
 * @param event  Pointer to the SDL_Event received from the main event loop.
 */
void game_handle_input(SDL_Event *event);

/**
 * @brief Advances all game simulation by @p delta_time seconds.
 *
 * Updates player movement, projectile physics, enemy AI, collision detection,
 * and any active animations. The caller clamps @p delta_time to 0.1 s max
 * before passing it here, preventing large jumps after frame stalls.
 *
 * @param delta_time  Elapsed time since the last frame, in seconds (≤ 0.1 s).
 */
void game_update(float delta_time);

/**
 * @brief Draws all entities, the HUD, and any UI overlays for the current frame.
 *
 * Assumes the renderer has already been cleared by the caller. Does not call
 * SDL_RenderPresent — the caller is responsible for flipping the back buffer.
 */
void game_render(void);

/**
 * @brief Toggles or explicitly sets the pause state.
 *
 * When pausing, looping audio (engine hum, ambient tracks) is stopped.
 * When resuming, those streams are restarted from where they left off.
 *
 * @param paused  Non-zero to pause, zero to resume.
 */
void game_set_paused(int paused);

/**
 * @brief Called when the OS window gains or loses focus.
 *
 * Automatically pauses the game on focus-loss and resumes on focus-gain,
 * preventing runaway simulation while the window is in the background.
 *
 * @param has_focus  Non-zero if the window now has focus, zero if it lost focus.
 */
void game_focus_changed(int has_focus);

#endif /* GAME_H */
