/* enemy_lictor.c — Lictors (Item 22).
 *
 * Elite aggressive interceptor saucers.  Where the Ascian patrols a rigid
 * polygon and the Rust-Weaver drifts and spits, the Lictor *hunts*: it
 * steers toward the player at high speed with a capped velocity and
 * shallow friction so it can swing wide arcs around its target.  When
 * the player is within detect range and the cooldown elapses, the
 * Lictor fires a single fast accurate bolt.
 *
 * Design contract (FULIGIN_ACTION_LIST.md item 22 + todo.md):
 *   - "Elite, aggressive interceptor saucers that pursue the Autodyne
 *      at high speeds."
 *   - "Arrival accelerates beat" — exposed via lictor_alive_count() so
 *      the caller in game.c can compress beat_delay while any Lictor is
 *      live on the field.
 *
 * Visual: cinnabar wedge — narrow aggressive delta silhouette with a
 * single bright nose pip.  Cinnabar (#E34234) contrasts with the magenta
 * Ascians and acid-green Rust-Weavers, reading immediately as "hostile
 * predator".  Module is silent — the caller plays SFX on hit / down.
 */
#include "enemy_lictor.h"
#include "ui.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ── Tuning constants ───────────────────────────────────────────────── */

#define LIC_DETECT_RADIUS    900.0f  /* longer than Ascian — they hunt    */
#define LIC_THRUST_ACCEL     240.0f  /* world units / s^2 toward player   */
#define LIC_MAX_SPEED        260.0f  /* hard speed cap                    */
#define LIC_FRICTION           0.18f /* per second linear drag            */
#define LIC_FIRE_CD            2.4f  /* seconds between accurate bolts    */
#define LIC_FIRE_ARM_DELAY     1.2f  /* arming delay after spawn          */
#define LIC_BOLT_SPEED       340.0f  /* faster than Ascian's 260          */
#define LIC_BOLT_LIFE          3.0f
#define LIC_HIT_RADIUS        16.0f
#define LIC_BOLT_HIT_RADIUS    7.0f

/* ── Pools (file-static — no globals leak) ──────────────────────────── */

static struct {
    int   active;
    float x, y;        /* world position                                  */
    float vx, vy;      /* world velocity                                  */
    float heading;     /* facing angle in radians (smoothed toward vel)   */
    float bolt_cd;
} lic[LICTOR_MAX];

static struct {
    int   active;
    float x, y, vx, vy;
    float life;
} bolts[LICTOR_BOLT_MAX];

/* ── Helpers ────────────────────────────────────────────────────────── */

static int bolt_alloc(void)
{
    int i;
    for (i = 0; i < LICTOR_BOLT_MAX; ++i) {
        if (!bolts[i].active) return i;
    }
    return -1;
}

/* Fire a single accurate bolt from (ox, oy) toward (tx, ty). */
static void emit_aimed_shot(float ox, float oy, float tx, float ty)
{
    int   s;
    float dx = tx - ox;
    float dy = ty - oy;
    float len, ux, uy;

    s = bolt_alloc();
    if (s < 0) return;

    len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) { dx = 1.0f; dy = 0.0f; len = 1.0f; }
    ux = dx / len;
    uy = dy / len;

    bolts[s].active = 1;
    bolts[s].x  = ox;
    bolts[s].y  = oy;
    bolts[s].vx = ux * LIC_BOLT_SPEED;
    bolts[s].vy = uy * LIC_BOLT_SPEED;
    bolts[s].life = LIC_BOLT_LIFE;
}

/* ── Public API ─────────────────────────────────────────────────────── */

void lictor_init(void)
{
    int i;
    for (i = 0; i < LICTOR_MAX; ++i) {
        lic[i].active = 0;
        lic[i].x = lic[i].y = 0.0f;
        lic[i].vx = lic[i].vy = 0.0f;
        lic[i].heading = 0.0f;
        lic[i].bolt_cd = 0.0f;
    }
    for (i = 0; i < LICTOR_BOLT_MAX; ++i) {
        bolts[i].active = 0;
        bolts[i].x = bolts[i].y = bolts[i].vx = bolts[i].vy = 0.0f;
        bolts[i].life = 0.0f;
    }
}

int lictor_spawn(float cx, float cy, float player_x, float player_y)
{
    int i;
    for (i = 0; i < LICTOR_MAX; ++i) {
        if (!lic[i].active) {
            float dx = player_x - cx;
            float dy = player_y - cy;
            float len = sqrtf(dx * dx + dy * dy);
            float ux = 1.0f, uy = 0.0f;
            if (len > 0.001f) { ux = dx / len; uy = dy / len; }

            lic[i].active = 1;
            lic[i].x = cx;
            lic[i].y = cy;
            /* Initial drift velocity toward the player at ~40% max speed
             * so the first frame is already in motion — gives a strong
             * "arriving from off-screen" silhouette as the patrol begins. */
            lic[i].vx = ux * LIC_MAX_SPEED * 0.4f;
            lic[i].vy = uy * LIC_MAX_SPEED * 0.4f;
            lic[i].heading = atan2f(uy, ux);
            lic[i].bolt_cd = LIC_FIRE_ARM_DELAY;
            return i;
        }
    }
    return -1;
}

void lictor_update(float dt, float player_x, float player_y)
{
    int i;

    for (i = 0; i < LICTOR_MAX; ++i) {
        float dx, dy, len, ux, uy;
        float speed, scale;
        float target_heading, delta;

        if (!lic[i].active) continue;

        /* Steer toward player — add an acceleration vector pointing at
         * the target every frame, then clamp to max speed.  Linear drag
         * keeps the ship from drifting forever if it overshoots. */
        dx = player_x - lic[i].x;
        dy = player_y - lic[i].y;
        len = sqrtf(dx * dx + dy * dy);
        ux = 1.0f; uy = 0.0f;
        if (len > 0.001f) { ux = dx / len; uy = dy / len; }

        lic[i].vx += ux * LIC_THRUST_ACCEL * dt;
        lic[i].vy += uy * LIC_THRUST_ACCEL * dt;

        /* Linear drag */
        lic[i].vx -= lic[i].vx * LIC_FRICTION * dt;
        lic[i].vy -= lic[i].vy * LIC_FRICTION * dt;

        /* Speed clamp */
        speed = sqrtf(lic[i].vx * lic[i].vx + lic[i].vy * lic[i].vy);
        if (speed > LIC_MAX_SPEED) {
            scale = LIC_MAX_SPEED / speed;
            lic[i].vx *= scale;
            lic[i].vy *= scale;
        }

        lic[i].x += lic[i].vx * dt;
        lic[i].y += lic[i].vy * dt;

        /* Smooth heading toward instantaneous velocity vector.  Wrap
         * the shortest-arc delta into [-pi, pi] and lerp at ~6 rad/s. */
        if (speed > 1.0f) {
            target_heading = atan2f(lic[i].vy, lic[i].vx);
            delta = target_heading - lic[i].heading;
            while (delta >  (float)M_PI) delta -= 2.0f * (float)M_PI;
            while (delta < -(float)M_PI) delta += 2.0f * (float)M_PI;
            lic[i].heading += delta * (6.0f * dt > 1.0f ? 1.0f : 6.0f * dt);
        }

        /* Fire logic — only when in detect range and cooldown elapsed. */
        lic[i].bolt_cd -= dt;
        if (lic[i].bolt_cd <= 0.0f && len < LIC_DETECT_RADIUS) {
            emit_aimed_shot(lic[i].x, lic[i].y, player_x, player_y);
            lic[i].bolt_cd = LIC_FIRE_CD;
        }
    }

    for (i = 0; i < LICTOR_BOLT_MAX; ++i) {
        if (!bolts[i].active) continue;
        bolts[i].x += bolts[i].vx * dt;
        bolts[i].y += bolts[i].vy * dt;
        bolts[i].life -= dt;
        if (bolts[i].life <= 0.0f) bolts[i].active = 0;
    }
}

static void draw_lictor(SDL_Renderer *r, float cx, float cy, float heading)
{
    /* Cinnabar narrow delta — aggressive predator silhouette.  Slightly
     * larger than Ascian (predator vs patroller) at 18-px nose. */
    SDL_Color body = HUD_CINNABAR;
    float c = cosf(heading), s = sinf(heading);
    /* Local-space points before rotation: nose-leading delta.            */
    float n[5][2] = {
        {  18.0f,   0.0f },   /* nose                                     */
        {  -8.0f,  -8.0f },   /* port aft                                 */
        {  -3.0f,   0.0f },   /* inner notch                              */
        {  -8.0f,   8.0f },   /* stbd aft                                 */
        {  18.0f,   0.0f }    /* loop back to nose                        */
    };
    int k;
    float wx[5], wy[5];
    for (k = 0; k < 5; ++k) {
        wx[k] = cx + n[k][0] * c - n[k][1] * s;
        wy[k] = cy + n[k][0] * s + n[k][1] * c;
    }
    SDL_SetRenderDrawColor(r, body.r, body.g, body.b, body.a);
    SDL_RenderDrawLineF(r, wx[0], wy[0], wx[1], wy[1]);
    SDL_RenderDrawLineF(r, wx[1], wy[1], wx[2], wy[2]);
    SDL_RenderDrawLineF(r, wx[2], wy[2], wx[3], wy[3]);
    SDL_RenderDrawLineF(r, wx[3], wy[3], wx[4], wy[4]);
    /* Bright nose pip — small accent dot at the leading vertex. */
    SDL_RenderDrawPointF(r, wx[0], wy[0]);
    SDL_RenderDrawPointF(r, cx, cy);
}

static void draw_lictor_bolt(SDL_Renderer *r, float x, float y, float vx, float vy)
{
    /* Amber streak — distinct from Ascian magenta and Rust-Weaver green. */
    SDL_Color a = HUD_AMBER;
    float len = sqrtf(vx * vx + vy * vy);
    float ux = 1.0f, uy = 0.0f;
    if (len > 0.001f) { ux = vx / len; uy = vy / len; }
    SDL_SetRenderDrawColor(r, a.r, a.g, a.b, a.a);
    /* 10 px streak — slightly longer than Ascian wedge bolts. */
    SDL_RenderDrawLineF(r,
        x - ux * 5.0f, y - uy * 5.0f,
        x + ux * 5.0f, y + uy * 5.0f);
}

void lictor_render(SDL_Renderer *r, float camera_x, float camera_y)
{
    int i;
    if (!r) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (i = 0; i < LICTOR_MAX; ++i) {
        float sx, sy;
        if (!lic[i].active) continue;
        sx = lic[i].x - camera_x;
        sy = lic[i].y - camera_y;
        if (sx < -64.0f || sx > 1984.0f || sy < -64.0f || sy > 1144.0f) continue;
        draw_lictor(r, sx, sy, lic[i].heading);
    }
    for (i = 0; i < LICTOR_BOLT_MAX; ++i) {
        float sx, sy;
        if (!bolts[i].active) continue;
        sx = bolts[i].x - camera_x;
        sy = bolts[i].y - camera_y;
        if (sx < -32.0f || sx > 1952.0f || sy < -32.0f || sy > 1112.0f) continue;
        draw_lictor_bolt(r, sx, sy, bolts[i].vx, bolts[i].vy);
    }
}

int lictor_check_player_hit(float px, float py, float hit_radius)
{
    int i;
    int hit = 0;
    float r = hit_radius + LIC_BOLT_HIT_RADIUS;
    float r2 = r * r;
    for (i = 0; i < LICTOR_BOLT_MAX; ++i) {
        float dx, dy;
        if (!bolts[i].active) continue;
        dx = bolts[i].x - px;
        dy = bolts[i].y - py;
        if (dx * dx + dy * dy < r2) {
            bolts[i].active = 0;
            hit = 1;
        }
    }
    return hit;
}

int lictor_hit_test(float bx, float by)
{
    int i;
    for (i = 0; i < LICTOR_MAX; ++i) {
        float dx, dy;
        if (!lic[i].active) continue;
        dx = lic[i].x - bx;
        dy = lic[i].y - by;
        if (dx * dx + dy * dy < LIC_HIT_RADIUS * LIC_HIT_RADIUS) {
            lic[i].active = 0;
            return i;
        }
    }
    return -1;
}

int lictor_alive_count(void)
{
    int i, n = 0;
    for (i = 0; i < LICTOR_MAX; ++i) if (lic[i].active) ++n;
    return n;
}
