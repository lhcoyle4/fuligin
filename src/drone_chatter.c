/*
 * src/drone_chatter.c — Pet shield-drone chatter (Item 27)
 *
 * Floats short "Share Tech Mono" mumbles above an orbital drone.  Pure
 * visual overlay: a tiny fixed-size pool, lifetimes tick down, lines
 * drift up and fade.  Renderer parameter accepted for API symmetry
 * even though drawing routes through vf_draw_string (which uses the
 * global SDL renderer internally).
 */

#include "drone_chatter.h"
#include "ui.h"             /* HUD_TEXT_CYAN */
#include "vector_font.h"    /* vf_draw_string */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- float pool ---------- */
static struct {
    int   active;
    float wx, wy;
    char  text[DRONE_CHATTER_LEN];
    float age;
} floats[DRONE_CHATTER_MAX];

/* ---------- canned lines (design brief verbatim) ---------- */
static const char *LINES_HELP[] = {
    "[HELP]",
    "[SHIELD CRITICAL]",
    "[GLITTERING SHIELDS DIM]"
};
static const char *LINES_TARGET_LARGE[] = {
    "TARGET: BIG ROCK",
    "TARGET: HEAVY STONE",
    "LARGE TARGET ACQUIRED"
};
static const char *LINES_TARGET_UFO[] = {
    "TARGET: CACOGEN",
    "HOSTILE INBOUND",
    "SAUCER IDENTIFIED"
};
static const char *LINES_COORDINATING[] = {
    "COORDINATING...",
    "ORBITAL TRIM OK",
    "STANDING BY"
};

/* ---------- init / emit ---------- */
void drone_chatter_init(void)
{
    memset(floats, 0, sizeof(floats));
}

/* Find an inactive slot; if all full, reuse the oldest (highest age). */
static int chatter_slot(void)
{
    int  oldest_idx = 0;
    float oldest    = -1.0f;
    for (int i = 0; i < DRONE_CHATTER_MAX; i++) {
        if (!floats[i].active) return i;
        if (floats[i].age > oldest) {
            oldest     = floats[i].age;
            oldest_idx = i;
        }
    }
    return oldest_idx;
}

void drone_chatter_say(float wx, float wy, const char *msg)
{
    if (!msg) return;
    int s = chatter_slot();
    floats[s].active = 1;
    floats[s].wx     = wx;
    floats[s].wy     = wy;
    floats[s].age    = 0.0f;
    snprintf(floats[s].text, DRONE_CHATTER_LEN, "%s", msg);
}

void drone_chatter_emit_event(float wx, float wy, DroneEvent evt)
{
    const char *const *bank = NULL;
    int                count = 0;

    switch (evt) {
        case DRONE_EVT_HELP:
            bank = LINES_HELP;
            count = (int)(sizeof(LINES_HELP) / sizeof(LINES_HELP[0]));
            break;
        case DRONE_EVT_TARGET_LARGE:
            bank = LINES_TARGET_LARGE;
            count = (int)(sizeof(LINES_TARGET_LARGE) / sizeof(LINES_TARGET_LARGE[0]));
            break;
        case DRONE_EVT_TARGET_UFO:
            bank = LINES_TARGET_UFO;
            count = (int)(sizeof(LINES_TARGET_UFO) / sizeof(LINES_TARGET_UFO[0]));
            break;
        case DRONE_EVT_COORDINATING:
            bank = LINES_COORDINATING;
            count = (int)(sizeof(LINES_COORDINATING) / sizeof(LINES_COORDINATING[0]));
            break;
        case DRONE_EVT_SHIELD_PCT:
            /* SHIELD_PCT needs a value — fall through to a safe default. */
            drone_chatter_say(wx, wy, "SHIELD AT ??%");
            return;
        case DRONE_EVT_COUNT:
        default:
            return;
    }

    if (!bank || count <= 0) return;
    int pick = rand() % count;
    drone_chatter_say(wx, wy, bank[pick]);
}

void drone_chatter_emit_value(float wx, float wy, DroneEvent evt, int value)
{
    if (evt == DRONE_EVT_SHIELD_PCT) {
        char buf[DRONE_CHATTER_LEN];
        if (value < 0)   value = 0;
        if (value > 999) value = 999;
        snprintf(buf, sizeof(buf), "SHIELD AT %d%%", value);
        drone_chatter_say(wx, wy, buf);
        return;
    }
    /* Other events: ignore value, route to the canned-line picker. */
    drone_chatter_emit_event(wx, wy, evt);
}

/* ---------- tick ---------- */
void drone_chatter_update(float dt)
{
    for (int i = 0; i < DRONE_CHATTER_MAX; i++) {
        if (!floats[i].active) continue;
        floats[i].age += dt;
        if (floats[i].age > DRONE_CHATTER_LIFE) {
            floats[i].active = 0;
        }
    }
}

/* ---------- render ---------- */
void drone_chatter_render(SDL_Renderer *r, float camera_x, float camera_y)
{
    (void)r;  /* vf_draw_string routes through the global renderer */

    for (int i = 0; i < DRONE_CHATTER_MAX; i++) {
        if (!floats[i].active) continue;

        float age01 = floats[i].age / DRONE_CHATTER_LIFE;
        if (age01 < 0.0f) age01 = 0.0f;
        if (age01 > 1.0f) age01 = 1.0f;

        float sx = floats[i].wx - camera_x;
        float sy = floats[i].wy - camera_y - 24.0f - (floats[i].age * 8.0f);

        /* Cull off-screen floats (generous margin for tiny glyphs). */
        if (sx < -64.0f || sx > 1984.0f) continue;
        if (sy < -32.0f || sy > 1112.0f) continue;

        /* Tiny "Share Tech Mono" tier: size 7, ~4px per char. */
        int   len   = (int)strlen(floats[i].text);
        float tw    = (float)(len * 4);

        SDL_Color col = HUD_TEXT_CYAN;
        float a = 200.0f * (1.0f - age01);
        if (a < 0.0f)   a = 0.0f;
        if (a > 255.0f) a = 255.0f;
        col.a = (Uint8)a;

        vf_draw_string(floats[i].text, sx - tw * 0.5f, sy, 7, col);
    }
}
