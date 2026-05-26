#ifndef ENEMY_RUSTWEAVER_H
#define ENEMY_RUSTWEAVER_H

#include <SDL2/SDL.h>

#define RUSTWEAVER_MAX        4
#define RUSTWEAVER_SPIT_MAX  16   /* per-drone independent corrosive bolts */

/* Initialize the pool to all inactive.  Call once during game init. */
void rustweaver_init(void);

/* Spawn one drone near (cx,cy).  Returns slot or -1 if full. */
int  rustweaver_spawn(float cx, float cy);

/* Per-frame update: drift toward player, emit corrosive spit periodically.
 * Pass player position so the drones can drift toward it.               */
void rustweaver_update(float dt, float player_x, float player_y);

/* Render drones + active spit projectiles. */
void rustweaver_render(SDL_Renderer *r);

/* Test a player position against all active corrosive spits.  Returns 1 if
 * the player is hit (and the spit is consumed).  Caller is responsible for
 * applying the "corrosion" effect — bypass shields, deduct hull directly. */
int  rustweaver_check_player_hit(float px, float py, float hit_radius);

/* Bullet-vs-drone hit test.  Returns drone slot index destroyed, or -1.   */
int  rustweaver_hit_test(float bx, float by);

#endif /* ENEMY_RUSTWEAVER_H */
