#ifndef ENEMY_ASCIAN_H
#define ENEMY_ASCIAN_H

#include <SDL2/SDL.h>

/* ─── Ascian (Item 21) ────────────────────────────────────────────────────
 * Voiceless interceptor saucers that patrol rigid geometric trajectories
 * (regular N-gon perimeter, constant angular speed, no randomness — pure
 * deterministic motion).  They never pursue the player; they only fire
 * a tight three-shot volley when the player crosses their detection
 * radius.  High-zone only — spawned by the game loop when player_zone >= 3.
 * ──────────────────────────────────────────────────────────────────────── */

#define ASCIAN_MAX        3   /* up to 3 simultaneous on screen */
#define ASCIAN_BOLT_MAX  24   /* volley projectile pool (3 × 3 × ~3 in-flight) */

/* Initialize the pool to all inactive.  Call once during game init. */
void ascian_init(void);

/* Spawn one Ascian patrolling a regular polygon centred at (cx, cy).
 *   sides  ∈ {3, 4, 5, 6}  — number of polygon vertices (clamped).
 *   radius                 — polygon circumradius in world units (≥ 60).
 * Returns slot index, or -1 if the pool is full. */
int  ascian_spawn(float cx, float cy, int sides, float radius);

/* Per-frame update: advance each Ascian deterministically along its
 * polygon path and emit a three-shot volley toward the player when
 * cooldown elapses AND the player is within detect radius. */
void ascian_update(float dt, float player_x, float player_y);

/* Render Ascians + active bolts.  (camera_x, camera_y) is the world-space
 * top-left corner of the screen — same calling convention as
 * rustweaver_render and drone_chatter_render. */
void ascian_render(SDL_Renderer *r, float camera_x, float camera_y);

/* Test player position against all active bolts.  Returns 1 if hit
 * (and the bolt is consumed).  Caller applies damage. */
int  ascian_check_player_hit(float px, float py, float hit_radius);

/* Bullet-vs-Ascian hit test.  Returns Ascian slot destroyed, or -1. */
int  ascian_hit_test(float bx, float by);

#endif /* ENEMY_ASCIAN_H */
