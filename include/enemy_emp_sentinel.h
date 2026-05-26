#ifndef ENEMY_EMP_SENTINEL_H
#define ENEMY_EMP_SENTINEL_H

#include <SDL2/SDL.h>

/* ─── EMP Sentinel (Item 25) ──────────────────────────────────────────────
 * Floating Eye analog from Rogue, adapted to the void.  The Sentinel
 * drifts in place with a small sinusoidal wobble.  When the player
 * crosses its detect radius, it enters a CHARGE state for ~1.4 s
 * (telegraphed by a brightening pulse) and then emits a brief
 * expanding EM pulse RING.  Any player inside the ring at any frame
 * during the pulse phase is EMP-locked — their steering and thrust
 * input is suppressed for a short duration so they drift, helpless,
 * through whatever else is on screen.
 *
 * Distinct from existing enemy modules:
 *   - Rust-Weaver Drone (Item 23): corrosive damage, bypasses shields
 *   - Ascian (Item 21):            magenta volley wedges, polygon patrol
 *   - Lictor (Item 22):            cinnabar pursuit + aimed bolts
 *   - EMP Sentinel (Item 25):      purple/cyan, no direct damage —
 *                                   instead, status disruption.
 *
 * Module is silent.  Game.c plays SFX on charge start, pulse emit,
 * player hit, and sentinel destruction.
 * ──────────────────────────────────────────────────────────────────────── */

#define EMP_SENTINEL_MAX  3   /* up to 3 simultaneous on the field          */

/* Initialize the pool — call once during game init. */
void emp_sentinel_init(void);

/* Spawn one EMP Sentinel at (cx, cy) drifting around that home point.
 * Returns the slot index, or -1 if the pool is full.                       */
int  emp_sentinel_spawn(float cx, float cy);

/* Per-frame update — drives the IDLE→CHARGE→PULSE→COOLDOWN cycle and
 * grows the pulse ring radius during PULSE.                                */
void emp_sentinel_update(float dt, float player_x, float player_y);

/* Render Sentinels + any active expanding pulse rings.  Same camera
 * convention as the other enemy modules: (camera_x, camera_y) is the
 * world-space top-left corner of the screen.                              */
void emp_sentinel_render(SDL_Renderer *r, float camera_x, float camera_y);

/* Returns 1 if any sentinel is currently in PULSE phase and (px, py)
 * lies inside its current ring radius.  The caller (game.c) then applies
 * the EMP-lock effect to the player by writing player.emp_lock_timer.
 * The pulse continues to expand after the hit — multiple hits within
 * the same pulse window simply re-arm the lock to its max duration.       */
int  emp_sentinel_check_player_pulse(float px, float py);

/* Bullet-vs-Sentinel hit test.  Returns the slot destroyed, or -1.
 * One bullet destroys a Sentinel (low HP — they are disruption units,
 * not bullet sponges).                                                    */
int  emp_sentinel_hit_test(float bx, float by);

/* Count currently active Sentinels — exposed for HUD or future tuning.   */
int  emp_sentinel_alive_count(void);

/* Returns 1 if any sentinel is currently CHARGING (telegraph phase) —
 * lets game.c flash a small "EMP CHARGING" warning indicator and play
 * an audible warning tone.                                                */
int  emp_sentinel_any_charging(void);

#endif /* ENEMY_EMP_SENTINEL_H */
