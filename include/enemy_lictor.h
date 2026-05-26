#ifndef ENEMY_LICTOR_H
#define ENEMY_LICTOR_H

#include <SDL2/SDL.h>

/* ─── Lictor (Item 22) ────────────────────────────────────────────────────
 * Elite, aggressive interceptor saucers that pursue the Autodyne at high
 * speeds.  Where the Ascian (Item 21) is voiceless and rigid (deterministic
 * polygon patrol), the Lictor is the opposite: it actively hunts.  It
 * applies a thrust vector toward the player every frame (capped max
 * velocity, low friction) and fires a fast single-bolt aimed shot whenever
 * its cooldown elapses.
 *
 * Tactical signature: "Arrival accelerates beat" — while any Lictor is
 * alive, the global beat system compresses its tempo.  The accessor
 * lictor_alive_count() is the public hook the game loop reads to mix
 * Lictor presence into the beat-delay calculation.
 *
 * Zone gating (Zone 4+ — beyond Ascians) is the caller's responsibility
 * in game.c.  This module is pursuit-only and has no SFX of its own beyond
 * what the caller plays on hit / spawn.
 * ──────────────────────────────────────────────────────────────────────── */

#define LICTOR_MAX        2   /* up to 2 simultaneous — rarer than Ascians   */
#define LICTOR_BOLT_MAX  12   /* 2 ships × ~6 in-flight bolts at most        */

/* Initialize the pool to all inactive.  Call once during game init. */
void lictor_init(void);

/* Spawn one Lictor at (cx, cy) with an initial heading toward the player.
 * Returns slot index, or -1 if the pool is full.                         */
int  lictor_spawn(float cx, float cy, float player_x, float player_y);

/* Per-frame update: steer each Lictor toward the player with smooth
 * acceleration, clamp speed, and emit a single accurate bolt when the
 * cooldown elapses and the player is within detect range.               */
void lictor_update(float dt, float player_x, float player_y);

/* Render Lictors + active bolts.  (camera_x, camera_y) is the world-space
 * top-left corner of the screen — same calling convention as
 * rustweaver_render and ascian_render.                                  */
void lictor_render(SDL_Renderer *r, float camera_x, float camera_y);

/* Test player position against all active Lictor bolts.  Returns 1 if hit
 * (and the bolt is consumed).  Caller applies damage.                   */
int  lictor_check_player_hit(float px, float py, float hit_radius);

/* Bullet-vs-Lictor hit test.  Returns Lictor slot destroyed, or -1.     */
int  lictor_hit_test(float bx, float by);

/* Number of currently active Lictors.  Read by the beat-pulse code to
 * compress tempo while these elites are on-screen ("arrival accelerates
 * beat").                                                               */
int  lictor_alive_count(void);

#endif /* ENEMY_LICTOR_H */
