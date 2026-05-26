/* enemy_ascian.c — Ascians (Item 21).
 *
 * Voiceless interceptor saucers patrolling rigid geometric trajectories.
 * Each Ascian walks the perimeter of a regular polygon at constant
 * angular speed.  No pursuit, no acceleration, no audio cues — they are
 * SILENT.  When the player enters detect range and the Ascian's volley
 * cooldown has elapsed, the Ascian fires a tight three-shot wedge of
 * magenta bolts toward the player's current position.
 *
 * Design contract (FULIGIN_ACTION_LIST.md item 21 + todo.md):
 *   - "Rigid geometric trajectories.  No randomness."
 *   - "Voiceless" — no SFX emitted by this module.
 *   - "High-zone only" — gating is handled by the caller in game.c.
 *
 * Visual: thin magenta triangle silhouette with a single inner pip,
 * contrasting with the rust-weaver's organic green star.
 */
#include "enemy_ascian.h"
#include "ui.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define ASC_DETECT_RADIUS   600.0f
#define ASC_PATROL_SPEED      0.06f   /* perimeter fraction per second  */
#define ASC_BOLT_SPEED      260.0f
#define ASC_BOLT_LIFE         3.5f
#define ASC_VOLLEY_CD         4.2f
#define ASC_VOLLEY_SPREAD     0.18f   /* radians between wedge bolts    */
#define ASC_HIT_RADIUS        14.0f
#define ASC_BOLT_HIT_RADIUS    6.0f

/* ── Pools (file-static — no globals leak) ──────────────────────────── */

static struct {
    int   active;
    float center_x, center_y;
    float radius;
    int   sides;        /* 3..6 */
    float phase;        /* 0..1 around perimeter */
    float bolt_cd;
    float x, y;         /* cached world position from last update */
} asc[ASCIAN_MAX];

static struct {
    int   active;
    float x, y, vx, vy;
    float life;
} bolts[ASCIAN_BOLT_MAX];

/* ── Helpers ────────────────────────────────────────────────────────── */

/* Map perimeter parameter tau (0..1) along a regular N-gon centred at
 * the origin with circumradius R to a world-relative offset (ox, oy).
 * The polygon's first vertex is at angle 0 (positive x axis) so the
 * shape is rotation-stable for rendering. */
static void polygon_point(int sides, float radius, float tau,
                          float *ox, float *oy)
{
    float seg_f;
    int   seg_i;
    float u;
    float a0, a1, x0, y0, x1, y1;

    if (sides < 3) sides = 3;
    if (sides > 6) sides = 6;
    while (tau < 0.0f) tau += 1.0f;
    while (tau >= 1.0f) tau -= 1.0f;

    seg_f = tau * (float)sides;
    seg_i = (int)seg_f;
    if (seg_i >= sides) seg_i = sides - 1;
    u     = seg_f - (float)seg_i;

    a0 = (2.0f * M_PI * (float)seg_i)        / (float)sides;
    a1 = (2.0f * M_PI * (float)(seg_i + 1))  / (float)sides;
    x0 = cosf(a0) * radius;  y0 = sinf(a0) * radius;
    x1 = cosf(a1) * radius;  y1 = sinf(a1) * radius;
    *ox = x0 + (x1 - x0) * u;
    *oy = y0 + (y1 - y0) * u;
}

static int bolt_alloc(void)
{
    int i;
    for (i = 0; i < ASCIAN_BOLT_MAX; ++i) {
        if (!bolts[i].active) return i;
    }
    return -1;
}

/* Fire a 3-bolt wedge from (ox, oy) toward (tx, ty). */
static void emit_volley(float ox, float oy, float tx, float ty)
{
    float dx = tx - ox;
    float dy = ty - oy;
    float base_ang, ang;
    int   k, s;

    if (dx * dx + dy * dy < 0.0001f) { dx = 1.0f; dy = 0.0f; }
    base_ang = atan2f(dy, dx);

    for (k = -1; k <= 1; ++k) {
        s = bolt_alloc();
        if (s < 0) return;
        ang = base_ang + (float)k * ASC_VOLLEY_SPREAD;
        bolts[s].active = 1;
        bolts[s].x  = ox;
        bolts[s].y  = oy;
        bolts[s].vx = cosf(ang) * ASC_BOLT_SPEED;
        bolts[s].vy = sinf(ang) * ASC_BOLT_SPEED;
        bolts[s].life = ASC_BOLT_LIFE;
    }
}

/* ── Public API ─────────────────────────────────────────────────────── */

void ascian_init(void)
{
    int i;
    for (i = 0; i < ASCIAN_MAX; ++i) {
        asc[i].active = 0;
        asc[i].center_x = asc[i].center_y = 0.0f;
        asc[i].radius   = 0.0f;
        asc[i].sides    = 0;
        asc[i].phase    = 0.0f;
        asc[i].bolt_cd  = 0.0f;
        asc[i].x = asc[i].y = 0.0f;
    }
    for (i = 0; i < ASCIAN_BOLT_MAX; ++i) {
        bolts[i].active = 0;
        bolts[i].x = bolts[i].y = bolts[i].vx = bolts[i].vy = 0.0f;
        bolts[i].life = 0.0f;
    }
}

int ascian_spawn(float cx, float cy, int sides, float radius)
{
    int i;
    float ox, oy;
    if (sides < 3) sides = 3;
    if (sides > 6) sides = 6;
    if (radius < 60.0f) radius = 60.0f;

    for (i = 0; i < ASCIAN_MAX; ++i) {
        if (!asc[i].active) {
            asc[i].active   = 1;
            asc[i].center_x = cx;
            asc[i].center_y = cy;
            asc[i].sides    = sides;
            asc[i].radius   = radius;
            /* Deterministic initial phase per slot — no rand() — so the
             * trajectory remains "rigid" and reproducible from a given
             * spawn point and slot. */
            asc[i].phase    = (float)i * 0.25f;
            asc[i].bolt_cd  = 1.6f;  /* short arming delay */
            polygon_point(sides, radius, asc[i].phase, &ox, &oy);
            asc[i].x = cx + ox;
            asc[i].y = cy + oy;
            return i;
        }
    }
    return -1;
}

void ascian_update(float dt, float player_x, float player_y)
{
    int i;

    for (i = 0; i < ASCIAN_MAX; ++i) {
        float ox, oy, ddx, ddy;
        if (!asc[i].active) continue;

        /* Advance perimeter parameter at constant rate. */
        asc[i].phase += ASC_PATROL_SPEED * dt;
        if (asc[i].phase >= 1.0f) asc[i].phase -= 1.0f;
        polygon_point(asc[i].sides, asc[i].radius, asc[i].phase, &ox, &oy);
        asc[i].x = asc[i].center_x + ox;
        asc[i].y = asc[i].center_y + oy;

        /* Volley logic — only fires when player is within detect radius. */
        asc[i].bolt_cd -= dt;
        ddx = player_x - asc[i].x;
        ddy = player_y - asc[i].y;
        if (asc[i].bolt_cd <= 0.0f &&
            (ddx * ddx + ddy * ddy) < ASC_DETECT_RADIUS * ASC_DETECT_RADIUS) {
            emit_volley(asc[i].x, asc[i].y, player_x, player_y);
            asc[i].bolt_cd = ASC_VOLLEY_CD;
        }
    }

    for (i = 0; i < ASCIAN_BOLT_MAX; ++i) {
        if (!bolts[i].active) continue;
        bolts[i].x += bolts[i].vx * dt;
        bolts[i].y += bolts[i].vy * dt;
        bolts[i].life -= dt;
        if (bolts[i].life <= 0.0f) bolts[i].active = 0;
    }
}

static void draw_ascian(SDL_Renderer *r, float cx, float cy, float phase)
{
    /* Magenta isoceles triangle, ~16 px tall, oriented along the polygon
     * tangent so the ship "faces" the direction of patrol. */
    SDL_Color m = HUD_MAGENTA;
    float facing = phase * 2.0f * M_PI;
    float c = cosf(facing), s = sinf(facing);
    float n[3][2] = {
        {  14.0f,  0.0f },   /* nose      */
        {  -8.0f, -7.0f },   /* port aft  */
        {  -8.0f,  7.0f }    /* stbd aft  */
    };
    int k;
    float wx[3], wy[3];
    for (k = 0; k < 3; ++k) {
        wx[k] = cx + n[k][0] * c - n[k][1] * s;
        wy[k] = cy + n[k][0] * s + n[k][1] * c;
    }
    SDL_SetRenderDrawColor(r, m.r, m.g, m.b, m.a);
    SDL_RenderDrawLineF(r, wx[0], wy[0], wx[1], wy[1]);
    SDL_RenderDrawLineF(r, wx[1], wy[1], wx[2], wy[2]);
    SDL_RenderDrawLineF(r, wx[2], wy[2], wx[0], wy[0]);
    /* Inner core pip — a single bright point. */
    SDL_RenderDrawPointF(r, cx, cy);
}

static void draw_bolt(SDL_Renderer *r, float x, float y, float vx, float vy)
{
    SDL_Color m = HUD_MAGENTA;
    float len = sqrtf(vx * vx + vy * vy);
    float ux = 1.0f, uy = 0.0f;
    if (len > 0.001f) { ux = vx / len; uy = vy / len; }
    SDL_SetRenderDrawColor(r, m.r, m.g, m.b, m.a);
    /* Short 8 px streak in the direction of travel. */
    SDL_RenderDrawLineF(r,
        x - ux * 4.0f, y - uy * 4.0f,
        x + ux * 4.0f, y + uy * 4.0f);
}

void ascian_render(SDL_Renderer *r, float camera_x, float camera_y)
{
    int i;
    if (!r) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (i = 0; i < ASCIAN_MAX; ++i) {
        float sx, sy;
        if (!asc[i].active) continue;
        sx = asc[i].x - camera_x;
        sy = asc[i].y - camera_y;
        if (sx < -64.0f || sx > 1984.0f || sy < -64.0f || sy > 1144.0f) continue;
        draw_ascian(r, sx, sy, asc[i].phase);
    }
    for (i = 0; i < ASCIAN_BOLT_MAX; ++i) {
        float sx, sy;
        if (!bolts[i].active) continue;
        sx = bolts[i].x - camera_x;
        sy = bolts[i].y - camera_y;
        if (sx < -32.0f || sx > 1952.0f || sy < -32.0f || sy > 1112.0f) continue;
        draw_bolt(r, sx, sy, bolts[i].vx, bolts[i].vy);
    }
}

int ascian_check_player_hit(float px, float py, float hit_radius)
{
    int i;
    int hit = 0;
    float r = hit_radius + ASC_BOLT_HIT_RADIUS;
    float r2 = r * r;
    for (i = 0; i < ASCIAN_BOLT_MAX; ++i) {
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

int ascian_hit_test(float bx, float by)
{
    int i;
    for (i = 0; i < ASCIAN_MAX; ++i) {
        float dx, dy;
        if (!asc[i].active) continue;
        dx = asc[i].x - bx;
        dy = asc[i].y - by;
        if (dx * dx + dy * dy < ASC_HIT_RADIUS * ASC_HIT_RADIUS) {
            asc[i].active = 0;
            return i;
        }
    }
    return -1;
}
