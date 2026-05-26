#ifndef ENEMY_SCAVENGER_H
#define ENEMY_SCAVENGER_H

#include <SDL2/SDL.h>

/* ─── Scavenger Probe (Item 24) ────────────────────────────────────────
 * "Leprechaun" analog from Rogue, adapted to the void.  The Scavenger
 * Probe is a small amber bow-tie silhouette that drifts in from the
 * dark, locks onto the player when they carry Void Steel, opens a
 * dashed tractor beam, and siphons units one-at-a-time across a slow
 * cadence.  Once it has stolen its fill (or the player runs dry), it
 * peels off and warps away with its loot.
 *
 * Probes do not shoot.  They are not damage threats.  They are loss
 * threats — kill them quickly or they leave with your Void Steel.
 * If killed mid-tractor, they drop their stolen cargo back at the
 * probe's last position so the player can scoop the orbs.
 *
 * Distinct from existing enemy modules:
 *   - Rust-Weaver Drone (Item 23):    corrosive damage, bypasses shields
 *   - Ascian (Item 21):               magenta volley wedges, polygon patrol
 *   - Lictor (Item 22):               cinnabar pursuit + aimed bolts
 *   - EMP Sentinel (Item 25):         purple/cyan pulse, input lockout
 *   - Scavenger Probe (Item 24):      amber, no attack — resource theft
 *
 * State machine (per probe):
 *   SEEK     drift in toward the player; switch to TRACTOR when in
 *            beam range AND the player still has Void Steel
 *   TRACTOR  hold position relative to the player; tick a 0.45 s
 *            siphon clock; each tick the module REQUESTS one unit of
 *            steel (see scavenger_update return value)
 *   ESCAPE   probe is full (3 units) OR the player is empty — peel off
 *            on a tangent vector and accelerate away from the player
 *   WARP     once far enough from the player, play a brief shrinking
 *            ring then despawn (with cargo gone — that's the loss)
 *
 * Module is silent.  game.c plays SFX on tractor-start, siphon-tick,
 * probe-down, and warp-out.
 * ──────────────────────────────────────────────────────────────────── */

#define SCAVENGER_MAX 4   /* up to 4 simultaneous probes                  */

/* Initialise the pool — call once during game init. */
void scavenger_init(void);

/* Spawn one Scavenger Probe at (cx, cy) drifting toward the player.
 * Returns the slot index, or -1 if the pool is full. */
int  scavenger_spawn(float cx, float cy, float player_x, float player_y);

/* Per-frame update.
 *  - dt:            seconds since last tick
 *  - player_x/y:    world-space player position
 *  - player_steel:  current res_void_steel count (read-only)
 *
 * Returns the number of Void Steel units the caller MUST subtract from
 * the player's inventory THIS FRAME (0 in most frames; can be >0 when
 * multiple probes finish a siphon tick on the same frame).  The module
 * has already updated its own internal cargo by the same amount, so
 * the caller's only job is to mutate res_void_steel and play SFX/floats.
 *
 * If player_steel is 0, no probe will request a siphon and all probes
 * already in TRACTOR will transition to ESCAPE. */
int  scavenger_update(float dt,
                      float player_x, float player_y,
                      int   player_steel);

/* Render all active probes + dashed tractor beams + warp rings.
 * Camera convention matches the other enemy modules: (camera_x, camera_y)
 * is the world-space top-left corner of the screen. */
void scavenger_render(SDL_Renderer *r,
                      float camera_x, float camera_y,
                      float player_x, float player_y);

/* Bullet vs probe hit-test.  Returns the slot destroyed (>=0) or -1.
 * On a hit, writes the probe's last position AND the amount of Void
 * Steel it had stolen out the *_out parameters so the caller can
 * spawn pickup orbs.  One bullet kills a probe — they're small.       */
int  scavenger_hit_test(float bx, float by,
                        int   *out_stolen,
                        float *out_drop_x,
                        float *out_drop_y);

/* Returns 1 if any probe is currently in TRACTOR state on the player.
 * Lets game.c trigger the steady "TRACTOR BEAM" warning float / SFX. */
int  scavenger_any_tractoring(void);

/* Count of currently active probes — for HUD or spawn-pacing logic. */
int  scavenger_alive_count(void);

#endif /* ENEMY_SCAVENGER_H */
