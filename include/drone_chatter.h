#ifndef DRONE_CHATTER_H
#define DRONE_CHATTER_H

/*
 * Pet shield-drone chatter (FULIGIN action-list item 27)
 * ------------------------------------------------------
 * Tiny low-contrast cyan text floats above an orbital drone giving it a
 * mumbling personality.  Decoupled from drone AI logic — call
 * drone_chatter_emit_event() at world-space coordinates whenever you want
 * a one-liner to drift up and fade out.
 */

#include <SDL2/SDL.h>

#define DRONE_CHATTER_MAX     6      /* simultaneous active floats         */
#define DRONE_CHATTER_LEN    32      /* max chars per float message        */
#define DRONE_CHATTER_LIFE  2.4f     /* seconds per float lifetime         */

/* Event codes for drone_chatter_emit_event() — each picks a canned line. */
typedef enum {
    DRONE_EVT_HELP         = 0,  /* shield low (<= 30%)        */
    DRONE_EVT_TARGET_LARGE = 1,  /* nearby size-3 asteroid     */
    DRONE_EVT_TARGET_UFO   = 2,  /* nearby UFO                 */
    DRONE_EVT_SHIELD_PCT   = 3,  /* generic "SHIELD AT NN%"    */
    DRONE_EVT_COORDINATING = 4,  /* idle status                */
    DRONE_EVT_COUNT
} DroneEvent;

/* Initialize the float pool to all inactive. */
void drone_chatter_init(void);

/* Push a literal message at world position (wx, wy).  Auto-clamped to len. */
void drone_chatter_say(float wx, float wy, const char *msg);

/* Pick a canned line keyed by event code and emit it at (wx, wy). */
void drone_chatter_emit_event(float wx, float wy, DroneEvent evt);

/* For events that carry a numeric value (e.g. SHIELD_PCT), pass `value`. */
void drone_chatter_emit_value(float wx, float wy, DroneEvent evt, int value);

/* Advance fade timers.  Call once per gameplay tick. */
void drone_chatter_update(float dt);

/* Render all active floats in world space.  `camera_x,y` is the camera
 * world-space top-left so floats can be screen-positioned correctly. */
void drone_chatter_render(SDL_Renderer *r, float camera_x, float camera_y);

#endif /* DRONE_CHATTER_H */
