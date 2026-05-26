/* enemy_emp_sentinel.c — EMP Sentinels (Item 25).
 *
 * Floating-Eye analog: a stationary-ish purple sentinel that, when the
 * player approaches, charges for ~1.4 s and emits a brief expanding
 * EM-pulse ring.  Any player caught inside the ring during its short
 * active window is EMP-locked — steering and thrust input are
 * suppressed by game.c for a brief duration, leaving the ship drifting.
 *
 * Design contract (FULIGIN_ACTION_LIST.md item 25 + todo.md 5.3):
 *   - "EM pulse locks steering and thrust briefly."
 *   - "Pulse a freezing electromagnetic field, locking ship steering
 *      and thrust for a brief duration."
 *   - Floating-Eye analog from Rogue — telegraphed, area-of-effect
 *      status disruption rather than direct damage.
 *
 * Visual: HUD_PURPLE hexagonal body with a single bright cyan "pupil"
 * dot in the center.  During CHARGE: the body brightens, the pupil
 * pulses at ~6 Hz, and a faint inner glow ring appears.  During PULSE:
 * the body collapses to dim and an expanding bright purple/cyan ring
 * radiates outward over 0.45 s up to 380 world units.  The ring is
 * drawn as a circle of line segments approximating a stepped circle
 * (16 segments) so it reads as "vector arcade", not raster.
 *
 * Module is silent — caller plays SFX on charge start / pulse emit /
 * player-hit / sentinel-down.
 */
#include "enemy_emp_sentinel.h"
#include "ui.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ── State machine constants ─────────────────────────────────────────── */

#define EMP_STATE_IDLE     0
#define EMP_STATE_CHARGE   1
#define EMP_STATE_PULSE    2
#define EMP_STATE_COOLDOWN 3

/* ── Tuning ──────────────────────────────────────────────────────────── */

#define EMP_DETECT_RADIUS   500.0f  /* enter CHARGE when player closer than this */
#define EMP_CHARGE_TIME       1.4f  /* CHARGE phase duration — telegraph         */
#define EMP_PULSE_TIME        0.45f /* PULSE phase duration — ring active        */
#define EMP_COOLDOWN_TIME     4.0f  /* COOLDOWN before re-detection              */
#define EMP_PULSE_MAX_RADIUS 380.0f /* peak radius the ring expands to           */
#define EMP_HIT_RADIUS       16.0f  /* bullet vs sentinel body radius            */
#define EMP_DRIFT_AMP          6.0f /* world units of sinusoidal wobble drift    */
#define EMP_DRIFT_FREQ         0.7f /* radians/s for the bob                     */

/* ── Pool (file-static — no globals leak to other TUs) ────────────────── */

static struct {
    int   active;
    int   state;
    float home_x, home_y;   /* spawn anchor — Sentinel wobbles around this    */
    float drift_t;          /* phase accumulator for the sinusoidal wobble    */
    float state_t;          /* time elapsed inside the current state          */
    float pulse_radius;     /* current ring radius during PULSE phase         */
} sents[EMP_SENTINEL_MAX];

/* ── Helpers ─────────────────────────────────────────────────────────── */

/* Current rendered position = home + small wobble.  Pure function of
 * the sentinel's own state, so we can call it from both update (for
 * pulse origin) and render (for body draw) without drift.            */
static void current_pos(int i, float *out_x, float *out_y)
{
    float dx = cosf(sents[i].drift_t)            * EMP_DRIFT_AMP;
    float dy = sinf(sents[i].drift_t * 1.31f)    * EMP_DRIFT_AMP;
    *out_x = sents[i].home_x + dx;
    *out_y = sents[i].home_y + dy;
}

/* ── Public API ──────────────────────────────────────────────────────── */

void emp_sentinel_init(void)
{
    int i;
    for (i = 0; i < EMP_SENTINEL_MAX; ++i) {
        sents[i].active = 0;
        sents[i].state  = EMP_STATE_IDLE;
        sents[i].home_x = sents[i].home_y = 0.0f;
        sents[i].drift_t = 0.0f;
        sents[i].state_t = 0.0f;
        sents[i].pulse_radius = 0.0f;
    }
}

int emp_sentinel_spawn(float cx, float cy)
{
    int i;
    for (i = 0; i < EMP_SENTINEL_MAX; ++i) {
        if (!sents[i].active) {
            sents[i].active = 1;
            sents[i].state  = EMP_STATE_IDLE;
            sents[i].home_x = cx;
            sents[i].home_y = cy;
            /* Stagger initial wobble phases so multiple Sentinels
             * don't bob in lock-step.  Use the slot index as a seed
             * — deterministic and visually irregular.              */
            sents[i].drift_t = (float)i * 1.7f;
            sents[i].state_t = 0.0f;
            sents[i].pulse_radius = 0.0f;
            return i;
        }
    }
    return -1;
}

void emp_sentinel_update(float dt, float player_x, float player_y)
{
    int i;
    for (i = 0; i < EMP_SENTINEL_MAX; ++i) {
        float dx, dy, dist2;
        float px, py;

        if (!sents[i].active) continue;

        /* Always tick the wobble phase, regardless of state. */
        sents[i].drift_t += dt * EMP_DRIFT_FREQ * (float)(2.0 * M_PI);

        /* Distance from current Sentinel position to player (squared). */
        current_pos(i, &px, &py);
        dx = player_x - px;
        dy = player_y - py;
        dist2 = dx * dx + dy * dy;

        sents[i].state_t += dt;

        switch (sents[i].state) {
        case EMP_STATE_IDLE:
            if (dist2 < EMP_DETECT_RADIUS * EMP_DETECT_RADIUS) {
                sents[i].state   = EMP_STATE_CHARGE;
                sents[i].state_t = 0.0f;
            }
            break;

        case EMP_STATE_CHARGE:
            if (sents[i].state_t >= EMP_CHARGE_TIME) {
                /* Commit to pulse regardless of player range — the
                 * sentinel has finished winding up; letting the player
                 * "outrun" the charge would defang the whole mechanic. */
                sents[i].state        = EMP_STATE_PULSE;
                sents[i].state_t      = 0.0f;
                sents[i].pulse_radius = 0.0f;
            }
            break;

        case EMP_STATE_PULSE:
            /* Linear expansion from 0 to EMP_PULSE_MAX_RADIUS over
             * EMP_PULSE_TIME.  At max radius the ring vanishes and
             * we enter COOLDOWN.                                       */
            sents[i].pulse_radius =
                EMP_PULSE_MAX_RADIUS
                * (sents[i].state_t / EMP_PULSE_TIME);
            if (sents[i].state_t >= EMP_PULSE_TIME) {
                sents[i].state        = EMP_STATE_COOLDOWN;
                sents[i].state_t      = 0.0f;
                sents[i].pulse_radius = 0.0f;
            }
            break;

        case EMP_STATE_COOLDOWN:
            if (sents[i].state_t >= EMP_COOLDOWN_TIME) {
                sents[i].state   = EMP_STATE_IDLE;
                sents[i].state_t = 0.0f;
            }
            break;

        default:
            sents[i].state = EMP_STATE_IDLE;
            sents[i].state_t = 0.0f;
            break;
        }
    }
}

/* ── Rendering ───────────────────────────────────────────────────────── */

static void draw_hex_body(SDL_Renderer *r, float cx, float cy,
                          int charging, float charge_t)
{
    /* Hexagonal eye silhouette — 6 vertices on a 14u radius. */
    float verts[7][2];
    int   k;
    Uint8 br;
    SDL_Color body  = HUD_PURPLE;
    SDL_Color pupil = HUD_TEXT_CYAN;

    for (k = 0; k < 7; ++k) {
        float a = ((float)k / 6.0f) * 2.0f * (float)M_PI;
        verts[k][0] = cx + cosf(a) * 14.0f;
        verts[k][1] = cy + sinf(a) * 14.0f;
    }

    /* Brighten the body while charging — interpolated pulse from
     * dim (start of charge) to fully saturated (just before pulse). */
    if (charging) {
        float t = charge_t / EMP_CHARGE_TIME;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        br = (Uint8)(120.0f + t * 135.0f);
        body.r = (Uint8)((int)body.r * br / 255);
        body.g = (Uint8)((int)body.g * br / 255);
        body.b = (Uint8)((int)body.b * br / 255);
    } else {
        body.r = (Uint8)((int)body.r * 180 / 255);
        body.g = (Uint8)((int)body.g * 180 / 255);
        body.b = (Uint8)((int)body.b * 180 / 255);
    }

    SDL_SetRenderDrawColor(r, body.r, body.g, body.b, body.a);
    for (k = 0; k < 6; ++k) {
        SDL_RenderDrawLineF(r,
            verts[k][0],     verts[k][1],
            verts[k + 1][0], verts[k + 1][1]);
    }

    /* Central pupil — flickers faster as the charge crests. */
    if (charging) {
        float t = charge_t / EMP_CHARGE_TIME;
        float flick = 0.5f + 0.5f * sinf(t * 30.0f);
        Uint8 pa = (Uint8)(160.0f + flick * 95.0f);
        SDL_SetRenderDrawColor(r, pupil.r, pupil.g, pupil.b, pa);
    } else {
        SDL_SetRenderDrawColor(r, pupil.r, pupil.g, pupil.b, 160);
    }
    /* 2x2 pupil — bright but small */
    SDL_RenderDrawPointF(r, cx,        cy);
    SDL_RenderDrawPointF(r, cx + 1.0f, cy);
    SDL_RenderDrawPointF(r, cx,        cy + 1.0f);
    SDL_RenderDrawPointF(r, cx + 1.0f, cy + 1.0f);
}

static void draw_pulse_ring(SDL_Renderer *r, float cx, float cy,
                            float radius)
{
    /* Vector-arcade stepped circle — 16 segments.  Alpha fades to
     * zero as the radius approaches its maximum so the ring "blurs
     * out" at the edge of effect.                                    */
    const int   SEG = 16;
    float t = radius / EMP_PULSE_MAX_RADIUS;
    Uint8 ra, gg, bb, aa;
    float prev_x, prev_y;
    int   k;

    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    /* Lerp HUD_PURPLE → HUD_MAGENTA at ring peak — visual reads
     * "ionizing energy field" rather than static UI accent.        */
    ra = (Uint8)(120.0f + t * 135.0f);
    gg = (Uint8)(80.0f  - t * 80.0f);
    bb = (Uint8)(220.0f + t * 35.0f);
    aa = (Uint8)(240.0f * (1.0f - t * 0.75f));
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, ra, gg, bb, aa);

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

void emp_sentinel_render(SDL_Renderer *r, float camera_x, float camera_y)
{
    int i;
    if (!r) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

    for (i = 0; i < EMP_SENTINEL_MAX; ++i) {
        float wx, wy, sx, sy;
        if (!sents[i].active) continue;
        current_pos(i, &wx, &wy);
        sx = wx - camera_x;
        sy = wy - camera_y;

        /* Skip the body if entirely off-screen — but allow a generous
         * margin because the pulse ring can extend well beyond the
         * sentinel itself.                                            */
        if (sx > -EMP_PULSE_MAX_RADIUS - 16.0f
         && sx <  1280.0f + EMP_PULSE_MAX_RADIUS + 16.0f
         && sy > -EMP_PULSE_MAX_RADIUS - 16.0f
         && sy <  960.0f + EMP_PULSE_MAX_RADIUS + 16.0f) {

            int charging = (sents[i].state == EMP_STATE_CHARGE);
            draw_hex_body(r, sx, sy, charging, sents[i].state_t);

            if (sents[i].state == EMP_STATE_PULSE
                && sents[i].pulse_radius > 1.0f) {
                draw_pulse_ring(r, sx, sy, sents[i].pulse_radius);
            }
        }
    }
}

/* ── Player-pulse intersection ───────────────────────────────────────── */

int emp_sentinel_check_player_pulse(float px, float py)
{
    int i;
    int hit = 0;
    for (i = 0; i < EMP_SENTINEL_MAX; ++i) {
        float wx, wy, dx, dy, dist2, r;

        if (!sents[i].active) continue;
        if (sents[i].state != EMP_STATE_PULSE) continue;
        if (sents[i].pulse_radius < 1.0f) continue;

        current_pos(i, &wx, &wy);
        dx = px - wx;
        dy = py - wy;
        dist2 = dx * dx + dy * dy;

        /* Inside the expanding ring — treat as a hit.  The ring is a
         * filled disc for hit-test purposes (the visual is just the
         * leading edge), which matches the "shockwave" reading.       */
        r = sents[i].pulse_radius;
        if (dist2 < r * r) hit = 1;
    }
    return hit;
}

int emp_sentinel_hit_test(float bx, float by)
{
    int i;
    for (i = 0; i < EMP_SENTINEL_MAX; ++i) {
        float wx, wy, dx, dy;

        if (!sents[i].active) continue;
        current_pos(i, &wx, &wy);
        dx = wx - bx;
        dy = wy - by;
        if (dx * dx + dy * dy < EMP_HIT_RADIUS * EMP_HIT_RADIUS) {
            sents[i].active = 0;
            return i;
        }
    }
    return -1;
}

int emp_sentinel_alive_count(void)
{
    int i, n = 0;
    for (i = 0; i < EMP_SENTINEL_MAX; ++i) if (sents[i].active) ++n;
    return n;
}

int emp_sentinel_any_charging(void)
{
    int i;
    for (i = 0; i < EMP_SENTINEL_MAX; ++i) {
        if (sents[i].active && sents[i].state == EMP_STATE_CHARGE) return 1;
    }
    return 0;
}
