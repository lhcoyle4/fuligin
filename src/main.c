/* fuligin/src/main.c
 * Entry point for FULIGIN — an arcade space shooter set in the dying light of Urth.
 * Initializes SDL2, creates the window and renderer, then runs the main game loop.
 *
 * Author: [author]
 * Date: 2025
 */

#include <SDL2/SDL.h>
#include "vector_graphics.h"
#include "game.h"
#include "audio.h"
#include <stdlib.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

SDL_Window   *g_window   = NULL;
SDL_Renderer *g_renderer = NULL;
Uint32        last_ticks;
int           running = 1;

/**
 * @brief Processes one iteration of the game loop: poll events, advance simulation, render.
 *
 * Called directly in the native build loop, or registered as the Emscripten callback
 * for the browser's requestAnimationFrame-based scheduler.
 */
void main_loop(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = 0;
            /* On the WebAssembly target the browser owns the loop;
             * cancelling here hands control back to the JS runtime cleanly. */
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
        } else if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                game_set_paused(1);
            }
        } else {
            game_handle_input(&event);
        }
    }

    Uint32 current_ticks = SDL_GetTicks();
    float dt = (current_ticks - last_ticks) / 1000.0f;
    last_ticks = current_ticks;

    /* Cap dt so that a long frame spike (debugger pause, OS jank, etc.) does not
     * feed an enormous timestep into the physics integrator and send objects flying. */
    if (dt > 0.1f) dt = 0.1f;

    game_update(dt);
    game_render();
}

/**
 * @brief Application entry point. Brings up SDL2 subsystems, creates the window and
 *        hardware-accelerated renderer, seeds the RNG, then drives the game loop until
 *        the player quits.
 */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    srand((unsigned int)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        SDL_Log("SDL could not initialize! SDL_Error: %s", SDL_GetError());
        return 1;
    }

    g_window = SDL_CreateWindow(
        "FULIGIN",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!g_window) {
        SDL_Log("Window could not be created! SDL_Error: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    g_renderer = SDL_CreateRenderer(
        g_window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE
    );

    if (!g_renderer) {
        SDL_Log("Renderer could not be created! SDL_Error: %s", SDL_GetError());
        SDL_DestroyWindow(g_window);
        SDL_Quit();
        return 1;
    }

    if (!audio_init()) {
        SDL_Log("Warning: Audio system failed to initialize.");
    }

    vg_init(g_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    vg_clear();
    game_init();

    last_ticks = SDL_GetTicks();

    /* On the WebAssembly/browser target Emscripten replaces the blocking while-loop
     * with its own scheduler tied to requestAnimationFrame, keeping the browser tab
     * responsive and honouring the display refresh rate. */
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    while (running) {
        main_loop();
    }
#endif

    audio_cleanup();
    SDL_DestroyRenderer(g_renderer);
    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return 0;
}
