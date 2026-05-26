/* enemy_scavenger.c — Scavenger Probes (Item 24).
 *
 * Leprechaun analog: a small amber bow-tie probe that does not shoot.
 * It hunts the player when the player carries Void Steel, opens a
 * dashed tractor beam at close range, siphons units one at a time on
 * a slow clock, then accelerates away to warp out with its cargo.
 * Killing the probe mid-theft drops the stolen cargo back into the
 * world as recoverable orbs (game.c spawns those).
 *
 * Design contract (FULIGIN_ACTION_LIST.md item 24 + todo.md 5.4):
 *   - "Tractor-beam your collected Void Steel and attempt to warp away."
 *   - "Do not attack directly."
 *   - Loss-pressure enemy: kill them quickly or your inventory shrinks.
 *
 * Visual: amber bow-tie silhouette (two triangles sharing an apex) ~14u
 * across with a CINNABAR centre dot.  In TRACTOR, a dashed cyan beam
 * connects probe → player (alpha pulses with siphon timing).  In WARP,
 * the probe is replaced by a shrinking amber ring for ~0.6 s before
 * despawn.
 *
 * Module is silent — game.c plays SFX on tractor-start, siphon-tick,
 * probe-down, warp-out.
 */
#include "enemy_scavenger.h"
#include "ui.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ── State machine ───────────────────────────────────────────────────── */

#define SC_STATE_SEEK    0
#define SC_STATE_TRACTOR 1
#define SC_STATE_ESCAPE  2
#define SC_STATE_WARP    3

/* ── Tuning ──────────────────────────────────────────────────────────── */

#define SC_DETECT_RADIUS    1100.0f /* probes "see" the player anywhere within */
#define SC_TRACTOR_RADIUS    220.0f /* must be within this to lock the beam     */
#define SC_TRACTOR_BREAK     320.0f /* if the player out-runs this, fall to SEEK */
#define SC_SIPHON_INTERVAL    0.45f /* seconds between Void Steel siphon ticks */
#define SC_CARGO_MAX          3     /* probe escapes once it has this many     */
#define SC_THRUST_ACCEL     180.0f  /* SEEK / ESCAPE thrust (u/s²)             */
#define SC_FRICTION          0.55f  /* per-second linear drag                   */
#define SC_MAX_SPEED        260.0f  /* hard cap on probe speed                  */
#define SC_WARP_DISTANCE   1500.0f  /* once this far from player, begin WARP   */
#define SC_WARP_DURATION     0.6f   /* warp-out animation length                */
#define SC_WARP_RING_R0     22.0f   /* warp ring starting radius                */
#define SC_HIT_RADIUS       12.0f   /* bullet vs probe body                     */
#define SC_BODY_HALF         7.0f   /* half-extent of bow-tie silhouette        */

/* ── Pool (file-static — no globals leak) ───────────────────────────── */

static struct {
    int   active;
    int   state;
    float x, y;
    float vx, vy;
    float heading;          /* rendered facing angle (rad)                    */
    float siphon_t;         /* accumulates toward SC_SIPHON_INTERVAL          */
    float warp_t;           /* counts up during WARP                          */
    float beam_pulse;       /* visual phase for dashed beam shimmer           */
    int   cargo;            /* stolen Void Steel units carried                */
} probes[SCAVENGER_MAX];

/* ── Helpers ─────────────────────────────────────────────────────────── */

static void clamp_speed(int i)
{
    float s2 = probes[i].vx * probes[i].vx + probes[i].vy * probes[i].vy;
    if (s2 > SC_MAX_SPEED * SC_MAX_SPEED) {
        float s = sqrtf(s2);
        float k = SC_MAX_SPEED / s;
        probes[i].vx *= k;
        probes[i].vy *= k;
    }
}

/* ── Public API ──────────────────────────────────────────────────────── */

void scavenger_init(void)
{
    int i;
    for (i = 0; i < SCAVENGER_MAX; ++i) {
        probes[i].active = 0;
        probes[i].state  = SC_STATE_SEEK;
        probes[i].x = probes[i].y = 0.0f;
        probes[i].vx = probes[i].vy = 0.0f;
        probes[i].heading = 0.0f;
        probes[i].siphon_t = 0.0f;
        probes[i].warp_t = 0.0f;
        probes[i].beam_pulse = 0.0f;
        probes[i].cargo = 0;
    }
}

int scavenger_spawn(float cx, float cy, float player_x, float player_y)
{
    int i;
    for (i = 0; i < SCAVENGER_MAX; ++i) {
        if (!probes[i].active) {
            float dx = player_x - cx;
            float dy = player_y - cy;
            float len = sqrtf(dx * dx + dy * dy);
            float ux = 1.0f, uy = 0.0f;
            if (len > 0.001f) { ux = dx / len; uy = dy / len; }

            probes[i].active = 1;
            probes[i].state  = SC_STATE_SEEK;
            probes[i].x = cx;
            probes[i].y = cy;
            /* Start with a small kick toward the player so the entry
             * silhouette reads "arriving from off-screen". */
            probes[i].vx = ux * SC_MAX_SPEED * 0.35f;
            probes[i].vy = uy * SC_MAX_SPEED * 0.35f;
            probes[i].heading = atan2f(uy, ux);
            probes[i].siphon_t = 0.0f;
            probes[i].warp_t = 0.0f;
            probes[i].beam_pulse = (float)i * 0.7f;  /* desync visuals */
            probes[i].cargo = 0;
            return i;
        }
    }
    return -1;
}

int scavenger_update(float dt,
                     float player_x, float player_y,
                     int   player_steel)
{
    int i;
    int total_siphoned = 0;

    for (i = 0; i < SCAVENGER_MAX; ++i) {
        float dx, dy, dist2, dist;
        float ux, uy;

        if (!probes[i].active) continue;

        /* Distance to player (current). */
        dx = player_x - probes[i].x;
        dy = player_y - probes[i].y;
        dist2 = dx * dx + dy * dy;
        dist  = sqrtf(dist2);

        ux = 1.0f; uy = 0.0f;
        if (dist > 0.001f) { ux = dx / dist; uy = dy / dist; }

        /* Beam-pulse phase ticks every frame for visual shimmer. */
        probes[i].beam_pulse += dt * 6.0f;

        switch (probes[i].state) {

        case SC_STATE_SEEK: {
            /* Drift toward player.  Once we're within tractor range AND
             * the player still has steel, lock the beam.  If the player
             * has no steel at all, immediately peel off to escape — no
             * point in pursuing an empty hold. */
            if (player_steel <= 0) {
                probes[i].state = SC_STATE_ESCAPE;
                probes[i].siphon_t = 0.0f;
                break;
            }
            probes[i].vx += ux * SC_THRUST_ACCEL * dt;
            probes[i].vy += uy * SC_THRUST_ACCEL * dt;
            probes[i].vx -= probes[i].vx * SC_FRICTION * dt;
            probes[i].vy -= probes[i].vy * SC_FRICTION * dt;
            clamp_speed(i);
            probes[i].x += probes[i].vx * dt;
            probes[i].y += probes[i].vy * dt;

            if (dist < SC_TRACTOR_RADIUS) {
                probes[i].state    = SC_STATE_TRACTOR;
                probes[i].siphon_t = 0.0f;
            }
            break;
        }

        case SC_STATE_TRACTOR: {
            /* Hold position relative to player by shedding speed.
             * Don't actively orbit — that's an Ascian thing.  A motionless
             * tether is more menacing because it reads "anchored leech". */
            probes[i].vx *= (1.0f - SC_FRICTION * dt * 2.0f);
            probes[i].vy *= (1.0f - SC_FRICTION * dt * 2.0f);
            probes[i].x += probes[i].vx * dt;
            probes[i].y += probes[i].vy * dt;

            /* If the player has nothing left, give up on this beam. */
            if (player_steel <= 0) {
                probes[i].state = SC_STATE_ESCAPE;
                probes[i].siphon_t = 0.0f;
                break;
            }
            /* Player outran the beam — fall back to SEEK so the probe
             * either re-locks or gives up via the empty-inventory path. */
            if (dist > SC_TRACTOR_BREAK) {
                probes[i].state = SC_STATE_SEEK;
                probes[i].siphon_t = 0.0f;
                break;
            }

            /* Siphon cadence.  Each elapsed SC_SIPHON_INTERVAL requests
             * one unit.  Cap at SC_CARGO_MAX — once we hit it, peel off. */
            probes[i].siphon_t += dt;
            if (probes[i].siphon_t >= SC_SIPHON_INTERVAL) {
                probes[i].siphon_t -= SC_SIPHON_INTERVAL;
                /* Caller will subtract from player_steel.  We optimistically
                 * record one tick here and let game.c sync; player_steel
                 * passed in is *this frame's* value, and we trust the
                 * caller to pass an updated value next frame.            */
                if (player_steel - total_siphoned > 0) {
                    probes[i].cargo += 1;
                    total_siphoned   += 1;
                }
            }

            /* Full cargo — peel off and run. */
            if (probes[i].cargo >= SC_CARGO_MAX) {
                probes[i].state = SC_STATE_ESCAPE;
                probes[i].siphon_t = 0.0f;
            }
            break;
        }

        case SC_STATE_ESCAPE: {
            /* Accelerate directly away from the player. */
            probes[i].vx -= ux * SC_THRUST_ACCEL * dt;
            probes[i].vy -= uy * SC_THRUST_ACCEL * dt;
            probes[i].vx -= probes[i].vx * SC_FRICTION * 0.4f * dt;
            probes[i].vy -= probes[i].vy * SC_FRICTION * 0.4f * dt;
            clamp_speed(i);
            probes[i].x += probes[i].vx * dt;
            probes[i].y += probes[i].vy * dt;

            if (dist > SC_WARP_DISTANCE) {
                probes[i].state  = SC_STATE_WARP;
                probes[i].warp_t = 0.0f;
            }
            break;
        }

        case SC_STATE_WARP: {
            /* Brief shrinking ring, then despawn.  Cargo is gone. */
            probes[i].warp_t += dt;
            if (probes[i].warp_t >= SC_WARP_DURATION) {
                probes[i].active = 0;
            }
            break;
        }

        default:
            probes[i].state = SC_STATE_SEEK;
            probes[i].siphon_t = 0.0f;
            break;
        }

        /* Update rendered heading toward velocity vector (when moving). */
        {
            float vs2 = probes[i].vx * probes[i].vx
                      + probes[i].vy * probes[i].vy;
            if (vs2 > 4.0f) {  /* > 2 u/s — non-zero motion */
                probes[i].heading =
                    atan2f(probes[i].vy, probes[i].vx);
            }
        }
    }
    return total_siphoned;
}

/* ── Rendering ───────────────────────────────────────────────────────── */

static void draw_bowtie(SDL_Renderer *r, float cx, float cy,
                        float heading, int tractoring)
{
    /* Two triangles sharing the centre — bow-tie silhouette.
     * Tip points along the heading; the "wings" splay perpendicular. */
    float c = cosf(heading);
    float s = sinf(heading);

    /* Local coords (along heading=+x):
     *   front-tip:  (+SC_BODY_HALF,  0)
     *   right-rear: (-SC_BODY_HALF, +SC_BODY_HALF*0.9)
     *   left-rear:  (-SC_BODY_HALF, -SC_BODY_HALF*0.9)
     *   wing-up:    (0, -SC_BODY_HALF*0.7)
     *   wing-down:  (0, +SC_BODY_HALF*0.7)                          */
    float fx = SC_BODY_HALF;
    float rx = -SC_BODY_HALF;
    float wy = SC_BODY_HALF * 0.9f;
    float ny = SC_BODY_HALF * 0.7f;

    float px[6], py[6];

    /* Helper: rotate local (lx,ly) → world (cx,cy) + R(heading)*(lx,ly). */
    #define LX(LX_,LY_,IDX_) do {                          \
        px[IDX_] = cx + (LX_) * c - (LY_) * s;             \
        py[IDX_] = cy + (LX_) * s + (LY_) * c;             \
    } while (0)
    LX( fx,  0.0f, 0);
    LX( 0.0f, -ny, 1);
    LX( 0.0f,  ny, 2);
    LX( rx,  wy,   3);
    LX( rx, -wy,   4);
    /* tip duplicated for closing the bow-tie wedge */
    LX( fx, 0.0f, 5);
    #undef LX

    {
        SDL_Color body = HUD_AMBER;
        if (tractoring) {
            /* Brighten to near-cinnabar while siphoning — visual reads
             * "actively bleeding the player". */
            body.r = 255;
            body.g = 180;
            body.b = 60;
            body.a = 240;
        }
        SDL_SetRenderDrawColor(r, body.r, body.g, body.b, body.a);

        /* Front wedge: tip → upper-wing → rear-right → tip */
        SDL_RenderDrawLineF(r, px[0], py[0], px[1], py[1]);
        SDL_RenderDrawLineF(r, px[1], py[1], px[3], py[3]);
        SDL_RenderDrawLineF(r, px[3], py[3], px[0], py[0]);

        /* Rear wedge: tip → lower-wing → rear-left → tip */
        SDL_RenderDrawLineF(r, px[0], py[0], px[2], py[2]);
        SDL_RenderDrawLineF(r, px[2], py[2], px[4], py[4]);
        SDL_RenderDrawLineF(r, px[4], py[4], px[0], py[0]);
    }

    /* Centre marker — cinnabar dot reads as "heat" / charge */
    {
        SDL_Color core = HUD_CINNABAR;
        SDL_SetRenderDrawColor(r, core.r, core.g, core.b, 220);
        SDL_RenderDrawPointF(r, cx, cy);
        SDL_RenderDrawPointF(r, cx + 1.0f, cy);
        SDL_RenderDrawPointF(r, cx, cy + 1.0f);
    }
}

static void draw_tractor_beam(SDL_Renderer *r,
                              float px_, float py_,
                              float qx,  float qy,
                              float pulse)
{
    /* Dashed cyan beam — alternating bright/dim segments along the line.
     * 8 segments total; pulse phase scrolls the bright cells along. */
    const int N = 8;
    int k;
    float dx = qx - px_;
    float dy = qy - py_;

    SDL_Color bright = HUD_TEXT_CYAN;

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    for (k = 0; k < N; ++k) {
        float t0 = (float)k       / (float)N;
        float t1 = (float)(k + 1) / (float)N;
        float x0 = px_ + dx * t0;
        float y0 = py_ + dy * t0;
        float x1 = px_ + dx * t1;
        float y1 = py_ + dy * t1;

        /* Pulse pattern: every-other cell bright, with a sliding offset. */
        int   on    = ((k + (int)pulse) & 1) == 0;
        Uint8 alpha = on ? (Uint8)200 : (Uint8)60;
        SDL_SetRenderDrawColor(r, bright.r, bright.g, bright.b, alpha);
        SDL_RenderDrawLineF(r, x0, y0, x1, y1);
    }
}

static void draw_warp_ring(SDL_Renderer *r, float cx, float cy, float t)
{
    /* t ∈ [0,1] — shrinking ring + fading alpha. */
    const int SEG = 14;
    float radius = SC_WARP_RING_R0 * (1.0f - t);
    Uint8 alpha;
    int k;
    float prev_x, prev_y;
    SDL_Color amber = HUD_AMBER;

    if (radius < 0.5f) return;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    alpha = (Uint8)(220.0f * (1.0f - t));

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, amber.r, amber.g, amber.b, alpha);

    prev_x = cx + radius;
    prev_y = cy;
    for (k = 1; k <= SEG; ++k) {
        float a  = ((float)k / (float)SEG) * 2.0f * (float)M_PI;
        float nx = cx + cosf(a) * radius;
        float ny = cy + sinf(a) * radius;
        SDL_RenderDrawLineF(r, prev_x, prev_y, nx, ny);
        prev_x = nx;
        prev_y = ny;
    }
}

void scavenger_render(SDL_Renderer *r,
                      float camera_x, float camera_y,
                      float player_x, float player_y)
{
    int i;
    if (!r) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (i = 0; i < SCAVENGER_MAX; ++i) {
        float sx, sy;

        if (!probes[i].active) continue;
        sx = probes[i].x - camera_x;
        sy = probes[i].y - camera_y;

        /* Cheap on-screen cull with a wide margin for the beam reach. */
        if (sx < -SC_TRACTOR_RADIUS - 32.0f) continue;
        if (sx >  1280.0f + SC_TRACTOR_RADIUS + 32.0f) continue;
        if (sy < -SC_TRACTOR_RADIUS - 32.0f) continue;
        if (sy >  960.0f + SC_TRACTOR_RADIUS + 32.0f) continue;

        if (probes[i].state == SC_STATE_WARP) {
            float t = probes[i].warp_t / SC_WARP_DURATION;
            draw_warp_ring(r, sx, sy, t);
            continue;
        }

        /* Tractor beam first so the body draws on top of it. */
        if (probes[i].state == SC_STATE_TRACTOR) {
            float pxs = player_x - camera_x;
            float pys = player_y - camera_y;
            draw_tractor_beam(r, sx, sy, pxs, pys,
                              probes[i].beam_pulse);
        }

        draw_bowtie(r, sx, sy, probes[i].heading,
                    probes[i].state == SC_STATE_TRACTOR);
    }
}

/* ── Hit-test ────────────────────────────────────────────────────────── */

int scavenger_hit_test(float bx, float by,
                       int   *out_stolen,
                       float *out_drop_x,
                       float *out_drop_y)
{
    int i;
    for (i = 0; i < SCAVENGER_MAX; ++i) {
        float dx, dy;
        if (!probes[i].active) continue;
        /* Don't allow hitting a warping probe — they're "phasing out". */
        if (probes[i].state == SC_STATE_WARP) continue;

        dx = probes[i].x - bx;
        dy = probes[i].y - by;
        if (dx * dx + dy * dy < SC_HIT_RADIUS * SC_HIT_RADIUS) {
            if (out_stolen)  *out_stolen  = probes[i].cargo;
            if (out_drop_x)  *out_drop_x  = probes[i].x;
            if (out_drop_y)  *out_drop_y  = probes[i].y;
            probes[i].active = 0;
            return i;
        }
    }
    return -1;
}

int scavenger_any_tractoring(void)
{
    int i;
    for (i = 0; i < SCAVENGER_MAX; ++i) {
        if (probes[i].active && probes[i].state == SC_STATE_TRACTOR) {
            return 1;
        }
    }
    return 0;
}

int scavenger_alive_count(void)
{
    int i, n = 0;
    for (i = 0; i < SCAVENGER_MAX; ++i) {
        if (probes[i].active) ++n;
    }
    return n;
}
