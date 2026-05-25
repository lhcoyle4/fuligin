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
#include <stdint.h>

/* Canonical game resolution. All rendering and layout targets these dimensions. */
#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT  960

/* =========================================================================
 * SETTINGS-RELATED ENUMS
 * ========================================================================= */

typedef enum {
    COLORBLIND_NONE = 0,
    COLORBLIND_DEUTERANOPIA,
    COLORBLIND_PROTANOPIA,
    COLORBLIND_TRITANOPIA,
    COLORBLIND_COUNT
} ColorblindMode;

typedef enum {
    CROSSHAIR_NONE = 0,
    CROSSHAIR_DOT,
    CROSSHAIR_CROSS,
    CROSSHAIR_CHEVRON,
    CROSSHAIR_COUNT
} CrosshairStyle;

/* NOTE: game.c defines its own internal Difficulty enum with DIFFICULTY_* names.
 * The Settings struct uses a separate Diff enum to avoid collision. */
typedef enum {
    DIFF_AUTODYNE = 0,
    DIFF_STANDARD,
    DIFF_CACOGEN,
    DIFF_COUNT
} Diff;

/* =========================================================================
 * SETTINGS STRUCTS
 * ========================================================================= */

typedef struct {
    int  fullscreen;
    int  resolution_idx;
    int  vsync;
    int  refresh_rate;
} VideoSettings;

typedef struct {
    int   glow_intensity;
    int   particle_count;
    int   scanlines;
    int   vignette;
    int   motion_blur;
    int   screen_shake;
} GraphicsSettings;

typedef struct {
    int   master_vol;
    int   music_vol;
    int   sfx_vol;
    int   ui_vol;
    int   streamer_mode;
} AudioSettings;

typedef struct {
    int   mouse_aim;
    int   mouse_sensitivity;
    int   autofire;
    int   aim_assist;
    int   ctrl_deadzone;
    int   invert_y;
    /* Mouse button bindings: stores SDL_BUTTON_* values (1=Left, 2=Mid, 3=Right).
     * Defaults: fire=Left, thrust=Right, hyperspace=Middle. */
    int   mouse_fire_btn;
    int   mouse_thrust_btn;
    int   mouse_hyper_btn;
} ControlsSettings;

typedef struct {
    int            show_fps;
    int            show_minimap;
    CrosshairStyle crosshair;
    int            hud_scale;
    int            show_combo;
    int            show_zone_name;
} HUDSettings;

typedef struct {
    ColorblindMode colorblind;
    int            high_contrast;
    int            font_size_delta;
    int            reduce_motion;
} AccessibilitySettings;

typedef struct {
    int  starting_lives;
    Diff difficulty;
} GameplaySettings;

typedef struct {
    uint32_t seed;
    float    asteroid_density;
    float    enemy_density;
    float    loot_multiplier;
    int      zone_sharpness;
    int      starting_zone;
} WorldGenParams;

typedef struct {
    VideoSettings       video;
    GraphicsSettings    graphics;
    AudioSettings       audio;
    ControlsSettings    controls;
    HUDSettings         hud;
    AccessibilitySettings accessibility;
    GameplaySettings    gameplay;
    WorldGenParams      world;
    int                 show_intro;
} Settings;

/* Extern declarations for the globals defined in game.c */
extern Settings  g_settings;
extern int       settings_tab;
extern int       settings_row;
extern int       settings_keybind_pending;
extern uint32_t  g_world_seed;

/* Settings function declarations */
Settings settings_defaults(void);
void     settings_save(const Settings *s);
void     settings_load(Settings *s);

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
