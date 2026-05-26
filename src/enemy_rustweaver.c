/* enemy_rustweaver.c — Rust-Weaver Drones (Item 23).
 * Slow corrosive drones that spit acid-green bolts which bypass shields
 * and degrade hull plating directly.  The corrosion effect itself is
 * applied by the caller in response to rustweaver_check_player_hit().
 */
#include "enemy_rustweaver.h"
#include "ui.h"

#include <math.h>
#include <stdlib.h>

#define RW_DRIFT_ACCEL   30.0f
#define RW_MAX_SPEED     60.0f
#define RW_SPIT_SPEED   220.0f
#define RW_SPIT_LIFE      3.5f
#define RW_INIT_CD        2.0f

static struct {
    int   active;
    float x, y, vx, vy;
    float spit_cd;
} drones[RUSTWEAVER_MAX];

static struct {
    int   active;
    float x, y, vx, vy;
    float life;
} spits[RUSTWEAVER_SPIT_MAX];

static float rw_frand(void) {
    return (float)rand() / (float)RAND_MAX;
}

void rustweaver_init(void) {
    int i;
    for (i = 0; i < RUSTWEAVER_MAX; ++i) {
        drones[i].active = 0;
        drones[i].x = drones[i].y = drones[i].vx = drones[i].vy = 0.0f;
        drones[i].spit_cd = 0.0f;
    }
    for (i = 0; i < RUSTWEAVER_SPIT_MAX; ++i) {
        spits[i].active = 0;
        spits[i].x = spits[i].y = spits[i].vx = spits[i].vy = 0.0f;
        spits[i].life = 0.0f;
    }
}

int rustweaver_spawn(float cx, float cy) {
    int i;
    for (i = 0; i < RUSTWEAVER_MAX; ++i) {
        if (!drones[i].active) {
            drones[i].active = 1;
            drones[i].x = cx;
            drones[i].y = cy;
            drones[i].vx = (rw_frand() - 0.5f) * 20.0f;
            drones[i].vy = (rw_frand() - 0.5f) * 20.0f;
            drones[i].spit_cd = RW_INIT_CD;
            return i;
        }
    }
    return -1;
}

static int spit_alloc(void) {
    int i;
    for (i = 0; i < RUSTWEAVER_SPIT_MAX; ++i) {
        if (!spits[i].active) return i;
    }
    return -1;
}

static void emit_spit(float ox, float oy, float tx, float ty) {
    int s = spit_alloc();
    float dx, dy, len;
    if (s < 0) return;
    dx = tx - ox;
    dy = ty - oy;
    len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) { dx = 1.0f; dy = 0.0f; len = 1.0f; }
    spits[s].active = 1;
    spits[s].x = ox;
    spits[s].y = oy;
    spits[s].vx = (dx / len) * RW_SPIT_SPEED;
    spits[s].vy = (dy / len) * RW_SPIT_SPEED;
    spits[s].life = RW_SPIT_LIFE;
}

void rustweaver_update(float dt, float player_x, float player_y) {
    int i;
    for (i = 0; i < RUSTWEAVER_MAX; ++i) {
        float dx, dy, len, ax, ay, sp;
        if (!drones[i].active) continue;

        dx = player_x - drones[i].x;
        dy = player_y - drones[i].y;
        len = sqrtf(dx * dx + dy * dy);
        if (len > 0.001f) {
            ax = (dx / len) * RW_DRIFT_ACCEL;
            ay = (dy / len) * RW_DRIFT_ACCEL;
            drones[i].vx += ax * dt;
            drones[i].vy += ay * dt;
        }

        sp = sqrtf(drones[i].vx * drones[i].vx + drones[i].vy * drones[i].vy);
        if (sp > RW_MAX_SPEED) {
            drones[i].vx = (drones[i].vx / sp) * RW_MAX_SPEED;
            drones[i].vy = (drones[i].vy / sp) * RW_MAX_SPEED;
        }

        drones[i].x += drones[i].vx * dt;
        drones[i].y += drones[i].vy * dt;

        drones[i].spit_cd -= dt;
        if (drones[i].spit_cd <= 0.0f) {
            emit_spit(drones[i].x, drones[i].y, player_x, player_y);
            drones[i].spit_cd = 2.5f + (float)(rand() % 50) / 50.0f;
        }
    }

    for (i = 0; i < RUSTWEAVER_SPIT_MAX; ++i) {
        if (!spits[i].active) continue;
        spits[i].x += spits[i].vx * dt;
        spits[i].y += spits[i].vy * dt;
        spits[i].life -= dt;
        if (spits[i].life <= 0.0f) spits[i].active = 0;
    }
}

static void draw_drone(SDL_Renderer *r, float cx, float cy) {
    /* 5-spoke asterisk/star, ~14 px radius */
    const float R = 14.0f;
    int k;
    SDL_Color g = HUD_GREEN;
    SDL_SetRenderDrawColor(r, g.r, g.g, g.b, g.a);
    for (k = 0; k < 5; ++k) {
        float ang = (float)k * (6.2831853f / 5.0f);
        float ex = cx + cosf(ang) * R;
        float ey = cy + sinf(ang) * R;
        SDL_RenderDrawLineF(r, cx, cy, ex, ey);
    }
    /* small inner ring to read as a body */
    for (k = 0; k < 5; ++k) {
        float a0 = (float)k * (6.2831853f / 5.0f);
        float a1 = (float)(k + 1) * (6.2831853f / 5.0f);
        float r0 = R * 0.35f;
        SDL_RenderDrawLineF(r,
            cx + cosf(a0) * r0, cy + sinf(a0) * r0,
            cx + cosf(a1) * r0, cy + sinf(a1) * r0);
    }
}

static void draw_spit(SDL_Renderer *r, float x, float y, float vx, float vy) {
    const float S = 6.0f;
    SDL_Color g = HUD_GREEN;
    SDL_Color c = HUD_CINNABAR;
    float len = sqrtf(vx * vx + vy * vy);
    float tx = x, ty = y;
    if (len > 0.001f) {
        tx = x - (vx / len) * S;
        ty = y - (vy / len) * S;
    }
    SDL_SetRenderDrawColor(r, g.r, g.g, g.b, g.a);
    SDL_RenderDrawLineF(r, x,        y - S,    x + S, y);
    SDL_RenderDrawLineF(r, x + S,    y,        x,     y + S);
    SDL_RenderDrawLineF(r, x,        y + S,    x - S, y);
    SDL_RenderDrawLineF(r, x - S,    y,        x,     y - S);
    /* trailing cinnabar pixel */
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderDrawPointF(r, tx, ty);
}

void rustweaver_render(SDL_Renderer *r, float camera_x, float camera_y) {
    int i;
    if (!r) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    /* Translate world → screen coords by subtracting the camera's top-left.
     * Generous off-screen cull (~SCREEN_WIDTH/HEIGHT plus a 64 px margin)
     * skips the spoke/spit rasterizer for entities the player can't see. */
    for (i = 0; i < RUSTWEAVER_MAX; ++i) {
        float sx, sy;
        if (!drones[i].active) continue;
        sx = drones[i].x - camera_x;
        sy = drones[i].y - camera_y;
        if (sx < -64.0f || sx > 1984.0f || sy < -64.0f || sy > 1144.0f) continue;
        draw_drone(r, sx, sy);
    }
    for (i = 0; i < RUSTWEAVER_SPIT_MAX; ++i) {
        float sx, sy;
        if (!spits[i].active) continue;
        sx = spits[i].x - camera_x;
        sy = spits[i].y - camera_y;
        if (sx < -32.0f || sx > 1952.0f || sy < -32.0f || sy > 1112.0f) continue;
        draw_spit(r, sx, sy, spits[i].vx, spits[i].vy);
    }
}

int rustweaver_check_player_hit(float px, float py, float hit_radius) {
    int i;
    int hit = 0;
    float r2 = hit_radius * hit_radius;
    for (i = 0; i < RUSTWEAVER_SPIT_MAX; ++i) {
        float dx, dy;
        if (!spits[i].active) continue;
        dx = spits[i].x - px;
        dy = spits[i].y - py;
        if (dx * dx + dy * dy < r2) {
            spits[i].active = 0;
            hit = 1;
        }
    }
    return hit;
}

int rustweaver_hit_test(float bx, float by) {
    int i;
    for (i = 0; i < RUSTWEAVER_MAX; ++i) {
        float dx, dy;
        if (!drones[i].active) continue;
        dx = drones[i].x - bx;
        dy = drones[i].y - by;
        if (dx * dx + dy * dy < 16.0f * 16.0f) {
            drones[i].active = 0;
            return i;
        }
    }
    return -1;
}
