/* fuligin/src/game.c
 * Core game simulation for FULIGIN — an arcade space shooter set in a dying future.
 * Manages all game entities, physics, AI, input dispatch, progression, and rendering.
 * Inspired by the lore of Gene Wolfe's Book of the New Sun and Jack Vance's Dying Earth.
 *
 * Architecture: Single translation unit containing all game state as static module globals.
 * Public API (declared in game.h): game_init, game_handle_input, game_update, game_render,
 * game_set_paused, game_focus_changed.
 *
 * Author: [author]
 * Date: 2025
 */

/* =========== INCLUDES =========== */

#include "game.h"
#include "vector_graphics.h"
#include "vector_font.h"
#include "audio.h"
#include "ui.h"
#include "world_builder.h"
#include "drone_chatter.h"
#include "enemy_rustweaver.h"
#include "enemy_ascian.h"
#include "enemy_lictor.h"
#include "enemy_emp_sentinel.h"
#include "enemy_scavenger.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* =========== EXTERNAL REFERENCES =========== */

extern SDL_Renderer *g_renderer; /* defined in main.c */
extern SDL_Window   *g_window;   /* defined in main.c */

/* =========== MATH CONSTANTS =========== */

#ifndef M_PI
#define M_PI   3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

/* =========== ENTITY POOL LIMITS =========== */

#define MAX_ASTEROIDS    128
#define MAX_BULLETS       48
#define MAX_UFO_BULLETS   32
#define MAX_PARTICLES    128
#define MAX_ORBS         100
#define MAX_NPC            4
#define MAX_STRUCTURE      6
#define MAX_SCORE_FLOATS  24
#define MAX_EVENT_FLOATS  16   /* labeled text pop-ups: CRIT!, STRIKE!, HIT, etc. */
#define MAX_WARP_LOCS      8
#define SHOP_ITEMS_PER_PAGE 16
#define PHOS_TRAIL_LEN    14  /* phosphor trail length (samples) */

/* =========== PHYSICS CONSTANTS =========== */

#define FRICTION                  0.99f   /* per-frame velocity decay factor */
#define ROTATION_SPEED            4.5f    /* ship turn rate, radians/sec */
#define THRUST_FORCE            350.0f    /* ship thrust acceleration, units/sec^2 */
#define BULLET_SPEED            600.0f    /* player bullet muzzle velocity, units/sec */
#define BULLET_LIFE               1.2f    /* player bullet lifetime, seconds */
#define UFO_BULLET_SPEED        450.0f    /* UFO bullet muzzle velocity, units/sec */
#define UFO_BULLET_LIFE           1.8f    /* UFO bullet lifetime, seconds */
#define COMBO_WINDOW              1.8f    /* seconds between kills to maintain combo */
#define ATTRACT_DELAY            10.0f    /* idle seconds before attract mode starts */
#define HYPERSPACE_DEATH_CHANCE   0.015f  /* probability of dying on hyperspace jump */
#define WARP_FUEL_COST           20.0f    /* fuel consumed per warp-drive jump */
#define FUEL_BURN_RATE            3.5f    /* fuel drained per second while thrusting */
/* World-scale: the universe was scaled up 10x.  All "distance from origin"
 * and absolute world-space constants below were multiplied by 10 so the
 * navigable map feels 10x larger.  Density-around-player constants (spawn
 * rings, follow distances) intentionally stayed the same so encounter
 * frequency near the player is unchanged — only travel distance grew. */
#define FUEL_REGEN_RADIUS       6000.0f   /* distance from home at which fuel regenerates (was 600, now 10x) */
#define FUEL_PASSIVE_BASE_RATE    0.08f   /* idle reactor drain even without thrusting */
#define FUEL_RELIC_RATE           0.04f   /* extra drain per active relic/weapon system */
#define FUEL_DRIFT_PENALTY_TIME  10.0f   /* seconds adrift before emergency hull breach */
#define FUEL_DRIFT_RESERVE        0.20f   /* fuel fraction restored on emergency refill */
/* Solar Flare Cycles (todo.md §2.3 / Lore §3) + Star-Shadow Radiation Shielding
 * (todo.md §3 / Lore §3 second half).  Periodic flares triple passive reactor
 * drain in non-Home zones.  When a large/medium asteroid lies between the
 * player and the origin (the dying sun), the drain is partially negated —
 * a faint cinnabar vector shadow-line is drawn from the asteroid to the
 * shielded player so the mechanic is readable. */
#define SOLAR_FLARE_MIN_INTERVAL    90.0f
#define SOLAR_FLARE_MAX_INTERVAL   150.0f
#define SOLAR_FLARE_WARN_DURATION    3.0f
#define SOLAR_FLARE_ACTIVE_DURATION  5.0f
#define SOLAR_FLARE_FUEL_MULT        3.0f  /* drain multiplier during ACTIVE phase */
#define SOLAR_FLARE_SHADOW_MULT      1.4f  /* reduced multiplier while in asteroid shadow */
#define SOLAR_FLARE_MIN_ZONE          1    /* zone 0 (Home Space) is exempt */
#define SOLAR_FLARE_SHADOW_HALF_W   80.0f  /* perpendicular shadow corridor half-width (world units) */
#define XP_THRESHOLD_GROWTH       1.6f    /* XP threshold multiplier per player level */
#define EXTRA_LIFE_SCORE_INTERVAL 10000   /* score increment between extra-life awards */

/* Spawn ring radii (world units from origin) */
#define ASTEROID_SPAWN_RING_MIN  2000.0f
#define ASTEROID_SPAWN_RING_MAX  2500.0f
#define UFO_SPAWN_RING_MIN       1600.0f
#define UFO_SPAWN_RING_MAX       2400.0f
#define RIFT_SPAWN_RING_MIN       850.0f
#define RIFT_SPAWN_RING_MAX      1100.0f

/* Named zone radii (world units from origin) */
#define ZONE_HOME_RADIUS        10000.0f  /* home zone — safe area near station (10x scale) */
#define ZONE_INNER_RADIUS       35000.0f  /* inner belt — mid-difficulty band (10x scale) */
#define ZONE_VOID_RADIUS        80000.0f  /* void zone — outer ring, maximum threat (10x scale) */
#define ZONE_ABYSS_RADIUS      140000.0f  /* the abyss — boundary into DEEP DRIFT (zone 4, item 13) */

/* =========== ENUMERATIONS =========== */

/** @brief Player-selected challenge level affecting enemy aggression and score multipliers. */
typedef enum {
    DIFFICULTY_EASY,
    DIFFICULTY_NORMAL,
    DIFFICULTY_HARD,
    DIFFICULTY_ACE
} Difficulty;

/** @brief Top-level game state machine. Controls which screen/logic branch is active. */
typedef enum {
    STATE_TITLE,               /* main title / attract idle */
    STATE_PLAYING,             /* active gameplay */
    STATE_PAUSED,              /* gameplay paused */
    STATE_GAMEOVER,            /* death sequence / game-over screen */
    STATE_NEW_HIGHSCORE,       /* high-score initial entry */
    STATE_UPGRADE_SELECT,      /* level-up upgrade choice */
    STATE_HIGHSCORES,          /* high-score display */
    STATE_SETTINGS,            /* settings menu */
    STATE_KEYBINDS,            /* key/controller binding screen */
    STATE_ATTRACT_INSTRUCTIONS,/* attract: instructions overlay */
    STATE_ATTRACT_GAMEPLAY,    /* attract: AI plays the game */
    STATE_SHOP,                /* resource shop */
    STATE_WARP_MENU            /* warp-drive destination picker */
} GameState;

/** @brief All available player upgrade types, resolved at level-up or shop purchase. */
typedef enum {
    UPGRADE_TRIPLE_SHOT,
    UPGRADE_BOUNCE,
    UPGRADE_SHIELD,
    UPGRADE_RAPID_FIRE,
    UPGRADE_PIERCING,
    UPGRADE_SPEED,
    UPGRADE_SIZE_DOWN,
    UPGRADE_HOMING,
    UPGRADE_GHOST_SIGHT,
    UPGRADE_VOID_ROUNDS,
    UPGRADE_TIME_WARP,
    UPGRADE_ORB_MAGNET,
    UPGRADE_OVERCHARGE,
    UPGRADE_MIRROR_IMAGE,
    UPGRADE_REAR_GUN,
    UPGRADE_ORBITAL_MINE,
    UPGRADE_CRIT_CHANCE,
    UPGRADE_HEALTH_BOOST,
    UPGRADE_THRUST_TRAIL,
    UPGRADE_SPLIT_SHOT,
    UPGRADE_VORTEX_GRENADE,
    UPGRADE_PHASE_SHIFT,
    UPGRADE_AUTO_TURRET,
    UPGRADE_NOVA_EXPLOSION,
    UPGRADE_XP_BOOST,
    UPGRADE_ARMOR_PLATE,
    UPGRADE_THERMAL_HULL,
    UPGRADE_SINGULARITY_DISPLACER,
    UPGRADE_SINGULARITY_WHIP,
    UPGRADE_RESONANCE_CASCADE,
    UPGRADE_COUNT /* sentinel — always last */
} UpgradeType;

/** @brief Keyboard action identifiers for rebindable key actions. */
typedef enum {
    KB_ROTATE_LEFT = 0,
    KB_ROTATE_RIGHT,
    KB_THRUST,
    KB_FIRE,
    KB_PAUSE,
    KB_HYPERSPACE,
    KB_ABILITY1,
    KB_ABILITY2,
    KB_ABILITY3,
    KB_ABILITY4,
    KB_MINIMAP,
    KB_COUNT
} KeyAction;

/** @brief Controller button action identifiers for rebindable gamepad actions. */
typedef enum {
    CT_THRUST = 0,
    CT_FIRE,
    CT_HYPERSPACE,
    CT_ABILITY1,
    CT_ABILITY2,
    CT_ABILITY3,
    CT_ABILITY4,
    CT_MINIMAP,
    CT_COUNT
} CtrlAction;

/* =========== STRUCT DEFINITIONS =========== */

/**
 * @brief A single entry in the persistent high-score table.
 * Stores three-character initials and the achieved score.
 */
typedef struct {
    char initials[4];
    int  score;
} HighScore;

/**
 * @brief A short-lived visual debris or explosion fragment.
 * Emitted in bursts by spawn_particles(); fades and scales down over its lifetime.
 */
typedef struct {
    Vec2      pos;
    Vec2      vel;
    float     angle;
    float     rot_speed;
    float     scale;
    float     life;
    float     max_life;
    SDL_Color color;
} Particle;

/**
 * @brief The player's ship entity.
 * Carries position/velocity physics state, collision radius, vector line geometry,
 * a phosphor-trail ring buffer, and per-frame timing fields.
 */
typedef struct {
    int   active;
    Vec2  pos;
    Vec2  vel;
    float angle;
    float rot_speed;
    float radius;
    int   size;                    /* display scale tier (unused for player sizing) */
    Line  lines[16];
    int   line_count;
    float bullet_cooldown;
    float invuln_timer;
    float emp_lock_timer;          /* Item 25: EMP-locked seconds remaining     */
    float sensor_static_timer;     /* Status: sensor blackout (blindness) secs  */
    float reverse_drift_timer;     /* Status: thrust vector inverted (confusion)*/
    float tox_hallucination_timer; /* Status: wireframe scramble (hallucination)*/
    Vec2  trail_pos[PHOS_TRAIL_LEN];
    float trail_ang[PHOS_TRAIL_LEN];
    int   trail_head;              /* ring-buffer write index */
} ShipEntity;

/**
 * @brief A void-stone (asteroid) entity.
 * Sizes 3/2/1 split on destruction. Optionally shielded (requires two hits) and
 * carries a material type that determines resource drops.
 */
typedef struct {
    int   active;
    Vec2  pos;
    Vec2  vel;
    float angle;
    float rot_speed;
    float radius;
    int   size;                    /* 3=Large 2=Medium 1=Small */
    Line  lines[16];
    int   line_count;
    int   has_shield;              /* 1 = shielded (requires extra hit, level >= 3) */
    int   material;                /* 0=rock  1=metal  2=crystal */
    int   hit_count;               /* total hits received (for stress-crack rendering) */
    int   health;                  /* hit points remaining; metal/crystal size-3 start at 2 */
    Vec2  trail_pos[PHOS_TRAIL_LEN];
    float trail_ang[PHOS_TRAIL_LEN];
    int   trail_head;
} AsteroidEntity;

/**
 * @brief A projectile fired by the player.
 * Supports bouncing (max_bounces) and homing behaviour. The piercing flag is
 * resolved from player_upgrades.piercing at fire time.
 */
typedef struct {
    int       active;
    Vec2      pos;
    Vec2      vel;
    float     life;
    SDL_Color color;
    int       bounces;
    int       is_homing;
    int       pierces;    /* non-zero: bolt passes through targets without being consumed */
    Vec2      trail_pos[PHOS_TRAIL_LEN];
    float     trail_ang[PHOS_TRAIL_LEN];
    int       trail_head;
} BulletEntity;

/**
 * @brief A UFO enemy entity.
 * size field doubles as enemy sub-type (1=small, 2=large, 3=kamikaze,
 * 4=bomber, 5=leviathan). Carries geometry, AI timers, and a phosphor trail.
 */
typedef struct {
    int   active;
    Vec2  pos;
    Vec2  vel;
    int   size;
    float radius;
    float angle;          /* current facing angle (radians) — used by kamikaze */
    Line  lines[16];
    int   line_count;
    float fire_timer;
    float change_dir_timer;
    float target_y;
    Vec2  trail_pos[PHOS_TRAIL_LEN];
    float trail_ang[PHOS_TRAIL_LEN];
    int   trail_head;
} UfoEntity;

/**
 * @brief A static or slowly-rotating background structure (e.g. home station).
 * Used for collision anchors and zone definition.
 */
typedef struct {
    int   active;
    int   type;           /* 0=home_station */
    Vec2  pos;
    float angle;
    float rot_speed;
    float radius;
} StructureEntity;

/**
 * @brief A friendly or neutral NPC ship that orbits the player.
 * Follows the player when contact_timer exceeds a threshold.
 */
typedef struct {
    int   active;
    int   following;      /* 0=idle  1=following player */
    Vec2  pos;
    Vec2  vel;
    float angle;
    float radius;
    float orbit_angle;    /* current orbit angle around player */
    float contact_timer;  /* accumulated time player has been nearby */
    float chatter_timer;  /* countdown until next drone_chatter emit */
} NpcEntity;

/**
 * @brief A spatial rift entity that exerts gravitational pull on nearby objects.
 * Used both for world-spawned rifts and player-triggered displacement rifts.
 */
typedef struct {
    int   active;
    Vec2  pos;
    float radius;
    float angle1;
    float angle2;
    float pulse_timer;
    float spawn_timer;
} RiftEntity;

/**
 * @brief A black hole entity that exerts a strong gravitational pull.
 */
typedef struct {
    int   active;
    Vec2  pos;
    float mass;
    float radius;
} GravityWellEntity;

#define MAX_GRAVITY_WELLS 4

/**
 * @brief A collectible chronicle-orb entity.
 * Drifts toward the player when within magnet radius. Worth value XP on pickup.
 */
typedef struct {
    int   active;
    Vec2  pos;
    Vec2  vel;
    int   value;
    float life;
    Vec2  trail_pos[PHOS_TRAIL_LEN];
    float trail_ang[PHOS_TRAIL_LEN];
    int   trail_head;
} OrbEntity;

/**
 * @brief An expanding shockwave ring emitted by certain upgrades (e.g. Nova Shell).
 * Grows from radius=0 to max_radius while fading its alpha.
 */
typedef struct {
    int   active;
    Vec2  pos;
    float radius;
    float max_radius;
    float thickness;
    float life;
} ShockwaveEntity;

/**
 * @brief Accumulated flags for all upgrades the player currently holds.
 * Fields are checked each frame rather than recalculated to avoid branching.
 */
typedef struct {
    int   triple_shot;
    int   max_bounces;
    float fire_cooldown_mult;
    float bullet_speed_mult;
    int   shield_active;
    float speed_mult;
    int   piercing;
    float size_mult;
    int   homing;
    float magnet_radius;
    int   rear_gun;
    int   split_shot;
    float xp_mult;
    int   mirror_image;
    int   phase_shift;
    int   thermal_hull;
    int   singularity_displacer;
    int   singularity_whip;
    int   resonance_cascade;
    int   vortex_grenade;
    int   auto_turret;
    int   nova_explosion;
} PlayerUpgrades;

/**
 * @brief A floating score pop-up that drifts upward and fades out.
 * Emitted whenever the player scores points.
 */
typedef struct {
    float x, y, vy;
    int   value;
    float life;
    int   active;
} ScoreFloat;

/**
 * @brief A labeled text event pop-up (CRIT!, STRIKE!, HIT, VENT!) that
 *        drifts upward in world space and fades out over ~1.5 seconds.
 *        Distinct from ScoreFloat which shows a numeric value.
 */
typedef struct {
    float     x, y, vy;
    char      label[24];
    SDL_Color color;
    float     life;
    float     max_life;
    int       active;
} EventFloat;

/**
 * @brief A named warp-drive destination recorded for the warp-menu.
 */
typedef struct {
    Vec2 pos;
    char label[32];
} WarpLoc;

/**
 * @brief Return type from wrap_position_ext() — records whether a wrap occurred
 * and the world positions before/after the wrap for rift-displacement logic.
 */
typedef struct {
    int  wrapped;
    Vec2 entry_pos;
    Vec2 exit_pos;
} WrapEvent;

/* =========== STATIC GEOMETRY TABLES =========== */

/* Player ship — five-line arrowhead */
static Line ship_lines[] = {
    {{ 0.0f, -12.0f}, {-8.0f,  10.0f}},
    {{-8.0f,  10.0f}, {-3.0f,   6.0f}},
    {{-3.0f,   6.0f}, { 3.0f,   6.0f}},
    {{ 3.0f,   6.0f}, { 8.0f,  10.0f}},
    {{ 8.0f,  10.0f}, { 0.0f, -12.0f}}
};

/* Small/large UFO saucer silhouette */
static Line ufo_lines[] = {
    {{-16.0f,   0.0f}, { 16.0f,   0.0f}},
    {{-16.0f,   0.0f}, { -8.0f,   5.0f}},
    {{ -8.0f,   5.0f}, {  8.0f,   5.0f}},
    {{  8.0f,   5.0f}, { 16.0f,   0.0f}},
    {{-16.0f,   0.0f}, { -8.0f,  -5.0f}},
    {{ -8.0f,  -5.0f}, {  8.0f,  -5.0f}},
    {{  8.0f,  -5.0f}, { 16.0f,   0.0f}},
    {{ -8.0f,  -5.0f}, { -4.0f, -10.0f}},
    {{ -4.0f, -10.0f}, {  4.0f, -10.0f}},
    {{  4.0f, -10.0f}, {  8.0f,  -6.0f}}
};

/* Kamikaze UFO — compact arrowhead */
static Line kamikaze_lines[] = {
    {{ 12.0f,  0.0f}, {-8.0f, -8.0f}},
    {{ -8.0f, -8.0f}, {-4.0f,  0.0f}},
    {{ -4.0f,  0.0f}, {-8.0f,  8.0f}},
    {{ -8.0f,  8.0f}, {12.0f,  0.0f}}
};

/* Bomber UFO — wider saucer variant */
static Line bomber_lines[] = {
    {{-20.0f,  0.0f}, { 20.0f,  0.0f}},
    {{-20.0f,  0.0f}, {-10.0f,  7.0f}},
    {{-10.0f,  7.0f}, { 10.0f,  7.0f}},
    {{ 10.0f,  7.0f}, { 20.0f,  0.0f}},
    {{-20.0f,  0.0f}, {-12.0f, -6.0f}},
    {{-12.0f, -6.0f}, { 12.0f, -6.0f}},
    {{ 12.0f, -6.0f}, { 20.0f,  0.0f}},
    {{-12.0f, -6.0f}, { -6.0f,-12.0f}},
    {{ -6.0f,-12.0f}, {  6.0f,-12.0f}},
    {{  6.0f,-12.0f}, { 12.0f, -6.0f}}
};

/* Home space station — octagon + cross + inner box */
static Line home_station_lines[] = {
    {{-22.0f,-52.0f}, { 22.0f,-52.0f}},
    {{ 22.0f,-52.0f}, { 52.0f,-22.0f}},
    {{ 52.0f,-22.0f}, { 52.0f, 22.0f}},
    {{ 52.0f, 22.0f}, { 22.0f, 52.0f}},
    {{ 22.0f, 52.0f}, {-22.0f, 52.0f}},
    {{-22.0f, 52.0f}, {-52.0f, 22.0f}},
    {{-52.0f, 22.0f}, {-52.0f,-22.0f}},
    {{-52.0f,-22.0f}, {-22.0f,-52.0f}},
    {{-52.0f,  0.0f}, {-24.0f,  0.0f}},
    {{ 52.0f,  0.0f}, { 24.0f,  0.0f}},
    {{  0.0f,-52.0f}, {  0.0f,-24.0f}},
    {{  0.0f, 52.0f}, {  0.0f, 24.0f}},
    {{-12.0f,-12.0f}, { 12.0f,-12.0f}},
    {{ 12.0f,-12.0f}, { 12.0f, 12.0f}},
    {{ 12.0f, 12.0f}, {-12.0f, 12.0f}},
    {{-12.0f, 12.0f}, {-12.0f,-12.0f}}
};

/* Eldritch Tendril — Zone 2+ enemy, asymmetric tentacle shape */
static Line eldritch_lines[] = {
    {{  0.0f,-16.0f}, { 10.0f, -6.0f}},
    {{ 10.0f, -6.0f}, { 16.0f,  4.0f}},
    {{ 16.0f,  4.0f}, {  8.0f, 16.0f}},
    {{  0.0f,-16.0f}, {-10.0f, -6.0f}},
    {{-10.0f, -6.0f}, {-16.0f,  4.0f}},
    {{-16.0f,  4.0f}, { -8.0f, 16.0f}},
    {{  8.0f, 16.0f}, {  0.0f, 10.0f}},
    {{ -8.0f, 16.0f}, {  0.0f, 10.0f}},
    {{  0.0f,-16.0f}, {  0.0f, -8.0f}},
    {{ -4.0f,  0.0f}, {  4.0f,  0.0f}}
};

/* Daemon Sigil — Zone 3+ enemy, angular chaos symbol */
static Line daemon_lines[] = {
    {{  0.0f,-20.0f}, { 12.0f, -8.0f}},
    {{ 12.0f, -8.0f}, { 20.0f,  0.0f}},
    {{ 20.0f,  0.0f}, { 12.0f, 12.0f}},
    {{ 12.0f, 12.0f}, {  0.0f, 20.0f}},
    {{  0.0f, 20.0f}, {-12.0f, 12.0f}},
    {{-12.0f, 12.0f}, {-20.0f,  0.0f}},
    {{-20.0f,  0.0f}, {-12.0f, -8.0f}},
    {{-12.0f, -8.0f}, {  0.0f,-20.0f}},
    {{ -8.0f, -8.0f}, {  8.0f, -8.0f}},
    {{  8.0f, -8.0f}, {  8.0f,  8.0f}},
    {{  8.0f,  8.0f}, { -8.0f,  8.0f}},
    {{ -8.0f,  8.0f}, { -8.0f, -8.0f}},
    {{  0.0f,-12.0f}, {  0.0f, 12.0f}},
    {{-12.0f,  0.0f}, { 12.0f,  0.0f}},
    {{ -6.0f, -6.0f}, {  6.0f,  6.0f}},
    {{  6.0f, -6.0f}, { -6.0f,  6.0f}}
};

/* Friendly NPC shield drone — small diamond with inner square hint */
static Line npc_drone_lines[] = {
    {{  0.0f,-10.0f}, { 10.0f,  0.0f}},
    {{ 10.0f,  0.0f}, {  0.0f, 10.0f}},
    {{  0.0f, 10.0f}, {-10.0f,  0.0f}},
    {{-10.0f,  0.0f}, {  0.0f,-10.0f}},
    {{ -5.0f, -5.0f}, {  5.0f, -5.0f}},
    {{  5.0f, -5.0f}, {  5.0f,  5.0f}},
    {{  5.0f,  5.0f}, { -5.0f,  5.0f}}
};

/* =========== STRING TABLES =========== */

static const char *upgrade_names[] = {
    "TRISKELION BURST", "VOID RICOCHET", "ETHER SHROUD", "OVERCLOCK",
    "ICHOROUS ROUNDS", "AUTODYNE BOOST", "SMALL FORM", "SEEKER ICHORS",
    "PALE SIGHT", "CORRUPTION ROUNDS", "TEMPORAL FOLD", "ORB MAGNET",
    "SOLAR FURY", "WRAITH TWIN", "AFT CANNON", "ORBIT MINE",
    "CRIT CHANCE", "RESTORED VIGOR", "BANE TRAIL", "FISSION SHOT",
    "VORTEX GRENADE", "PHASE SHIFT", "AUTO TURRET", "NOVA SHELL",
    "CHRONICLE BOOST", "OSSIFIED HULL", "THERMAL HULL", "RIFT DISPLACER",
    "BANE WHIP", "RESONANCE CASCADE"
};

static const char *upgrade_descs[] = {
    "Three-bolt spread from the prow",
    "Bolts rebound off the void's edge",
    "One absorption per life",
    "30% faster discharge",
    "Bolts pierce through Void Stones",
    "20% faster Autodyne thrust",
    "Reduce ship collision radius",
    "Bolts seek the nearest stone",
    "Reveal hidden Remnant threats",
    "Bolts corrode on contact",
    "Briefly slow the drift of time",
    "Draw chronicle orbs to you",
    "Rage boils when hull is intact",
    "A phantom twin follows your path",
    "Discharge from the aft thruster",
    "Mines orbit the hull",
    "Random bolts strike twice",
    "The Autarch grants another life",
    "Thruster wash scorches Void Stones",
    "Bolts split on first impact",
    "Pulls Void Stones into ruin",
    "Pass through one killing blow",
    "An ancient turret follows you",
    "Hull detonates on impact",
    "Harvest 50% more chronicle",
    "Ancient plating resists damage",
    "Ram Void Stones at full thrust",
    "Double-tap thrust to rift-jump",
    "Thruster trail scorches the drift",
    "Bolts unleash shockwaves"
};

static const char *difficulty_names[] = {
    "EASY", "NORMAL", "HARD", "ACE"
};

/* =========== MODULE-SCOPE GLOBALS =========== */

/* --- Core state --- */
static GameState  game_state  = STATE_TITLE;
static Difficulty difficulty  = DIFFICULTY_NORMAL;
static float      game_time   = 0.0f;  /* accumulated game time (seconds), for physics */
static float      g_boot_timer = 0.0f; /* terminal boot sequence timer */
static float      hyperjump_flash_timer = 0.0f;

/* --- High scores & initials entry --- */
static HighScore high_scores[5];
static int       new_high_score_idx = -1;
static char      temp_initials[4]   = "A  ";
static int       cur_initial_char   = 0;

/* --- Player entity & related --- */
static ShipEntity    player;
static PlayerUpgrades player_upgrades;
static int   is_thrusting        = 0;
static float fire_cooldown_timer  = 0.0f;
static float thrust_timer         = 0.0f;

/* --- Entity pools --- */
static AsteroidEntity asteroids[MAX_ASTEROIDS];
static BulletEntity   bullets[MAX_BULLETS];
static BulletEntity   ufo_bullets[MAX_UFO_BULLETS];
static UfoEntity      ufo;
static Particle       particles[MAX_PARTICLES];
static OrbEntity      orbs[MAX_ORBS];
static RiftEntity     rift;
static RiftEntity     player_rift;
static GravityWellEntity gravity_wells[MAX_GRAVITY_WELLS];
static ShockwaveEntity shockwaves[4];
static StructureEntity structures[MAX_STRUCTURE];
static NpcEntity      npcs[MAX_NPC];

/* --- Score & progression --- */
static int   score                    = 0;
static int   lives                    = 3;
static int   level                    = 1;
static int   player_level             = 1;
static int   player_xp                = 0;
static int   xp_threshold             = 100;
static int   total_asteroids_destroyed = 0;
static int   wave_asteroids_destroyed  = 0;
static int   wave_cleared_pending      = 0;
static float last_extra_life_score     = 0.0f; /* score milestone for next extra life */
static float xp_flash_timer            = 0.0f;

/* --- Combo system --- */
static int   combo_count = 0;
static float combo_timer  = 0.0f;

/* --- Bowling combo system (item 19) --- */
/* bowling_timer[i] > 0 means asteroid slot i is an active bowling fragment */
static float bowling_timer[MAX_ASTEROIDS];
static int   bowling_chain_count = 0;
static float bowling_chain_timer = 0.0f;
static int   g_bowling_next_spawn = 0; /* when 1, next spawn_asteroid() marks slot as bowling */

/* ── Cugel-9 board computer (item 26) ──────────────────────────────────
 * A circular buffer of melancholic robot commentary displayed in a
 * ui_panel_terminal() strip just above the bottom HUD panels.
 * Format on screen: [CUGEL-9]: <message body>
 * Messages fade in over CUGEL9_FADE_DUR, stay, then fade out. */
#define CUGEL9_MAX_MSGS  4
#define CUGEL9_MSG_LEN   72
#define CUGEL9_SHOW_DUR  3.5f    /* seconds fully visible              */
#define CUGEL9_FADE_DUR  0.35f   /* fade-in / fade-out each            */

typedef struct { char text[CUGEL9_MSG_LEN]; float timer; } Cugel9Msg;
static Cugel9Msg cugel9_buf[CUGEL9_MAX_MSGS];
static int       cugel9_head     = 0;    /* next write slot (circular) */
static float     cugel9_cooldown = 0.0f; /* debounce between messages  */

/* --- Score floaters --- */
static ScoreFloat score_floats[MAX_SCORE_FLOATS];

/* --- Event text floaters (CRIT!, STRIKE!, HIT, VENT!) --- */
static EventFloat event_floats[MAX_EVENT_FLOATS];

/* --- Upgrade selection ---
 * Narrow extern: these globals are defined in src/state.c and read by both
 * game.c (input handler at STATE_UPGRADE_SELECT) and ui_hud.c (renderer).
 * Sharing the same memory across translation units is what fixes the bug
 * where the powerup cursor visually never advanced and Enter selected a
 * stale option.  We can't include state.h because it currently re-declares
 * game.c's entire data model and has a syntax error at line 223; once the
 * state.h refactor is finished, this pair of declarations can be deleted in
 * favour of the full include. */
extern UpgradeType upgrade_options[3];
extern int         selected_option;

/* --- Camera & world --- */
static Vec2  camera_pos        = {0.0f, 0.0f};
static int   player_zone            = 0;
static float player_dist_origin     = 0.0f;
static float home_station_angle     = 0.0f;

/* --- Zone transition banner --- */
static float zone_banner_timer      = 0.0f;   /* seconds remaining; 0 = hidden */
static int   zone_banner_prev_zone  = -1;     /* -1 = not yet initialised */

/* --- Chronicle chord harmonics --- */
static float orb_chord_timer        = 0.0f;   /* window for rapid-collection chain */
static int   orb_chord_note         = 0;      /* next pentatonic note index [0-4] */
static int   minimap_visible   = 1;

/* --- Solar Flare Cycles + Star-Shadow Shielding (todo.md §2.3 / §3) --- */
static float solar_flare_timer      = 0.0f;   /* countdown to next WARNING phase   */
static float solar_flare_warn_t     = 0.0f;   /* time remaining in WARNING phase   */
static float solar_flare_active_t   = 0.0f;   /* time remaining in ACTIVE  phase   */
static float solar_flare_next_iv    = 120.0f; /* rolled interval for current cycle */
static int   solar_flare_in_shadow  = 0;      /* set each frame by shadow check    */

/* --- Sensor Static (Blindness) status-malfunction roll state ---
 * Driven from inside the Solar Flare ACTIVE phase: every few seconds,
 * if the player is NOT in asteroid shadow, roll a random chance to
 * scramble the sensors (minimap + DANGER readout blanked) for ~2.4s.
 * Hiding in shadow prevents the scramble entirely — a second reward
 * on top of the reduced fuel drain. */
#define SENSOR_STATIC_DURATION   2.4f   /* seconds of blackout once triggered */
#define SENSOR_STATIC_ROLL_MIN   3.5f   /* min seconds between scramble rolls */
#define SENSOR_STATIC_ROLL_MAX   6.5f   /* max seconds between scramble rolls */
#define SENSOR_STATIC_HIT_CHANCE 0.45f  /* per-roll probability of scramble  */
static float sensor_static_roll_t = 0.0f;    /* countdown to next scramble roll */

/* --- Reverse Drift (Confusion) status-malfunction roll state ---
 * Companion to Sensor Static.  Driven from the same Solar Flare ACTIVE
 * branch: every 4.5–8.5 s, if the player is NOT in asteroid shadow,
 * roll a 30 % chance to flip the thrust vector for 3.2 s.  Hiding in
 * shadow blocks the roll outright — a second-order benefit of cover.
 * Implementation: a sign multiplier applied to the thrust vector in
 * update_player_physics(); rotation input is NOT affected (only the
 * direction the engines push the ship). */
#define REVERSE_DRIFT_DURATION   3.2f   /* seconds of inverted thrust         */
#define REVERSE_DRIFT_ROLL_MIN   4.5f   /* min seconds between confusion rolls*/
#define REVERSE_DRIFT_ROLL_MAX   8.5f   /* max seconds between confusion rolls*/
#define REVERSE_DRIFT_HIT_CHANCE 0.30f  /* per-roll probability of confusion  */
static float reverse_drift_roll_t = 0.0f;    /* countdown to next confusion roll */

/* --- Tox-Gas Hallucinations (Confusion-Visual) status-malfunction state ---
 * Third member of the Solar-Flare status-malfunction trio (alongside Sensor
 * Static and Reverse Drift).  todo.md §6 spec: "Tox-Gas Hallucinations —
 * Space sickness or reactor leaks scramble visual outlines, turning
 * wireframes into shifting geometric patterns (e.g., rendering simple
 * asteroids as elite Cacogens)."  Implementation: per-frame angle + radius
 * jitter applied to every asteroid's draw transform plus a low-alpha cyan
 * triangular "ghost-Cacogen" overlay on roughly one-third of the rocks,
 * so simple medium-size asteroids LOOK like they could be UFOs at a
 * glance.  Bounded entirely to render_entities() — no gameplay collision
 * change.  Same Star-Shadow gating as #35/#36: hiding behind a big rock
 * blocks the roll outright.  Tuned a touch milder than Sensor Static
 * (3.0 s effect, 25 % hit chance, 5–9 s cadence) because purely visual
 * confusion is less actionable than a sensor blackout. */
#define TOX_HALLUCINATION_DURATION   3.0f /* seconds of wireframe scramble    */
#define TOX_HALLUCINATION_ROLL_MIN   5.0f /* min seconds between halluc rolls */
#define TOX_HALLUCINATION_ROLL_MAX   9.0f /* max seconds between halluc rolls */
#define TOX_HALLUCINATION_HIT_CHANCE 0.25f /* per-roll probability of halluc  */
static float tox_hallucination_roll_t = 0.0f; /* countdown to next halluc roll */

/* --- Screen shake --- */
static float screen_shake_timer     = 0.0f;
static float screen_shake_intensity  = 0.0f;

/* --- Menu navigation --- */
static int      menu_selection    = 0;   /* 0=Start  1=High Scores  2=Settings */
static GameState settings_back_state = STATE_TITLE;

/* --- Settings --- */
/* Definitions live in state.c (shared state module); extern here to avoid duplicate symbols */
extern int  settings_volume;
extern int  settings_sfx_vol;
extern int  settings_music_vol;
extern int  settings_dynamic_range;
extern int  settings_mute_unfocused;
static int settings_fullscreen       = 0;
static int settings_glow             = 3;  /* 0=OFF 1=LOW 2=MED 3=HIGH 4=MAX */
static int settings_crt_curve        = 0;  /* CRT glass curvature — off by default (item 31) */
extern int  settings_tab;               /* 0=VIDEO 1=AUDIO 2=GAMEPLAY 3=CONTROLS */
static int settings_screen_shake     = 1;
static int settings_show_fps         = 0;
static int settings_mouse_aim        = 1;
static int settings_crosshair_style  = 1;  /* 0=OFF 1=CROSS 2=DOT */
static int settings_autofire         = 0;
static int settings_controller_deadzone = 1; /* 0=LOW 1=MED 2=HIGH */
static int settings_invert_y         = 0;
static int settings_control_scheme   = 1;  /* 0=ARCADE  1=TWIN_STICK */

/* --- New unified settings (FF7R/CoQ HUD overhaul) --- */
extern Settings  g_settings;
extern int       settings_row;
extern int       settings_keybind_pending;
extern uint32_t  g_world_seed;

/* --- Key binds --- */
static SDL_Scancode keybinds[KB_COUNT] = {
    SDL_SCANCODE_LEFT,   /* KB_ROTATE_LEFT  */
    SDL_SCANCODE_RIGHT,  /* KB_ROTATE_RIGHT */
    SDL_SCANCODE_UP,     /* KB_THRUST       */
    SDL_SCANCODE_SPACE,  /* KB_FIRE         */
    SDL_SCANCODE_ESCAPE, /* KB_PAUSE        */
    SDL_SCANCODE_RETURN, /* KB_HYPERSPACE   */
    SDL_SCANCODE_1,      /* KB_ABILITY1     */
    SDL_SCANCODE_2,      /* KB_ABILITY2     */
    SDL_SCANCODE_3,      /* KB_ABILITY3     */
    SDL_SCANCODE_4,      /* KB_ABILITY4     */
    SDL_SCANCODE_TAB     /* KB_MINIMAP      */
};
static const char *kb_action_names[KB_COUNT] = {
    "ROTATE LEFT", "ROTATE RIGHT", "THRUST", "FIRE",
    "PAUSE", "HYPERSPACE",
    "ABILITY 1", "ABILITY 2", "ABILITY 3", "ABILITY 4",
    "MINIMAP"
};
static int rebinding_action  = -1; /* -1 = not currently rebinding */
static int keybind_selection = 0;
static int keybind_page      = 0;  /* 0=keyboard  1=controller */

/* --- Controller binds --- */
static SDL_GameController *g_controller = NULL;
static SDL_GameControllerButton ctrl_binds[CT_COUNT] = {
    SDL_CONTROLLER_BUTTON_A,             /* CT_THRUST      */
    SDL_CONTROLLER_BUTTON_X,             /* CT_FIRE        */
    SDL_CONTROLLER_BUTTON_Y,             /* CT_HYPERSPACE  */
    SDL_CONTROLLER_BUTTON_B,             /* CT_ABILITY1    */
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,  /* CT_ABILITY2    */
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, /* CT_ABILITY3    */
    SDL_CONTROLLER_BUTTON_MAX,           /* CT_ABILITY4 — unbound */
    SDL_CONTROLLER_BUTTON_BACK           /* CT_MINIMAP     */
};
static const char *ct_action_names[CT_COUNT] = {
    "THRUST", "FIRE", "HYPERSPACE",
    "ABILITY 1", "ABILITY 2", "ABILITY 3", "ABILITY 4",
    "MINIMAP"
};
static int ctrl_rebinding_action = -1;

/* --- Twin-stick fire direction override --- */
static float twin_stick_fire_angle  = 0.0f;
static int   twin_stick_fire_active = 0;

/* --- Resource system --- */
static int res_void_stone     = 0;
static int res_void_steel     = 0;
static int res_autodyne_frags = 0;
static int res_hex_modules    = 0;
static int res_ammo           = 0;
static int res_rockets        = 0;
static int res_contraband     = 0;
static int res_isotopes       = 0;
static int res_coolant        = 0;
static int res_medicinals     = 0;
static int res_biomatter      = 0;
static int res_shield_caps    = 0;
static int res_alien_flora    = 0;
static int chrome             = 0;

/* --- Fuel system --- */
static float fuel_current        = 100.0f;
static float fuel_max            = 100.0f;
static float drift_penalty_timer = 0.0f;   /* seconds in Emergency Drift Mode */

/* --- Player death animation --- */
static Vec2  player_death_pos   = {0.0f, 0.0f};
static float player_death_angle = 0.0f;
static float player_death_timer = 0.0f;

/* --- Shop & warp menu --- */
static int     shop_page       = 0;
static int     shop_sel        = 0;
static WarpLoc warp_locs[MAX_WARP_LOCS];
static int     warp_loc_count  = 0;
static int     warp_menu_sel   = 0;
static float   warp_drive_range = 3000.0f;

/* --- Attract / idle --- */
static int   is_attract_ai      = 0;
static float idle_timer         = 0.0f;

/* --- Mirror image state --- */
static Vec2  mirror_pos         = {0.0f, 0.0f};
static float mirror_angle       = 0.0f;
static int   mirror_active_flag = 0;

/* --- Cheat / debug --- */
static int   god_mode           = 0;
static float god_mode_msg_timer = 0.0f;

/* --- FPS counter --- */
static float fps_accum       = 0.0f;
static int   fps_frame_count = 0;
static int   fps_display_val = 0;

/* --- Enemy wave pacing --- */
static float level_start_timer = 2.0f;
static float ufo_spawn_timer   = 15.0f;
static float beat_timer        = 1.0f;
static int   cur_beat          = 0;

/* --- Edge/XP warning flash --- */
static float edge_flash_timer    = 0.0f;
static float near_miss_cooldown  = 0.0f;

/* --- Warp-drive singularity exit effect ---
 * Counts down from WARP_FLASH_DUR after a warp jump. Drives an expanding
 * vector ring + CRT bloom flash rendered in screen space (see render_overlays). */
static float       warp_flash_timer = 0.0f;
static const float WARP_FLASH_DUR   = 0.65f;

/* --- Respawn animation state --- */
static int   respawn_phase = 0;   /* 0=waiting, 1=blinking-in */
static float respawn_blink = 0.0f;

/* --- Cached asteroid count (written by update_asteroids, read by update_spawning) --- */
static int cached_active_asteroids = 0;

/* --- Void Stone Mechanics --- */
static int time_stop_frames = 0;
#define POS_HISTORY_LEN 100
static Vec2 player_pos_history[POS_HISTORY_LEN];
static int pos_history_idx = 0;
static int check_void_stone(void) {
    if (lives <= 1 && res_void_stone > 0) {
        res_void_stone--;
        time_stop_frames = 30;
        player.pos = player_pos_history[(pos_history_idx + POS_HISTORY_LEN - 100) % POS_HISTORY_LEN];
        player.invuln_timer = 2.0f;
        return 1;
    }
    return 0;
}

/* =========== FORWARD DECLARATIONS =========== */

/* Physics helpers */
static WrapEvent wrap_position_ext(Vec2 *pos, float padding);
static Vec2      calculate_external_forces(Vec2 pos);
static WrapEvent update_physics(Vec2 *pos, Vec2 *vel, float delta_time,
                                float padding, float friction);
static int       check_collision(Vec2 p1, float r1, Vec2 p2, float r2);
static float     distance_sq(Vec2 p1, Vec2 p2);
static void      on_screen_wrap(WrapEvent event);

/* Persistence */
static void load_high_scores(void);
static void save_high_scores(void);
static void save_game(void);
static void load_game(void);

/* Entity helpers */
static void spawn_particles(Vec2 pos, int count, SDL_Color color);
static void spawn_event_float(float x, float y, const char *label, SDL_Color color);
static void init_asteroid_shape(AsteroidEntity *a, int size);
static void spawn_asteroid(Vec2 pos, int size);
static void reset_player(void);
static int  get_zone(Vec2 pos);
static void spawn_npc(Vec2 pos);
static void init_home_area(void);
static void spawn_ufo(void);
static void trigger_hyperspace(void);
static void spawn_orb(Vec2 pos, int value);

/* Progression */
static void spawn_upgrade_options(void);
static void apply_upgrade(UpgradeType type);
static void start_next_level(void);
static void start_new_game(void);

/* Settings */
/** @brief Maps an SDL_BUTTON_* value to a short display label for the settings UI. */
static const char *mouse_btn_label(int btn)
{
    switch (btn) {
        case SDL_BUTTON_LEFT:   return "LMB";
        case SDL_BUTTON_MIDDLE: return "MMB";
        case SDL_BUTTON_RIGHT:  return "RMB";
        case SDL_BUTTON_X1:     return "BTN4";
        case SDL_BUTTON_X2:     return "BTN5";
        default:                return "???";
    }
}

static void settings_adjust(int dir);
Settings settings_defaults(void);
void     settings_save(const Settings *s);
void     settings_load(Settings *s);

/* game_update sub-helpers (extracted for clarity) */
static void update_player_physics(float dt);
static void update_player_bullets(float dt);
static void update_asteroids(float dt);
static void update_ufo(float dt);
static void update_ufo_bullets(float dt);
static void update_particles_orbs_npcs(float dt);
static void update_collisions(float dt);
static void update_spawning(float dt);
static void update_progression(float dt);
static void update_camera_and_audio(float dt);

/* game_render sub-helpers (extracted for clarity) */
static void render_entities(void);
static void render_hud(void);
static void render_minimap(void);
static void render_menus(void);
static void render_overlays(void);
static Vec2 get_controller_button_pos(SDL_GameControllerButton btn,
                                      float cx, float cy);
static void draw_gamepad(float cx, float cy, int highlighted_btn);
static void render_controller_binds_page(float cx, float cy,
                                         int keybind_sel,
                                         SDL_GameControllerButton *ctrl_binds_arg,
                                         const char **ct_action_names_arg);

/* =========== PHYSICS AND MATH HELPERS =========== */

/**
 * @brief Wraps world-space coordinates at screen boundaries with the given
 *        padding margin.  Both axes are checked independently; wrapping on
 *        either axis sets the `wrapped` flag.
 *
 * @param pos     Pointer to the position vector to modify in-place.
 * @param padding Extra pixels beyond the screen edge before the wrap fires,
 *                allowing entities to slide slightly off-screen before
 *                reappearing on the opposite side.
 * @return        WrapEvent whose `wrapped` field is non-zero when a wrap
 *                occurred, `entry_pos` holds the position before wrapping,
 *                and `exit_pos` holds the position after wrapping.
 */
static WrapEvent wrap_position_ext(Vec2 *pos, float padding)
{
    WrapEvent ev = {0, *pos, *pos};

    if (pos->x < -padding)                  { pos->x = SCREEN_WIDTH  + padding; ev.wrapped = 1; }
    if (pos->x > SCREEN_WIDTH  + padding)   { pos->x = -padding;                ev.wrapped = 1; }
    if (pos->y < -padding)                  { pos->y = SCREEN_HEIGHT + padding; ev.wrapped = 1; }
    if (pos->y > SCREEN_HEIGHT + padding)   { pos->y = -padding;                ev.wrapped = 1; }

    ev.exit_pos = *pos;
    return ev;
}

/* -------------------------------------------------------------------------- */

/**
 * @brief Computes the combined gravitational pull acting on a world-space
 *        position from all active rift sources and the Eye of the Void enemy.
 *
 *        Three sources are considered:
 *          - The global void rift (`rift`): pull strength up to 400 units.
 *          - The player-placed rift (`player_rift`): pull strength up to 500.
 *          - The Eye of the Void UFO (`ufo` with size == 5): up to 250 units.
 *        Pull falls off with distance squared and is capped to prevent
 *        runaway acceleration at very close range.
 *
 * @param pos  The world-space position of the entity being affected.
 * @return     Accumulated force vector (units: acceleration per second).
 */
static Vec2 calculate_external_forces(Vec2 pos)
{
    Vec2 force = {0.0f, 0.0f};

    if (rift.active) {
        float dx      = rift.pos.x - pos.x;
        float dy      = rift.pos.y - pos.y;
        float dist_sq = dx * dx + dy * dy;
        if (dist_sq > 1.0f && dist_sq < 800.0f * 800.0f) {
            float dist     = sqrtf(dist_sq);
            float pull_str = 200000.0f / dist_sq;
            if (pull_str > 400.0f) pull_str = 400.0f;
            force.x += (dx / dist) * pull_str;
            force.y += (dy / dist) * pull_str;
        }
    }

    if (player_rift.active) {
        float dx      = player_rift.pos.x - pos.x;
        float dy      = player_rift.pos.y - pos.y;
        float dist_sq = dx * dx + dy * dy;
        if (dist_sq > 1.0f && dist_sq < 600.0f * 600.0f) {
            float dist     = sqrtf(dist_sq);
            float pull_str = 300000.0f / dist_sq;
            if (pull_str > 500.0f) pull_str = 500.0f;
            force.x += (dx / dist) * pull_str;
            force.y += (dy / dist) * pull_str;
        }
    }

    /* Eye of the Void: ufo.size == 5 */
    if (ufo.active && ufo.size == 5) {
        float dx      = ufo.pos.x - pos.x;
        float dy      = ufo.pos.y - pos.y;
        float dist_sq = dx * dx + dy * dy;
        if (dist_sq > 1.0f && dist_sq < 1000.0f * 1000.0f) {
            float dist     = sqrtf(dist_sq);
            float pull_str = 150000.0f / dist_sq;
            if (pull_str > 250.0f) pull_str = 250.0f;
            force.x += (dx / dist) * pull_str;
            force.y += (dy / dist) * pull_str;
        }
    }

    /* Gravity Wells (Black Holes) */
    for (int i = 0; i < MAX_GRAVITY_WELLS; i++) {
        if (gravity_wells[i].active) {
            float dx      = gravity_wells[i].pos.x - pos.x;
            float dy      = gravity_wells[i].pos.y - pos.y;
            float dist_sq = dx * dx + dy * dy;
            if (dist_sq > 1.0f && dist_sq < 2500.0f * 2500.0f) {
                float dist     = sqrtf(dist_sq);
                float pull_str = gravity_wells[i].mass / dist_sq;
                if (pull_str > 500.0f) pull_str = 500.0f;
                force.x += (dx / dist) * pull_str;
                force.y += (dy / dist) * pull_str;
            }
        }
    }

    return force;
}

/* -------------------------------------------------------------------------- */

/**
 * @brief Performs a single physics integration step for one entity.
 *
 *        Order of operations:
 *          1. Accumulate external forces (rifts, Eye of the Void) into velocity.
 *          2. Apply exponential friction decay so that damping is frame-rate
 *             independent (uses powf so that friction == 1.0 means no drag).
 *          3. Integrate velocity into position.
 *          4. Wrap position at screen boundaries and return the wrap event.
 *
 *        Note: oscillatory AI behaviour in Eye of the Void and Eldritch
 *        Tendril enemies uses `game_time` (accumulated frame dt) rather than
 *        SDL_GetTicks() so that pausing and frame-rate changes are handled
 *        correctly.  The pattern `sinf(SDL_GetTicks() / X) * Y * dt` is
 *        replaced with `sinf(game_time * (1000.0f / X)) * Y * dt` in the AI
 *        update code.
 *
 * @param pos         Entity position, modified in-place.
 * @param vel         Entity velocity, modified in-place.
 * @param delta_time  Elapsed time this frame in seconds.
 * @param padding     Wrap padding margin in pixels (see wrap_position_ext).
 * @param friction    Per-tick friction coefficient in [0, 1].  Values below
 *                    1.0 apply drag; 1.0 means frictionless.
 * @return            WrapEvent describing whether and where wrapping occurred.
 */
static WrapEvent update_physics(Vec2 *pos, Vec2 *vel,
                                float delta_time, float padding, float friction)
{
    Vec2 ext_f = calculate_external_forces(*pos);
    vel->x += ext_f.x * delta_time;
    vel->y += ext_f.y * delta_time;

    if (friction < 1.0f) {
        float decay = powf(friction, delta_time * 60.0f);
        vel->x *= decay;
        vel->y *= decay;
    }

    pos->x += vel->x * delta_time;
    pos->y += vel->y * delta_time;

    return wrap_position_ext(pos, padding);
}

/* -------------------------------------------------------------------------- */

/* Forward declarations required by hook functions below. */
static void spawn_particles(Vec2 pos, int count, SDL_Color color);
static void spawn_asteroid(Vec2 pos, int size);
static void spawn_orb(Vec2 pos, int value);

/* -------------------------------------------------------------------------- */

/**
 * @brief Handles asteroid-player collision with the Thermal Hull upgrade.
 *
 *        When the player is moving faster than 120 px/s and the Thermal Hull
 *        upgrade is active, the asteroid is destroyed instead of the player
 *        taking damage.  Fragments, particles, resource drops, and score are
 *        all applied here.
 *
 * @param is_player_asteroid  Non-zero when the collision involves the player
 *                            and an asteroid.
 * @param asteroid_idx        Index into the `asteroids` array for the
 *                            colliding asteroid.
 * @return  1 to cancel default collision logic; 0 to let it proceed normally.
 */
static int on_collision(int is_player_asteroid, int asteroid_idx)
{
    if (is_player_asteroid && player_upgrades.thermal_hull) {
        float speed_sq = player.vel.x * player.vel.x + player.vel.y * player.vel.y;
        if (speed_sq > 120.0f * 120.0f) {
            asteroids[asteroid_idx].active = 0;
            wave_asteroids_destroyed++;
            int next_size = asteroids[asteroid_idx].size - 1;
            spawn_particles(asteroids[asteroid_idx].pos, 25,
                            (SDL_Color){255, 100, 50, 255});
            if (next_size >= 1) {
                spawn_asteroid(asteroids[asteroid_idx].pos, next_size);
                spawn_asteroid(asteroids[asteroid_idx].pos, next_size);
            }
            int orb_val = (asteroids[asteroid_idx].size == 3) ? 20
                        : (asteroids[asteroid_idx].size == 2) ? 10 : 4;
            spawn_orb(asteroids[asteroid_idx].pos, orb_val);

            /* Resource drop on asteroid kill */
            {
                int roll = rand() % 100;
                if (asteroids[asteroid_idx].material == 1 ||
                    asteroids[asteroid_idx].has_shield) {
                    if (roll < 60)      res_void_steel++;
                    else if (roll < 80) res_autodyne_frags++;
                    else                res_hex_modules++;
                } else if (asteroids[asteroid_idx].material == 2) {
                    if (roll < 50)      res_isotopes++;
                    else if (roll < 75) res_shield_caps++;
                    else                res_hex_modules++;
                } else {
                    if (roll < 40)      res_alien_flora++;
                    else if (roll < 55) res_biomatter++;
                    else if (roll < 65) res_ammo++;
                }
            }

            score += 200;
            audio_play(SFX_EXPLOSION_LG);

            player.vel.x      *= -0.5f;
            player.vel.y      *= -0.5f;
            player.invuln_timer = 0.5f;
            return 1;
        }
    }
    return 0;
}

/* -------------------------------------------------------------------------- */

/**
 * @brief Called each time the player wraps across a screen boundary.
 *
 *        When the Singularity Displacer relic is active, a player-placed rift
 *        is spawned at the wrap entry point.  The rift persists for 3 seconds
 *        and exerts gravitational pull on nearby Void Stones and enemies,
 *        rewarding aggressive use of the screen edges.
 *
 * @param event  WrapEvent produced by update_physics / wrap_position_ext,
 *               supplying the pre-wrap entry position.
 */
static void on_screen_wrap(WrapEvent event)
{
    if (player_upgrades.singularity_displacer) {
        player_rift.active      = 1;
        player_rift.pos         = event.entry_pos;
        player_rift.radius      = 20.0f;
        player_rift.pulse_timer = 0.0f;
        player_rift.spawn_timer = 3.0f;
    }
}

/* -------------------------------------------------------------------------- */

/**
 * @brief Circle-circle overlap test.
 *
 * @param p1  Centre of the first circle.
 * @param r1  Radius of the first circle.
 * @param p2  Centre of the second circle.
 * @param r2  Radius of the second circle.
 * @return    1 if the circles overlap, 0 otherwise.
 */
static int check_collision(Vec2 p1, float r1, Vec2 p2, float r2)
{
    float dx      = p1.x - p2.x;
    float dy      = p1.y - p2.y;
    float dist_sq = dx * dx + dy * dy;
    float sum_r   = r1 + r2;
    return dist_sq < (sum_r * sum_r);
}

/* -------------------------------------------------------------------------- */

/**
 * @brief Returns the squared Euclidean distance between two Vec2 points.
 *
 *        Avoids a sqrtf() call, making it suitable for performance-sensitive
 *        comparisons such as range checks and nearest-target queries.
 *        Only call sqrtf() on the result when the actual distance is needed.
 *
 * @param p1  First point.
 * @param p2  Second point.
 * @return    Squared distance (px^2).
 */
static float distance_sq(Vec2 p1, Vec2 p2)
{
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return dx * dx + dy * dy;
}

/* =========== PERSISTENCE =========== */

/**
 * @brief Reads the top-5 high-score table from `highscores.dat` (binary).
 *
 *        If the file does not exist or cannot be opened, a default leaderboard
 *        using FULIGIN lore names is installed in memory so the game always
 *        starts with a populated table.  Default scores descend from 10 000
 *        to 2 000 in steps of 2 000.
 */
static void load_high_scores(void)
{
    FILE *f = fopen("highscores.dat", "rb");
    if (f) {
        fread(high_scores, sizeof(HighScore), 5, f);
        fclose(f);
    } else {
        /* Default leaderboard — FULIGIN lore names */
        strcpy(high_scores[0].initials, "FUL"); high_scores[0].score = 10000;
        strcpy(high_scores[1].initials, "VEX"); high_scores[1].score =  8000;
        strcpy(high_scores[2].initials, "ORM"); high_scores[2].score =  6000;
        strcpy(high_scores[3].initials, "SIB"); high_scores[3].score =  4000;
        strcpy(high_scores[4].initials, "AUT"); high_scores[4].score =  2000;
    }
}

/* -------------------------------------------------------------------------- */

/**
 * @brief Writes the current 5-entry high-score table to `highscores.dat` as
 *        a raw binary dump of HighScore structs.  Silent no-op on I/O error.
 */
static void save_high_scores(void)
{
    FILE *f = fopen("highscores.dat", "wb");
    if (f) {
        fwrite(high_scores, sizeof(HighScore), 5, f);
        fclose(f);
    }
}

/* -------------------------------------------------------------------------- */

/**
 * @brief Serializes the current run state to `savegame.dat`.
 *
 *        Fields written (in order): score, lives, level, player_level,
 *        player_xp, xp_threshold, player_upgrades (full struct),
 *        player.pos, and difficulty.  The file is created or overwritten;
 *        silent no-op if the file cannot be opened.
 */
static void save_game(void)
{
    FILE *f = fopen("savegame.dat", "wb");
    if (!f) return;

    fwrite(&score,           sizeof(int),           1, f);
    fwrite(&lives,           sizeof(int),           1, f);
    fwrite(&level,           sizeof(int),           1, f);
    fwrite(&player_level,    sizeof(int),           1, f);
    fwrite(&player_xp,       sizeof(int),           1, f);
    fwrite(&xp_threshold,    sizeof(int),           1, f);
    fwrite(&player_upgrades, sizeof(PlayerUpgrades),1, f);
    fwrite(&player.pos,      sizeof(Vec2),          1, f);
    fwrite(&difficulty,      sizeof(Difficulty),    1, f);

    fclose(f);
}

/* -------------------------------------------------------------------------- */

/**
 * @brief Deserializes `savegame.dat` and resumes a saved run.
 *
 *        Reads the same fields in the same order as save_game().  After a
 *        successful load the player entity is marked active and the game
 *        transitions directly to STATE_PLAYING.  Silent no-op if the file
 *        does not exist.
 */
static void load_game(void)
{
    FILE *f = fopen("savegame.dat", "rb");
    if (!f) return;

    fread(&score,           sizeof(int),           1, f);
    fread(&lives,           sizeof(int),           1, f);
    fread(&level,           sizeof(int),           1, f);
    fread(&player_level,    sizeof(int),           1, f);
    fread(&player_xp,       sizeof(int),           1, f);
    fread(&xp_threshold,    sizeof(int),           1, f);
    fread(&player_upgrades, sizeof(PlayerUpgrades),1, f);
    fread(&player.pos,      sizeof(Vec2),          1, f);
    fread(&difficulty,      sizeof(Difficulty),    1, f);

    fclose(f);

    player.active = 1;
    game_state    = STATE_PLAYING;
}

/* =========== PARTICLES =========== */

/**
 * @brief Finds free slots in the global particle pool and initialises up to
 *        `count` new particles at the given world-space position.
 *
 *        Each particle is assigned:
 *          - A random direction (uniform over 2π) and speed in [50, 150] px/s.
 *          - A random rotation speed in [-5, +5] rad/s.
 *          - A random scale in [2, 7].
 *          - A random lifetime in [0.5, 1.3] seconds.
 *        Particles whose `life` has reached zero are treated as free slots.
 *        If fewer than `count` free slots exist, as many as possible are used.
 *
 * @param pos    World-space spawn origin (typically an explosion centre).
 * @param count  Desired number of particles to emit.
 * @param color  RGBA tint applied to every spawned particle.
 */
static void spawn_particles(Vec2 pos, int count, SDL_Color color)
{
    int spawned = 0;
    for (int i = 0; i < MAX_PARTICLES && spawned < count; i++) {
        if (particles[i].life <= 0.0f) {
            particles[i].pos      = pos;
            float angle           = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float speed           = 50.0f + 100.0f * ((float)rand() / RAND_MAX);
            particles[i].vel.x    = cosf(angle) * speed;
            particles[i].vel.y    = sinf(angle) * speed;
            particles[i].angle    = angle;
            particles[i].rot_speed= -5.0f + 10.0f * ((float)rand() / RAND_MAX);
            particles[i].scale    =  2.0f +  5.0f * ((float)rand() / RAND_MAX);
            particles[i].max_life =  0.5f +  0.8f * ((float)rand() / RAND_MAX);
            particles[i].life     = particles[i].max_life;
            particles[i].color    = color;
            spawned++;
        }
    }
}

/* =========== ENTITY SPAWNING =========== */

/* ----------- Event Text Floaters ----------- */

/** @brief Spawns a labeled event text pop-up at world position (x, y).
 *  Drifts upward and fades over ~1.5 s. Used for CRIT!, HIT, VENT!, etc.
 *  @param label  Short null-terminated string (max 23 chars).
 *  @param color  Render tint — HUD_CINNABAR for damage, HUD_AMBER for STRIKE!.
 */
static void spawn_event_float(float x, float y, const char *label, SDL_Color color)
{
    for (int i = 0; i < MAX_EVENT_FLOATS; i++) {
        if (!event_floats[i].active) {
            event_floats[i].active   = 1;
            event_floats[i].x        = x;
            event_floats[i].y        = y - 16.0f;
            event_floats[i].vy       = -55.0f;
            event_floats[i].color    = color;
            event_floats[i].life     = 1.5f;
            event_floats[i].max_life = 1.5f;
            int n = 0;
            while (label[n] && n < 23) { event_floats[i].label[n] = label[n]; n++; }
            event_floats[i].label[n] = '\0';
            break;
        }
    }
}

/* ----------- Asteroid Shape & Spawning ----------- */

/** @brief Generates a randomized polygon shape for an asteroid.
 *  Creates 8-12 vertices in a rough circle with random radial perturbation
 *  to give each asteroid a unique silhouette. The Autodyne's pilots learn
 *  to read these shapes like runes. */
static void init_asteroid_shape(AsteroidEntity *ast, int size)
{
    ast->size   = size;
    ast->radius = (size == 3) ? 35.0f : ((size == 2) ? 18.0f : 9.0f);

    int num_points  = 8 + rand() % 5; /* 8 to 12 vertices */
    ast->line_count = num_points;

    Vec2 points[16];
    for (int i = 0; i < num_points; i++) {
        float angle = i * 2.0f * (float)M_PI / num_points;
        float r     = ast->radius * (0.8f + 0.4f * ((float)rand() / RAND_MAX));
        points[i].x = r * cosf(angle);
        points[i].y = r * sinf(angle);
    }

    for (int i = 0; i < num_points; i++) {
        ast->lines[i].p1 = points[i];
        ast->lines[i].p2 = points[(i + 1) % num_points];
    }
}

/** @brief Finds a free slot in the asteroid pool and initializes it.
 *  Asteroids are the Void Stones that fill the drift lanes between zones.
 *  Higher levels increase the chance of shielded (metal) and crystalline variants.
 *  Spawned ASTEROID_SPAWN_RING_MIN to ASTEROID_SPAWN_RING_MAX units from a given position. */
static void spawn_asteroid(Vec2 pos, int size)
{
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) {
            asteroids[i].active = 1;
            asteroids[i].pos    = pos;
            /* Tag as bowling fragment when requested by the collision system */
            bowling_timer[i]    = g_bowling_next_spawn ? 1.5f : 0.0f;

            float angle       = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float speed_scale = 1.0f + (difficulty * 0.3f);
            float speed       = (30.0f + 50.0f * (4 - size) + 10.0f * level) * speed_scale;
            asteroids[i].vel.x    = cosf(angle) * speed;
            asteroids[i].vel.y    = sinf(angle) * speed;
            asteroids[i].angle    = angle;
            asteroids[i].rot_speed =
                (-1.5f + 3.0f * ((float)rand() / RAND_MAX)) * speed_scale;
            init_asteroid_shape(&asteroids[i], size);
            asteroids[i].has_shield =
                (level >= 3 && rand() % 100 < (15 + difficulty * 10));
            /* Material: 0=rock (common), 1=metal, 2=crystal.
             * Chance of rare materials scales with player level. */
            {
                int r = rand() % 100;
                int metal_chance   = 15 + level * 2;
                int crystal_chance = 5  + level;
                if (r < crystal_chance)               asteroids[i].material = 2;
                else if (r < metal_chance + crystal_chance) asteroids[i].material = 1;
                else                                  asteroids[i].material = 0;
            }
            asteroids[i].hit_count = 0;
            /* Metal (1) and crystal (2) size-3 asteroids are tougher — two hits
             * before shattering, giving the player time to see stress cracks. */
            asteroids[i].health = (size == 3
                                   && (asteroids[i].material == 1
                                       || asteroids[i].material == 2)) ? 2 : 1;
            break;
        }
    }
}

/* ----------- Player ----------- */

/** @brief Respawns the Autodyne at a random screen position.
 *  Grants 2 seconds of invulnerability to allow orientation after death. */
static void reset_player(void)
{
    player.active = 1;
    /* Respawn at random position within the current visible screen area */
    float margin  = 80.0f;
    player.pos.x  = camera_pos.x + margin
                    + ((float)rand() / RAND_MAX) * (SCREEN_WIDTH  - margin * 2.0f);
    player.pos.y  = camera_pos.y + margin
                    + ((float)rand() / RAND_MAX) * (SCREEN_HEIGHT - margin * 2.0f);
    player.vel    = (Vec2){0.0f, 0.0f};
    player.angle  = 0.0f;
    player.invuln_timer = 2.0f; /* 2 seconds invulnerability */
    player.emp_lock_timer = 0.0f; /* Item 25: clear EMP lock on respawn */
    player.sensor_static_timer = 0.0f; /* Status: clear blindness on respawn */
    player.reverse_drift_timer = 0.0f; /* Status: clear confusion on respawn */
    player.tox_hallucination_timer = 0.0f; /* Status: clear hallucination on respawn */
}

/* ----------- Zone Classification ----------- */

/** @brief Returns the danger zone (0-4) based on distance from the origin.
 *  Zone 0: Home Space (< ZONE_HOME_RADIUS)  — relative safety near the station.
 *  Zone 1: Inner Belt (< ZONE_INNER_RADIUS) — common Void Stones and saucers.
 *  Zone 2: Deep Void (< ZONE_VOID_RADIUS)   — Eldritch Tendrils emerge here.
 *  Zone 3: The Abyss (< ZONE_ABYSS_RADIUS)  — Daemon Sigils and Ascian patrols.
 *  Zone 4: Deep Drift (beyond)              — bleached endless drift, Lictors hunt here. */
static int get_zone(Vec2 pos)
{
    float d = sqrtf(pos.x * pos.x + pos.y * pos.y);
    if (d < ZONE_HOME_RADIUS)  return 0;
    if (d < ZONE_INNER_RADIUS) return 1;
    if (d < ZONE_VOID_RADIUS)  return 2;
    if (d < ZONE_ABYSS_RADIUS) return 3;
    return 4;
}

/* ----------- NPC & Home Area ----------- */

/** @brief Spawns a friendly drone NPC at the given position.
 *  These sentinels of the Home Station will orbit the Autodyne and warn of danger. */
static void spawn_npc(Vec2 pos)
{
    for (int i = 0; i < MAX_NPC; i++) {
        if (!npcs[i].active) {
            npcs[i].active       = 1;
            npcs[i].following    = 0;
            npcs[i].pos          = pos;
            npcs[i].vel          = (Vec2){0.0f, 0.0f};
            npcs[i].angle        = 0.0f;
            npcs[i].radius       = 10.0f;
            npcs[i].orbit_angle  =
                ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            npcs[i].contact_timer = 0.0f;
            npcs[i].chatter_timer = 1.5f + ((float)rand() / RAND_MAX) * 4.5f;
            return;
        }
    }
}

/** @brief Creates the home station structure and its guardian drones.
 *  The Home Station is the Autodyne's only safe harbor in the drift. */
static void init_home_area(void)
{
    /* Home station structure at world origin */
    structures[0].active    = 1;
    structures[0].type      = 0;
    structures[0].pos       = (Vec2){0.0f, 0.0f};
    structures[0].radius    = 55.0f;
    structures[0].rot_speed = 0.004f;
    structures[0].angle     = 0.0f;

    /* Spawn a couple of friendly drones near home */
    Vec2 d1 = {  200.0f + ((float)rand() / RAND_MAX) * 100.0f,  80.0f };
    Vec2 d2 = { -180.0f - ((float)rand() / RAND_MAX) * 100.0f, -90.0f };
    spawn_npc(d1);
    spawn_npc(d2);
}

/* ----------- UFO / Enemy Vessel ----------- */

/** @brief Selects and spawns an enemy vessel based on the current zone and drift level.
 *  Enemy types escalate with zone depth:
 *    Sizes 1-2: Saucers (Inner Belt and beyond)
 *    Size 3:    Vector Stalker  — kamikaze interceptor with predictive AI
 *    Size 4:    Boundary Weaver — sinusoidal sweep bomber
 *    Size 5:    Eye of the Void — gravitational harvester, pulls all matter
 *    Size 6:    Eldritch Tendril (Zone 2+) — drift-sinusoidal pursuer
 *    Size 7:    Daemon Sigil    (Zone 3+) — teleport-lurching horror
 *  Spawns UFO_SPAWN_RING_MIN to UFO_SPAWN_RING_MAX units from the Autodyne. */
/* Enqueue a Cugel-9 message. Silently drops if cooldown is active. */
static void cugel9_say(const char *msg)
{
    if (cugel9_cooldown > 0.0f) return;
    Cugel9Msg *m = &cugel9_buf[cugel9_head];
    strncpy(m->text, msg, CUGEL9_MSG_LEN - 1);
    m->text[CUGEL9_MSG_LEN - 1] = '\0';
    m->timer        = CUGEL9_SHOW_DUR + 2.0f * CUGEL9_FADE_DUR;
    cugel9_head     = (cugel9_head + 1) % CUGEL9_MAX_MSGS;
    cugel9_cooldown = 1.2f; /* 1.2 s before another message is accepted */
}

static void spawn_ufo(void)
{
    ufo.active = 1;

    /* Zone + level based enemy type selection */
    int r = rand() % 100;
    if (player_zone >= 3 && r < 15) {
        ufo.size   = 7;     /* Daemon Sigil (Zone 3+) */
        ufo.radius = 22.0f;
    } else if (player_zone >= 2 && r < 30) {
        ufo.size   = 6;     /* Eldritch Tendril (Zone 2+) */
        ufo.radius = 18.0f;
    } else if (level >= 4 && r < 45) {
        ufo.size   = 5;     /* Eye of the Void */
        ufo.radius = 24.0f;
    } else if (level >= 3 && r < 60) {
        ufo.size   = 4;     /* Boundary Weaver */
        ufo.radius = 20.0f;
    } else if (level >= 2 && r < 75) {
        ufo.size   = 3;     /* Vector Stalker */
        ufo.radius = 12.0f;
    } else {
        ufo.size   = (rand() % 100 < 40 + level * 5) ? 1 : 2;
        ufo.radius = (ufo.size == 2) ? 16.0f : 8.0f;
    }

    /* Spawn UFO_SPAWN_RING_MIN to UFO_SPAWN_RING_MAX units from the Autodyne */
    float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
    float dist  = UFO_SPAWN_RING_MIN
                  + ((float)rand() / RAND_MAX) * (UFO_SPAWN_RING_MAX - UFO_SPAWN_RING_MIN);
    ufo.pos.x = player.pos.x + cosf(angle) * dist;
    ufo.pos.y = player.pos.y + sinf(angle) * dist;

    /* Set initial velocity generally towards the Autodyne */
    float base_speed = 100.0f + 25.0f * level;
    ufo.vel.x = (ufo.pos.x < player.pos.x) ? base_speed : -base_speed;
    ufo.target_y        = ufo.pos.y;
    ufo.vel.y           = 0.0f;
    ufo.fire_timer      = 1.0f;
    ufo.change_dir_timer = 1.0f;

    /* Setup model lines per vessel type */
    if (ufo.size == 3) {
        ufo.line_count = sizeof(kamikaze_lines) / sizeof(Line);
        for (int i = 0; i < ufo.line_count; i++)
            ufo.lines[i] = kamikaze_lines[i];
    } else if (ufo.size == 4) {
        ufo.line_count = sizeof(bomber_lines) / sizeof(Line);
        for (int i = 0; i < ufo.line_count; i++)
            ufo.lines[i] = bomber_lines[i];
    } else if (ufo.size == 5) {
        ufo.line_count = 0; /* custom procedural rendering */
    } else if (ufo.size == 6) {
        ufo.line_count = sizeof(eldritch_lines) / sizeof(Line);
        for (int i = 0; i < ufo.line_count; i++)
            ufo.lines[i] = eldritch_lines[i];
    } else if (ufo.size == 7) {
        ufo.line_count = sizeof(daemon_lines) / sizeof(Line);
        for (int i = 0; i < ufo.line_count; i++)
            ufo.lines[i] = daemon_lines[i];
    } else {
        ufo.line_count = sizeof(ufo_lines) / sizeof(Line);
        for (int i = 0; i < ufo.line_count; i++)
            ufo.lines[i] = ufo_lines[i];
    }

    audio_play(SFX_UFO_LOOP);
}

/* ----------- Hyperspace ----------- */

/** @brief Executes an emergency hyperspace jump, teleporting the Autodyne to a random position.
 *  Carries a HYPERSPACE_DEATH_CHANCE probability of catastrophic misalignment (instant death).
 *  "To fold space without the Autarch's blessing is to risk annihilation." */
static void trigger_hyperspace(void)
{
    if (!player.active) return;

    spawn_particles(player.pos, 10, (SDL_Color){150, 180, 255, 255});
    audio_play(SFX_EXPLOSION_SM);

    /* Catastrophic misalignment — FULIGIN classic risk */
    if (((float)rand() / RAND_MAX) < HYPERSPACE_DEATH_CHANCE) {
        if (check_void_stone()) return;
        player.active = 0;
        audio_play(SFX_EXPLOSION_LG);
        spawn_particles(player.pos, 30, (SDL_Color){255, 100, 100, 255});
        player_death_pos   = player.pos;
        player_death_angle = player.angle;
        player_death_timer = 1.8f;
        cugel9_say("HYPERSPACE MALFUNCTION. IN RETROSPECT: SHOULD HAVE CHECKED THAT.");
        lives--;
        if (lives <= 0) {
            game_state = STATE_GAMEOVER;
            audio_stop(SFX_THRUST);
            audio_stop(SFX_UFO_LOOP);
        }
        return;
    }

    /* Hyperspace lands within the current visible screen area (not world origin) */
    float margin  = 80.0f;
    player.pos.x  = camera_pos.x + margin
                    + ((float)rand() / RAND_MAX) * (SCREEN_WIDTH  - margin * 2.0f);
    player.pos.y  = camera_pos.y + margin
                    + ((float)rand() / RAND_MAX) * (SCREEN_HEIGHT - margin * 2.0f);
    player.vel          = (Vec2){0.0f, 0.0f};
    player.invuln_timer = 0.5f;

    /* Hook massive white rectangle flash into hyperspace logic */
    hyperjump_flash_timer = 1.0f;
}

/* ----------- Chronicle Orbs ----------- */

/** @brief Spawns a chronicle orb (XP collectible) at the given position.
 *  Chronicle orbs encode fragments of the destroyed vessel's memory. */
static void spawn_orb(Vec2 pos, int value)
{
    for (int i = 0; i < MAX_ORBS; i++) {
        if (!orbs[i].active) {
            orbs[i].active = 1;
            orbs[i].pos    = pos;
            float angle    = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float speed    = 20.0f + 30.0f * ((float)rand() / RAND_MAX);
            orbs[i].vel.x  = cosf(angle) * speed;
            orbs[i].vel.y  = sinf(angle) * speed;
            orbs[i].value  = value;
            orbs[i].life   = 10.0f;
            break;
        }
    }
}

/* =========== PROGRESSION AND UPGRADES =========== */

/** @brief Randomly selects 3 unique relics from the full pool for the player to choose. */
static void spawn_upgrade_options()
{
    audio_stop(SFX_THRUST);
    audio_stop(SFX_UFO_LOOP);

    UpgradeType all_upgrades[UPGRADE_COUNT];
    for (int i = 0; i < UPGRADE_COUNT; i++) all_upgrades[i] = (UpgradeType)i;
    int total_available = UPGRADE_COUNT;

    /* Fisher-Yates partial shuffle — pick 3 unique relics */
    for (int i = 0; i < 3; i++) {
        int idx = i + rand() % (total_available - i);
        UpgradeType temp = all_upgrades[i];
        all_upgrades[i]  = all_upgrades[idx];
        all_upgrades[idx] = temp;
        upgrade_options[i] = all_upgrades[i];
    }
    selected_option = 0;
}

/** @brief Applies the chosen relic's effect to the Autodyne's capabilities.
 *  Each relic permanently alters player_upgrades until the run ends. */
static void apply_upgrade(UpgradeType type)
{
    switch (type) {
        /* relic: fires two additional forward bolts per shot */
        case UPGRADE_TRIPLE_SHOT:
            player_upgrades.triple_shot = 1;
            break;

        /* relic: grants one additional ricochet bounce to all projectiles */
        case UPGRADE_BOUNCE:
            player_upgrades.max_bounces += 1;
            break;

        /* relic: activates a regenerating energy barrier around the hull */
        case UPGRADE_SHIELD:
            player_upgrades.shield_active = 1;
            break;

        /* relic: reduces the weapon cycle cooldown by 30% */
        case UPGRADE_RAPID_FIRE:
            player_upgrades.fire_cooldown_mult *= 0.7f;
            break;

        /* relic: rounds pass through targets without being consumed */
        case UPGRADE_PIERCING:
            player_upgrades.piercing = 1;
            break;

        /* relic: boosts thruster output, increasing top speed by 20% */
        case UPGRADE_SPEED:
            player_upgrades.speed_mult *= 1.2f;
            break;

        /* relic: compresses hull cross-section, shrinking collision radius by 20% */
        case UPGRADE_SIZE_DOWN:
            player_upgrades.size_mult *= 0.8f;
            player.radius  *= 0.8f;
            break;

        /* relic: rounds acquire and track the nearest hostile */
        case UPGRADE_HOMING:
            player_upgrades.homing = 1;
            break;

        /* relic: extends the XP orb attraction field by 50 units */
        case UPGRADE_ORB_MAGNET:
            player_upgrades.magnet_radius += 50.0f;
            break;

        /* relic: mounts a rearward-facing cannon on the stern */
        case UPGRADE_REAR_GUN:
            player_upgrades.rear_gun = 1;
            break;

        /* relic: each shot fans into three diverging bolts on impact */
        case UPGRADE_SPLIT_SHOT:
            player_upgrades.split_shot = 1;
            break;

        /* relic: drift-core resonance multiplies XP yield by 50% */
        case UPGRADE_XP_BOOST:
            player_upgrades.xp_mult += 0.5f;
            break;

        /* relic: emergency life support restoration — grants +1 hull integrity */
        case UPGRADE_HEALTH_BOOST:
            lives++;
            break;

        /* relic: projects a phantom duplicate that mirrors all fire */
        case UPGRADE_MIRROR_IMAGE:
            player_upgrades.mirror_image = 1;
            break;

        /* relic: brief temporal displacement on collision — blinks through matter */
        case UPGRADE_PHASE_SHIFT:
            player_upgrades.phase_shift = 1;
            break;

        /* relic: hull plating absorbs heat, rendering fire damage negligible */
        case UPGRADE_THERMAL_HULL:
            player_upgrades.thermal_hull = 1;
            break;

        /* relic: warps a nearby foe to a random position in the drift */
        case UPGRADE_SINGULARITY_DISPLACER:
            player_upgrades.singularity_displacer = 1;
            break;

        /* relic: releases a lashing singularity tendril on near-miss */
        case UPGRADE_SINGULARITY_WHIP:
            player_upgrades.singularity_whip = 1;
            break;

        /* relic: destroyed targets trigger a chain detonation cascade */
        case UPGRADE_RESONANCE_CASCADE:
            player_upgrades.resonance_cascade = 1;
            break;

        /* relic: secondary weapon — launches a void vortex grenade */
        case UPGRADE_VORTEX_GRENADE:
            player_upgrades.vortex_grenade = 1;
            break;

        /* relic: deploys a self-targeting gun platform that orbits the ship */
        case UPGRADE_AUTO_TURRET:
            player_upgrades.auto_turret = 1;
            break;

        /* relic: each kill triggers a short-range plasma nova burst */
        case UPGRADE_NOVA_EXPLOSION:
            player_upgrades.nova_explosion = 1;
            break;

        default:
            break;
    }
}

/** @brief Advances to the next drift level, clearing entities and spawning new formations. */
static void start_next_level()
{
    level++;
    int count = 10 + level * 3 + (difficulty * 3);
    if (count > MAX_ASTEROIDS - 4) count = MAX_ASTEROIDS - 4;

    /* Deactivate all bullets and particles */
    for (int i = 0; i < MAX_BULLETS; i++)     bullets[i].active    = 0;
    for (int i = 0; i < MAX_UFO_BULLETS; i++) ufo_bullets[i].active = 0;
    for (int i = 0; i < MAX_PARTICLES; i++)   particles[i].life    = 0.0f;
    for (int i = 0; i < MAX_GRAVITY_WELLS; i++) gravity_wells[i].active = 0;

    ufo.active = 0;
    audio_stop(SFX_UFO_LOOP);

    /* Spawn new Gravity Wells (Black Holes) starting at level 2 */
    if (level >= 2) {
        int gw_count = level / 2;
        if (gw_count > MAX_GRAVITY_WELLS) gw_count = MAX_GRAVITY_WELLS;
        for (int i = 0; i < gw_count; i++) {
            gravity_wells[i].active = 1;
            float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float dist  = 600.0f + ((float)rand() / RAND_MAX) * 2000.0f;
            gravity_wells[i].pos.x = player.pos.x + cosf(angle) * dist;
            gravity_wells[i].pos.y = player.pos.y + sinf(angle) * dist;
            gravity_wells[i].mass = 400000.0f + ((float)rand() / RAND_MAX) * 200000.0f;
            gravity_wells[i].radius = 35.0f;
        }
    }

    /* Spawn new asteroid formation at safe distance from the Autodyne */
    WorldRegion region;
    wb_init_region(&region, g_settings.world.asteroid_density, g_settings.world.enemy_density, g_settings.gameplay.starting_lives);
    wb_generate_chondrite_gyre(&region, 8000, 8000);

    for (int i = 0; i < MAX_CHONDRITES; i++) {
        if (region.asteroids[i].active) {
            Vec2 pos = {
                player.pos.x + region.asteroids[i].position.x - 4000.0f,
                player.pos.y + region.asteroids[i].position.y - 4000.0f
            };
            spawn_asteroid(pos, 3);
        }
    }

    /* Spawn Anomalous Void Rift starting at level 4 */
    if (level >= 4) {
        rift.active = 1;
        float angle  = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
        float dist   = 450.0f + ((float)rand() / RAND_MAX) * 300.0f;
        rift.pos.x   = player.pos.x + cosf(angle) * dist;
        rift.pos.y   = player.pos.y + sinf(angle) * dist;
        rift.radius  = 35.0f;
        rift.angle1  = 0.0f;
        rift.angle2  = 0.0f;
        rift.pulse_timer = 0.0f;
        rift.spawn_timer = 5.0f;
    } else {
        rift.active = 0;
    }

    level_start_timer = 1.5f;
    ufo_spawn_timer   = 15.0f + ((float)rand() / RAND_MAX) * 15.0f;
    cugel9_say("SECTOR SCAN COMPLETE. THREATS IDENTIFIED. SURVIVAL ODDS: CLASSIFIED.");
}

/** @brief Full reset of all game state for a new run.
 *  Clears all entity pools, resets score/lives/level, initializes the home area. */
static void start_new_game()
{
    score                     = 0;
    lives                     = 3;
    level                     = 0; /* becomes 1 on first start_next_level() */
    total_asteroids_destroyed = 0;
    player_level              = 1;
    player_xp                 = 0;
    xp_threshold              = 100;

    /* Clear entity pools */
    for (int i = 0; i < MAX_ASTEROIDS; i++)    asteroids[i].active    = 0;
    for (int i = 0; i < MAX_BULLETS; i++)      bullets[i].active      = 0;
    for (int i = 0; i < MAX_UFO_BULLETS; i++)  ufo_bullets[i].active  = 0;
    for (int i = 0; i < MAX_PARTICLES; i++)    particles[i].life      = 0.0f;
    for (int i = 0; i < MAX_ORBS; i++)         orbs[i].active         = 0;
    for (int i = 0; i < MAX_SCORE_FLOATS; i++) score_floats[i].active = 0;
    for (int i = 0; i < MAX_EVENT_FLOATS; i++)  event_floats[i].active  = 0;
    for (int i = 0; i < MAX_GRAVITY_WELLS; i++) gravity_wells[i].active = 0;

    combo_count         = 0;
    combo_timer         = 0.0f;
    bowling_chain_count = 0;
    for (int ci = 0; ci < CUGEL9_MAX_MSGS; ci++) cugel9_buf[ci].timer = 0.0f;
    cugel9_head = 0; cugel9_cooldown = 0.0f;
    bowling_chain_timer = 0.0f;
    for (int i = 0; i < MAX_ASTEROIDS; i++) bowling_timer[i] = 0.0f;
    ufo.active  = 0;
    wave_asteroids_destroyed = 0;
    wave_cleared_pending     = 0;
    camera_pos               = (Vec2){0.0f, 0.0f};

    /* Init zone structures and NPCs */
    for (int i = 0; i < MAX_STRUCTURE; i++) structures[i].active = 0;
    for (int i = 0; i < MAX_NPC; i++)       npcs[i].active       = 0;
    init_home_area();

    /* Reset all relic upgrades */
    player_upgrades.triple_shot         = 0;
    player_upgrades.max_bounces         = 0;
    player_upgrades.fire_cooldown_mult  = 1.0f;
    player_upgrades.bullet_speed_mult   = 1.0f;
    player_upgrades.shield_active       = 0;
    player_upgrades.speed_mult          = 1.0f;
    player_upgrades.piercing            = 0;
    player_upgrades.size_mult           = 1.0f;
    player_upgrades.homing              = 0;
    player_upgrades.magnet_radius       = 60.0f;
    player_upgrades.rear_gun            = 0;
    player_upgrades.split_shot          = 0;
    player_upgrades.xp_mult             = 1.0f;
    player_upgrades.mirror_image        = 0;
    player_upgrades.phase_shift         = 0;
    player_upgrades.vortex_grenade      = 0;
    player_upgrades.auto_turret         = 0;
    player_upgrades.nova_explosion      = 0;
    mirror_active_flag                  = 0;

    /* Clear rift and screen-shake state */
    rift.active            = 0;
    screen_shake_timer     = 0.0f;
    screen_shake_intensity = 0.0f;
    vg_set_shake(0, 0);

    audio_stop(SFX_THRUST);
    audio_stop(SFX_UFO_LOOP);

    /* Reset resource pools */
    res_void_steel    = 0; res_autodyne_frags = 0; res_hex_modules = 0;
    res_ammo          = 0; res_rockets        = 0; res_contraband  = 0;
    res_isotopes      = 0; res_coolant        = 0; res_medicinals  = 0;
    res_biomatter     = 0; res_shield_caps    = 0; res_alien_flora = 0;
    fuel_current = fuel_max = 100.0f;
    drift_penalty_timer = 0.0f;

    /* Populate warp loci — home base + 4 zone beacons */
    warp_loc_count = 0;
    /* Warp loci — 10x scale to match the expanded universe (was max 3500u). */
    warp_locs[warp_loc_count++] = (WarpLoc){{      0.0f,      0.0f }, "HOME STATION"};
    warp_locs[warp_loc_count++] = (WarpLoc){{  20000.0f,   5000.0f }, "SCRAP FIELDS"};
    warp_locs[warp_loc_count++] = (WarpLoc){{ -18000.0f,  12000.0f }, "VOID REACHES"};
    warp_locs[warp_loc_count++] = (WarpLoc){{  35000.0f, -20000.0f }, "IRON SHOALS" };
    warp_locs[warp_loc_count++] = (WarpLoc){{ -30000.0f, -15000.0f }, "DEEP DRIFT"  };
    warp_menu_sel = 0;

    reset_player();
    start_next_level();
    game_state = STATE_PLAYING;
}

/* =========== SETTINGS FUNCTIONS =========== */

Settings settings_defaults(void)
{
    Settings s = {0};
    s.video.fullscreen        = 0;
    s.video.resolution_idx    = 0;
    s.video.vsync             = 1;
    s.video.refresh_rate      = 0;
    s.graphics.glow_intensity = 2;
    s.graphics.particle_count = 32;
    s.graphics.scanlines      = 1;
    s.graphics.vignette       = 1;
    s.graphics.screen_shake   = 1;
    s.audio.master_vol        = 80;
    s.audio.music_vol         = 70;
    s.audio.sfx_vol           = 80;
    s.audio.ui_vol            = 60;
    s.controls.mouse_aim         = 1;
    s.controls.mouse_sensitivity  = 100;
    s.controls.mouse_fire_btn     = SDL_BUTTON_LEFT;   /* LMB → shoot     */
    s.controls.mouse_thrust_btn   = SDL_BUTTON_RIGHT;  /* RMB → thrust    */
    s.controls.mouse_hyper_btn    = SDL_BUTTON_MIDDLE; /* MMB → hyperspace */
    s.controls.autofire       = 0;
    s.hud.show_fps            = 0;
    s.hud.show_minimap        = 1;
    s.hud.crosshair           = CROSSHAIR_CHEVRON;
    s.hud.hud_scale           = 100;
    s.hud.show_combo          = 1;
    s.hud.show_zone_name      = 1;
    s.accessibility.colorblind      = COLORBLIND_NONE;
    s.accessibility.font_size_delta = 0;
    s.gameplay.starting_lives = 3;
    s.gameplay.difficulty     = DIFF_STANDARD;
    s.world.seed              = 0;
    s.world.asteroid_density  = 1.0f;
    s.world.enemy_density     = 1.0f;
    s.world.loot_multiplier   = 1.0f;
    s.show_intro              = 1;
    return s;
}

void settings_save(const Settings *s)
{
    FILE *f = fopen("fuligin.cfg", "w");
    if (!f) return;
    fprintf(f, "glow=%d\n",          s->graphics.glow_intensity);
    fprintf(f, "particles=%d\n",     s->graphics.particle_count);
    fprintf(f, "scanlines=%d\n",     s->graphics.scanlines);
    fprintf(f, "vignette=%d\n",      s->graphics.vignette);
    fprintf(f, "screen_shake=%d\n",  s->graphics.screen_shake);
    fprintf(f, "master_vol=%d\n",    s->audio.master_vol);
    fprintf(f, "music_vol=%d\n",     s->audio.music_vol);
    fprintf(f, "sfx_vol=%d\n",       s->audio.sfx_vol);
    fprintf(f, "mouse_aim=%d\n",        s->controls.mouse_aim);
    fprintf(f, "sensitivity=%d\n",      s->controls.mouse_sensitivity);
    fprintf(f, "mouse_fire_btn=%d\n",   s->controls.mouse_fire_btn);
    fprintf(f, "mouse_thrust_btn=%d\n", s->controls.mouse_thrust_btn);
    fprintf(f, "mouse_hyper_btn=%d\n",  s->controls.mouse_hyper_btn);
    fprintf(f, "show_fps=%d\n",      s->hud.show_fps);
    fprintf(f, "show_minimap=%d\n",  s->hud.show_minimap);
    fprintf(f, "crosshair=%d\n",     (int)s->hud.crosshair);
    fprintf(f, "hud_scale=%d\n",     s->hud.hud_scale);
    fprintf(f, "colorblind=%d\n",    (int)s->accessibility.colorblind);
    fprintf(f, "high_contrast=%d\n", s->accessibility.high_contrast);
    fprintf(f, "font_delta=%d\n",    s->accessibility.font_size_delta);
    fprintf(f, "reduce_motion=%d\n", s->accessibility.reduce_motion);
    fprintf(f, "lives=%d\n",         s->gameplay.starting_lives);
    fprintf(f, "difficulty=%d\n",    (int)s->gameplay.difficulty);
    fprintf(f, "world_seed=%u\n",    s->world.seed);
    fprintf(f, "ast_density=%.2f\n", s->world.asteroid_density);
    fprintf(f, "enemy_density=%.2f\n", s->world.enemy_density);
    fprintf(f, "loot_mult=%.2f\n",   s->world.loot_multiplier);
    fprintf(f, "zone_sharp=%d\n",    s->world.zone_sharpness);
    fprintf(f, "vsync=%d\n",         s->video.vsync);
    fprintf(f, "fullscreen=%d\n",    s->video.fullscreen);
    fprintf(f, "show_intro=%d\n",    s->show_intro);
    fclose(f);
}

void settings_load(Settings *s)
{
    *s = settings_defaults();
    FILE *f = fopen("fuligin.cfg", "r");
    if (!f) return;
    char key[64], val[64];
    while (fscanf(f, "%63[^=]=%63s\n", key, val) == 2) {
        if      (!strcmp(key, "glow"))          s->graphics.glow_intensity = atoi(val);
        else if (!strcmp(key, "particles"))     s->graphics.particle_count = atoi(val);
        else if (!strcmp(key, "scanlines"))     s->graphics.scanlines      = atoi(val);
        else if (!strcmp(key, "vignette"))      s->graphics.vignette       = atoi(val);
        else if (!strcmp(key, "screen_shake"))  s->graphics.screen_shake   = atoi(val);
        else if (!strcmp(key, "master_vol"))    s->audio.master_vol        = atoi(val);
        else if (!strcmp(key, "music_vol"))     s->audio.music_vol         = atoi(val);
        else if (!strcmp(key, "sfx_vol"))       s->audio.sfx_vol           = atoi(val);
        else if (!strcmp(key, "mouse_aim"))        s->controls.mouse_aim            = atoi(val);
        else if (!strcmp(key, "sensitivity"))      s->controls.mouse_sensitivity    = atoi(val);
        else if (!strcmp(key, "mouse_fire_btn"))   s->controls.mouse_fire_btn       = atoi(val);
        else if (!strcmp(key, "mouse_thrust_btn")) s->controls.mouse_thrust_btn     = atoi(val);
        else if (!strcmp(key, "mouse_hyper_btn"))  s->controls.mouse_hyper_btn      = atoi(val);
        else if (!strcmp(key, "show_fps"))      s->hud.show_fps            = atoi(val);
        else if (!strcmp(key, "show_minimap"))  s->hud.show_minimap        = atoi(val);
        else if (!strcmp(key, "crosshair"))     s->hud.crosshair           = (CrosshairStyle)atoi(val);
        else if (!strcmp(key, "hud_scale"))     s->hud.hud_scale           = atoi(val);
        else if (!strcmp(key, "colorblind"))    s->accessibility.colorblind = (ColorblindMode)atoi(val);
        else if (!strcmp(key, "high_contrast")) s->accessibility.high_contrast = atoi(val);
        else if (!strcmp(key, "font_delta"))    s->accessibility.font_size_delta = atoi(val);
        else if (!strcmp(key, "reduce_motion")) s->accessibility.reduce_motion = atoi(val);
        else if (!strcmp(key, "lives"))         s->gameplay.starting_lives = atoi(val);
        else if (!strcmp(key, "difficulty"))    s->gameplay.difficulty     = (Diff)atoi(val);
        else if (!strcmp(key, "world_seed"))    s->world.seed              = (uint32_t)strtoul(val, NULL, 10);
        else if (!strcmp(key, "ast_density"))   s->world.asteroid_density  = (float)atof(val);
        else if (!strcmp(key, "enemy_density")) s->world.enemy_density     = (float)atof(val);
        else if (!strcmp(key, "loot_mult"))     s->world.loot_multiplier   = (float)atof(val);
        else if (!strcmp(key, "zone_sharp"))    s->world.zone_sharpness    = atoi(val);
        else if (!strcmp(key, "vsync"))         s->video.vsync             = atoi(val);
        else if (!strcmp(key, "fullscreen"))    s->video.fullscreen        = atoi(val);
        else if (!strcmp(key, "show_intro"))    s->show_intro              = atoi(val);
    }
    fclose(f);
}

/* =========== GAME INIT =========== */

/** @brief One-time initialization called after SDL is ready.
 *  Loads persistent data (high scores), builds the ship's line geometry,
 *  and detects connected game controllers. */
void game_init()
{
    load_high_scores();
    vf_init();
    drone_chatter_init();   /* Item 27: pet shield-drone chatter pool */
    rustweaver_init();      /* Item 23: rust-weaver corrosive-spit drone pool */
    ascian_init();          /* Item 21: ascian voiceless polygon-patrol interceptors */
    lictor_init();          /* Item 22: lictor elite pursuit interceptors          */
    emp_sentinel_init();    /* Item 25: emp sentinels — status-disruption units    */
    scavenger_init();       /* Item 24: scavenger probes — void-steel theft units  */

    /* Define the Autodyne's silhouette from its static line segments */
    player.line_count = sizeof(ship_lines) / sizeof(Line);
    for (int i = 0; i < player.line_count; i++) {
        player.lines[i] = ship_lines[i];
    }
    player.radius     = 8.0f;
    player.active     = 0;
    player.emp_lock_timer = 0.0f;
    player.sensor_static_timer = 0.0f;
    player.reverse_drift_timer = 0.0f;
    player.tox_hallucination_timer = 0.0f;
    player.trail_head = 0;
    for (int i = 0; i < PHOS_TRAIL_LEN; i++) {
        player.trail_pos[i] = (Vec2){0.0f, 0.0f};
        player.trail_ang[i] = 0.0f;
    }

    /* Detect and open game controller at startup */
    int has_controller  = 0;
    int num_joysticks   = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; i++) {
        if (SDL_IsGameController(i)) {
            if (!g_controller) {
                g_controller = SDL_GameControllerOpen(i);
            }
            if (g_controller) {
                has_controller = 1;
                break;
            }
        }
    }
    settings_mouse_aim = has_controller ? 0 : 1;

    /* Load unified settings and propagate to legacy globals for backwards compat */
    settings_load(&g_settings);
    settings_glow         = g_settings.graphics.glow_intensity;
    settings_show_fps     = g_settings.hud.show_fps;
    if (g_settings.controls.mouse_aim || !has_controller)
        settings_mouse_aim = g_settings.controls.mouse_aim;
    settings_screen_shake = g_settings.graphics.screen_shake;
    settings_fullscreen   = g_settings.video.fullscreen;
    if (g_settings.gameplay.starting_lives > 0)
        lives = g_settings.gameplay.starting_lives;
    g_world_seed = g_settings.world.seed;

    /* Solar Flare Cycles: roll the first interval up-front so flares
     * never fire instantly at startup.  The state machine in
     * update_progression() will count this down and transition to
     * WARNING → ACTIVE → COUNTUP again. */
    {
        float t = (float)rand() / (float)RAND_MAX;
        solar_flare_next_iv = SOLAR_FLARE_MIN_INTERVAL
            + t * (SOLAR_FLARE_MAX_INTERVAL - SOLAR_FLARE_MIN_INTERVAL);
        solar_flare_timer    = solar_flare_next_iv;
        solar_flare_warn_t   = 0.0f;
        solar_flare_active_t = 0.0f;
        solar_flare_in_shadow = 0;
    }
}

/** @brief Mutates a settings value based on the current selection and direction (+1/-1). */
static void settings_adjust(int dir)
{
    int r = settings_row;
    #define TOGGLE(x)   ((x) = !(x))
    #define CLAMP_ADJ(x, lo, hi) \
        do { if (dir < 0) { if ((x) > (lo)) (x)--; } \
             else          { if ((x) < (hi)) (x)++; } } while (0)

    if (settings_tab == 0) { /* VIDEO */
        if      (r == 0) {
            TOGGLE(g_settings.video.fullscreen);
            settings_fullscreen = g_settings.video.fullscreen;
            SDL_SetWindowFullscreen(g_window,
                g_settings.video.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        } else if (r == 1) {
            CLAMP_ADJ(g_settings.video.refresh_rate, 30, 240);
        } else if (r == 2) {
            TOGGLE(g_settings.video.vsync);
        }

    } else if (settings_tab == 1) { /* GRAPHICS */
        if      (r == 0) {
            CLAMP_ADJ(g_settings.graphics.glow_intensity, 0, 4);
            settings_glow = g_settings.graphics.glow_intensity;
        } else if (r == 1) {
            CLAMP_ADJ(g_settings.graphics.particle_count, 0, 64);
        } else if (r == 2) {
            TOGGLE(g_settings.graphics.scanlines);
        } else if (r == 3) {
            TOGGLE(g_settings.graphics.vignette);
        } else if (r == 4) {
            TOGGLE(g_settings.graphics.motion_blur);
        } else if (r == 5) {
            TOGGLE(g_settings.graphics.screen_shake);
            settings_screen_shake = g_settings.graphics.screen_shake;
        }

    } else if (settings_tab == 2) { /* AUDIO */
        if      (r == 0) {
            CLAMP_ADJ(g_settings.audio.master_vol, 0, 100);
            audio_set_volume(g_settings.audio.master_vol);
            settings_volume = g_settings.audio.master_vol;
        } else if (r == 1) {
            CLAMP_ADJ(g_settings.audio.music_vol, 0, 100);
            settings_music_vol = g_settings.audio.music_vol;
        } else if (r == 2) {
            CLAMP_ADJ(g_settings.audio.sfx_vol, 0, 100);
            settings_sfx_vol = g_settings.audio.sfx_vol;
        } else if (r == 3) {
            CLAMP_ADJ(g_settings.audio.ui_vol, 0, 100);
        } else if (r == 4) {
            TOGGLE(g_settings.audio.streamer_mode);
            settings_mute_unfocused = g_settings.audio.streamer_mode;
        }

    } else if (settings_tab == 3) { /* CONTROLS */
        if      (r == 0) {
            TOGGLE(g_settings.controls.mouse_aim);
            settings_mouse_aim = g_settings.controls.mouse_aim;
        } else if (r == 1) {
            CLAMP_ADJ(g_settings.controls.mouse_sensitivity, 1, 10);
        } else if (r == 2) {
            TOGGLE(g_settings.controls.autofire);
            settings_autofire = g_settings.controls.autofire;
        } else if (r == 3) {
            TOGGLE(g_settings.controls.aim_assist);
        } else if (r == 4) {
            CLAMP_ADJ(g_settings.controls.ctrl_deadzone, 0, 2);
            settings_controller_deadzone = g_settings.controls.ctrl_deadzone;
        } else if (r == 5) {
            TOGGLE(g_settings.controls.invert_y);
            settings_invert_y = g_settings.controls.invert_y;
        } else if (r == 6 || r == 7 || r == 8) {
            /* Cycle mouse fire/thrust/hyper button through L/M/R.
             * Each button must be unique; skip values already used by
             * the other two bindings to prevent conflicts. */
            static const int mouse_btn_cycle[] = {
                SDL_BUTTON_LEFT, SDL_BUTTON_MIDDLE, SDL_BUTTON_RIGHT,
                SDL_BUTTON_X1,   SDL_BUTTON_X2
            };
            const int cycle_len = (int)(sizeof(mouse_btn_cycle) / sizeof(mouse_btn_cycle[0]));
            int *target = (r == 6) ? &g_settings.controls.mouse_fire_btn
                        : (r == 7) ? &g_settings.controls.mouse_thrust_btn
                                   : &g_settings.controls.mouse_hyper_btn;
            /* Find current position in cycle table. */
            int cur = 0;
            for (int ci = 0; ci < cycle_len; ci++)
                if (mouse_btn_cycle[ci] == *target) { cur = ci; break; }
            /* Advance (direction comes from the caller via the sign on `dir`). */
            cur = (cur + dir + cycle_len) % cycle_len;
            *target = mouse_btn_cycle[cur];
        }
        /* r == 9: KEYBINDS — handled in caller */

    } else if (settings_tab == 4) { /* HUD */
        if      (r == 0) {
            TOGGLE(g_settings.hud.show_fps);
            settings_show_fps = g_settings.hud.show_fps;
        } else if (r == 1) {
            TOGGLE(g_settings.hud.show_minimap);
        } else if (r == 2) {
            CLAMP_ADJ(g_settings.hud.crosshair, 0, CROSSHAIR_COUNT - 1);
            settings_crosshair_style = (int)g_settings.hud.crosshair;
        } else if (r == 3) {
            CLAMP_ADJ(g_settings.hud.hud_scale, 50, 200);
        } else if (r == 4) {
            TOGGLE(g_settings.hud.show_combo);
        } else if (r == 5) {
            TOGGLE(g_settings.hud.show_zone_name);
        }

    } else if (settings_tab == 5) { /* ACCESSIBILITY */
        if      (r == 0) {
            CLAMP_ADJ(g_settings.accessibility.colorblind, 0,
                      COLORBLIND_COUNT - 1);
        } else if (r == 1) {
            TOGGLE(g_settings.accessibility.high_contrast);
        } else if (r == 2) {
            CLAMP_ADJ(g_settings.accessibility.font_size_delta, -4, 4);
        } else if (r == 3) {
            TOGGLE(g_settings.accessibility.reduce_motion);
        }

    } else if (settings_tab == 6) { /* GAMEPLAY */
        if      (r == 0) {
            CLAMP_ADJ(g_settings.gameplay.difficulty, 0, DIFF_COUNT - 1);
            /* Mirror to legacy difficulty variable */
            difficulty = (int)g_settings.gameplay.difficulty;
        } else if (r == 1) {
            CLAMP_ADJ(g_settings.gameplay.starting_lives, 1, 9);
        }

    } else if (settings_tab == 7) { /* WORLD */
        if      (r == 0) {
            g_settings.world.seed += (uint32_t)(dir * 1);
            g_world_seed = g_settings.world.seed;
        } else if (r == 1) {
            float v = g_settings.world.asteroid_density + dir * 0.1f;
            if (v < 0.1f) v = 0.1f;
            if (v > 3.0f) v = 3.0f;
            g_settings.world.asteroid_density = v;
        } else if (r == 2) {
            float v = g_settings.world.enemy_density + dir * 0.1f;
            if (v < 0.0f) v = 0.0f;
            if (v > 3.0f) v = 3.0f;
            g_settings.world.enemy_density = v;
        } else if (r == 3) {
            float v = g_settings.world.loot_multiplier + dir * 0.1f;
            if (v < 0.1f) v = 0.1f;
            if (v > 5.0f) v = 5.0f;
            g_settings.world.loot_multiplier = v;
        } else if (r == 4) {
            CLAMP_ADJ(g_settings.world.zone_sharpness, 0, 10);
        } else if (r == 5) {
            CLAMP_ADJ(g_settings.world.starting_zone, 0, 3);
        }

    } else if (settings_tab == 8) { /* SYSTEM */
        if (r == 0) {
            TOGGLE(g_settings.show_intro);
        }
        /* r == 1: RESET DEFAULTS — handled in caller */
        /* r == 2: SAVE & QUIT — handled in caller */
    }

    #undef TOGGLE
    #undef CLAMP_ADJ
}

/** @brief Pauses or resumes the simulation.
 *  Stops looping audio (thrust, UFO) when pausing; restores on resume. */
void game_set_paused(int paused)
{
    if (game_state == STATE_PLAYING && paused) {
        game_state = STATE_PAUSED;
        audio_stop(SFX_THRUST);
        audio_stop(SFX_UFO_LOOP);
    } else if (game_state == STATE_PAUSED && !paused) {
        game_state = STATE_PLAYING;
        if (ufo.active) {
            audio_play(SFX_UFO_LOOP);
        }
    }
}

/* =========== INPUT SUB-HANDLERS =========== */

/** @brief Handles input during active gameplay (STATE_PLAYING). */
static void handle_input_playing(SDL_Event *event)
{
    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            game_set_paused(1);
        } else if (event->key.keysym.scancode == keybinds[KB_MINIMAP]) {
            minimap_visible = !minimap_visible;
        } else if ((event->key.keysym.sym == SDLK_RETURN ||
                    event->key.keysym.sym == SDLK_DOWN) && player.active) {
            trigger_hyperspace();
        } else if (event->key.keysym.sym == SDLK_b && player.active) {
            float _dx = player.pos.x, _dy = player.pos.y;
            if (sqrtf(_dx * _dx + _dy * _dy) < 600.0f) {
                game_state = STATE_SHOP;
                shop_sel   = 0;
            }
        }
    } else if (event->type == SDL_MOUSEBUTTONDOWN && player.active) {
        /* Use the user-configured hyperspace mouse button (default: middle). */
        if (event->button.button == g_settings.controls.mouse_hyper_btn) {
            trigger_hyperspace();
        }
    } else if (event->type == SDL_CONTROLLERBUTTONDOWN) {
        SDL_GameControllerButton btn = event->cbutton.button;
        if (btn == SDL_CONTROLLER_BUTTON_START)      game_set_paused(1);
        if (btn == ctrl_binds[CT_HYPERSPACE])        trigger_hyperspace();
        if (btn == ctrl_binds[CT_MINIMAP])           minimap_visible = !minimap_visible;
    }
}

/** @brief Handles input on the title screen (STATE_TITLE). */
static void handle_input_title(SDL_Event *event)
{
    if (event->type == SDL_KEYDOWN) {
        /* Ctrl+F5: enter attract/AI gameplay mode (FULIGIN demo loop) */
        if (event->key.keysym.sym == SDLK_F5 && (SDL_GetModState() & KMOD_CTRL)) {
            is_attract_ai = 1;
            start_new_game();
            game_state = STATE_ATTRACT_GAMEPLAY;
            idle_timer  = 0.0f;
            return;
        }
        if (event->key.keysym.sym == SDLK_UP   || event->key.keysym.sym == SDLK_w)
            menu_selection = (menu_selection + 2) % 3;
        if (event->key.keysym.sym == SDLK_DOWN  || event->key.keysym.sym == SDLK_s)
            menu_selection = (menu_selection + 1) % 3;
        if (event->key.keysym.sym == SDLK_SPACE || event->key.keysym.sym == SDLK_RETURN) {
            if (menu_selection == 0) start_new_game();
            else if (menu_selection == 1) game_state = STATE_HIGHSCORES;
            else if (menu_selection == 2) {
                settings_back_state = STATE_TITLE;
                game_state          = STATE_SETTINGS;
                menu_selection      = 0;
                settings_tab        = 0;
            }
        }
        if (event->key.keysym.sym == SDLK_ESCAPE) {
            SDL_Event qe; qe.type = SDL_QUIT; SDL_PushEvent(&qe);
        }
    } else if (event->type == SDL_CONTROLLERBUTTONDOWN) {
        SDL_GameControllerButton btn = event->cbutton.button;
        if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP)
            menu_selection = (menu_selection + 2) % 3;
        if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
            menu_selection = (menu_selection + 1) % 3;
        if (btn == SDL_CONTROLLER_BUTTON_A || btn == SDL_CONTROLLER_BUTTON_START) {
            if (menu_selection == 0) start_new_game();
            else if (menu_selection == 1) game_state = STATE_HIGHSCORES;
            else if (menu_selection == 2) {
                settings_back_state = STATE_TITLE;
                game_state          = STATE_SETTINGS;
                menu_selection      = 0;
                settings_tab        = 0;
            }
        }
        if (btn == SDL_CONTROLLER_BUTTON_B) {
            SDL_Event qe; qe.type = SDL_QUIT; SDL_PushEvent(&qe);
        }
    }
}

/** @brief Handles input on menus (settings, keybinds, high scores, paused, game-over, etc.). */
static void handle_input_menus(SDL_Event *event)
{
    if (event->type == SDL_KEYDOWN) {
        if (game_state == STATE_PAUSED) {
            SDL_Keycode sym = event->key.keysym.sym;
            if (sym == SDLK_ESCAPE || sym == SDLK_SPACE || sym == SDLK_RETURN) {
                game_set_paused(0);
            } else if (sym == SDLK_s) {
                save_game();
            } else if (sym == SDLK_l) {
                load_game();
            } else if (sym == SDLK_p) {
                settings_back_state = STATE_PAUSED;
                game_state          = STATE_SETTINGS;
                menu_selection      = 0;
                settings_tab        = 0;
            } else if (sym == SDLK_q) {
                game_state = STATE_TITLE;
                audio_stop(SFX_THRUST);
                audio_stop(SFX_UFO_LOOP);
            }
        } else if (game_state == STATE_SETTINGS) {
            /* Max row index per tab (0-based, inclusive) */
            const int tab_maxrow[] = {2, 5, 4, 9, 5, 3, 1, 5, 2};
            const int tab_count_s  = 9;
            int max_sel = tab_maxrow[settings_tab];

            SDL_Keycode sym = event->key.keysym.sym;
            if (sym == SDLK_q) {
                settings_tab = (settings_tab + tab_count_s - 1) % tab_count_s;
                settings_row = 0; menu_selection = 0;
            }
            if (sym == SDLK_e) {
                settings_tab = (settings_tab + 1) % tab_count_s;
                settings_row = 0; menu_selection = 0;
            }
            if (sym == SDLK_UP   || sym == SDLK_w) {
                settings_row = (settings_row == 0) ? 0 : settings_row - 1;
                menu_selection = settings_row;
            }
            if (sym == SDLK_DOWN || sym == SDLK_s) {
                if (settings_row < max_sel) settings_row++;
                menu_selection = settings_row;
            }
            if (sym == SDLK_LEFT  || sym == SDLK_a) settings_adjust(-1);
            if (sym == SDLK_RIGHT || sym == SDLK_d) settings_adjust(1);
            if (sym == SDLK_RETURN || sym == SDLK_SPACE) {
                /* CONTROLS tab, KEYBINDS row */
                if (settings_tab == 3 && settings_row == 9) {
                    game_state            = STATE_KEYBINDS;
                    keybind_selection     = 0;
                    keybind_page          = 0;
                    rebinding_action      = -1;
                    ctrl_rebinding_action = -1;
                /* SYSTEM tab — RESET DEFAULTS row */
                } else if (settings_tab == 8 && settings_row == 1) {
                    g_settings = settings_defaults();
                /* SYSTEM tab — SAVE & QUIT row */
                } else if (settings_tab == 8 && settings_row == 2) {
                    settings_save(&g_settings);
                    game_state = settings_back_state;
                    settings_row = 0; menu_selection = 0;
                } else {
                    settings_adjust(1);
                }
            }
            if (sym == SDLK_ESCAPE) {
                settings_save(&g_settings);
                game_state = settings_back_state;
                settings_row = 0; menu_selection = 0;
            }
        } else if (game_state == STATE_KEYBINDS) {
            if (rebinding_action >= 0) {
                SDL_Scancode sc = event->key.keysym.scancode;
                if (sc != SDL_SCANCODE_ESCAPE) keybinds[rebinding_action] = sc;
                rebinding_action = -1;
            } else {
                int page_count = (keybind_page == 0) ? KB_COUNT : CT_COUNT;
                SDL_Keycode sym = event->key.keysym.sym;
                if (sym == SDLK_UP   || sym == SDLK_w)
                    keybind_selection = (keybind_selection + page_count - 1) % page_count;
                if (sym == SDLK_DOWN || sym == SDLK_s)
                    keybind_selection = (keybind_selection + 1) % page_count;
                if (sym == SDLK_q || sym == SDLK_e) {
                    keybind_page          = 1 - keybind_page;
                    keybind_selection     = 0;
                    rebinding_action      = -1;
                    ctrl_rebinding_action = -1;
                }
                if (sym == SDLK_RETURN || sym == SDLK_SPACE) {
                    if (keybind_page == 0) rebinding_action      = keybind_selection;
                    else                   ctrl_rebinding_action = keybind_selection;
                }
                if (sym == SDLK_ESCAPE) {
                    game_state            = STATE_SETTINGS;
                    menu_selection        = 0;
                    settings_tab          = 3;
                    rebinding_action      = -1;
                    ctrl_rebinding_action = -1;
                }
            }
        } else if (game_state == STATE_GAMEOVER) {
            SDL_Keycode sym = event->key.keysym.sym;
            if (sym == SDLK_ESCAPE || sym == SDLK_SPACE || sym == SDLK_RETURN) {
                int is_hs = 0;
                for (int i = 0; i < 5; i++) {
                    if (score > high_scores[i].score) { is_hs = 1; new_high_score_idx = i; break; }
                }
                if (is_hs) {
                    game_state       = STATE_NEW_HIGHSCORE;
                    strcpy(temp_initials, "A  ");
                    cur_initial_char = 0;
                } else {
                    game_state = STATE_HIGHSCORES;
                }
            }
        } else if (game_state == STATE_NEW_HIGHSCORE) {
            SDL_Keycode sym = event->key.keysym.sym;
            if (sym == SDLK_RETURN || sym == SDLK_KP_ENTER) {
                high_scores[new_high_score_idx].score = score;
                strcpy(high_scores[new_high_score_idx].initials, temp_initials);
                save_high_scores();
                game_state = STATE_HIGHSCORES;
            } else if (sym == SDLK_BACKSPACE) {
                if (cur_initial_char > 0) {
                    cur_initial_char--;
                    temp_initials[cur_initial_char] = ' ';
                }
            } else if (sym >= SDLK_a && sym <= SDLK_z) {
                if (cur_initial_char < 3) {
                    temp_initials[cur_initial_char] = 'A' + (sym - SDLK_a);
                    cur_initial_char++;
                }
            }
        } else if (game_state == STATE_HIGHSCORES) {
            SDL_Keycode sym = event->key.keysym.sym;
            if (sym == SDLK_ESCAPE || sym == SDLK_SPACE || sym == SDLK_RETURN) {
                game_state = STATE_TITLE;
            }
        } else if (game_state == STATE_WARP_MENU) {
            SDL_Keycode sym = event->key.keysym.sym;
            if (sym == SDLK_ESCAPE) {
                game_state = STATE_PLAYING;
            } else if ((sym == SDLK_UP || sym == SDLK_w) && warp_loc_count > 0) {
                warp_menu_sel = (warp_menu_sel + warp_loc_count - 1) % warp_loc_count;
            } else if ((sym == SDLK_DOWN || sym == SDLK_s) && warp_loc_count > 0) {
                warp_menu_sel = (warp_menu_sel + 1) % warp_loc_count;
            } else if (sym == SDLK_RETURN && warp_loc_count > 0) {
                int   _wi    = warp_menu_sel;
                float _wdx   = warp_locs[_wi].pos.x - player.pos.x;
                float _wdy   = warp_locs[_wi].pos.y - player.pos.y;
                float _wdist = sqrtf(_wdx * _wdx + _wdy * _wdy);
                if (_wdist <= warp_drive_range && fuel_current >= 20.0f) {
                    player.pos   = warp_locs[_wi].pos;
                    player.vel   = (Vec2){0.0f, 0.0f};
                    fuel_current -= 20.0f;
                    camera_pos.x = player.pos.x - SCREEN_WIDTH  * 0.5f;
                    camera_pos.y = player.pos.y - SCREEN_HEIGHT * 0.5f;
                    vg_set_shake(6.0f, 0.3f);
                    warp_flash_timer = WARP_FLASH_DUR;   /* trigger singularity exit FX */
                    game_state   = STATE_PLAYING;
                }
            }
        }
    }

    if (event->type == SDL_CONTROLLERBUTTONDOWN) {
        SDL_GameControllerButton btn = event->cbutton.button;
        if (game_state == STATE_PAUSED) {
            if (btn == SDL_CONTROLLER_BUTTON_START || btn == SDL_CONTROLLER_BUTTON_B)
                game_set_paused(0);
            else if (btn == SDL_CONTROLLER_BUTTON_Y || btn == SDL_CONTROLLER_BUTTON_BACK) {
                settings_back_state = STATE_PAUSED;
                game_state          = STATE_SETTINGS;
                menu_selection      = 0;
                settings_tab        = 0;
            }
        } else if (game_state == STATE_SETTINGS) {
            const int tab_maxrow_c[] = {2, 5, 4, 9, 5, 3, 1, 5, 2};
            const int tab_count_c    = 9;
            int max_sel = tab_maxrow_c[settings_tab];

            if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                settings_row = (settings_row == 0) ? 0 : settings_row - 1;
                menu_selection = settings_row;
            }
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                if (settings_row < max_sel) settings_row++;
                menu_selection = settings_row;
            }
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT)  settings_adjust(-1);
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) settings_adjust(1);
            if (btn == SDL_CONTROLLER_BUTTON_A) {
                if (settings_tab == 3 && settings_row == 9) {
                    game_state            = STATE_KEYBINDS;
                    keybind_selection     = 0;
                    keybind_page          = 0;
                    rebinding_action      = -1;
                    ctrl_rebinding_action = -1;
                } else if (settings_tab == 8 && settings_row == 1) {
                    g_settings = settings_defaults();
                } else if (settings_tab == 8 && settings_row == 2) {
                    settings_save(&g_settings);
                    game_state = settings_back_state;
                    settings_row = 0; menu_selection = 0;
                } else {
                    settings_adjust(1);
                }
            }
            if (btn == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) {
                settings_tab = (settings_tab + tab_count_c - 1) % tab_count_c;
                settings_row = 0; menu_selection = 0;
            }
            if (btn == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                settings_tab = (settings_tab + 1) % tab_count_c;
                settings_row = 0; menu_selection = 0;
            }
            if (btn == SDL_CONTROLLER_BUTTON_B || btn == SDL_CONTROLLER_BUTTON_START) {
                settings_save(&g_settings);
                game_state     = settings_back_state;
                settings_row   = 0; menu_selection = 0;
            }
        } else if (game_state == STATE_KEYBINDS) {
            if (ctrl_rebinding_action >= 0) {
                if (btn != SDL_CONTROLLER_BUTTON_START && btn != SDL_CONTROLLER_BUTTON_BACK)
                    ctrl_binds[ctrl_rebinding_action] = btn;
                ctrl_rebinding_action = -1;
            } else {
                int page_count = (keybind_page == 0) ? KB_COUNT : CT_COUNT;
                if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP)
                    keybind_selection = (keybind_selection + page_count - 1) % page_count;
                else if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
                    keybind_selection = (keybind_selection + 1) % page_count;
                else if (btn == SDL_CONTROLLER_BUTTON_A) {
                    if (keybind_page == 0) rebinding_action      = keybind_selection;
                    else                   ctrl_rebinding_action = keybind_selection;
                } else if (btn == SDL_CONTROLLER_BUTTON_LEFTSHOULDER ||
                           btn == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
                    keybind_page          = 1 - keybind_page;
                    keybind_selection     = 0;
                    rebinding_action      = -1;
                    ctrl_rebinding_action = -1;
                } else if (btn == SDL_CONTROLLER_BUTTON_B) {
                    game_state     = STATE_SETTINGS;
                    menu_selection = 0;
                }
            }
        } else if (game_state == STATE_GAMEOVER) {
            if (btn == SDL_CONTROLLER_BUTTON_A || btn == SDL_CONTROLLER_BUTTON_START) {
                int is_hs = 0;
                for (int i = 0; i < 5; i++) {
                    if (score > high_scores[i].score) { is_hs = 1; new_high_score_idx = i; break; }
                }
                if (is_hs) {
                    game_state       = STATE_NEW_HIGHSCORE;
                    strcpy(temp_initials, "A  ");
                    cur_initial_char = 0;
                } else {
                    game_state = STATE_HIGHSCORES;
                }
            }
        } else if (game_state == STATE_HIGHSCORES) {
            if (btn == SDL_CONTROLLER_BUTTON_B || btn == SDL_CONTROLLER_BUTTON_START)
                game_state = STATE_TITLE;
        }
    }
}

/** @brief Handles input during the upgrade selection screen (STATE_UPGRADE_SELECT). */
static void handle_input_upgrade(SDL_Event *event)
{
    if (event->type == SDL_CONTROLLERBUTTONDOWN) {
        SDL_GameControllerButton btn = event->cbutton.button;
        if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT ||
            btn == SDL_CONTROLLER_BUTTON_DPAD_UP)
            selected_option = (selected_option + 2) % 3;
        if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT ||
            btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
            selected_option = (selected_option + 1) % 3;
        if (btn == SDL_CONTROLLER_BUTTON_A) {
            apply_upgrade(upgrade_options[selected_option]);
            if (wave_cleared_pending) { wave_cleared_pending = 0; start_next_level(); }
            game_state = is_attract_ai ? STATE_ATTRACT_GAMEPLAY : STATE_PLAYING;
            if (ufo.active) audio_play(SFX_UFO_LOOP);
        }
    }
}

/** @brief Handles input in the shop overlay (STATE_SHOP). */
static void handle_input_shop(SDL_Event *event)
{
    if (event->type != SDL_KEYDOWN) return;

    SDL_Keycode sym = event->key.keysym.sym;
    if (sym == SDLK_ESCAPE) {
        game_state = STATE_PLAYING;
    } else if (sym == SDLK_UP   || sym == SDLK_w) {
        shop_sel = (shop_sel + 15) % 16;
    } else if (sym == SDLK_DOWN || sym == SDLK_s) {
        shop_sel = (shop_sel + 1) % 16;
    } else if (sym == SDLK_RETURN || sym == SDLK_SPACE) {
        switch (shop_sel) {
            case  0: if (res_void_steel >= 3)
                         { res_void_steel -= 3; fuel_current = fuel_max; } break;
            case  1: if (res_autodyne_frags >= 4)
                         { res_autodyne_frags -= 4; res_ammo += 100; } break;
            case  2: if (res_hex_modules >= 2)
                         { res_hex_modules -= 2; res_rockets += 5; } break;
            case  3: if (res_void_steel >= 5 && res_hex_modules >= 1)
                         { res_void_steel -= 5; res_hex_modules -= 1;
                           player_upgrades.shield_active = 1; } break;
            case  4: if (res_isotopes >= 3)
                         { res_isotopes -= 3; player_upgrades.speed_mult *= 1.1f; } break;
            case  5: if (res_coolant >= 3)
                         { res_coolant -= 3; player_upgrades.fire_cooldown_mult *= 0.9f; } break;
            case  6: if (res_contraband >= 1)
                         { res_contraband -= 1; score += 5000; } break;
            case  7: if (res_void_steel >= 8)
                         { res_void_steel -= 8; player_upgrades.rear_gun = 1; } break;
            case  8: if (res_hex_modules >= 4)
                         { res_hex_modules -= 4; player_upgrades.split_shot = 1; } break;
            case  9: if (res_isotopes >= 6)
                         { res_isotopes -= 6; player_upgrades.phase_shift = 1; } break;
            case 10: if (res_coolant >= 2)
                         { res_coolant -= 2; fuel_max += 50.0f; fuel_current += 50.0f; } break;
            case 11: if (res_contraband >= 2)
                         { res_contraband -= 2; res_ammo += 20; res_rockets += 3; } break;
            case 12: if (res_biomatter >= 5)
                         { res_biomatter -= 5; res_void_steel += 5; } break;
            case 13: if (res_alien_flora >= 3)
                         { res_alien_flora -= 3; player_upgrades.magnet_radius += 100.0f; } break;
            case 14: if (chrome >= 1)
                         { chrome -= 1; apply_upgrade(UPGRADE_TRIPLE_SHOT); } break;
            case 15: if (chrome >= 1)
                         { chrome -= 1; apply_upgrade(UPGRADE_SHIELD); } break;
            default: break;
        }
    }
}

/* =========== INPUT DISPATCHER =========== */

/** @brief Top-level input dispatcher. Routes SDL events to the handler for the active game state.
 *  Handles keyboard, mouse, and SDL_GameController input. */
void game_handle_input(SDL_Event *event)
{
    idle_timer = 0.0f; /* reset attract-mode idle timer on any input */

    /* Attract mode: almost any input snaps back to the title screen */
    if (game_state == STATE_ATTRACT_INSTRUCTIONS ||
        game_state == STATE_ATTRACT_GAMEPLAY) {
        if (event->type == SDL_KEYDOWN) {
            SDL_Keycode _asym = event->key.keysym.sym;
            if (_asym == SDLK_ESCAPE && game_state == STATE_ATTRACT_GAMEPLAY) {
                game_state = STATE_PAUSED;
                is_attract_ai = 0;
                audio_stop(SFX_THRUST);
                audio_stop(SFX_UFO_LOOP);
                return;
            }
            /* Ignore bare modifier keys */
            if (_asym == SDLK_LCTRL  || _asym == SDLK_RCTRL  ||
                _asym == SDLK_LSHIFT || _asym == SDLK_RSHIFT  ||
                _asym == SDLK_LALT   || _asym == SDLK_RALT    ||
                _asym == SDLK_LGUI   || _asym == SDLK_RGUI) {
                return;
            }
            /* Ctrl+1..9: debug relic cheat codes (attract / dev only) */
            if ((SDL_GetModState() & KMOD_CTRL) &&
                (_asym >= SDLK_1 && _asym <= SDLK_9)) {
                switch ((int)(_asym - SDLK_1)) {
                    case 0: player_upgrades.triple_shot       = 1;          break;
                    case 1: player_upgrades.homing            = 1;          break;
                    case 2: player_upgrades.shield_active     = 1;          break;
                    case 3: player_upgrades.speed_mult       *= 1.25f;      break;
                    case 4: player_upgrades.fire_cooldown_mult *= 0.75f;    break;
                    case 5: player_upgrades.piercing          = 1;          break;
                    case 6: player_upgrades.mirror_image      = 1;          break;
                    case 7: player_upgrades.resonance_cascade = 1;          break;
                    case 8: player_upgrades.thermal_hull      = 1;          break;
                }
                return;
            }
        } else if (event->type == SDL_MOUSEBUTTONDOWN ||
                   event->type == SDL_CONTROLLERBUTTONDOWN) {
            /* Mouse click or controller button press exits attract mode */
        } else {
            /* Ignore motion, releases, axis events, etc. */
            return;
        }
        game_state    = STATE_TITLE;
        is_attract_ai = 0;
        audio_stop(SFX_THRUST);
        audio_stop(SFX_UFO_LOOP);
        return;
    }

    /* Controller hotplug */
    if (event->type == SDL_CONTROLLERDEVICEADDED) {
        if (!g_controller && SDL_IsGameController(event->cdevice.which)) {
            g_controller = SDL_GameControllerOpen(event->cdevice.which);
            if (g_controller) settings_mouse_aim = 0;
        }
        return;
    }
    if (event->type == SDL_CONTROLLERDEVICEREMOVED) {
        if (g_controller) {
            SDL_Joystick *joy = SDL_GameControllerGetJoystick(g_controller);
            if (joy && SDL_JoystickInstanceID(joy) == event->cdevice.which) {
                SDL_GameControllerClose(g_controller);
                g_controller = NULL;
                settings_mouse_aim = 1;
                /* Attempt to re-open another connected controller */
                int num_joysticks = SDL_NumJoysticks();
                for (int i = 0; i < num_joysticks; i++) {
                    if (SDL_IsGameController(i)) {
                        g_controller = SDL_GameControllerOpen(i);
                        if (g_controller) { settings_mouse_aim = 0; break; }
                    }
                }
            }
        }
        return;
    }

    /* Ctrl+F8: toggle god mode (global) */
    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_F8 &&
        (SDL_GetModState() & KMOD_CTRL)) {
        god_mode           = !god_mode;
        god_mode_msg_timer = 2.0f;
        audio_play(SFX_EXPLOSION_LG);
    }

    /* Route to the appropriate sub-handler */
    switch (game_state) {
        case STATE_PLAYING:
            handle_input_playing(event);
            break;
        case STATE_TITLE:
            handle_input_title(event);
            break;
        case STATE_UPGRADE_SELECT:
            handle_input_upgrade(event);
            break;
        case STATE_SHOP:
            handle_input_shop(event);
            break;
        default:
            handle_input_menus(event);
            break;
    }
}

/* =========== GAME UPDATE =========== */

/*
 * game_update() has been decomposed into ten focused static helpers.
 * Each helper owns one subsystem and operates on the shared file-scope
 * game state declared in part 1.  game_update() itself is a thin
 * dispatcher that calls them in the correct dependency order.
 *
 * Dependency order (top to bottom):
 *   player_physics  → player_bullets (needs current player pos/vel)
 *   asteroids       → (independent, but reads player pos for respawn ring)
 *   ufo / ufo_bullets → (reads player pos; fires after asteroids so the
 *                        near-miss check in player_physics sees fresh data)
 *   particles_orbs_npcs → (reads player pos; updates XP collection)
 *   collisions      → (all positions must be final for this frame)
 *   spawning        → (may activate new entities; safe after collisions)
 *   progression     → (score / XP deltas are complete after collisions)
 *   camera_and_audio → (final step: uses updated game_state, player pos)
 */

/* ------------------------------------------------------------------ */
/* Forward declarations for sub-helpers (definitions follow below)    */
/* ------------------------------------------------------------------ */
/* (These match the forward declarations already emitted in part 1.)  */

/* ================================================================== */

/** @brief Updates player thrust, rotation, and hyperspace input state.
 *
 *  Handles both keyboard/mouse and gamepad input for all three control
 *  schemes (arcade, twin-stick).  Applies thrust force, external
 *  gravitational forces, friction, mirror-image positioning, auto-turret
 *  tick, and invulnerability blink.  Also manages the respawn countdown
 *  when the player is not active.
 */
static void update_player_physics(float dt)
{
    if (player.active) {
        int thrust_key_down = 0;
        int fire_key_down   = 0;
        float move_x = 0.0f;
        float move_y = 0.0f;
        /* Item 25: snapshot pre-input heading so EMP lock can snap-restore
         * it after the input phase has already mutated player.angle.  We
         * use this even when emp_lock_timer == 0 so the cost is just one
         * float copy — the actual snap-restore is gated below. */
        float snap_emp_angle = player.angle;

        /* ── Attract-mode AI pilot ─────────────────────────────────── */
        if (game_state == STATE_ATTRACT_GAMEPLAY) {
            float closest_dist = 9999999.0f;
            int   closest_idx  = -1;
            for (int i = 0; i < MAX_ASTEROIDS; i++) {
                if (asteroids[i].active) {
                    float dx = asteroids[i].pos.x - player.pos.x;
                    float dy = asteroids[i].pos.y - player.pos.y;
                    float d2 = dx*dx + dy*dy;
                    if (d2 < closest_dist) {
                        closest_dist = d2;
                        closest_idx  = i;
                    }
                }
            }
            if (closest_idx != -1) {
                float dx = asteroids[closest_idx].pos.x - player.pos.x;
                float dy = asteroids[closest_idx].pos.y - player.pos.y;
                player.angle = atan2f(dy, dx) + (float)M_PI_2;
                if (closest_dist > 150.0f * 150.0f) thrust_key_down = 1;
                if (closest_dist < 300.0f * 300.0f) fire_key_down   = 1;
            }
        } else {
            /* ── Human input ───────────────────────────────────────── */
            const Uint8 *keys = SDL_GetKeyboardState(NULL);
            int mx, my;
            Uint32 mouse_buttons = SDL_GetMouseState(&mx, &my);

            int   right_stick_active = 0;
            float rx_val = 0.0f, ry_val = 0.0f;

            /* Gamepad */
            if (g_controller) {
                float deadzone =
                    (float[]){0.1f, 0.2f, 0.35f}[settings_controller_deadzone]
                    * 32767.0f;
                int16_t lt =
                    SDL_GameControllerGetAxis(g_controller,
                                             SDL_CONTROLLER_AXIS_TRIGGERLEFT);
                int16_t rt =
                    SDL_GameControllerGetAxis(g_controller,
                                             SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

                if (settings_control_scheme == 0) {
                    /* Arcade: left-stick steers, triggers fire/thrust */
                    int16_t lx =
                        SDL_GameControllerGetAxis(g_controller,
                                                 SDL_CONTROLLER_AXIS_LEFTX);
                    if (fabsf((float)lx) > deadzone)
                        player.angle +=
                            ((float)lx / 32767.0f) * ROTATION_SPEED * dt * 2.5f;
                    if (lt > 8000
                        || SDL_GameControllerGetButton(
                               g_controller, ctrl_binds[CT_THRUST]))
                        thrust_key_down = 1;
                    if (rt > 8000
                        || SDL_GameControllerGetButton(
                               g_controller, ctrl_binds[CT_FIRE]))
                        fire_key_down = 1;
                    twin_stick_fire_active = 0;
                } else {
                    /* Twin-stick: left stick moves, right stick aims/fires */
                    int16_t lx =
                        SDL_GameControllerGetAxis(g_controller,
                                                 SDL_CONTROLLER_AXIS_LEFTX);
                    int16_t ly =
                        SDL_GameControllerGetAxis(g_controller,
                                                 SDL_CONTROLLER_AXIS_LEFTY);
                    float lmag = sqrtf((float)lx*(float)lx
                                       + (float)ly*(float)ly);
                    if (lmag > deadzone) {
                        move_x = (float)lx / 32767.0f;
                        move_y = (float)ly / 32767.0f;
                        thrust_key_down = 1;
                    }

                    int16_t rx =
                        SDL_GameControllerGetAxis(g_controller,
                                                 SDL_CONTROLLER_AXIS_RIGHTX);
                    int16_t ry =
                        SDL_GameControllerGetAxis(g_controller,
                                                 SDL_CONTROLLER_AXIS_RIGHTY);
                    float rmag = sqrtf((float)rx*(float)rx
                                       + (float)ry*(float)ry);
                    if (rmag > deadzone * 1.2f) {
                        rx_val = (float)rx / rmag;
                        ry_val = (float)ry / rmag;
                        right_stick_active    = 1;
                        twin_stick_fire_angle = atan2f((float)rx, -(float)ry);
                        twin_stick_fire_active = 1;
                        fire_key_down = 1;
                    } else {
                        twin_stick_fire_active = 0;
                    }

                    if (lt > 8000
                        || SDL_GameControllerGetButton(
                               g_controller, ctrl_binds[CT_THRUST]))
                        thrust_key_down = 1;
                    if (rt > 8000
                        || SDL_GameControllerGetButton(
                               g_controller, ctrl_binds[CT_FIRE]))
                        fire_key_down = 1;
                }
            }

            /* Keyboard / mouse */
            if (settings_control_scheme == 0) {
                /* Arcade keyboard: rotate left/right */
                if (keys[keybinds[KB_ROTATE_LEFT]]  || keys[SDL_SCANCODE_A])
                    player.angle -= ROTATION_SPEED * dt;
                if (keys[keybinds[KB_ROTATE_RIGHT]] || keys[SDL_SCANCODE_D])
                    player.angle += ROTATION_SPEED * dt;
                if (keys[keybinds[KB_THRUST]] || keys[SDL_SCANCODE_W]
                    || (settings_mouse_aim
                        && (mouse_buttons & SDL_BUTTON(g_settings.controls.mouse_thrust_btn))))
                    thrust_key_down = 1;
                if (keys[keybinds[KB_FIRE]] || keys[SDL_SCANCODE_SPACE]
                    || (settings_mouse_aim
                        && (mouse_buttons & SDL_BUTTON(g_settings.controls.mouse_fire_btn))))
                    fire_key_down = 1;

                if (settings_mouse_aim) {
                    float spx = player.pos.x - camera_pos.x;
                    float spy = player.pos.y - camera_pos.y;
                    float dx = (float)mx - spx;
                    float dy = (float)my - spy;
                    if (dx*dx + dy*dy > 9.0f)
                        player.angle = atan2f(dy, dx) + (float)M_PI_2;
                }
            } else {
                /* Twin-stick keyboard: WASD translates, mouse aims */
                float kb_x = 0.0f, kb_y = 0.0f;
                if (keys[SDL_SCANCODE_A] || keys[keybinds[KB_ROTATE_LEFT]])
                    kb_x -= 1.0f;
                if (keys[SDL_SCANCODE_D] || keys[keybinds[KB_ROTATE_RIGHT]])
                    kb_x += 1.0f;
                if (keys[SDL_SCANCODE_W] || keys[keybinds[KB_THRUST]])
                    kb_y -= 1.0f;
                if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])
                    kb_y += 1.0f;

                if (kb_x != 0.0f || kb_y != 0.0f) {
                    float len = sqrtf(kb_x*kb_x + kb_y*kb_y);
                    move_x = kb_x / len;
                    move_y = kb_y / len;
                    thrust_key_down = 1;
                }
                if (keys[keybinds[KB_FIRE]] || keys[SDL_SCANCODE_SPACE])
                    fire_key_down = 1;

                int   mouse_aim_active = 0;
                float mouse_dx = 0.0f, mouse_dy = 0.0f;
                if (settings_mouse_aim) {
                    float spx = player.pos.x - camera_pos.x;
                    float spy = player.pos.y - camera_pos.y;
                    float dx = (float)mx - spx;
                    float dy = (float)my - spy;
                    if (dx*dx + dy*dy > 9.0f) {
                        mouse_dx = dx;
                        mouse_dy = dy;
                        mouse_aim_active = 1;
                    }
                    if (mouse_buttons & SDL_BUTTON(g_settings.controls.mouse_thrust_btn))
                        thrust_key_down = 1;
                    if (mouse_buttons & SDL_BUTTON(g_settings.controls.mouse_fire_btn))
                        fire_key_down = 1;
                }

                /* Heading priority: right-stick > mouse > movement direction */
                if (right_stick_active)
                    player.angle = atan2f(rx_val, -ry_val);
                else if (mouse_aim_active)
                    player.angle = atan2f(mouse_dx, -mouse_dy);
                else if (move_x != 0.0f || move_y != 0.0f)
                    player.angle = atan2f(move_x, -move_y);
            }
        }

        /* ── Item 25: EMP lock — suppress all steering & thrust input ─
         * Set non-zero by the EMP Sentinel pulse hit in update_collisions.
         * While locked, freeze the player's heading at the value it held
         * at the START of this frame (snap_emp_angle, captured below),
         * and zero out any thrust/movement vector the input phase set.
         * Bullets can still fire — only steering & thrust are locked,
         * matching the spec ("EM pulse locks steering and thrust"). */
        if (player.emp_lock_timer > 0.0f) {
            player.angle = snap_emp_angle;
            thrust_key_down = 0;
            move_x = 0.0f;
            move_y = 0.0f;
            is_thrusting = 0;
            player.emp_lock_timer -= dt;
            if (player.emp_lock_timer < 0.0f) player.emp_lock_timer = 0.0f;
        }

        /* ── Sensor Static (Blindness) tick ──────────────────────────────
         * Pure timer decay: input is NOT suppressed (the pilot can still
         * fly), but the HUD render code blanks the minimap and the
         * proximity DANGER readout while > 0. Triggered by unshielded
         * solar flare exposure in update_progression(). */
        if (player.sensor_static_timer > 0.0f) {
            player.sensor_static_timer -= dt;
            if (player.sensor_static_timer < 0.0f) player.sensor_static_timer = 0.0f;
        }

        /* ── Reverse Drift (Confusion) tick ──────────────────────────────
         * Pure timer decay: input is NOT suppressed.  The thrust force
         * block below substitutes a negative drift_sign when this timer
         * is positive, so engaging thrust pushes the ship backward
         * instead of forward.  Triggered by unshielded solar flare
         * exposure in update_progression() (same gating as Sensor Static). */
        if (player.reverse_drift_timer > 0.0f) {
            player.reverse_drift_timer -= dt;
            if (player.reverse_drift_timer < 0.0f) player.reverse_drift_timer = 0.0f;
        }

        /* ── Tox-Gas Hallucinations tick ────────────────────────────────
         * Pure timer decay: input is NOT suppressed.  The asteroid render
         * block jitters its draw transforms and overlays low-alpha
         * ghost-Cacogen rings while the timer is positive, so the player
         * sees a shifting, second-guessing wireframe field.  Triggered by
         * unshielded solar flare exposure in update_progression() (same
         * Star-Shadow gating as Sensor Static and Reverse Drift). */
        if (player.tox_hallucination_timer > 0.0f) {
            player.tox_hallucination_timer -= dt;
            if (player.tox_hallucination_timer < 0.0f) player.tox_hallucination_timer = 0.0f;
        }

        /* ── Singularity Displacer: double-tap thrust to rift-jump ── */
        static Uint32 last_thrust_tap    = 0;
        static int    thrust_key_was_down = 0;
        is_thrusting = thrust_key_down;

        float thrust_angle = player.angle;
        if (settings_control_scheme == 1 && (move_x != 0.0f || move_y != 0.0f))
            thrust_angle = atan2f(move_x, -move_y);

        if (thrust_key_down && !thrust_key_was_down) {
            Uint32 now = SDL_GetTicks();
            if (player_upgrades.singularity_displacer
                && (now - last_thrust_tap) < 300) {
                Vec2 old_pos = player.pos;
                player.pos.x += sinf(thrust_angle) * 400.0f;
                player.pos.y -= cosf(thrust_angle) * 400.0f;
                spawn_particles(old_pos,    20, (SDL_Color){100, 100, 255, 255});
                spawn_particles(player.pos, 20, (SDL_Color){100, 100, 255, 255});
                audio_play(SFX_EXPLOSION_MD);
            }
            last_thrust_tap = now;
        }
        thrust_key_was_down = thrust_key_down;

        /* ── Thrust force + fuel burn ──────────────────────────────── */
        /* Emergency Drift Mode: thrust is unavailable at 0 fuel */
        int in_emergency_drift = (fuel_current <= 0.0f);

        if (thrust_key_down && !in_emergency_drift) {
            /* Reverse Drift (Confusion): when active, the thrust vector
             * is flipped so engaging thrust pushes the ship away from
             * the heading instead of toward it.  Rotation is unaffected. */
            float drift_sign = (player.reverse_drift_timer > 0.0f) ? -1.0f : 1.0f;
            player.vel.x += drift_sign * sinf(thrust_angle)
                            * THRUST_FORCE * player_upgrades.speed_mult * dt;
            player.vel.y -= drift_sign * cosf(thrust_angle)
                            * THRUST_FORCE * player_upgrades.speed_mult * dt;
            is_thrusting = 1;
            thrust_timer += dt;
            fuel_current -= FUEL_BURN_RATE * dt;
            if (fuel_current < 0.0f) fuel_current = 0.0f;
            audio_play(SFX_THRUST);
        } else {
            is_thrusting = 0;
            audio_stop(SFX_THRUST);
        }

        /* ── Passive reactor drain (idle + per active relic) ───────────
         * Even without thrusting the reactor slowly burns fuel.  Each
         * equipped relic/weapon system adds load.  This creates meaningful
         * resource tension in the outer zones without punishing early-game
         * players who haven't collected many relics yet.                  */
        {
            int relic_count = player_upgrades.triple_shot
                            + player_upgrades.shield_active
                            + player_upgrades.piercing
                            + player_upgrades.homing
                            + player_upgrades.rear_gun
                            + player_upgrades.split_shot
                            + player_upgrades.mirror_image
                            + player_upgrades.phase_shift
                            + player_upgrades.thermal_hull
                            + player_upgrades.singularity_displacer
                            + player_upgrades.singularity_whip
                            + player_upgrades.resonance_cascade
                            + player_upgrades.vortex_grenade
                            + player_upgrades.auto_turret
                            + player_upgrades.nova_explosion;
            float passive_drain = FUEL_PASSIVE_BASE_RATE
                                + (float)relic_count * FUEL_RELIC_RATE;
            /* Solar Flare drain hook (todo.md §2.3 / §3): during ACTIVE
             * phase the drain triples (SOLAR_FLARE_FUEL_MULT); if a large
             * asteroid is between the player and the dying sun, the
             * star-shadow soaks the radiation and the multiplier drops
             * to SOLAR_FLARE_SHADOW_MULT. */
            if (solar_flare_active_t > 0.0f) {
                passive_drain *= solar_flare_in_shadow
                    ? SOLAR_FLARE_SHADOW_MULT
                    : SOLAR_FLARE_FUEL_MULT;
            }
            fuel_current -= passive_drain * dt;
            if (fuel_current < 0.0f) fuel_current = 0.0f;
        }

        /* ── Emergency Drift Mode hull breach penalty ──────────────────
         * While adrift (fuel == 0), the reactor slowly tears the hull
         * apart.  After FUEL_DRIFT_PENALTY_TIME seconds the ship takes a
         * hull breach: player loses a life, fuel is emergency-refilled to
         * FUEL_DRIFT_RESERVE so the player can limp home.                */
        if (in_emergency_drift && game_state == STATE_PLAYING) {
            drift_penalty_timer += dt;
            if (drift_penalty_timer >= FUEL_DRIFT_PENALTY_TIME) {
                drift_penalty_timer = 0.0f;
                fuel_current = fuel_max * FUEL_DRIFT_RESERVE;
                cugel9_say("HULL BREACH. EMERGENCY RESERVE ACTIVATED. LOGGING: PILOT ERROR.");
                lives--;
                spawn_event_float(player.pos.x, player.pos.y - 32.0f,
                                  "HULL BREACH",
                                  (SDL_Color){HUD_CINNABAR.r,
                                              HUD_CINNABAR.g,
                                              HUD_CINNABAR.b, 255});
                spawn_particles(player.pos, 30,
                                (SDL_Color){227, 66, 52, 220});
                audio_play(SFX_EXPLOSION_SM);
                screen_shake_timer     = 0.4f;
                screen_shake_intensity = 6.0f;
                if (lives <= 0)
                    game_state = STATE_GAMEOVER;
            }
        } else {
            drift_penalty_timer = 0.0f;
        }

        /* Passive fuel regen inside FUEL_REGEN_RADIUS of the home station */
        {
            float hx = player.pos.x, hy = player.pos.y;
            if (sqrtf(hx*hx + hy*hy) < FUEL_REGEN_RADIUS
                && fuel_current < fuel_max) {
                fuel_current += 8.0f * dt;
                if (fuel_current > fuel_max) fuel_current = fuel_max;
            }
        }

        /* ── Autofire cooldown ─────────────────────────────────────── */
        if (fire_cooldown_timer > 0.0f)
            fire_cooldown_timer -= dt;
        if (settings_autofire && fire_cooldown_timer <= 0.0f)
            fire_key_down = 1;

        /* ── Trail recording ───────────────────────────────────────── */
        player.trail_pos[player.trail_head] = player.pos;
        player.trail_ang[player.trail_head] = player.angle;
        player.trail_head = (player.trail_head + 1) % PHOS_TRAIL_LEN;

        /* ── External forces + friction + integration ──────────────── */
        Vec2 ext_f = calculate_external_forces(player.pos);
        player.vel.x += ext_f.x * dt;
        player.vel.y += ext_f.y * dt;

        /* ── Gravity slingshot: tangential boost in rift close orbit ──
         * When the player orbits within the slingshot band (150–380 units
         * from the void rift) and has meaningful tangential velocity (i.e.
         * they are curving around rather than falling straight in), apply a
         * continuous tangential kick proportional to depth in the band and
         * current tangential speed.  This rewards deliberate orbit manoeuvres
         * with a free velocity boost — no fuel cost, no button press.
         *
         * Band geometry:
         *   < 150 u  : inside the singularity (normal gravity dominates)
         *   150–380 u: slingshot zone — boost active
         *   > 380 u  : too far, normal gravity only
         *
         * On first entry (3 s cooldown), spawn particles + EventFloat.      */
        if (rift.active) {
            static float sling_float_cd = 0.0f;
            if (sling_float_cd > 0.0f) sling_float_cd -= dt;

            float sdx  = player.pos.x - rift.pos.x;
            float sdy  = player.pos.y - rift.pos.y;
            float sdst = sqrtf(sdx * sdx + sdy * sdy);

            if (sdst > 150.0f && sdst < 380.0f && sdst > 0.5f) {
                /* Radial unit vector (rift → player) */
                float rx = sdx / sdst;
                float ry = sdy / sdst;
                /* Tangential unit vector (clockwise perp. to radial) */
                float tx = -ry;
                float ty =  rx;
                /* Signed tangential component of current velocity */
                float tan_v = player.vel.x * tx + player.vel.y * ty;

                /* Only boost if player has significant orbital motion */
                if (fabsf(tan_v) > 30.0f) {
                    /* Boost peaks at inner edge (150 u), fades to 0 at 380 u */
                    float depth = (380.0f - sdst) / 230.0f;   /* 0..1 */
                    float sign  = (tan_v > 0.0f) ? 1.0f : -1.0f;
                    float boost = sign * 90.0f * depth * dt;
                    player.vel.x += tx * boost;
                    player.vel.y += ty * boost;

                    /* Feedback: particles + EventFloat, once per 3 s */
                    if (sling_float_cd <= 0.0f) {
                        spawn_particles(player.pos, 12,
                                        (SDL_Color){80, 200, 255, 210});
                        spawn_event_float(player.pos.x,
                                          player.pos.y - 28.0f,
                                          "GRAVITY ASSIST",
                                          (SDL_Color){80, 200, 255, 240});
                        sling_float_cd = 3.0f;
                    }
                }
            }
        }

        player.vel.x *= powf(FRICTION, dt * 60.0f);
        player.vel.y *= powf(FRICTION, dt * 60.0f);
        player.pos.x += player.vel.x * dt;
        player.pos.y += player.vel.y * dt;

        /* ── Mirror image: phantom twin orbits 200 px behind the prow ─ */
        if (player_upgrades.mirror_image) {
            mirror_active_flag = 1;
            mirror_pos.x  = player.pos.x - sinf(player.angle) * 200.0f;
            mirror_pos.y  = player.pos.y + cosf(player.angle) * 200.0f;
            mirror_angle  = player.angle;
        } else {
            mirror_active_flag = 0;
        }

        /* ── Auto turret: fires a 4-way ring every 2 seconds ──────── */
        static float auto_turret_timer = 0.0f;
        if (player_upgrades.auto_turret) {
            auto_turret_timer += dt;
            if (auto_turret_timer >= 2.0f) {
                auto_turret_timer = 0.0f;
                for (int ring = 0; ring < 4; ring++) {
                    float ang = (float)ring * ((float)M_PI * 0.5f);
                    for (int bi = 0; bi < MAX_BULLETS; bi++) {
                        if (!bullets[bi].active) {
                            bullets[bi].active   = 1;
                            bullets[bi].pos      = player.pos;
                            bullets[bi].vel.x    = sinf(ang) * BULLET_SPEED * 0.57f;
                            bullets[bi].vel.y    = -cosf(ang) * BULLET_SPEED * 0.57f;
                            bullets[bi].life     = 1.8f;
                            bullets[bi].bounces  = 0;
                            bullets[bi].is_homing = 0;
                            bullets[bi].pierces  = player_upgrades.piercing;
                            break;
                        }
                    }
                }
            }
        } else {
            auto_turret_timer = 0.0f;
        }

        /* ── Primary weapon fire ───────────────────────────────────── */
        if (fire_key_down && fire_cooldown_timer <= 0.0f) {
            int   bolts_to_fire  = player_upgrades.triple_shot ? 3 : 1;
            float spread         = 0.25f;
            /* In twin-stick mode bolts travel along the right-stick axis */
            float base_fire_angle = twin_stick_fire_active
                                    ? twin_stick_fire_angle
                                    : player.angle;

            for (int b = 0; b < bolts_to_fire; b++) {
                for (int i = 0; i < MAX_BULLETS; i++) {
                    if (!bullets[i].active) {
                        float ang = base_fire_angle;
                        if (bolts_to_fire == 3) ang += (b - 1) * spread;

                        float nose_x = player.pos.x + sinf(ang) * 12.0f;
                        float nose_y = player.pos.y - cosf(ang) * 12.0f;

                        bullets[i].active   = 1;
                        bullets[i].life     = BULLET_LIFE;
                        bullets[i].bounces  = 0;
                        bullets[i].pierces  = 0;
                        bullets[i].pos      = (Vec2){nose_x, nose_y};
                        bullets[i].trail_head = 0;
                        for (int t = 0; t < PHOS_TRAIL_LEN; t++) {
                            bullets[i].trail_pos[t] = bullets[i].pos;
                            bullets[i].trail_ang[t] = 0.0f;
                        }
                        bullets[i].vel.x =
                            sinf(ang) * BULLET_SPEED
                            * player_upgrades.bullet_speed_mult
                            + player.vel.x * 0.3f;
                        bullets[i].vel.y =
                            -cosf(ang) * BULLET_SPEED
                            * player_upgrades.bullet_speed_mult
                            + player.vel.y * 0.3f;
                        bullets[i].color     = (SDL_Color){255, 255, 255, 255};
                        bullets[i].is_homing = player_upgrades.homing ? 1 : 0;
                        break;
                    }
                }
            }

            /* Rear gun fires from the aft thruster port */
            if (player_upgrades.rear_gun) {
                for (int i = 0; i < MAX_BULLETS; i++) {
                    if (!bullets[i].active) {
                        float rear_ang = base_fire_angle + (float)M_PI;
                        float rear_x   = player.pos.x
                                         - sinf(base_fire_angle) * 12.0f;
                        float rear_y   = player.pos.y
                                         + cosf(base_fire_angle) * 12.0f;
                        bullets[i].active    = 1;
                        bullets[i].life      = BULLET_LIFE;
                        bullets[i].bounces   = 0;
                        bullets[i].pierces   = 0;
                        bullets[i].pos       = (Vec2){rear_x, rear_y};
                        bullets[i].trail_head = 0;
                        for (int t = 0; t < PHOS_TRAIL_LEN; t++) {
                            bullets[i].trail_pos[t] = bullets[i].pos;
                            bullets[i].trail_ang[t] = 0.0f;
                        }
                        bullets[i].vel.x =
                            sinf(rear_ang) * BULLET_SPEED
                            * player_upgrades.bullet_speed_mult
                            + player.vel.x * 0.3f;
                        bullets[i].vel.y =
                            -cosf(rear_ang) * BULLET_SPEED
                            * player_upgrades.bullet_speed_mult
                            + player.vel.y * 0.3f;
                        bullets[i].color     = (SDL_Color){255, 180, 80, 255};
                        bullets[i].is_homing = 0;
                        break;
                    }
                }
            }

            audio_play(SFX_FIRE);
            fire_cooldown_timer =
                0.25f * player_upgrades.fire_cooldown_mult;
        }

        /* ── God mode cheat: unlimited lives and rapid triple fire ── */
        if (god_mode) {
            lives = 99;
            player_upgrades.triple_shot        = 1;
            player_upgrades.fire_cooldown_mult  = 0.2f;
            player_upgrades.magnet_radius       = 500.0f;
        }

        /* ── Invulnerability blink countdown ───────────────────────── */
        if (player.invuln_timer > 0.0f)
            player.invuln_timer -= dt;

    } else {
        /* ── Respawn countdown ─────────────────────────────────────── */
        static float spawn_delay = 2.2f;
        if (lives > 0) {
            spawn_delay -= dt;
            if (spawn_delay <= 0.6f && respawn_phase == 0) {
                respawn_phase = 1;
                respawn_blink = 0.0f;
            }
            if (spawn_delay <= 0.0f) {
                reset_player();
                spawn_delay   = 2.2f;
                respawn_phase = 0;
                respawn_blink = 0.0f;
            }
        }
    }
}

/* ================================================================== */

/** @brief Advances all active player bullets and handles their lifecycle.
 *
 *  Applies external gravitational forces, homing steering, movement
 *  integration, and bounce/cull logic.  Bullets that exceed the cull
 *  radius (1300 u from the player) or exhaust their lifetime are
 *  deactivated.
 */
static void update_player_bullets(float dt)
{
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;

        /* Trail recording */
        bullets[i].trail_pos[bullets[i].trail_head] = bullets[i].pos;
        bullets[i].trail_ang[bullets[i].trail_head] = 0.0f;
        bullets[i].trail_head = (bullets[i].trail_head + 1) % PHOS_TRAIL_LEN;

        bullets[i].life -= dt;
        if (bullets[i].life <= 0.0f) {
            bullets[i].active = 0;
            continue;
        }

        /* External gravitational forces (Void Rifts, home-station gravity) */
        Vec2 ext_f = calculate_external_forces(bullets[i].pos);
        bullets[i].vel.x += ext_f.x * dt;
        bullets[i].vel.y += ext_f.y * dt;

        /* Homing steering: turn toward nearest asteroid */
        if (bullets[i].is_homing) {
            float min_d2 = 1e18f;
            Vec2  home_target = bullets[i].pos;
            for (int ha = 0; ha < MAX_ASTEROIDS; ha++) {
                if (!asteroids[ha].active) continue;
                float hdx = asteroids[ha].pos.x - bullets[i].pos.x;
                float hdy = asteroids[ha].pos.y - bullets[i].pos.y;
                float hd2 = hdx*hdx + hdy*hdy;
                if (hd2 < min_d2) { min_d2 = hd2; home_target = asteroids[ha].pos; }
            }
            float hdx  = home_target.x - bullets[i].pos.x;
            float hdy  = home_target.y - bullets[i].pos.y;
            float hdist = sqrtf(hdx*hdx + hdy*hdy);
            if (hdist > 1.0f) {
                float str = 280.0f;
                bullets[i].vel.x += (hdx / hdist) * str * dt;
                bullets[i].vel.y += (hdy / hdist) * str * dt;
                float bspd = sqrtf(bullets[i].vel.x * bullets[i].vel.x
                                   + bullets[i].vel.y * bullets[i].vel.y);
                float max_spd = BULLET_SPEED * player_upgrades.bullet_speed_mult;
                if (bspd > max_spd) {
                    bullets[i].vel.x = bullets[i].vel.x / bspd * max_spd;
                    bullets[i].vel.y = bullets[i].vel.y / bspd * max_spd;
                }
            }
        }

        bullets[i].pos.x += bullets[i].vel.x * dt;
        bullets[i].pos.y += bullets[i].vel.y * dt;

        /* Bouncy bullets: reflect off the current camera viewport edges */
        if (player_upgrades.max_bounces > 0
            && bullets[i].bounces < player_upgrades.max_bounces) {
            int bounced = 0;
            if (bullets[i].pos.x < camera_pos.x) {
                bullets[i].pos.x = camera_pos.x;
                bullets[i].vel.x = -bullets[i].vel.x;
                bounced = 1;
            } else if (bullets[i].pos.x > camera_pos.x + SCREEN_WIDTH) {
                bullets[i].pos.x = camera_pos.x + SCREEN_WIDTH;
                bullets[i].vel.x = -bullets[i].vel.x;
                bounced = 1;
            }
            if (bullets[i].pos.y < camera_pos.y) {
                bullets[i].pos.y = camera_pos.y;
                bullets[i].vel.y = -bullets[i].vel.y;
                bounced = 1;
            } else if (bullets[i].pos.y > camera_pos.y + SCREEN_HEIGHT) {
                bullets[i].pos.y = camera_pos.y + SCREEN_HEIGHT;
                bullets[i].vel.y = -bullets[i].vel.y;
                bounced = 1;
            }
            if (bounced) bullets[i].bounces++;
        } else {
            /* Non-bouncy: cull when far from player */
            if (player.active
                && distance_sq(bullets[i].pos, player.pos)
                   > 1300.0f * 1300.0f) {
                bullets[i].active = 0;
            }
        }
    }
}

/* ================================================================== */

/** @brief Updates asteroid physics, rotation, trail, and repositioning.
 *
 *  Each active asteroid is integrated with external forces and its rotation
 *  advanced.  Asteroids that drift beyond 3500 u from the player are
 *  teleported into a fresh spawn ring (2000-2500 u) to maintain density.
 *  If the active count falls below the target, a new large asteroid is
 *  spawned off-screen.  The Anomalous Void Rift is activated when the
 *  player reaches level 4.
 */
static void update_asteroids(float dt)
{
    int active_count = 0;

    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) continue;
        active_count++;

        /* Trail recording before position update */
        asteroids[i].trail_pos[asteroids[i].trail_head] = asteroids[i].pos;
        asteroids[i].trail_ang[asteroids[i].trail_head] = asteroids[i].angle;
        asteroids[i].trail_head = (asteroids[i].trail_head + 1) % PHOS_TRAIL_LEN;

        asteroids[i].angle += asteroids[i].rot_speed * dt;

        /* External force integration (rift gravity, etc.) */
        Vec2 ext_f = calculate_external_forces(asteroids[i].pos);
        asteroids[i].vel.x += ext_f.x * dt;
        asteroids[i].vel.y += ext_f.y * dt;
        asteroids[i].pos.x += asteroids[i].vel.x * dt;
        asteroids[i].pos.y += asteroids[i].vel.y * dt;

        /* Teleport back into the spawn ring if too far from player */
        if (player.active) {
            float d2 = distance_sq(asteroids[i].pos, player.pos);
            if (d2 > 3500.0f * 3500.0f) {
                float ang  = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
                float dist = UFO_SPAWN_RING_MIN
                             + ((float)rand() / RAND_MAX)
                               * (UFO_SPAWN_RING_MAX - UFO_SPAWN_RING_MIN);
                asteroids[i].pos.x = player.pos.x + cosf(ang) * dist;
                asteroids[i].pos.y = player.pos.y + sinf(ang) * dist;
                /* Reset trail to avoid streak artefacts across the void */
                for (int t = 0; t < PHOS_TRAIL_LEN; t++)
                    asteroids[i].trail_pos[t] = asteroids[i].pos;
            }
        }
    }

    /* Maintain target asteroid density based on level and difficulty */
    int target = 12 + player_level * 3 + (difficulty * 3);
    if (target > MAX_ASTEROIDS - 4) target = MAX_ASTEROIDS - 4;

    if (player.active && active_count < target) {
        float ang  = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
        float dist = UFO_SPAWN_RING_MIN
                     + ((float)rand() / RAND_MAX)
                       * (UFO_SPAWN_RING_MAX - UFO_SPAWN_RING_MIN);
        Vec2 pos = {
            player.pos.x + cosf(ang) * dist,
            player.pos.y + sinf(ang) * dist
        };
        spawn_asteroid(pos, 3);
    }

    /* Cache active count for the beat system (read in update_spawning) */
    cached_active_asteroids = active_count;
}

/* ================================================================== */

/** @brief Runs the active enemy vessel's AI and movement.
 *
 *  Each UFO variant has a distinct movement personality:
 *
 *  size 1/2 — Normal UFO: horizontal drift with random vertical reversals.
 *  size 3   — Vector Stalker (Kamikaze): predictive intercept.  The target
 *              position is extrapolated by t = dist/200 seconds forward
 *              along the player's current velocity, then the stalker drives
 *              straight at that future point at 200 u/s.
 *  size 4   — Boundary Weaver (Bomber): constant horizontal sweep; Y tracks
 *              a target lane with a PD-style correction.
 *  size 5   — Eye of the Void (Gravitational Harvester): minimal horizontal
 *              drift; vertical position oscillates sinusoidally using
 *              game_time (deterministic, frame-rate independent).
 *  size 6   — Eldritch Tendril: slow approach toward player, perturbed by
 *              Lissajous-style cosine/sine accelerations derived from
 *              game_time, speed-capped at 120 u/s.
 *  size 7   — Daemon Sigil: fast spin, erratic teleport-lurches on a
 *              random timer, velocity decays between lurches.
 *
 *  All variants share trail recording, external force application, and a
 *  maximum engagement range of 3500 u.
 */
static void update_ufo(float dt)
{
    if (!ufo.active) return;

    /* Kamikaze (size 3) faces its movement direction */
    {
        float vspd = sqrtf(ufo.vel.x * ufo.vel.x + ufo.vel.y * ufo.vel.y);
        if (ufo.size == 3 && vspd > 1.0f)
            ufo.angle = atan2f(ufo.vel.y, ufo.vel.x);
    }

    /* Trail recording */
    ufo.trail_pos[ufo.trail_head] = ufo.pos;
    ufo.trail_ang[ufo.trail_head] = ufo.angle;
    ufo.trail_head = (ufo.trail_head + 1) % PHOS_TRAIL_LEN;

    /* External forces (rift gravity, etc.) */
    Vec2 ext_f = calculate_external_forces(ufo.pos);
    ufo.vel.x += ext_f.x * dt;
    ufo.vel.y += ext_f.y * dt;

    if (ufo.size == 3 && player.active) {
        /*
         * Vector Stalker — predictive intercept:
         *   t = time-to-target at cruise speed 200 u/s
         *   p_target = player_pos + player_vel * t
         * The stalker sets velocity directly toward that future point so
         * it leads the player rather than chasing the current position.
         */
        float dx   = player.pos.x - ufo.pos.x;
        float dy   = player.pos.y - ufo.pos.y;
        float dist = sqrtf(dx*dx + dy*dy);
        float t    = dist / 200.0f;
        Vec2  p_target = {
            player.pos.x + player.vel.x * t,
            player.pos.y + player.vel.y * t
        };
        float tdx  = p_target.x - ufo.pos.x;
        float tdy  = p_target.y - ufo.pos.y;
        float td   = sqrtf(tdx*tdx + tdy*tdy);
        if (td > 0.1f) {
            ufo.vel.x = (tdx / td) * 200.0f;
            ufo.vel.y = (tdy / td) * 200.0f;
        }
        ufo.pos.x += ufo.vel.x * dt;
        ufo.pos.y += ufo.vel.y * dt;

    } else if (ufo.size == 4) {
        /*
         * Boundary Weaver — constant horizontal sweep with lane-tracking:
         * vel.x is fixed at spawn; Y converges on target_y with a
         * proportional correction factor of 2.0.
         */
        ufo.pos.x += ufo.vel.x * dt;
        float dy    = ufo.target_y - ufo.pos.y;
        ufo.pos.y  += dy * 2.0f * dt;

    } else if (ufo.size == 5) {
        /*
         * Eye of the Void — Gravitational Harvester:
         * Horizontal drift is intentionally slow (30% of vel.x).
         * Vertical sinusoidal oscillation uses game_time so the wave
         * is deterministic and frame-rate independent.
         * Frequency: sinf(game_time * 2.0f) matches the original
         * sinf(SDL_GetTicks() / 500.0f) at a constant 60 fps.
         */
        ufo.pos.x += ufo.vel.x * 0.3f * dt;
        ufo.pos.y += sinf(game_time * 2.0f) * 20.0f * dt;

    } else if (ufo.size == 6) {
        /*
         * Eldritch Tendril — sinusoidal drift with slow player approach:
         * A gentle attractive acceleration pulls it toward the player.
         * Two independent Lissajous frequencies (1.625 Hz, 1.125 Hz)
         * perturb velocity for an organic wandering motion.
         * game_time * 1.25f replaces SDL_GetTicks() / 800.0f.
         * Speed is hard-capped at 120 u/s.
         */
        if (player.active) {
            float dx = player.pos.x - ufo.pos.x;
            float dy = player.pos.y - ufo.pos.y;
            float d  = sqrtf(dx*dx + dy*dy);
            if (d > 0.1f) {
                ufo.vel.x += (dx / d) * 30.0f * dt;
                ufo.vel.y += (dy / d) * 30.0f * dt;
            }
        }
        float t6 = game_time * 1.25f;         /* = SDL_GetTicks()/800 at 60fps */
        ufo.vel.x += cosf(t6 * 1.3f) * 40.0f * dt;
        ufo.vel.y += sinf(t6 * 0.9f) * 40.0f * dt;
        float spd6 = sqrtf(ufo.vel.x * ufo.vel.x + ufo.vel.y * ufo.vel.y);
        if (spd6 > 120.0f) {
            ufo.vel.x = ufo.vel.x / spd6 * 120.0f;
            ufo.vel.y = ufo.vel.y / spd6 * 120.0f;
        }
        ufo.pos.x  += ufo.vel.x * dt;
        ufo.pos.y  += ufo.vel.y * dt;
        ufo.angle   = atan2f(ufo.vel.y, ufo.vel.x);

    } else if (ufo.size == 7) {
        /*
         * Daemon Sigil — erratic teleport-lurch pattern:
         * Spins at 3 rad/s continuously.  Every 0.8–2.0 s it leaps to a
         * random nearby position and resets velocity in that direction.
         * Velocity then decays with a 1.5 s time constant between lurches.
         */
        ufo.angle += 3.0f * dt;
        ufo.change_dir_timer -= dt;
        if (ufo.change_dir_timer <= 0.0f) {
            ufo.change_dir_timer = 0.8f + ((float)rand() / RAND_MAX) * 1.2f;
            float a7   = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float lurch = 80.0f + ((float)rand() / RAND_MAX) * 80.0f;
            ufo.pos.x += cosf(a7) * lurch;
            ufo.pos.y += sinf(a7) * lurch;
            ufo.vel.x  = cosf(a7) * 160.0f;
            ufo.vel.y  = sinf(a7) * 160.0f;
        }
        ufo.pos.x  += ufo.vel.x * dt;
        ufo.pos.y  += ufo.vel.y * dt;
        ufo.vel.x  *= (1.0f - 1.5f * dt);
        ufo.vel.y  *= (1.0f - 1.5f * dt);

    } else {
        /* Normal UFO (size 1 or 2): horizontal drift, random Y reversals */
        ufo.pos.x += ufo.vel.x * dt;
        ufo.change_dir_timer -= dt;
        if (ufo.change_dir_timer <= 0.0f) {
            ufo.change_dir_timer = 1.0f + ((float)rand() / RAND_MAX) * 1.5f;
            ufo.vel.y = (-1.0f + 2.0f * (rand() % 2)) * 60.0f;
        }
        ufo.pos.y += ufo.vel.y * dt;
    }

    /* Deactivate if the player has flown too far away */
    if (player.active
        && distance_sq(ufo.pos, player.pos) > 3500.0f * 3500.0f) {
        ufo.active = 0;
        audio_stop(SFX_UFO_LOOP);
        ufo_spawn_timer = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
        return;
    }

    /* ── UFO weapon fire ─────────────────────────────────────────── */
    ufo.fire_timer -= dt;
    if (ufo.fire_timer <= 0.0f && player.active) {
        /* Fire rate varies by vessel type */
        if      (ufo.size == 6) ufo.fire_timer = 2.2f;
        else if (ufo.size == 7) ufo.fire_timer = 0.7f;
        else                    ufo.fire_timer = (ufo.size == 2) ? 1.5f : 1.0f;

        for (int i = 0; i < MAX_UFO_BULLETS; i++) {
            if (!ufo_bullets[i].active) {
                ufo_bullets[i].active    = 1;
                ufo_bullets[i].life      = BULLET_LIFE;
                ufo_bullets[i].pos       = ufo.pos;
                ufo_bullets[i].trail_head = 0;
                for (int t = 0; t < PHOS_TRAIL_LEN; t++) {
                    ufo_bullets[i].trail_pos[t] = ufo.pos;
                    ufo_bullets[i].trail_ang[t] = 0.0f;
                }

                float fire_angle;
                if (ufo.size == 2) {
                    /* Wanderer fires in a random direction */
                    fire_angle = ((float)rand() / RAND_MAX)
                                 * 2.0f * (float)M_PI;
                } else {
                    /* All others aim at the player with accuracy scatter
                     * that decreases as level rises: offset ~ 0.4/level */
                    float dx = player.pos.x - ufo.pos.x;
                    float dy = player.pos.y - ufo.pos.y;
                    fire_angle  = atan2f(dx, -dy);
                    fire_angle += (-0.3f + 0.6f * ((float)rand() / RAND_MAX))
                                  * (4.0f / (3.0f + level));
                }

                ufo_bullets[i].vel.x =
                    sinf(fire_angle) * UFO_BULLET_SPEED;
                ufo_bullets[i].vel.y =
                    -cosf(fire_angle) * UFO_BULLET_SPEED;
                ufo_bullets[i].color = (SDL_Color){255, 120, 120, 255};
                audio_play(SFX_UFO_FIRE);
                break;
            }
        }
    }
}

/* ================================================================== */

/** @brief Advances all active enemy bullets.
 *
 *  Applies external forces, integrates position, and culls bullets that
 *  exceed their lifetime or stray further than 1300 u from the player.
 */
static void update_ufo_bullets(float dt)
{
    for (int i = 0; i < MAX_UFO_BULLETS; i++) {
        if (!ufo_bullets[i].active) continue;

        /* Trail recording */
        ufo_bullets[i].trail_pos[ufo_bullets[i].trail_head] = ufo_bullets[i].pos;
        ufo_bullets[i].trail_ang[ufo_bullets[i].trail_head] = 0.0f;
        ufo_bullets[i].trail_head =
            (ufo_bullets[i].trail_head + 1) % PHOS_TRAIL_LEN;

        ufo_bullets[i].life -= dt;
        if (ufo_bullets[i].life <= 0.0f) {
            ufo_bullets[i].active = 0;
            continue;
        }

        Vec2 ext_f = calculate_external_forces(ufo_bullets[i].pos);
        ufo_bullets[i].vel.x += ext_f.x * dt;
        ufo_bullets[i].vel.y += ext_f.y * dt;
        ufo_bullets[i].pos.x += ufo_bullets[i].vel.x * dt;
        ufo_bullets[i].pos.y += ufo_bullets[i].vel.y * dt;

        if (player.active
            && distance_sq(ufo_bullets[i].pos, player.pos) > 1300.0f * 1300.0f)
            ufo_bullets[i].active = 0;
    }
}

/* ================================================================== */

/** @brief Updates particle lifetimes, XP orb magnetics, NPC AI, shockwaves,
 *         rifts, and the Singularity Whip trail-damage system.
 *
 *  Orbs within the player's magnet radius are accelerated toward the player.
 *  NPC ships idle-drift when alone and orbit the player at 85 u when following.
 *  Shockwaves expand outward and destroy asteroids caught in their ring.
 */
static void update_particles_orbs_npcs(float dt)
{
    /* ── Particles ────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life <= 0.0f) continue;
        particles[i].life    -= dt;
        particles[i].angle   += particles[i].rot_speed * dt;
        particles[i].pos.x   += particles[i].vel.x * dt;
        particles[i].pos.y   += particles[i].vel.y * dt;
        /* Cull particles far from player to keep the pool healthy */
        if (player.active
            && distance_sq(particles[i].pos, player.pos) > 1300.0f * 1300.0f)
            particles[i].life = 0.0f;
    }

    /* ── XP Orbs ──────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_ORBS; i++) {
        if (!orbs[i].active) continue;

        /* Trail recording */
        orbs[i].trail_pos[orbs[i].trail_head] = orbs[i].pos;
        orbs[i].trail_ang[orbs[i].trail_head] = 0.0f;
        orbs[i].trail_head = (orbs[i].trail_head + 1) % PHOS_TRAIL_LEN;

        /* Magnet attraction: accelerates orbs within the magnet radius */
        if (player.active) {
            float dx     = player.pos.x - orbs[i].pos.x;
            float dy     = player.pos.y - orbs[i].pos.y;
            float dist2  = dx*dx + dy*dy;
            float mag_r  = player_upgrades.magnet_radius + 40.0f;
            if (dist2 < mag_r * mag_r) {
                float dist = sqrtf(dist2);
                float pull = god_mode ? 1500.0f : 1200.0f;
                orbs[i].vel.x *= (1.0f - 5.0f * dt);
                orbs[i].vel.y *= (1.0f - 5.0f * dt);
                orbs[i].vel.x += (dx / dist) * pull * dt;
                orbs[i].vel.y += (dy / dist) * pull * dt;
            }
        }
        orbs[i].pos.x += orbs[i].vel.x * dt;
        orbs[i].pos.y += orbs[i].vel.y * dt;

        /* Cull distant orbs */
        if (player.active
            && distance_sq(orbs[i].pos, player.pos) > 1300.0f * 1300.0f) {
            orbs[i].active = 0;
            continue;
        }

        /* Collection: touching the player collects the orb (handled in
         * update_progression where XP deltas are applied this frame) */
        if (player.active
            && check_collision(player.pos, player.radius + 5.0f,
                               orbs[i].pos, 6.0f)) {
            orbs[i].active = 0;
            int gained = (int)(orbs[i].value * player_upgrades.xp_mult);
            player_xp += gained;
            /* Chronicle Chord Harmonics: rapid collection plays ascending
             * pentatonic minor scale (C Eb F G Bb) for a harmonious chain. */
            if (orb_chord_timer > 0.0f) {
                audio_play(SFX_CHORD_C4 + (orb_chord_note % 5));
                orb_chord_note = (orb_chord_note + 1) % 5;
            } else {
                audio_play(SFX_EXPLOSION_SM);
                orb_chord_note = 0;
            }
            orb_chord_timer = 0.5f;   /* 0.5 s window to chain the next orb */
            if (player_xp >= xp_threshold) {
                player_xp      -= xp_threshold;
                player_level++;
                level            = player_level;
                xp_threshold     = (int)(xp_threshold * XP_THRESHOLD_GROWTH);
                xp_flash_timer   = 0.4f;
                spawn_upgrade_options();
                game_state = STATE_UPGRADE_SELECT;
            }
        }
    }

    /* ── Zone / home station ──────────────────────────────────────── */
    if (player.active) {
        player_dist_origin = sqrtf(player.pos.x * player.pos.x
                                   + player.pos.y * player.pos.y);
        player_zone = get_zone(player.pos);

        /* Zone Transition Banner: detect zone change and start the display timer */
        if (zone_banner_prev_zone < 0) {
            zone_banner_prev_zone = player_zone;   /* silent init on first frame */
        } else if (zone_banner_prev_zone != player_zone) {
            zone_banner_timer     = 2.5f;          /* 0.4s fade-in, 1.7s hold, 0.4s fade-out */
            zone_banner_prev_zone = player_zone;
        }
    }
    home_station_angle += 0.004f * dt * 60.0f;
    (void)structures; /* Structures are indestructible this build */

    /* ── Friendly NPCs ────────────────────────────────────────────── */
    for (int i = 0; i < MAX_NPC; i++) {
        if (!npcs[i].active) continue;

        float dx   = player.pos.x - npcs[i].pos.x;
        float dy   = player.pos.y - npcs[i].pos.y;
        float dist = sqrtf(dx*dx + dy*dy);

        if (!npcs[i].following) {
            /* Idle drift: gentle sinusoidal circles */
            npcs[i].orbit_angle += 0.4f * dt;
            npcs[i].vel.x += cosf(npcs[i].orbit_angle) * 20.0f * dt;
            npcs[i].vel.y += sinf(npcs[i].orbit_angle) * 20.0f * dt;
            npcs[i].vel.x *= (1.0f - 2.0f * dt);
            npcs[i].vel.y *= (1.0f - 2.0f * dt);
            npcs[i].pos.x += npcs[i].vel.x * dt;
            npcs[i].pos.y += npcs[i].vel.y * dt;

            /* Start following if the player stays within 120 u for 2 s */
            if (dist < 120.0f && player.active) {
                npcs[i].contact_timer += dt;
                if (npcs[i].contact_timer > 2.0f) {
                    npcs[i].following    = 1;
                    npcs[i].orbit_angle  = atan2f(dy, dx) + (float)M_PI;
                }
            } else {
                npcs[i].contact_timer *= 0.95f;
            }
        } else {
            /* Orbit player at 85 u using spring-damper */
            float target_dist = 85.0f;
            npcs[i].orbit_angle += 1.1f * dt;
            float tx = player.pos.x + cosf(npcs[i].orbit_angle) * target_dist;
            float ty = player.pos.y + sinf(npcs[i].orbit_angle) * target_dist;
            float ex = tx - npcs[i].pos.x;
            float ey = ty - npcs[i].pos.y;
            npcs[i].vel.x += ex * 8.0f * dt;
            npcs[i].vel.y += ey * 8.0f * dt;
            npcs[i].vel.x *= (1.0f - 3.0f * dt);
            npcs[i].vel.y *= (1.0f - 3.0f * dt);
            npcs[i].pos.x += npcs[i].vel.x * dt;
            npcs[i].pos.y += npcs[i].vel.y * dt;

            if (!player.active || dist > 400.0f)
                npcs[i].following = 0;
        }
        npcs[i].angle = atan2f(npcs[i].vel.y, npcs[i].vel.x);

        /* ─── Item 27: drone chatter — periodic mumbles above each drone ── */
        npcs[i].chatter_timer -= dt;
        if (npcs[i].chatter_timer <= 0.0f) {
            /* Pick an event based on current game state. Threat events take
             * priority over idle chatter. Float spawns at the drone's head. */
            float fx = npcs[i].pos.x;
            float fy = npcs[i].pos.y;
            DroneEvent evt = DRONE_EVT_COORDINATING;

            /* UFO threat: prefer TARGET_UFO when a saucer is active and the
             * drone is closer than 600 u to it (or to the player it shadows). */
            if (ufo.active) {
                float udx = ufo.pos.x - npcs[i].pos.x;
                float udy = ufo.pos.y - npcs[i].pos.y;
                if (udx*udx + udy*udy < 600.0f * 600.0f) {
                    evt = DRONE_EVT_TARGET_UFO;
                }
            }

            /* Big asteroid nearby: TARGET_LARGE if a size-3+ rock is within 250 u. */
            if (evt == DRONE_EVT_COORDINATING) {
                for (int ai = 0; ai < MAX_ASTEROIDS; ai++) {
                    if (!asteroids[ai].active) continue;
                    if (asteroids[ai].size < 3) continue;
                    float ax = asteroids[ai].pos.x - npcs[i].pos.x;
                    float ay = asteroids[ai].pos.y - npcs[i].pos.y;
                    if (ax*ax + ay*ay < 250.0f * 250.0f) {
                        evt = DRONE_EVT_TARGET_LARGE;
                        break;
                    }
                }
            }

            /* Player in trouble: emit HELP if lives are dangerously low. */
            if (evt == DRONE_EVT_COORDINATING && player.active && lives <= 1) {
                evt = DRONE_EVT_HELP;
            }

            drone_chatter_emit_event(fx, fy, evt);

            /* Re-arm: 2.5–6 s for idle, 1.2–2.8 s for threat callouts. */
            float min_t = (evt == DRONE_EVT_COORDINATING) ? 2.5f : 1.2f;
            float max_t = (evt == DRONE_EVT_COORDINATING) ? 6.0f : 2.8f;
            npcs[i].chatter_timer =
                min_t + ((float)rand() / RAND_MAX) * (max_t - min_t);
        }

        /* NPC dies if struck by a player bullet */
        for (int b = 0; b < MAX_BULLETS; b++) {
            if (!bullets[b].active) continue;
            float bx = bullets[b].pos.x - npcs[i].pos.x;
            float by = bullets[b].pos.y - npcs[i].pos.y;
            if (sqrtf(bx*bx + by*by) < npcs[i].radius) {
                bullets[b].active = 0;
                npcs[i].active    = 0;
                spawn_particles(npcs[i].pos, 12, (SDL_Color){80, 255, 80, 255});
                audio_play(SFX_EXPLOSION_SM);
                break;
            }
        }
    }

    /* ── Void Rift updates ────────────────────────────────────────── */
    if (player_rift.active) {
        player_rift.spawn_timer -= dt;
        if (player_rift.spawn_timer <= 0.0f) {
            player_rift.active = 0;
        } else {
            player_rift.pulse_timer += dt * 5.0f;
            player_rift.angle1      += 2.0f * dt;
            player_rift.angle2      -= 1.5f * dt;
        }
    }
    if (rift.active) {
        rift.pulse_timer += dt * 5.0f;
        rift.angle1      += 2.0f * dt;
        rift.angle2      -= 1.5f * dt;
        /* Keep the Anomalous Void Rift within engagement range */
        if (player.active
            && distance_sq(rift.pos, player.pos) > 1600.0f * 1600.0f) {
            float ang  = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float dist = 850.0f + ((float)rand() / RAND_MAX) * 250.0f;
            rift.pos.x = player.pos.x + cosf(ang) * dist;
            rift.pos.y = player.pos.y + sinf(ang) * dist;
        }
    }

    /* ── Shockwaves (Resonance Cascade upgrade) ───────────────────── */
    for (int i = 0; i < 4; i++) {
        if (!shockwaves[i].active) continue;
        shockwaves[i].life   -= dt;
        shockwaves[i].radius += (shockwaves[i].max_radius / 0.5f) * dt;
        if (shockwaves[i].life <= 0.0f) {
            shockwaves[i].active = 0;
            continue;
        }
        /* Destroy asteroids whose centres fall within the expanding ring */
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;
            float dx   = shockwaves[i].pos.x - asteroids[a].pos.x;
            float dy   = shockwaves[i].pos.y - asteroids[a].pos.y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist > shockwaves[i].radius - 10.0f
                && dist < shockwaves[i].radius + 10.0f) {
                asteroids[a].active = 0;
                int next = asteroids[a].size - 1;
                spawn_particles(asteroids[a].pos, 15,
                                (SDL_Color){100, 255, 255, 255});
                if (next >= 1) {
                    spawn_asteroid(asteroids[a].pos, next);
                    spawn_asteroid(asteroids[a].pos, next);
                }
                score += 50;
            }
        }
    }

    /* ── Singularity Whip: trail-damage kills Void Stones on contact ─ */
    if (player.active && player_upgrades.singularity_whip) {
        for (int t = 0; t < PHOS_TRAIL_LEN; t++) {
            Vec2 tp = player.trail_pos[t];
            if (tp.x == 0.0f && tp.y == 0.0f) continue;
            for (int a = 0; a < MAX_ASTEROIDS; a++) {
                if (!asteroids[a].active) continue;
                if (check_collision(tp, 15.0f,
                                    asteroids[a].pos, asteroids[a].radius)) {
                    asteroids[a].active = 0;
                    int next = asteroids[a].size - 1;
                    spawn_particles(asteroids[a].pos, 15,
                                    (SDL_Color){255, 100, 255, 255});
                    wave_asteroids_destroyed++;
                    if (next >= 1) {
                        spawn_asteroid(asteroids[a].pos, next);
                        spawn_asteroid(asteroids[a].pos, next);
                    }
                    score += 100;
                }
            }
        }
    }
}

/* ================================================================== */

/** @brief Runs circle-circle collision detection between all relevant pairs.
 *
 *  Collision pairs evaluated each frame (in priority order):
 *    1. Player bolts vs Void Stones — scoring, splitting, combo multiplier,
 *       Nova Shell burst, Resonance Cascade shockwave, Split Shot fragments.
 *       Spawned fragments are tagged as bowling balls for section 6.
 *    2. UFO bolts vs Void Stones   — silent destruction, split logic.
 *    3. Player bolts vs UFO        — destroys vessel, XP orb shower.
 *    4. Player hull vs Void Stones — death or shield absorption.
 *    5. Player hull vs UFO/bullets — death, shield, Phase Shift, mirror decoy.
 *    6. Bowling Combos             — bowling_timer fragments vs other stones:
 *       chain counter, STRIKE! float, bonus score × chain depth.
 *
 *  The combo multiplier follows a step function:
 *    combo_count > 4  → ×4   (Void Fury)
 *    combo_count > 2  → ×2   (Breach Bonus)
 *    otherwise        → ×1
 *
 *  Extra lives are awarded every EXTRA_LIFE_SCORE_INTERVAL points using the
 *  module-scope last_extra_life_score rather than a local static so the value
 *  survives across new-game resets handled elsewhere in the file.
 */
static void update_collisions(float dt)
{
    /* ── 1. Player bolts vs Void Stones ──────────────────────────── */
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active) continue;
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;
            if (!check_collision(bullets[b].pos, 1.0f,
                                 asteroids[a].pos, asteroids[a].radius))
                continue;

            /* Shielded stone: strip shield, deflect non-piercing bolt */
            if (asteroids[a].has_shield) {
                asteroids[a].has_shield = 0;
                if (!player_upgrades.piercing) bullets[b].active = 0;
                spawn_particles(asteroids[a].pos, 18,
                                (SDL_Color){255, 220, 50, 255});
                audio_play(SFX_EXPLOSION_SM);
                break;
            }

            /* Nova Shell: spawn a ring of 6 fragment bolts on impact */
            if (player_upgrades.nova_explosion) {
                float bx = bullets[b].pos.x, by = bullets[b].pos.y;
                for (int nv = 0; nv < 6; nv++) {
                    float na = (float)nv * (2.0f * (float)M_PI / 6.0f);
                    for (int nbi = 0; nbi < MAX_BULLETS; nbi++) {
                        if (!bullets[nbi].active) {
                            bullets[nbi].active    = 1;
                            bullets[nbi].pos       = (Vec2){bx, by};
                            bullets[nbi].vel.x     = sinf(na) * 220.0f;
                            bullets[nbi].vel.y     = -cosf(na) * 220.0f;
                            bullets[nbi].life      = 0.8f;
                            bullets[nbi].bounces   = 0;
                            bullets[nbi].is_homing = 0;
                            bullets[nbi].pierces   = 0;
                            break;
                        }
                    }
                }
            }

            /* Resonance Cascade: emit a shockwave ring */
            if (player_upgrades.resonance_cascade) {
                for (int s = 0; s < 4; s++) {
                    if (!shockwaves[s].active) {
                        shockwaves[s].active     = 1;
                        shockwaves[s].pos        = asteroids[a].pos;
                        shockwaves[s].radius     = 10.0f;
                        shockwaves[s].max_radius = 150.0f;
                        shockwaves[s].thickness  = 5.0f;
                        shockwaves[s].life       = 0.5f;
                        break;
                    }
                }
            }

            if (!player_upgrades.piercing) bullets[b].active = 0;

            /* Fission Shot: spawn two angled fragment bolts on first impact */
            if (player_upgrades.split_shot) {
                float ba   = atan2f(bullets[b].vel.x, -bullets[b].vel.y);
                float bspd = sqrtf(bullets[b].vel.x * bullets[b].vel.x
                                   + bullets[b].vel.y * bullets[b].vel.y);
                for (int ss = 0; ss < 2; ss++) {
                    for (int si = 0; si < MAX_BULLETS; si++) {
                        if (!bullets[si].active) {
                            float sa = ba + (ss == 0 ? 0.65f : -0.65f);
                            bullets[si].active    = 1;
                            bullets[si].life      = BULLET_LIFE * 0.45f;
                            bullets[si].bounces   = 0;
                            bullets[si].pierces   = 0;
                            bullets[si].pos       = asteroids[a].pos;
                            bullets[si].trail_head = 0;
                            for (int t = 0; t < PHOS_TRAIL_LEN; t++) {
                                bullets[si].trail_pos[t] = bullets[si].pos;
                                bullets[si].trail_ang[t] = 0.0f;
                            }
                            bullets[si].vel.x  = sinf(sa) * bspd * 0.75f;
                            bullets[si].vel.y  = -cosf(sa) * bspd * 0.75f;
                            bullets[si].color  = (SDL_Color){255, 200, 80, 255};
                            bullets[si].is_homing = 0;
                            break;
                        }
                    }
                }
            }

            /* Vector Stress-Cracking: metal/crystal size-3 asteroids absorb the
             * first hit — spawn amber/teal crack particles and survive. The visual
             * crack lines are rendered below (render_entities) while health > 0. */
            asteroids[a].hit_count++;
            if (asteroids[a].health > 1) {
                asteroids[a].health--;
                SDL_Color crack_col = (asteroids[a].material == 2)
                    ? (SDL_Color){80,  255, 200, 255}   /* crystal: ice-teal  */
                    : (SDL_Color){255, 180,  60, 255};  /* metal:   amber-gold */
                spawn_particles(asteroids[a].pos, 10, crack_col);
                audio_play(SFX_EXPL_ROCK_SM);
                if (!player_upgrades.piercing) bullets[b].active = 0;
                break;  /* asteroid survives this hit — skip destroy logic */
            }

            /* Destroy the Void Stone */
            asteroids[a].active = 0;
            wave_asteroids_destroyed++;

            int next_size   = asteroids[a].size - 1;
            int pts         = (asteroids[a].size == 3) ? 20
                            : (asteroids[a].size == 2) ? 50 : 100;

            /*
             * Combo multiplier step function:
             *   >4 kills in COMBO_WINDOW → ×4 (Void Fury)
             *   >2 kills                 → ×2 (Breach Bonus)
             *   default                  → ×1
             * combo_count resets to 0 when combo_timer expires (see main loop).
             */
            combo_count++;
            combo_timer = COMBO_WINDOW;
            int combo_mult = (combo_count > 4) ? 4
                           : (combo_count > 2) ? 2 : 1;
            pts *= combo_mult;
            score += pts;

            /* Score floater at impact site */
            for (int sf = 0; sf < MAX_SCORE_FLOATS; sf++) {
                if (!score_floats[sf].active) {
                    score_floats[sf].active = 1;
                    score_floats[sf].x      = asteroids[a].pos.x;
                    score_floats[sf].y      = asteroids[a].pos.y;
                    score_floats[sf].vy     = -60.0f;
                    score_floats[sf].value  = pts;
                    score_floats[sf].life   = 1.2f;
                    break;
                }
            }

            spawn_particles(asteroids[a].pos, 15, (SDL_Color){255, 255, 255, 255});

            /* XP orb value scales with stone size */
            int orb_val = (asteroids[a].size == 3) ? 10
                        : (asteroids[a].size == 2) ?  5 : 2;
            spawn_orb(asteroids[a].pos, orb_val);

            /* Split into two smaller stones — tag fragments as bowling balls */
            if (next_size >= 1) {
                g_bowling_next_spawn = 1;
                spawn_asteroid(asteroids[a].pos, next_size);
                spawn_asteroid(asteroids[a].pos, next_size);
                g_bowling_next_spawn = 0;
                audio_play(asteroids[a].size == 3
                           ? SFX_EXPLOSION_MD : SFX_EXPLOSION_SM);
            } else {
                audio_play(SFX_EXPLOSION_SM);
            }

            /* Extra life every EXTRA_LIFE_SCORE_INTERVAL points.
             * last_extra_life_score is module-scope (declared in part 1)
             * so it survives across game-state transitions. */
            if (score / EXTRA_LIFE_SCORE_INTERVAL > last_extra_life_score) {
                lives++;
                last_extra_life_score = score / EXTRA_LIFE_SCORE_INTERVAL;
            }
            break;
        }
    }

    /* ── 2. UFO bolts vs Void Stones ─────────────────────────────── */
    for (int b = 0; b < MAX_UFO_BULLETS; b++) {
        if (!ufo_bullets[b].active) continue;
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;
            if (!check_collision(ufo_bullets[b].pos, 1.0f,
                                 asteroids[a].pos, asteroids[a].radius))
                continue;

            ufo_bullets[b].active = 0;
            asteroids[a].active   = 0;
            int next = asteroids[a].size - 1;
            spawn_particles(asteroids[a].pos, 15, (SDL_Color){255, 120, 120, 255});
            if (next >= 1) {
                spawn_asteroid(asteroids[a].pos, next);
                spawn_asteroid(asteroids[a].pos, next);
                audio_play(asteroids[a].size == 3
                           ? SFX_EXPLOSION_MD : SFX_EXPLOSION_SM);
            } else {
                audio_play(SFX_EXPLOSION_SM);
            }
            break;
        }
    }

    /* ── 3. Player bolts vs UFO ──────────────────────────────────── */
    if (ufo.active) {
        for (int b = 0; b < MAX_BULLETS; b++) {
            if (!bullets[b].active) continue;
            if (!check_collision(bullets[b].pos, 1.0f, ufo.pos, ufo.radius))
                continue;

            bullets[b].active = 0;
            ufo.active        = 0;
            audio_stop(SFX_UFO_LOOP);
            audio_play(SFX_EXPLOSION_LG);
            spawn_particles(ufo.pos, 25, (SDL_Color){255, 180, 50, 255});

            /* UFO drops an XP orb shower proportional to its threat level */
            int orb_count = (ufo.size == 2) ? 5 : 15;
            for (int o = 0; o < orb_count; o++)
                spawn_orb(ufo.pos, (ufo.size == 2) ? 10 : 20);

            score += (ufo.size == 2) ? 200 : 1000;
            ufo_spawn_timer = 25.0f + ((float)rand() / RAND_MAX) * 15.0f;
            break;
        }
    }

    /* ── 4. Player hull vs Void Stones ───────────────────────────── */
    if (player.active && player.invuln_timer <= 0.0f) {
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;
            if (!check_collision(player.pos, player.radius,
                                 asteroids[a].pos, asteroids[a].radius))
                continue;

            /* Allow gameplay hooks to intercept / cancel the collision */
            if (on_collision(1, a)) continue;

            if (player_upgrades.shield_active) {
                /* Ether Shroud absorbs one impact */
                player_upgrades.shield_active = 0;
                player.invuln_timer           = 2.0f;
                audio_play(SFX_EXPLOSION_MD);
                asteroids[a].active = 0;
                wave_asteroids_destroyed++;
                int next = asteroids[a].size - 1;
                spawn_particles(asteroids[a].pos, 15,
                                (SDL_Color){255, 255, 255, 255});
                if (next >= 1) {
                    spawn_asteroid(asteroids[a].pos, next);
                    spawn_asteroid(asteroids[a].pos, next);
                }
                continue;
            }

            if (check_void_stone()) continue;

            /* Player destroyed */
            player.active = 0;
            audio_play(SFX_EXPLOSION_LG);
            audio_stop(SFX_THRUST);
            spawn_particles(player.pos, 35, (SDL_Color){255, 200, 100, 255});
            spawn_event_float(player.pos.x, player.pos.y - 20.0f,
                              "HIT", HUD_CINNABAR);
            cugel9_say("STRUCTURAL INTEGRITY COMPROMISED. COMPENSATING FOR PILOT INEPTITUDE.");

            /* Stone splits on impact */
            asteroids[a].active = 0;
            wave_asteroids_destroyed++;
            int next = asteroids[a].size - 1;
            if (next >= 1) {
                spawn_asteroid(asteroids[a].pos, next);
                spawn_asteroid(asteroids[a].pos, next);
            }

            lives--;
            if (lives <= 0) {
                game_state = STATE_GAMEOVER;
                audio_stop(SFX_UFO_LOOP);
            }
            break;
        }
    }

    /* ── 5. Player hull vs UFO / UFO bolts ───────────────────────── */
    if (player.active && player.invuln_timer <= 0.0f) {
        /* Hull vs UFO body */
        if (ufo.active
            && check_collision(player.pos, player.radius,
                               ufo.pos, ufo.radius)) {
            if (player_upgrades.shield_active) {
                player_upgrades.shield_active = 0;
                player.invuln_timer           = 2.0f;
                audio_play(SFX_EXPLOSION_MD);
                ufo.active = 0;
                audio_stop(SFX_UFO_LOOP);
                spawn_particles(ufo.pos, 25, (SDL_Color){255, 180, 50, 255});
                ufo_spawn_timer = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
            } else {
                player.active = 0;
                ufo.active    = 0;
                audio_stop(SFX_THRUST);
                audio_stop(SFX_UFO_LOOP);
                audio_play(SFX_EXPLOSION_LG);
                spawn_particles(player.pos, 35, (SDL_Color){255, 200, 100, 255});
                spawn_particles(ufo.pos,    25, (SDL_Color){255, 180,  50, 255});
                spawn_event_float(player.pos.x, player.pos.y - 20.0f,
                                  "HIT", HUD_CINNABAR);
                cugel9_say("DIRECT IMPACT DETECTED. RECORDING FINAL MOMENTS FOR POSTERITY.");
                lives--;
                if (lives <= 0) game_state = STATE_GAMEOVER;
                ufo_spawn_timer = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
            }
        }

        /* Hull vs UFO bolts (re-check invuln in case above set it) */
        if (player.active && player.invuln_timer <= 0.0f) {
            for (int b = 0; b < MAX_UFO_BULLETS; b++) {
                if (!ufo_bullets[b].active) continue;
                if (!check_collision(player.pos, player.radius,
                                     ufo_bullets[b].pos, 1.0f))
                    continue;

                if (player_upgrades.shield_active) {
                    /* Ether Shroud */
                    player_upgrades.shield_active = 0;
                    player.invuln_timer           = 2.0f;
                    ufo_bullets[b].active         = 0;
                    audio_play(SFX_EXPLOSION_MD);
                } else if (player_upgrades.phase_shift) {
                    /* Phase Shift: one-time bullet pass-through */
                    player_upgrades.phase_shift = 0;
                    player.invuln_timer         = 3.5f;
                    ufo_bullets[b].active       = 0;
                    audio_play(SFX_EXPLOSION_MD);
                    spawn_particles(player.pos, 20,
                                    (SDL_Color){100, 200, 255, 255});
                } else if (mirror_active_flag
                           && distance_sq(ufo_bullets[b].pos, mirror_pos)
                              < 30.0f * 30.0f) {
                    /* Wraith Twin decoy absorbs the bolt */
                    mirror_active_flag          = 0;
                    player_upgrades.mirror_image = 0;
                    ufo_bullets[b].active       = 0;
                    spawn_particles(mirror_pos, 20,
                                    (SDL_Color){100, 180, 255, 255});
                } else {
                    /* No defence: player dies */
                    ufo_bullets[b].active = 0;
                    if (check_void_stone()) continue;
                    player.active         = 0;
                    audio_play(SFX_EXPLOSION_LG);
                    audio_stop(SFX_THRUST);
                    spawn_particles(player.pos, 35,
                                    (SDL_Color){255, 200, 100, 255});
                    spawn_event_float(player.pos.x, player.pos.y - 20.0f,
                                      "HIT", HUD_CINNABAR);
                    lives--;
                    if (lives <= 0) {
                        game_state = STATE_GAMEOVER;
                        audio_stop(SFX_UFO_LOOP);
                    }
                }
                break;
            }
        }
    }

    /* ── 5c. Ascian hit tests (Item 21) ────────────────────────────── */
    /* (a) Player bolts vs Ascians — destroys the interceptor, awards
     *     elite score + XP.  Ascians are rare so the reward is higher
     *     than rust-weavers.                                           */
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active) continue;
        int asc_slot = ascian_hit_test(bullets[b].pos.x, bullets[b].pos.y);
        if (asc_slot >= 0) {
            bullets[b].active = 0;
            spawn_particles(bullets[b].pos, 24, HUD_MAGENTA);
            audio_play(SFX_EXPLOSION_MD);
            spawn_orb(bullets[b].pos, 10);
            score += 150;
            spawn_event_float(bullets[b].pos.x, bullets[b].pos.y - 18.0f,
                              "ASCIAN DOWN", HUD_MAGENTA);
        }
    }

    /* (b) Ascian bolt vs player hull — standard projectile hit; the
     *     player's shields/Phase Shift can soak it (no special bypass).
     *     Invuln_timer still applies so post-spawn / post-Phase-Shift
     *     grace is preserved.                                          */
    if (player.active && player.invuln_timer <= 0.0f) {
        if (ascian_check_player_hit(player.pos.x, player.pos.y,
                                    player.radius + 2.0f)) {
            if (check_void_stone()) {
                /* Void Stone armour soaks one hit — near-miss flash.   */
            } else {
                player.active = 0;
                audio_play(SFX_EXPLOSION_LG);
                audio_stop(SFX_THRUST);
                spawn_particles(player.pos, 30, HUD_MAGENTA);
                spawn_event_float(player.pos.x, player.pos.y - 20.0f,
                                  "INTERCEPTED", HUD_MAGENTA);
                cugel9_say("ASCIAN BOLT BREACHED THE HULL. PERHAPS YOU SHOULD HAVE DODGED.");
                lives--;
                if (lives <= 0) {
                    game_state = STATE_GAMEOVER;
                    audio_stop(SFX_UFO_LOOP);
                } else {
                    player.invuln_timer = 2.0f;
                    /* Respawn near origin so the next life isn't stuck
                     * inside the same Ascian's volley wedge.           */
                    player.pos.x = (float)(SCREEN_WIDTH / 2);
                    player.pos.y = (float)(SCREEN_HEIGHT / 2);
                    player.vel.x = player.vel.y = 0.0f;
                }
            }
        }
    }

    /* ── 5d. Lictor hit tests (Item 22) ─────────────────────────────── */
    /* (a) Player bolts vs Lictors — destroys the interceptor, awards
     *     elite score + XP.  Higher reward than Ascians because the
     *     Lictor is harder to track (active pursuit, not rigid patrol). */
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active) continue;
        int lic_slot = lictor_hit_test(bullets[b].pos.x, bullets[b].pos.y);
        if (lic_slot >= 0) {
            bullets[b].active = 0;
            spawn_particles(bullets[b].pos, 28, HUD_CINNABAR);
            audio_play(SFX_EXPLOSION_MD);
            spawn_orb(bullets[b].pos, 12);
            score += 200;
            spawn_event_float(bullets[b].pos.x, bullets[b].pos.y - 18.0f,
                              "LICTOR DOWN", HUD_CINNABAR);
        }
    }

    /* (b) Lictor aimed bolt vs player hull — standard projectile hit;
     *     shields/Phase Shift can soak it (no special bypass).  Void
     *     Stone armour soaks one hit if equipped.                       */
    if (player.active && player.invuln_timer <= 0.0f) {
        if (lictor_check_player_hit(player.pos.x, player.pos.y,
                                    player.radius + 2.0f)) {
            if (check_void_stone()) {
                /* Void Stone armour soaks one hit — near-miss flash.   */
            } else {
                player.active = 0;
                audio_play(SFX_EXPLOSION_LG);
                audio_stop(SFX_THRUST);
                spawn_particles(player.pos, 32, HUD_CINNABAR);
                spawn_event_float(player.pos.x, player.pos.y - 20.0f,
                                  "PURSUED", HUD_CINNABAR);
                cugel9_say("LICTOR RAN YOU DOWN. PREDICTABLE OUTCOME, GIVEN YOUR PILOTING.");
                lives--;
                if (lives <= 0) {
                    game_state = STATE_GAMEOVER;
                    audio_stop(SFX_UFO_LOOP);
                } else {
                    player.invuln_timer = 2.0f;
                    /* Respawn near origin so the next life isn't immediately
                     * inside the pursuing Lictor's firing arc.              */
                    player.pos.x = (float)(SCREEN_WIDTH / 2);
                    player.pos.y = (float)(SCREEN_HEIGHT / 2);
                    player.vel.x = player.vel.y = 0.0f;
                }
            }
        }
    }

    /* ── 5e. EMP Sentinel hit tests (Item 25) ───────────────────────── */
    /* (a) Player bolts vs Sentinels — destroys the unit, +100 score,
     *     +6 XP orb.  Sentinels are fragile (one-shot kill) because the
     *     pulse-charge telegraph already gives the player a fair chance
     *     to engage before the pulse fires. */
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active) continue;
        int emp_slot = emp_sentinel_hit_test(bullets[b].pos.x, bullets[b].pos.y);
        if (emp_slot >= 0) {
            bullets[b].active = 0;
            spawn_particles(bullets[b].pos, 24, HUD_PURPLE);
            audio_play(SFX_EXPLOSION_MD);
            spawn_orb(bullets[b].pos, 6);
            score += 100;
            spawn_event_float(bullets[b].pos.x, bullets[b].pos.y - 18.0f,
                              "SENTINEL DOWN", HUD_TEXT_CYAN);
        }
    }

    /* (b) EMP pulse ring vs player — status effect (not damage).  Sets
     *     player.emp_lock_timer; update_player_physics reads it and
     *     suppresses steering/thrust input for the duration.  Bypasses
     *     Ether Shroud and Phase Shift because it is not a projectile
     *     hit — it's an EM field.  Void Stone armour does NOT soak it
     *     (matches the status-effect framing — armour blocks impacts,
     *     not radiation). */
    if (player.active && player.invuln_timer <= 0.0f) {
        if (emp_sentinel_check_player_pulse(player.pos.x, player.pos.y)) {
            /* Re-arm the lock to its max duration each frame the ring
             * touches us, so a stationary drift through the pulse keeps
             * us crippled for the full window rather than glitching out
             * after the first contact frame. */
            if (player.emp_lock_timer < 1.6f) player.emp_lock_timer = 1.6f;
            audio_play(SFX_EXPLOSION_SM);
            spawn_event_float(player.pos.x, player.pos.y - 20.0f,
                              "EMP LOCK", HUD_PURPLE);
            cugel9_say("STEERING SYSTEMS OFFLINE. CONGRATULATIONS, YOU ARE DEBRIS.");
        }
    }

    /* ── 5f. Scavenger Probe hit tests (Item 24) ───────────────────── */
    /* Player bolts vs probes — destroys the probe and DROPS its stolen
     * cargo back at the probe's last position as recoverable Chronicle
     * orbs (the same orb type used for XP since we have no dedicated
     * Void-Steel-orb entity; the player still has to fly back to pick
     * them up, which is the loss-pressure beat).  Scavengers do not
     * shoot, so there is no projectile-vs-player branch in this block —
     * the threat is entirely about resource theft. */
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active) continue;
        {
            int   stolen = 0;
            float drop_x = 0.0f, drop_y = 0.0f;
            int   sc_slot = scavenger_hit_test(bullets[b].pos.x,
                                               bullets[b].pos.y,
                                               &stolen, &drop_x, &drop_y);
            if (sc_slot >= 0) {
                bullets[b].active = 0;
                spawn_particles(bullets[b].pos, 22, HUD_AMBER);
                audio_play(SFX_EXPLOSION_MD);
                /* Always give a small XP orb for the kill itself. */
                spawn_orb(bullets[b].pos, 4);
                score += 120;
                spawn_event_float(bullets[b].pos.x,
                                  bullets[b].pos.y - 18.0f,
                                  "SCAVENGER DOWN", HUD_AMBER);
                /* Refund the stolen Void Steel.  We mutate the same
                 * static counter the shop/spawn code uses; one orb per
                 * unit so the player can see what came back. */
                if (stolen > 0) {
                    res_void_steel += stolen;
                    {
                        Vec2 d = { drop_x, drop_y };
                        spawn_particles(d, 8 + stolen * 4, HUD_TEXT_CYAN);
                    }
                    spawn_event_float(drop_x, drop_y - 12.0f,
                                      "+VOID STEEL", HUD_TEXT_CYAN);
                    cugel9_say("CARGO RECOVERED. THE UNIVERSE STILL HATES YOU, BUT LESS.");
                }
            }
        }
    }

    /* ── 5b. Rust-Weaver hit tests (Item 23) ───────────────────────── */
    /* (a) Player bolts vs drones — destroys the drone, awards score + XP.
     *     rustweaver_hit_test deactivates the drone if it's within ~16u of
     *     the bullet position and returns the slot (>= 0) on success.       */
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active) continue;
        int rw_slot = rustweaver_hit_test(bullets[b].pos.x, bullets[b].pos.y);
        if (rw_slot >= 0) {
            bullets[b].active = 0;
            spawn_particles(bullets[b].pos, 16,
                            (SDL_Color){57, 255, 20, 220});
            audio_play(SFX_EXPLOSION_MD);
            spawn_orb(bullets[b].pos, 8);
            score += 75;
            spawn_event_float(bullets[b].pos.x, bullets[b].pos.y - 18.0f,
                              "RUSTWEAVER DOWN",
                              (SDL_Color){57, 255, 20, 240});
        }
    }

    /* (b) Rust-Weaver corrosive spit vs player hull — bypasses Ether Shroud
     *     and Phase Shift per design contract ("degrades hull plating
     *     directly").  Still respects invuln_timer so the player can't be
     *     hit during the post-spawn / post-Phase-Shift grace window.       */
    if (player.active && player.invuln_timer <= 0.0f) {
        if (rustweaver_check_player_hit(player.pos.x, player.pos.y,
                                        player.radius + 2.0f)) {
            if (check_void_stone()) {
                /* Void Stone armour soaks one hit — treat as a near-miss. */
            } else {
                player.active = 0;
                audio_play(SFX_EXPLOSION_LG);
                audio_stop(SFX_THRUST);
                spawn_particles(player.pos, 35,
                                (SDL_Color){180, 255, 60, 255});
                spawn_event_float(player.pos.x, player.pos.y - 20.0f,
                                  "CORROSION", HUD_CINNABAR);
                cugel9_say("HULL PLATING DISSOLVED. THIS IS WHY WE CAN'T HAVE NICE THINGS.");
                lives--;
                if (lives <= 0) {
                    game_state = STATE_GAMEOVER;
                    audio_stop(SFX_UFO_LOOP);
                }
            }
        }
    }

    /* ── 6. Bowling Combos: bullet-spawned fragments vs other Void Stones ── */
    /*
     *  When a player bullet destroys an asteroid, its child fragments are tagged
     *  with bowling_timer > 0.  While that timer is live, if a fragment collides
     *  with another asteroid the chain counter increments: each subsequent collision
     *  keeps the chain alive, spawns a STRIKE! event float, and awards bonus score
     *  that scales with chain depth.  The chain resets if no new collision happens
     *  within bowling_chain_timer seconds.
     */
    {
        /* Decay the chain window — reset chain count when window expires */
        if (bowling_chain_timer > 0.0f) {
            bowling_chain_timer -= dt;
            if (bowling_chain_timer <= 0.0f) bowling_chain_count = 0;
        }

        /* ── Cugel-9 message timer tick ── */
        for (int ci = 0; ci < CUGEL9_MAX_MSGS; ci++) {
            if (cugel9_buf[ci].timer > 0.0f) {
                cugel9_buf[ci].timer -= dt;
                if (cugel9_buf[ci].timer < 0.0f) cugel9_buf[ci].timer = 0.0f;
            }
        }
        if (cugel9_cooldown > 0.0f) cugel9_cooldown -= dt;

        for (int ba = 0; ba < MAX_ASTEROIDS; ba++) {
            if (!asteroids[ba].active) continue;
            if (bowling_timer[ba] <= 0.0f) continue;
            bowling_timer[ba] -= dt;

            for (int bb = 0; bb < MAX_ASTEROIDS; bb++) {
                if (bb == ba) continue;
                if (!asteroids[bb].active) continue;
                if (!check_collision(asteroids[ba].pos, asteroids[ba].radius,
                                     asteroids[bb].pos, asteroids[bb].radius))
                    continue;

                /* ── Chain hit confirmed! ── */
                bowling_chain_count++;
                bowling_chain_timer = 1.0f; /* extend window for next link */

                Vec2 hit_mid = {
                    (asteroids[ba].pos.x + asteroids[bb].pos.x) * 0.5f,
                    (asteroids[ba].pos.y + asteroids[bb].pos.y) * 0.5f
                };

                /* Score: base points × chain depth */
                int pts_b = ((asteroids[bb].size == 3) ? 30
                           : (asteroids[bb].size == 2) ? 75 : 150)
                           * bowling_chain_count;
                score += pts_b;

                /* Destroy the struck Void Stone — spawn as new bowling fragments */
                int next_b = asteroids[bb].size - 1;
                asteroids[bb].active = 0;
                wave_asteroids_destroyed++;
                spawn_particles(asteroids[bb].pos, 22,
                                (SDL_Color){57, 255, 20, 210});

                if (next_b >= 1) {
                    g_bowling_next_spawn = 1; /* tag new fragments as bowling balls */
                    spawn_asteroid(asteroids[bb].pos, next_b);
                    spawn_asteroid(asteroids[bb].pos, next_b);
                    g_bowling_next_spawn = 0;
                    audio_play(SFX_EXPLOSION_MD);
                } else {
                    audio_play(SFX_EXPLOSION_SM);
                }

                /* STRIKE! event float above the impact midpoint */
                spawn_event_float(hit_mid.x, hit_mid.y - 30.0f,
                                  "STRIKE!",
                                  (SDL_Color){57, 255, 20, 240});

                /* Extra rumble on multi-chain hits */
                if (bowling_chain_count >= 2) {
                    screen_shake_timer     = 0.25f;
                    screen_shake_intensity = 5.0f;
                }

                /* The bowling-ball fragment keeps flying — refresh its window */
                bowling_timer[ba] = 0.7f;
                break; /* one chain link per bowling ball per frame */
            }
        }
    }
}

/* ================================================================== */

/** @brief Manages spawn timers for UFOs, rifts, and the beat-pulse system.
 *
 *  When the UFO is inactive its spawn timer counts down; spawn_ufo() is
 *  called when it expires.  The Anomalous Void Rift is activated once the
 *  player reaches level 4.  The beat system drives the tempo-based "thump"
 *  sound that accelerates as fewer Void Stones remain on screen.
 */
static void update_spawning(float dt)
{
    /* UFO spawn timer */
    if (!ufo.active) {
        ufo_spawn_timer -= dt;
        if (ufo_spawn_timer <= 0.0f) {
            spawn_ufo();
            cugel9_say("CACOGEN VESSEL INBOUND. EVASIVE ACTION ADVISED. OR DON'T.");
        }
    }

    /* Activate the Anomalous Void Rift at player level 4+ */
    if (player.active && player_level >= 4 && !rift.active) {
        rift.active = 1;
        float ang  = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
        float dist = 850.0f + ((float)rand() / RAND_MAX) * 250.0f;
        rift.pos.x      = player.pos.x + cosf(ang) * dist;
        rift.pos.y      = player.pos.y + sinf(ang) * dist;
        rift.radius     = 35.0f;
        rift.angle1     = 0.0f;
        rift.angle2     = 0.0f;
        rift.pulse_timer = 0.0f;
        rift.spawn_timer = 5.0f;
    }

    /* ── Item 23: Rust-Weaver Drone spawn pacing ──────────────────── */
    /* Slow corrosive drones appear only in Zone 2+ where the danger ramps.
     * Static timer keeps the pacing private to this function — no extra
     * globals.  Spawn ring 700–1000 u from player so they drift in over
     * several seconds; failure to allocate (pool full) backs off briefly. */
    {
        static float rustweaver_spawn_timer = 25.0f;
        if (player.active && player_zone >= 2) {
            rustweaver_spawn_timer -= dt;
            if (rustweaver_spawn_timer <= 0.0f) {
                float ang  = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
                float dist = 700.0f + ((float)rand() / RAND_MAX) * 300.0f;
                float sx   = player.pos.x + cosf(ang) * dist;
                float sy   = player.pos.y + sinf(ang) * dist;
                if (rustweaver_spawn(sx, sy) >= 0) {
                    /* Re-arm with jitter (35–55 s) so spawns don't clump. */
                    rustweaver_spawn_timer = 35.0f + (float)(rand() % 21);
                } else {
                    /* Pool full — short retry. */
                    rustweaver_spawn_timer = 5.0f;
                }
            }
        } else {
            /* Outside Zone 2+ the timer is held at a fresh value so the
             * first spawn after re-entry isn't immediate. */
            rustweaver_spawn_timer = 25.0f;
        }
    }

    /* ── Item 21: Ascian spawn pacing ─────────────────────────────── */
    /* Elite, rare, voiceless interceptors patrolling fixed regular
     * polygons.  Zone 3+ only — they belong to the deeper zones where
     * the danger ramp is highest.  Spawn timer is longer than the
     * rust-weaver pacing because their on-screen behaviour is heavier
     * (each one effectively claims a 200u patrol ring).
     *
     * Polygon parameters are deterministic per slot index so the patrol
     * geometry stays "rigid" frame-to-frame as required by the spec.
     * Center is placed 800-1100u from the player at a random angle so
     * the Ascian's first appearance is off-screen and drifts into
     * combat range over its own orbit. */
    {
        static float ascian_spawn_timer = 40.0f;
        static int   ascian_spawn_count = 0;
        if (player.active && player_zone >= 3) {
            ascian_spawn_timer -= dt;
            if (ascian_spawn_timer <= 0.0f) {
                float ang  = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
                float dist = 800.0f + ((float)rand() / RAND_MAX) * 300.0f;
                float sx   = player.pos.x + cosf(ang) * dist;
                float sy   = player.pos.y + sinf(ang) * dist;
                /* Cycle through triangle / square / pentagon for visual
                 * variety while remaining deterministic per-spawn.       */
                int sides  = 3 + (ascian_spawn_count % 3);
                float pr   = 140.0f + (float)(ascian_spawn_count % 2) * 40.0f;
                if (ascian_spawn(sx, sy, sides, pr) >= 0) {
                    ascian_spawn_count++;
                    /* Re-arm 55-85 s — sparse, elite cadence. */
                    ascian_spawn_timer = 55.0f + (float)(rand() % 31);
                } else {
                    ascian_spawn_timer = 8.0f;  /* pool full, short retry */
                }
            }
        } else {
            ascian_spawn_timer = 40.0f;
        }
    }

    /* ── Item 22: Lictor spawn pacing ─────────────────────────────── */
    /* Elite predators that pursue the player at high speed.  Zone 4+
     * only — they belong to the deepest tier of the danger ramp,
     * behind Ascians (Zone 3+) and Rust-Weavers (Zone 2+).  Spawn
     * cadence is sparse (90-130s) so each appearance feels like a
     * threat event, not a constant pressure.                          */
    {
        static float lictor_spawn_timer = 60.0f;
        if (player.active && player_zone >= 4) {
            lictor_spawn_timer -= dt;
            if (lictor_spawn_timer <= 0.0f) {
                float ang  = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
                float dist = 900.0f + ((float)rand() / RAND_MAX) * 300.0f;
                float sx   = player.pos.x + cosf(ang) * dist;
                float sy   = player.pos.y + sinf(ang) * dist;
                if (lictor_spawn(sx, sy, player.pos.x, player.pos.y) >= 0) {
                    /* Re-arm 90-130 s — rare, threat-event cadence. */
                    lictor_spawn_timer = 90.0f + (float)(rand() % 41);
                    /* Foreshadow the arrival with a sad Cugel-9 quip. */
                    cugel9_say("LICTOR ON INTERCEPT VECTOR. RECOMMEND PRAYER.");
                } else {
                    lictor_spawn_timer = 10.0f;  /* pool full, short retry */
                }
            }
        } else {
            lictor_spawn_timer = 60.0f;
        }
    }

    /* ── Item 25: EMP Sentinel spawn pacing ───────────────────────── */
    /* Status-disruption units appear from Zone 2 onward — sit alongside
     * Rust-Weavers (Zone 2+) and below Ascian/Lictor in the danger ramp
     * because the EMP lock is non-lethal on its own.  Cadence 50-80 s
     * with a 750-1000 u spawn ring; pool cap of 3 lives entirely inside
     * the module so a "pool full" return short-retries here. */
    {
        static float emp_spawn_timer = 45.0f;
        if (player.active && player_zone >= 2) {
            emp_spawn_timer -= dt;
            if (emp_spawn_timer <= 0.0f) {
                float ang  = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
                float dist = 750.0f + ((float)rand() / RAND_MAX) * 250.0f;
                float sx   = player.pos.x + cosf(ang) * dist;
                float sy   = player.pos.y + sinf(ang) * dist;
                if (emp_sentinel_spawn(sx, sy) >= 0) {
                    /* Re-arm 50-80 s — sparse so the player has a clear
                     * window between disruption events. */
                    emp_spawn_timer = 50.0f + (float)(rand() % 31);
                } else {
                    emp_spawn_timer = 8.0f;  /* pool full, short retry */
                }
            }
        } else {
            emp_spawn_timer = 45.0f;
        }
    }

    /* ── Item 24: Scavenger Probe spawn pacing ──────────────────────── */
    /* Probes only show up when the player has Void Steel to steal.
     * Otherwise the threat is null and the spawn would be pointless
     * theatre.  Zone 1+ (Inner Belt onward) — keep Home Space probe-
     * free so the early-game pickup loop is uninterrupted.  Cadence
     * 60-90 s with a 900-1200 u spawn ring; pool cap 4 lives entirely
     * inside the module so a "pool full" return short-retries here.    */
    {
        static float scav_spawn_timer = 75.0f;
        if (player.active && player_zone >= 1 && res_void_steel > 0) {
            scav_spawn_timer -= dt;
            if (scav_spawn_timer <= 0.0f) {
                float ang  = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
                float dist = 900.0f + ((float)rand() / RAND_MAX) * 300.0f;
                float sx   = player.pos.x + cosf(ang) * dist;
                float sy   = player.pos.y + sinf(ang) * dist;
                if (scavenger_spawn(sx, sy,
                                    player.pos.x, player.pos.y) >= 0) {
                    /* Foreshadow Cugel-9 quip on first arrival per
                     * cadence so the player learns to identify the
                     * sound + dialog as "guard your inventory now". */
                    cugel9_say("UNAUTHORIZED SALVAGE BIDDER APPROACHING. RECOMMEND PRECISION DISCOURAGEMENT.");
                    scav_spawn_timer = 60.0f + (float)(rand() % 31);
                } else {
                    scav_spawn_timer = 10.0f;  /* pool full, short retry */
                }
            }
        } else {
            /* No steel to steal or wrong zone — drift the timer back to a
             * "freshly armed" value so the next legitimate trigger has
             * the full window, not a cached countdown from earlier. */
            scav_spawn_timer = 75.0f;
        }
    }

    /*
     * Sound beat system: a two-tone pulse whose period compresses as the
     * number of active Void Stones falls.  speed_mult ∈ [0.2, 1.0] maps
     * to a delay of [0.25 s, 1.0 s].
     * cached_active_asteroids is written by update_asteroids() earlier
     * this frame.
     */
    if (cached_active_asteroids > 0) {
        float speed_mult = (float)cached_active_asteroids / (float)(4 + level);
        if (speed_mult < 0.2f) speed_mult = 0.2f;
        if (speed_mult > 1.0f) speed_mult = 1.0f;
        float beat_delay = 0.25f + 0.75f * speed_mult;

        /* Item 22: "Arrival accelerates beat" — while any Lictor is on
         * the field, compress the beat tempo by ~35 % so the danger
         * pulse rises with the elite predator's presence.  Clamp the
         * lower bound to keep the audio from clipping into a buzz. */
        if (lictor_alive_count() > 0) {
            beat_delay *= 0.65f;
            if (beat_delay < 0.15f) beat_delay = 0.15f;
        }

        beat_timer -= dt;
        if (beat_timer <= 0.0f) {
            cur_beat   = 1 - cur_beat;
            audio_play(cur_beat ? SFX_BEAT1 : SFX_BEAT2);
            beat_timer = beat_delay;
        }
    }
}

/* ================================================================== */

/** @brief Handles XP collection thresholds, combo timing, near-miss XP,
 *         score floaters, and edge-flash detection.
 *
 *  The combo timer counts down every frame; hitting zero resets combo_count.
 *  Near-miss adrenaline XP: if a UFO bolt passes within 35 u of the player
 *  (but outside the hull radius) a small orb is spawned and a 1-second
 *  cooldown is started to prevent spam.
 */
static void update_progression(float dt)
{
    /* Combo window countdown */
    if (combo_timer > 0.0f) {
        combo_timer -= dt;
        if (combo_timer <= 0.0f) combo_count = 0;
    }

    /* Near-miss cooldown */
    if (near_miss_cooldown > 0.0f)
        near_miss_cooldown -= dt;

    /* Adrenaline bonus: reward the player for surviving a close shave */
    if (player.active && near_miss_cooldown <= 0.0f) {
        for (int b = 0; b < MAX_UFO_BULLETS; b++) {
            if (!ufo_bullets[b].active) continue;
            float dx = ufo_bullets[b].pos.x - player.pos.x;
            float dy = ufo_bullets[b].pos.y - player.pos.y;
            float d2 = dx*dx + dy*dy;
            if (d2 < 35.0f * 35.0f
                && d2 > player.radius * player.radius) {
                spawn_orb(player.pos, 3);
                near_miss_cooldown = 1.0f;
                break;
            }
        }
    }

    /* Edge-flash: warn when the player approaches viewport edges at speed */
    if (player.active) {
        float spd = sqrtf(player.vel.x * player.vel.x
                          + player.vel.y * player.vel.y);
        if (spd > 120.0f) {
            float margin = 40.0f;
            if (player.pos.x < margin
                || player.pos.x > SCREEN_WIDTH  - margin
                || player.pos.y < margin
                || player.pos.y > SCREEN_HEIGHT - margin) {
                edge_flash_timer = 0.15f;
            }
        }
    }
    if (edge_flash_timer > 0.0f)
        edge_flash_timer -= dt;
    if (warp_flash_timer > 0.0f)
        warp_flash_timer -= dt;

    /* Score floaters: drift upward and fade */
    for (int i = 0; i < MAX_SCORE_FLOATS; i++) {
        if (!score_floats[i].active) continue;
        score_floats[i].y    += score_floats[i].vy * dt;
        score_floats[i].life -= dt;
        if (score_floats[i].life <= 0.0f)
            score_floats[i].active = 0;
    }

    /* Event text floaters: drift upward and fade (CRIT!, HIT, VENT!, etc.) */
    for (int i = 0; i < MAX_EVENT_FLOATS; i++) {
        if (!event_floats[i].active) continue;
        event_floats[i].y    += event_floats[i].vy * dt;
        event_floats[i].life -= dt;
        if (event_floats[i].life <= 0.0f)
            event_floats[i].active = 0;
    }

    /* XP-bar flash decay */
    if (xp_flash_timer > 0.0f)
        xp_flash_timer -= dt;

    /* God-mode message banner decay */
    if (god_mode_msg_timer > 0.0f)
        god_mode_msg_timer -= dt;

    /* Zone transition banner decay */
    if (zone_banner_timer > 0.0f)
        zone_banner_timer -= dt;

    /* Chronicle chord harmonics: orb-chain collection window */
    if (orb_chord_timer > 0.0f)
        orb_chord_timer -= dt;

    /* ── Solar Flare Cycles + Star-Shadow check ─────────────────────
     * Three-phase state machine: COUNTUP → WARNING → ACTIVE → re-roll.
     * Home Space (zone 0) is exempt — flares only tick once the player
     * pushes past `SOLAR_FLARE_MIN_ZONE`.  During WARNING and ACTIVE the
     * star-shadow helper scans the asteroid pool for cover; the boolean
     * is consumed by update_player_physics (drain multiplier) and by
     * render_entities (shadow-line visual).                          */
    if (player.active
        && (game_state == STATE_PLAYING
            || game_state == STATE_ATTRACT_GAMEPLAY)
        && player_zone >= SOLAR_FLARE_MIN_ZONE)
    {
        if (solar_flare_active_t > 0.0f) {
            /* ACTIVE phase: drain × SOLAR_FLARE_FUEL_MULT (or shadow mult) */
            solar_flare_active_t -= dt;
            /* Re-check shadow each frame — asteroids drift */
            {
                float px = player.pos.x, py = player.pos.y;
                float pd = sqrtf(px*px + py*py);
                int in_shadow = 0;
                if (pd > 1.0f) {
                    float ux = -px / pd, uy = -py / pd;
                    for (int i = 0; i < MAX_ASTEROIDS; i++) {
                        if (!asteroids[i].active) continue;
                        if (asteroids[i].size < 2) continue;
                        float ax = asteroids[i].pos.x, ay = asteroids[i].pos.y;
                        float ad = sqrtf(ax*ax + ay*ay);
                        if (ad >= pd) continue;
                        float dx = ax - px, dy = ay - py;
                        float t  = dx*ux + dy*uy;
                        if (t < 0.0f || t > pd) continue;
                        float perp_x = dx - t*ux;
                        float perp_y = dy - t*uy;
                        float perp = sqrtf(perp_x*perp_x + perp_y*perp_y);
                        float half_w = SOLAR_FLARE_SHADOW_HALF_W
                                     + asteroids[i].radius * 0.6f;
                        if (perp <= half_w) { in_shadow = 1; break; }
                    }
                }
                solar_flare_in_shadow = in_shadow;
            }
            /* Sensor Static roll: while the flare is ACTIVE and the
             * player is OUT of asteroid shadow, every few seconds we
             * roll for a temporary sensor blackout (minimap + DANGER
             * readout). Hiding in shadow short-circuits the roll. */
            if (solar_flare_in_shadow == 0
                && player.sensor_static_timer <= 0.0f)
            {
                sensor_static_roll_t -= dt;
                if (sensor_static_roll_t <= 0.0f) {
                    float r = (float)rand() / (float)RAND_MAX;
                    if (r < SENSOR_STATIC_HIT_CHANCE) {
                        player.sensor_static_timer = SENSOR_STATIC_DURATION;
                        spawn_event_float(player.pos.x, player.pos.y - 32.0f,
                                          "SENSOR STATIC",
                                          (SDL_Color){HUD_CINNABAR.r,
                                                      HUD_CINNABAR.g,
                                                      HUD_CINNABAR.b, 255});
                        cugel9_say("SENSORS BLINDED BY STELLAR WIND. NAVIGATE BY INTUITION OR DESPAIR.");
                        audio_play(SFX_EXPLOSION_SM);
                    }
                    /* Re-arm the roll regardless of outcome */
                    {
                        float t = (float)rand() / (float)RAND_MAX;
                        sensor_static_roll_t = SENSOR_STATIC_ROLL_MIN
                            + t * (SENSOR_STATIC_ROLL_MAX - SENSOR_STATIC_ROLL_MIN);
                    }
                }
            } else if (solar_flare_in_shadow) {
                /* While safely shadowed, hold the roll timer at a small
                 * positive value so the very-next-frame post-shadow exit
                 * doesn't instantly fire a scramble. */
                if (sensor_static_roll_t < 1.0f) sensor_static_roll_t = 1.0f;
            }
            /* Reverse Drift (Confusion) roll: companion to Sensor Static.
             * Same shadow-gated cadence, but a lower hit chance and a
             * longer effect window — losing thrust direction is a more
             * disruptive penalty than losing the minimap, so we cap the
             * frequency.  Cugel-9 gets a different quip so the player
             * can audibly tell the two effects apart. */
            if (solar_flare_in_shadow == 0
                && player.reverse_drift_timer <= 0.0f)
            {
                reverse_drift_roll_t -= dt;
                if (reverse_drift_roll_t <= 0.0f) {
                    float r = (float)rand() / (float)RAND_MAX;
                    if (r < REVERSE_DRIFT_HIT_CHANCE) {
                        player.reverse_drift_timer = REVERSE_DRIFT_DURATION;
                        spawn_event_float(player.pos.x, player.pos.y - 32.0f,
                                          "REVERSE DRIFT",
                                          (SDL_Color){HUD_CINNABAR.r,
                                                      HUD_CINNABAR.g,
                                                      HUD_CINNABAR.b, 255});
                        cugel9_say("THRUST POLARITY INVERTED. ENJOY YOUR INVOLUNTARY RETROGRADE.");
                        audio_play(SFX_EXPLOSION_SM);
                    }
                    /* Re-arm the roll regardless of outcome */
                    {
                        float t = (float)rand() / (float)RAND_MAX;
                        reverse_drift_roll_t = REVERSE_DRIFT_ROLL_MIN
                            + t * (REVERSE_DRIFT_ROLL_MAX - REVERSE_DRIFT_ROLL_MIN);
                    }
                }
            } else if (solar_flare_in_shadow) {
                if (reverse_drift_roll_t < 1.0f) reverse_drift_roll_t = 1.0f;
            }
            /* Tox-Gas Hallucinations roll: third member of the malfunction
             * trio.  Same Star-Shadow gating as Sensor Static and Reverse
             * Drift, but a lower hit rate and a longer cadence — the
             * effect is purely visual (asteroid wireframes scramble +
             * ghost-Cacogen overlays) so the pilot can compensate without
             * mechanical penalty.  Tuning intent: the player should hit
             * a hallucination roughly once per flare on average, not
             * compound atop the other two malfunctions. */
            if (solar_flare_in_shadow == 0
                && player.tox_hallucination_timer <= 0.0f)
            {
                tox_hallucination_roll_t -= dt;
                if (tox_hallucination_roll_t <= 0.0f) {
                    float r = (float)rand() / (float)RAND_MAX;
                    if (r < TOX_HALLUCINATION_HIT_CHANCE) {
                        player.tox_hallucination_timer = TOX_HALLUCINATION_DURATION;
                        spawn_event_float(player.pos.x, player.pos.y - 32.0f,
                                          "TOX HALLUCINATION",
                                          (SDL_Color){HUD_CINNABAR.r,
                                                      HUD_CINNABAR.g,
                                                      HUD_CINNABAR.b, 255});
                        cugel9_say("REACTOR FUMES DETECTED. THAT ASTEROID IS PROBABLY NOT A CACOGEN. PROBABLY.");
                        audio_play(SFX_EXPLOSION_SM);
                    }
                    /* Re-arm the roll regardless of outcome */
                    {
                        float t = (float)rand() / (float)RAND_MAX;
                        tox_hallucination_roll_t = TOX_HALLUCINATION_ROLL_MIN
                            + t * (TOX_HALLUCINATION_ROLL_MAX - TOX_HALLUCINATION_ROLL_MIN);
                    }
                }
            } else if (solar_flare_in_shadow) {
                if (tox_hallucination_roll_t < 1.0f) tox_hallucination_roll_t = 1.0f;
            }
            if (solar_flare_active_t <= 0.0f) {
                solar_flare_active_t = 0.0f;
                solar_flare_in_shadow = 0;
                sensor_static_roll_t = 0.0f; /* reset between flare cycles */
                reverse_drift_roll_t = 0.0f; /* reset between flare cycles */
                tox_hallucination_roll_t = 0.0f; /* reset between flare cycles */
                spawn_event_float(player.pos.x, player.pos.y - 32.0f,
                                  "FLARE PASSED",
                                  (SDL_Color){HUD_TEXT_CYAN.r,
                                              HUD_TEXT_CYAN.g,
                                              HUD_TEXT_CYAN.b, 255});
                /* Re-roll next interval */
                {
                    float t = (float)rand() / (float)RAND_MAX;
                    solar_flare_next_iv = SOLAR_FLARE_MIN_INTERVAL
                        + t * (SOLAR_FLARE_MAX_INTERVAL - SOLAR_FLARE_MIN_INTERVAL);
                    solar_flare_timer = solar_flare_next_iv;
                }
            }
        } else if (solar_flare_warn_t > 0.0f) {
            /* WARNING telegraph */
            solar_flare_warn_t -= dt;
            if (solar_flare_warn_t <= 0.0f) {
                solar_flare_warn_t   = 0.0f;
                solar_flare_active_t = SOLAR_FLARE_ACTIVE_DURATION;
                spawn_event_float(player.pos.x, player.pos.y - 32.0f,
                                  "SOLAR FLARE",
                                  (SDL_Color){HUD_CINNABAR.r,
                                              HUD_CINNABAR.g,
                                              HUD_CINNABAR.b, 255});
                screen_shake_timer     = 0.35f;
                screen_shake_intensity = 4.0f;
                audio_play(SFX_EXPLOSION_SM);
            }
        } else {
            /* COUNTUP — ticks down to WARNING */
            solar_flare_timer -= dt;
            if (solar_flare_timer <= 0.0f) {
                solar_flare_timer  = 0.0f;
                solar_flare_warn_t = SOLAR_FLARE_WARN_DURATION;
                cugel9_say("SOLAR FLARE INBOUND. SEEK ASTEROID SHADOW. SURVIVAL OPTIONAL.");
                spawn_event_float(player.pos.x, player.pos.y - 32.0f,
                                  "FLARE INBOUND",
                                  (SDL_Color){HUD_AMBER.r,
                                              HUD_AMBER.g,
                                              HUD_AMBER.b, 255});
            }
        }
    } else {
        /* Player back in Home Space or inactive — pause the cycle but
         * keep partial progress so flares don't reset to full on every
         * dock-and-leave. */
        solar_flare_in_shadow = 0;
    }
}

/* ================================================================== */

/** @brief Updates camera position, screen shake, cursor visibility,
 *         FPS counter, attract-mode idle timer, and audio parameters.
 *
 *  The camera follows the player with a 6 Hz first-order lag filter.
 *  The audio system receives a combat_level (0-1) and spookiness (0-1)
 *  each frame so the adaptive music layers blend in real time.
 */
static void update_camera_and_audio(float dt)
{
    int is_gameplay = (game_state == STATE_PLAYING
                       || game_state == STATE_PAUSED
                       || game_state == STATE_UPGRADE_SELECT
                       || game_state == STATE_SHOP
                       || game_state == STATE_WARP_MENU
                       || game_state == STATE_ATTRACT_GAMEPLAY
                       || ((game_state == STATE_SETTINGS
                            || game_state == STATE_KEYBINDS)
                           && settings_back_state == STATE_PAUSED));
    int is_paused   = (game_state == STATE_PAUSED
                       || game_state == STATE_UPGRADE_SELECT
                       || game_state == STATE_SHOP
                       || game_state == STATE_WARP_MENU
                       || ((game_state == STATE_SETTINGS
                            || game_state == STATE_KEYBINDS)
                           && settings_back_state == STATE_PAUSED));

    /* ── Adaptive music parameters ────────────────────────────────── */
    {
        float combat_level = 0.0f;
        if (ufo.active) {
            combat_level = 1.0f;
        } else {
            int near_count = 0;
            if (player.active) {
                for (int i = 0; i < MAX_ASTEROIDS; i++) {
                    if (!asteroids[i].active) continue;
                    float dx = asteroids[i].pos.x - player.pos.x;
                    float dy = asteroids[i].pos.y - player.pos.y;
                    if (dx*dx + dy*dy < 400.0f * 400.0f)
                        near_count++;
                }
            }
            if      (near_count > 5) combat_level = 1.0f;
            else if (near_count > 0) combat_level = (float)near_count / 5.0f;
        }

        float spookiness = 0.0f;
        if (player.active) {
            float dist = sqrtf(player.pos.x * player.pos.x
                               + player.pos.y * player.pos.y);
            spookiness = dist / 8000.0f;
            if (spookiness > 1.0f) spookiness = 1.0f;
        }

        audio_set_music_params(combat_level, spookiness,
                               is_paused, is_gameplay, dt);
                               
        float intensity = (lives <= 1 && player.active) ? 1.0f : 0.0f;
        audio_set_intensity(intensity);
    }

    /* ── FPS counter (updated every 0.5 s) ───────────────────────── */
    fps_accum += dt;
    fps_frame_count++;
    if (fps_accum >= 0.5f) {
        fps_display_val = (int)(fps_frame_count / fps_accum);
        fps_accum       = 0.0f;
        fps_frame_count = 0;
    }

    /* ── Cursor visibility ────────────────────────────────────────── */
    SDL_ShowCursor((game_state == STATE_PLAYING && settings_mouse_aim)
                   ? SDL_DISABLE : SDL_ENABLE);

    /* ── Camera follow ────────────────────────────────────────────── */
    if (game_state == STATE_PLAYING || game_state == STATE_ATTRACT_GAMEPLAY) {
        if (player.active) {
            float tx = player.pos.x - SCREEN_WIDTH  / 2.0f;
            float ty = player.pos.y - SCREEN_HEIGHT / 2.0f;
            camera_pos.x += (tx - camera_pos.x) * 6.0f * dt;
            camera_pos.y += (ty - camera_pos.y) * 6.0f * dt;
        }
    } else if (!is_paused) {
        camera_pos = (Vec2){0.0f, 0.0f};
    }
}

/* ================================================================== */

/** @brief Main game update — advances the complete simulation by dt seconds.
 *
 *  Accumulates game_time for deterministic oscillator math, handles the
 *  attract-mode idle/AI cycle, then delegates to subsystem helpers in the
 *  order required by their data dependencies.
 */
void game_update(float dt)
{
    /* Accumulate elapsed time for physics oscillators.
     * Using game_time instead of SDL_GetTicks() keeps AI oscillations
     * deterministic and frame-rate independent (no fixed 60-fps assumption). */
    game_time += dt;

    if (game_state == STATE_TITLE) {
        g_boot_timer += dt;
    }
    if (hyperjump_flash_timer > 0.0f) {
        hyperjump_flash_timer -= dt;
    }

    /* ── Attract-mode idle sequencer ─────────────────────────────── */
    {
        int is_gameplay = (game_state == STATE_PLAYING
                           || game_state == STATE_PAUSED
                           || game_state == STATE_UPGRADE_SELECT
                           || game_state == STATE_SHOP
                           || game_state == STATE_WARP_MENU
                           || game_state == STATE_ATTRACT_GAMEPLAY
                           || ((game_state == STATE_SETTINGS
                                || game_state == STATE_KEYBINDS)
                               && settings_back_state == STATE_PAUSED));
        if (!is_gameplay) {
            idle_timer += dt;
            if (idle_timer >= ATTRACT_DELAY) {
                idle_timer = 0.0f;
                if (game_state == STATE_TITLE)
                    game_state = STATE_ATTRACT_INSTRUCTIONS;
                else if (game_state == STATE_ATTRACT_INSTRUCTIONS) {
                    is_attract_ai = 1;
                    start_new_game();
                    game_state = STATE_ATTRACT_GAMEPLAY;
                } else {
                    game_state = STATE_TITLE;
                }
            }
        }
    }

    /* ── Attract-mode AI upgrade picker ──────────────────────────── */
    if (game_state == STATE_UPGRADE_SELECT && is_attract_ai) {
        static float ai_upgrade_timer = 0.0f;
        ai_upgrade_timer += dt;
        if (ai_upgrade_timer >= 1.5f) {
            ai_upgrade_timer = 0.0f;
            selected_option  = rand() % 3;
            apply_upgrade(upgrade_options[selected_option]);
            if (wave_cleared_pending) {
                wave_cleared_pending = 0;
                start_next_level();
            }
            game_state = STATE_ATTRACT_GAMEPLAY;
            if (ufo.active) audio_play(SFX_UFO_LOOP);
        }
        /* Audio and camera still need updating even during UI pause */
        update_camera_and_audio(dt);
        return;
    }

    /* ── Early-out for non-gameplay states ───────────────────────── */
    if (game_state != STATE_PLAYING && game_state != STATE_ATTRACT_GAMEPLAY) {
        /* Keep audio and camera coherent even on menus / paused states */
        update_camera_and_audio(dt);
        return;
    }

    /* ── Void Stone Mechanics ─────────────────────────────────────── */
    if (time_stop_frames > 0) {
        time_stop_frames--;
        update_camera_and_audio(dt);
        return;
    }
    if (player.active) {
        player_pos_history[pos_history_idx] = player.pos;
        pos_history_idx = (pos_history_idx + 1) % POS_HISTORY_LEN;
    }

    /* ── Full simulation tick ─────────────────────────────────────── */
    update_player_physics(dt);
    update_player_bullets(dt);
    update_asteroids(dt);
    update_ufo(dt);
    update_ufo_bullets(dt);
    update_particles_orbs_npcs(dt);
    drone_chatter_update(dt);   /* Item 27: tick fade timers on drone chatter floats */
    rustweaver_update(dt, player.pos.x, player.pos.y);  /* Item 23: rust-weaver AI + spit projectiles */
    ascian_update(dt, player.pos.x, player.pos.y);      /* Item 21: ascian polygon patrol + volleys */
    lictor_update(dt, player.pos.x, player.pos.y);      /* Item 22: lictor pursuit + aimed bolts    */
    emp_sentinel_update(dt, player.pos.x, player.pos.y);/* Item 25: emp sentinel CHARGE/PULSE cycle */
    {
        /* Item 24: scavenger probe SEEK→TRACTOR→ESCAPE→WARP cycle.  The
         * module returns the number of Void Steel units siphoned from
         * the player THIS FRAME — game.c is the source of truth for the
         * resource counter so we apply the deduction here.  spawn a
         * float per siphon tick so the player feels the bleed. */
        int siphoned = scavenger_update(dt, player.pos.x, player.pos.y,
                                        res_void_steel);
        if (siphoned > 0) {
            if (siphoned > res_void_steel) siphoned = res_void_steel;
            res_void_steel -= siphoned;
            audio_play(SFX_EXPLOSION_SM);
            spawn_event_float(player.pos.x, player.pos.y - 28.0f,
                              "-VOID STEEL", HUD_CINNABAR);
        }
    }
    update_collisions(dt);

    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) continue;
        for (int j = i + 1; j < MAX_ASTEROIDS; j++) {
            if (!asteroids[j].active) continue;
            
            float dx = asteroids[j].pos.x - asteroids[i].pos.x;
            float dy = asteroids[j].pos.y - asteroids[i].pos.y;
            float dist_sq = dx * dx + dy * dy;
            float r_sum = asteroids[i].radius + asteroids[j].radius;
            
            if (dist_sq < r_sum * r_sum && dist_sq > 0.0001f) {
                float dist = sqrtf(dist_sq);
                float nx = dx / dist;
                float ny = dy / dist;
                
                float vx = asteroids[j].vel.x - asteroids[i].vel.x;
                float vy = asteroids[j].vel.y - asteroids[i].vel.y;
                float vn = vx * nx + vy * ny;
                
                float m1 = (float)(asteroids[i].size * asteroids[i].size);
                float m2 = (float)(asteroids[j].size * asteroids[j].size);
                if (m1 < 1.0f) m1 = 1.0f;
                if (m2 < 1.0f) m2 = 1.0f;
                
                if (vn < 0.0f) {
                    float j_impulse = -(2.0f * vn) / (1.0f / m1 + 1.0f / m2);
                    asteroids[i].vel.x -= (j_impulse / m1) * nx;
                    asteroids[i].vel.y -= (j_impulse / m1) * ny;
                    asteroids[j].vel.x += (j_impulse / m2) * nx;
                    asteroids[j].vel.y += (j_impulse / m2) * ny;
                }
                
                float overlap = r_sum - dist;
                float inv_m1 = 1.0f / m1;
                float inv_m2 = 1.0f / m2;
                float sum_inv_mass = inv_m1 + inv_m2;
                
                asteroids[i].pos.x -= nx * overlap * (inv_m1 / sum_inv_mass);
                asteroids[i].pos.y -= ny * overlap * (inv_m1 / sum_inv_mass);
                asteroids[j].pos.x += nx * overlap * (inv_m2 / sum_inv_mass);
                asteroids[j].pos.y += ny * overlap * (inv_m2 / sum_inv_mass);
            }
        }
    }

    update_spawning(dt);
    update_progression(dt);
    update_camera_and_audio(dt);
}

/* =========== GAMEPAD DIAGRAM HELPERS =========== */

/* HUD pixel-coordinate constants — used in render_hud() */
#define HUD_SCORE_X          42.0f
#define HUD_SCORE_Y          30.0f
#define HUD_TOPSCORE_Y       30.0f
#define HUD_LEVEL_Y          30.0f
#define HUD_COMBO_Y          68.0f
#define HUD_LIVES_Y         108.0f
#define HUD_LIVES_X_BASE     50.0f
#define HUD_LIVES_STEP       26.0f
#define HUD_UPGRADE_X    (SCREEN_WIDTH - 72.0f)
#define HUD_UPGRADE_Y0      140.0f
#define HUD_UPGRADE_STEP     20.0f

/* Module-scope animation timers — render-only visual state.
 * These accumulate a fixed delta per frame; exact frame-rate dependence
 * is acceptable for purely aesthetic effects (phosphor flicker, shield
 * pulse, flame shimmer).  Declared here rather than as local statics so
 * they are clearly visible as persistent state and can be reset on
 * player-death if desired in future. */
static float g_ghost_t     = 0.0f;  /* death-ghost pulse phase   */
static float g_flame_t     = 0.0f;  /* thruster flame flicker    */
static float g_shield_pulse = 0.0f; /* shield ring rotation      */

/* ────────────────────────────────────────────────────────────────────── */

/**
 * @brief Returns the screen-space position of a gamepad button on the
 *        vector controller diagram centred at (cx, cy).
 */
static Vec2 get_controller_button_pos(SDL_GameControllerButton btn,
                                      float cx, float cy)
{
    float f_cx = cx + 70.0f;
    float f_cy = cy - 20.0f;
    switch (btn) {
        case SDL_CONTROLLER_BUTTON_A:
            return (Vec2){f_cx, f_cy + 25.0f};
        case SDL_CONTROLLER_BUTTON_B:
            return (Vec2){f_cx + 25.0f, f_cy};
        case SDL_CONTROLLER_BUTTON_X:
            return (Vec2){f_cx - 25.0f, f_cy};
        case SDL_CONTROLLER_BUTTON_Y:
            return (Vec2){f_cx, f_cy - 25.0f};
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            return (Vec2){cx - 82.0f, cy - 63.0f};
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return (Vec2){cx + 82.0f, cy - 63.0f};
        case SDL_CONTROLLER_BUTTON_BACK:
            return (Vec2){cx - 20.0f, cy - 20.0f};
        case SDL_CONTROLLER_BUTTON_START:
            return (Vec2){cx + 20.0f, cy - 20.0f};
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:
            return (Vec2){cx - 70.0f, cy - 20.0f};
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
            return (Vec2){cx + 30.0f, cy + 30.0f};
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            return (Vec2){cx - 30.0f, cy + 15.0f};
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            return (Vec2){cx - 30.0f, cy + 45.0f};
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            return (Vec2){cx - 45.0f, cy + 30.0f};
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            return (Vec2){cx - 15.0f, cy + 30.0f};
        case SDL_CONTROLLER_BUTTON_GUIDE:
            return (Vec2){cx, cy - 20.0f};
        default:
            return (Vec2){cx, cy};
    }
}

/**
 * @brief Draws a vector-art gamepad shell centred at (cx, cy).
 *        highlighted_btn is an SDL_GameControllerButton value; the
 *        matching button is drawn in the highlight colour.
 */
static void draw_gamepad(float cx, float cy, int highlighted_btn)
{
    SDL_Color base_color      = (SDL_Color){100, 255, 255, 120};
    SDL_Color highlight_color = (SDL_Color){255, 220,  80, 255};

    /* 1. Controller outer shell */
    Line shell[] = {
        {{cx - 130, cy - 60}, {cx + 130, cy - 60}},
        {{cx - 130, cy - 60}, {cx - 170, cy - 30}},
        {{cx + 130, cy - 60}, {cx + 170, cy - 30}},
        {{cx - 170, cy - 30}, {cx - 145, cy + 95}},
        {{cx + 170, cy - 30}, {cx + 145, cy + 95}},
        {{cx - 145, cy + 95}, {cx -  95, cy + 95}},
        {{cx + 145, cy + 95}, {cx +  95, cy + 95}},
        {{cx -  95, cy + 95}, {cx -  60, cy + 25}},
        {{cx +  95, cy + 95}, {cx +  60, cy + 25}},
        {{cx -  60, cy + 25}, {cx,        cy + 40}},
        {{cx,        cy + 40}, {cx +  60, cy + 25}}
    };
    Shape shell_shape = {shell, sizeof(shell) / sizeof(Line), base_color};
    vg_draw_shape(&shell_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 2. Bumpers */
    SDL_Color lb_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
                         ? highlight_color : base_color;
    Line lb[] = {
        {{cx - 125, cy - 68}, {cx -  40, cy - 64}},
        {{cx -  40, cy - 64}, {cx -  43, cy - 58}},
        {{cx -  43, cy - 58}, {cx - 120, cy - 62}},
        {{cx - 120, cy - 62}, {cx - 125, cy - 68}}
    };
    Shape lb_shape = {lb, sizeof(lb) / sizeof(Line), lb_color};
    vg_draw_shape(&lb_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    SDL_Color rb_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
                         ? highlight_color : base_color;
    Line rb[] = {
        {{cx + 125, cy - 68}, {cx +  40, cy - 64}},
        {{cx +  40, cy - 64}, {cx +  43, cy - 58}},
        {{cx +  43, cy - 58}, {cx + 120, cy - 62}},
        {{cx + 120, cy - 62}, {cx + 125, cy - 68}}
    };
    Shape rb_shape = {rb, sizeof(rb) / sizeof(Line), rb_color};
    vg_draw_shape(&rb_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 3. Triggers (LT / RT) */
    Line lt[] = {
        {{cx - 120, cy - 83}, {cx -  70, cy - 80}},
        {{cx -  70, cy - 80}, {cx -  68, cy - 70}},
        {{cx -  68, cy - 70}, {cx - 115, cy - 73}},
        {{cx - 115, cy - 73}, {cx - 120, cy - 83}}
    };
    Shape lt_shape = {lt, sizeof(lt) / sizeof(Line), base_color};
    vg_draw_shape(&lt_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line rt[] = {
        {{cx + 120, cy - 83}, {cx +  70, cy - 80}},
        {{cx +  70, cy - 80}, {cx +  68, cy - 70}},
        {{cx +  68, cy - 70}, {cx + 115, cy - 73}},
        {{cx + 115, cy - 73}, {cx + 120, cy - 83}}
    };
    Shape rt_shape = {rt, sizeof(rt) / sizeof(Line), base_color};
    vg_draw_shape(&rt_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 4. Sticks (LSTICK & RSTICK) */
    float ls_x = cx - 70.0f, ls_y = cy - 20.0f;
    SDL_Color ls_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_LEFTSTICK)
                         ? highlight_color : base_color;
    Line ls_circle[12];
    for (int i = 0; i < 12; i++) {
        float a1 = i       * 2.0f * (float)M_PI / 12.0f;
        float a2 = (i + 1) * 2.0f * (float)M_PI / 12.0f;
        ls_circle[i].p1 = (Vec2){ls_x + cosf(a1) * 20.0f, ls_y + sinf(a1) * 20.0f};
        ls_circle[i].p2 = (Vec2){ls_x + cosf(a2) * 20.0f, ls_y + sinf(a2) * 20.0f};
    }
    Shape ls_shape = {ls_circle, 12, ls_color};
    vg_draw_shape(&ls_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line ls_cross[] = {
        {{ls_x - 12.0f, ls_y}, {ls_x + 12.0f, ls_y}},
        {{ls_x, ls_y - 12.0f}, {ls_x, ls_y + 12.0f}}
    };
    Shape ls_cross_shape = {ls_cross, 2, ls_color};
    vg_draw_shape(&ls_cross_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    float rs_x = cx + 30.0f, rs_y = cy + 30.0f;
    SDL_Color rs_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_RIGHTSTICK)
                         ? highlight_color : base_color;
    Line rs_circle[12];
    for (int i = 0; i < 12; i++) {
        float a1 = i       * 2.0f * (float)M_PI / 12.0f;
        float a2 = (i + 1) * 2.0f * (float)M_PI / 12.0f;
        rs_circle[i].p1 = (Vec2){rs_x + cosf(a1) * 20.0f, rs_y + sinf(a1) * 20.0f};
        rs_circle[i].p2 = (Vec2){rs_x + cosf(a2) * 20.0f, rs_y + sinf(a2) * 20.0f};
    }
    Shape rs_shape = {rs_circle, 12, rs_color};
    vg_draw_shape(&rs_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line rs_cross[] = {
        {{rs_x - 12.0f, rs_y}, {rs_x + 12.0f, rs_y}},
        {{rs_x, rs_y - 12.0f}, {rs_x, rs_y + 12.0f}}
    };
    Shape rs_cross_shape = {rs_cross, 2, rs_color};
    vg_draw_shape(&rs_cross_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 5. D-pad */
    float dp_x = cx - 30.0f, dp_y = cy + 30.0f;
    SDL_Color dp_u_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_DPAD_UP)
                           ? highlight_color : base_color;
    SDL_Color dp_d_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
                           ? highlight_color : base_color;
    SDL_Color dp_l_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
                           ? highlight_color : base_color;
    SDL_Color dp_r_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
                           ? highlight_color : base_color;

    Line dp_top[] = {
        {{dp_x - 6.0f, dp_y - 20.0f}, {dp_x + 6.0f, dp_y - 20.0f}},
        {{dp_x - 6.0f, dp_y - 20.0f}, {dp_x - 6.0f, dp_y -  6.0f}},
        {{dp_x + 6.0f, dp_y - 20.0f}, {dp_x + 6.0f, dp_y -  6.0f}}
    };
    Shape dp_top_shape = {dp_top, 3, dp_u_color};
    vg_draw_shape(&dp_top_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line dp_bot[] = {
        {{dp_x - 6.0f, dp_y + 20.0f}, {dp_x + 6.0f, dp_y + 20.0f}},
        {{dp_x - 6.0f, dp_y + 20.0f}, {dp_x - 6.0f, dp_y +  6.0f}},
        {{dp_x + 6.0f, dp_y + 20.0f}, {dp_x + 6.0f, dp_y +  6.0f}}
    };
    Shape dp_bot_shape = {dp_bot, 3, dp_d_color};
    vg_draw_shape(&dp_bot_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line dp_left[] = {
        {{dp_x - 20.0f, dp_y - 6.0f}, {dp_x - 20.0f, dp_y + 6.0f}},
        {{dp_x - 20.0f, dp_y - 6.0f}, {dp_x -  6.0f, dp_y - 6.0f}},
        {{dp_x - 20.0f, dp_y + 6.0f}, {dp_x -  6.0f, dp_y + 6.0f}}
    };
    Shape dp_left_shape = {dp_left, 3, dp_l_color};
    vg_draw_shape(&dp_left_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line dp_right[] = {
        {{dp_x + 20.0f, dp_y - 6.0f}, {dp_x + 20.0f, dp_y + 6.0f}},
        {{dp_x + 20.0f, dp_y - 6.0f}, {dp_x +  6.0f, dp_y - 6.0f}},
        {{dp_x + 20.0f, dp_y + 6.0f}, {dp_x +  6.0f, dp_y + 6.0f}}
    };
    Shape dp_right_shape = {dp_right, 3, dp_r_color};
    vg_draw_shape(&dp_right_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line dp_mid[] = {
        {{dp_x - 6.0f, dp_y - 6.0f}, {dp_x + 6.0f, dp_y - 6.0f}},
        {{dp_x + 6.0f, dp_y - 6.0f}, {dp_x + 6.0f, dp_y + 6.0f}},
        {{dp_x + 6.0f, dp_y + 6.0f}, {dp_x - 6.0f, dp_y + 6.0f}},
        {{dp_x - 6.0f, dp_y + 6.0f}, {dp_x - 6.0f, dp_y - 6.0f}}
    };
    Shape dp_mid_shape = {dp_mid, 4, base_color};
    vg_draw_shape(&dp_mid_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 6. Face buttons (A, B, X, Y) */
    float f_cx = cx + 70.0f, f_cy = cy - 20.0f;
    SDL_Color a_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_A) ? highlight_color : base_color;
    SDL_Color b_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_B) ? highlight_color : base_color;
    SDL_Color x_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_X) ? highlight_color : base_color;
    SDL_Color y_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_Y) ? highlight_color : base_color;
    float btn_r = 7.0f;
    float btn_positions[][2] = {
        {f_cx,        f_cy + 25.0f},   /* A */
        {f_cx + 25.0f, f_cy},           /* B */
        {f_cx - 25.0f, f_cy},           /* X */
        {f_cx,        f_cy - 25.0f}    /* Y */
    };
    SDL_Color btn_colors[] = {a_color, b_color, x_color, y_color};
    const char *btn_lbls[] = {"A", "B", "X", "Y"};
    for (int b = 0; b < 4; b++) {
        float bx = btn_positions[b][0];
        float by = btn_positions[b][1];
        Line b_circle[8];
        for (int i = 0; i < 8; i++) {
            float a1 = i       * 2.0f * (float)M_PI / 8.0f;
            float a2 = (i + 1) * 2.0f * (float)M_PI / 8.0f;
            b_circle[i].p1 = (Vec2){bx + cosf(a1) * btn_r, by + sinf(a1) * btn_r};
            b_circle[i].p2 = (Vec2){bx + cosf(a2) * btn_r, by + sinf(a2) * btn_r};
        }
        Shape b_shape = {b_circle, 8, btn_colors[b]};
        vg_draw_shape(&b_shape, (Vec2){0, 0}, 0.0f, 1.0f);
        vf_draw_string(btn_lbls[b], bx - 3.5f, by - 5.0f, 10.0f, btn_colors[b]);
    }

    /* 7. Back & Start buttons */
    SDL_Color back_color  = (highlighted_btn == SDL_CONTROLLER_BUTTON_BACK)
                            ? highlight_color : base_color;
    SDL_Color start_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_START)
                            ? highlight_color : base_color;
    Line back_btn[4] = {
        {{cx - 25.0f, cy - 23.0f}, {cx - 15.0f, cy - 23.0f}},
        {{cx - 15.0f, cy - 23.0f}, {cx - 15.0f, cy - 17.0f}},
        {{cx - 15.0f, cy - 17.0f}, {cx - 25.0f, cy - 17.0f}},
        {{cx - 25.0f, cy - 17.0f}, {cx - 25.0f, cy - 23.0f}}
    };
    Shape back_shape = {back_btn, 4, back_color};
    vg_draw_shape(&back_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line start_btn[4] = {
        {{cx + 15.0f, cy - 23.0f}, {cx + 25.0f, cy - 23.0f}},
        {{cx + 25.0f, cy - 23.0f}, {cx + 25.0f, cy - 17.0f}},
        {{cx + 25.0f, cy - 17.0f}, {cx + 15.0f, cy - 17.0f}},
        {{cx + 15.0f, cy - 17.0f}, {cx + 15.0f, cy - 23.0f}}
    };
    Shape start_shape = {start_btn, 4, start_color};
    vg_draw_shape(&start_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 8. Guide button (centre circle with X mark) */
    SDL_Color guide_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_GUIDE)
                            ? highlight_color : base_color;
    Line guide_circle[10];
    for (int i = 0; i < 10; i++) {
        float a1 = i       * 2.0f * (float)M_PI / 10.0f;
        float a2 = (i + 1) * 2.0f * (float)M_PI / 10.0f;
        guide_circle[i].p1 = (Vec2){cx + cosf(a1) * 11.0f, cy - 20.0f + sinf(a1) * 11.0f};
        guide_circle[i].p2 = (Vec2){cx + cosf(a2) * 11.0f, cy - 20.0f + sinf(a2) * 11.0f};
    }
    Shape guide_shape = {guide_circle, 10, guide_color};
    vg_draw_shape(&guide_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line guide_x[] = {
        {{cx - 4.0f, cy - 24.0f}, {cx + 4.0f, cy - 16.0f}},
        {{cx - 4.0f, cy - 16.0f}, {cx + 4.0f, cy - 24.0f}}
    };
    Shape guide_x_shape = {guide_x, 2, guide_color};
    vg_draw_shape(&guide_x_shape, (Vec2){0, 0}, 0.0f, 1.0f);
}

/* =========== CONTROLLER BINDS PAGE =========== */

/**
 * @brief Renders the controller-keybind page: the gamepad diagram plus
 *        labelled connector lines from each action to its bound button.
 */
static void render_controller_binds_page(float cx, float cy,
                                         int keybind_selection,
                                         SDL_GameControllerButton *ctrl_binds,
                                         const char **ct_action_names)
{
    static const char *btn_names[] = {
        "A", "B", "X", "Y", "BACK", "GUIDE", "START",
        "LSTICK", "RSTICK", "LB", "RB",
        "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT", "MISC"
    };

    vf_draw_string_centered("ENTER TO REBIND   Q/E TO SWITCH",
                            SCREEN_WIDTH / 2.0f, 100, 12.0f,
                            (SDL_Color){100, 100, 120, 255});

    int highlighted_btn = (int)ctrl_binds[keybind_selection];
    draw_gamepad(cx, cy, highlighted_btn);

    for (int i = 0; i < CT_COUNT; i++) {
        int is_selected = (keybind_selection == i);
        SDL_Color color      = is_selected
                               ? (SDL_Color){255, 220,  80, 255}
                               : (SDL_Color){100, 255, 255, 255};
        SDL_Color line_color = is_selected
                               ? (SDL_Color){255, 220,  80, 255}
                               : (SDL_Color){100, 255, 255,  60};

        int bval = (int)ctrl_binds[i];
        const char *bname = (bval >= 0 && bval < 16) ? btn_names[bval] : "UNBOUND";

        char row[64];
        sprintf(row, "%s: %s", ct_action_names[i], bname);

        float ax, ay, ax_mid;
        if (i < 4) {
            /* Left column */
            ax     = 390.0f;
            ay     = 220.0f + i * 110.0f;
            ax_mid = ax + 40.0f;
            vf_draw_string(row, 120.0f, ay - 6.0f, 15.0f, color);
            if (is_selected)
                vf_draw_string(">", 95.0f, ay - 6.0f, 15.0f, color);
        } else {
            /* Right column */
            ax     = 890.0f;
            ay     = 220.0f + (i - 4) * 110.0f;
            ax_mid = ax - 40.0f;
            vf_draw_string(row, 920.0f, ay - 6.0f, 15.0f, color);
            if (is_selected)
                vf_draw_string(">", 895.0f, ay - 6.0f, 15.0f, color);
        }

        if (bval >= 0 && bval < 16) {
            Vec2 bpos = get_controller_button_pos(ctrl_binds[i], cx, cy);

            Line conn[2] = {
                {{ax,     ay}, {ax_mid, ay}},
                {{ax_mid, ay}, {bpos.x, bpos.y}}
            };
            Shape conn_shape = {conn, 2, line_color};
            vg_draw_shape(&conn_shape, (Vec2){0, 0}, 0.0f, 1.0f);

            Line dot_anchor[] = {
                {{ax - 1.0f, ay},       {ax + 1.0f, ay}},
                {{ax,        ay - 1.0f},{ax,         ay + 1.0f}}
            };
            Shape dot_a_shape = {dot_anchor, 2, line_color};
            vg_draw_shape(&dot_a_shape, (Vec2){0, 0}, 0.0f, 1.0f);

            Line dot_btn[] = {
                {{bpos.x - 1.0f, bpos.y},       {bpos.x + 1.0f, bpos.y}},
                {{bpos.x,        bpos.y - 1.0f}, {bpos.x,        bpos.y + 1.0f}}
            };
            Shape dot_b_shape = {dot_btn, 2, line_color};
            vg_draw_shape(&dot_b_shape, (Vec2){0, 0}, 0.0f, 1.0f);
        }
    }
}

/* =========== RENDER HELPERS =========== */

/**
 * @brief Renders all game entities: asteroids, bullets, Autodyne (player),
 *        enemies, particles, orbs, rifts, shockwaves, NPCs, home station.
 *
 * Called with the world camera already set via vg_set_camera(camera_pos).
 */
static void render_entities(void)
{
    SDL_Color main_color = {220, 240, 255, 255}; /* phosphor cool-white */

    /* ── Particles ──────────────────────────────────────────────── */
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life <= 0.0f) continue;
        float fade = particles[i].life / particles[i].max_life;
        SDL_Color col = particles[i].color;
        col.a = (Uint8)(fade * 255);
        Vec2 p1 = {-1.0f, 0.0f};
        Vec2 p2 = { 1.0f, 0.0f};
        Line pl = {p1, p2};
        Shape s = {&pl, 1, col};
        vg_draw_shape(&s, particles[i].pos, particles[i].angle,
                      particles[i].scale * fade);
    }

    /* ── Orbs ───────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_ORBS; i++) {
        if (!orbs[i].active) continue;
        SDL_Color orb_col = {150, 255, 150, 255};
        static Line orb_lines[4];
        static int  orb_init = 0;
        if (!orb_init) {
            orb_init = 1;
            orb_lines[0] = (Line){{-2, -2}, { 2, -2}};
            orb_lines[1] = (Line){{ 2, -2}, { 2,  2}};
            orb_lines[2] = (Line){{ 2,  2}, {-2,  2}};
            orb_lines[3] = (Line){{-2,  2}, {-2, -2}};
        }
        Shape os = {orb_lines, 4, orb_col};
        vg_draw_shape_trail(&os, orbs[i].trail_pos, orbs[i].trail_ang,
                            PHOS_TRAIL_LEN, orbs[i].trail_head,
                            1.0f, 0.3f, 0.6f);
        vg_draw_shape(&os, orbs[i].pos, orbs[i].life * 5.0f, 1.0f);
    }

    /* ── Star-Shadow vector lines (Solar Flare lore tie-in) ─────────
     * During an ACTIVE flare, draw a low-alpha cinnabar tether from any
     * large asteroid that lies between the player and the origin (the
     * dying sun) out to the player.  Brightness scales with how centred
     * the player sits inside the shadow corridor — gives the mechanic a
     * legible visual.  Drawn *before* asteroids so the lines sit behind
     * the asteroid silhouette.                                        */
    if (solar_flare_active_t > 0.0f && player.active) {
        float px = player.pos.x, py = player.pos.y;
        float pd = sqrtf(px*px + py*py);
        if (pd > 1.0f) {
            float ux = -px / pd, uy = -py / pd;
            for (int i = 0; i < MAX_ASTEROIDS; i++) {
                if (!asteroids[i].active) continue;
                if (asteroids[i].size < 2) continue;
                float ax = asteroids[i].pos.x, ay = asteroids[i].pos.y;
                float ad = sqrtf(ax*ax + ay*ay);
                if (ad >= pd) continue;
                float dx = ax - px, dy = ay - py;
                float t  = dx*ux + dy*uy;
                if (t < 0.0f || t > pd) continue;
                float perp_x = dx - t*ux;
                float perp_y = dy - t*uy;
                float perp = sqrtf(perp_x*perp_x + perp_y*perp_y);
                float half_w = SOLAR_FLARE_SHADOW_HALF_W
                             + asteroids[i].radius * 0.6f;
                if (perp > half_w) continue;
                /* Alpha = 60..200 scaled by how centred the player is.
                 * Pulse at 4 Hz so the line shimmers during the flare. */
                float centre_factor = 1.0f - (perp / half_w);
                if (centre_factor < 0.0f) centre_factor = 0.0f;
                float pulse = 0.65f + 0.35f
                            * sinf(game_time * 8.0f);
                Uint8 a = (Uint8)(60.0f + 140.0f * centre_factor * pulse);
                SDL_Color shadow_col = (SDL_Color){
                    HUD_CINNABAR.r, HUD_CINNABAR.g, HUD_CINNABAR.b, a
                };
                /* Line from the player-facing edge of the asteroid out
                 * to the player.  Drawn in world space using a length-1
                 * Line passed through vg_draw_shape with the segment
                 * vector baked into the endpoints.                    */
                {
                    /* Endpoint A: asteroid edge nearest the player.   */
                    float seg_dx = px - ax, seg_dy = py - ay;
                    float seg_len = sqrtf(seg_dx*seg_dx + seg_dy*seg_dy);
                    if (seg_len < 0.001f) continue;
                    float nx = seg_dx / seg_len, ny = seg_dy / seg_len;
                    Vec2 p1 = { asteroids[i].radius * nx,
                                asteroids[i].radius * ny };
                    Vec2 p2 = { seg_dx, seg_dy };
                    Line sl  = {p1, p2};
                    Shape ss = {&sl, 1, shadow_col};
                    vg_draw_shape(&ss, asteroids[i].pos, 0.0f, 1.0f);
                }
            }
        }
    }

    /* ── Asteroids ──────────────────────────────────────────────── */
    /* Tox-Gas Hallucination strength once per frame, used to gate the
     * per-asteroid jitter+overlay block below.  Range: 0.0 (no effect) up
     * to ~1.0 just after onset, fading linearly with the timer. */
    float halluc_t  = player.tox_hallucination_timer;
    float halluc_f  = (halluc_t > 0.0f) ? (halluc_t / TOX_HALLUCINATION_DURATION) : 0.0f;
    if (halluc_f > 1.0f) halluc_f = 1.0f;
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) continue;
        Shape s;
        s.lines      = asteroids[i].lines;
        s.line_count = asteroids[i].line_count;
        s.color      = main_color;
        if (asteroids[i].has_shield)
            s.color = (SDL_Color){100, 255, 255, 255};
        vg_draw_shape_trail(&s, asteroids[i].trail_pos, asteroids[i].trail_ang,
                            PHOS_TRAIL_LEN, asteroids[i].trail_head,
                            1.0f, 0.5f, 0.8f);

        /* Tox-Gas Hallucinations: jitter draw transform when active.  Each
         * asteroid gets a unique phase via its index so the field shimmers
         * rather than rocking in unison.  Position jitter is small (a few
         * world units) so collision behaviour stays predictable while the
         * silhouette visibly drifts.  Angle wobble is the most legible
         * cue.  Scale breathes ±6 % to suggest "is that getting bigger?"
         * second-guessing.  All scaled by halluc_f so the effect fades
         * with the timer. */
        Vec2  draw_pos   = asteroids[i].pos;
        float draw_angle = asteroids[i].angle;
        float draw_scale = 1.0f;
        if (halluc_f > 0.0f) {
            float phase    = game_time * 5.3f + (float)i * 0.91f;
            float jitter_x = sinf(phase)            * 4.5f * halluc_f;
            float jitter_y = cosf(phase * 1.21f)   * 4.5f * halluc_f;
            float wobble   = sinf(phase * 0.77f)   * 0.22f * halluc_f;
            float breathe  = sinf(phase * 1.93f)   * 0.06f * halluc_f;
            draw_pos.x   += jitter_x;
            draw_pos.y   += jitter_y;
            draw_angle   += wobble;
            draw_scale   += breathe;
        }
        vg_draw_shape(&s, draw_pos, draw_angle, draw_scale);

        /* Tox-Gas Hallucinations: ghost-Cacogen overlay on a deterministic
         * ~1/3 of asteroids so the player misidentifies otherwise-harmless
         * rocks as elite UFOs.  Selection uses `i % 3 == 0` so the SAME
         * asteroids "transform" across consecutive frames (the
         * hallucination is consistent, not strobing), but the OVERLAY
         * itself shimmers in alpha.  The shape is a low-alpha cyan
         * triangular silhouette with a dorsal dome arc — a stylised
         * Cacogen tell.  Drawn at ~85 % asteroid radius so it sits inside
         * the wireframe and looks like the rock is hosting a UFO. */
        if (halluc_f > 0.0f && (i % 3) == 0) {
            float r       = asteroids[i].radius * 0.85f;
            float phase   = game_time * 6.7f + (float)i * 1.37f;
            float pulse   = 0.55f + 0.45f * (0.5f + 0.5f * sinf(phase));
            Uint8 ga      = (Uint8)(120.0f * halluc_f * pulse);
            SDL_Color ghost_col = (SDL_Color){
                (Uint8)(HUD_TEXT_CYAN.r * 0.85f),
                (Uint8)(HUD_TEXT_CYAN.g * 0.95f),
                (Uint8)(HUD_TEXT_CYAN.b * 1.0f),
                ga
            };
            /* Triangle wedge + crossbar = compact UFO silhouette */
            Vec2 g_tip   = { 0.0f,           -r * 0.85f };
            Vec2 g_left  = { -r * 0.95f,      r * 0.35f };
            Vec2 g_right = {  r * 0.95f,      r * 0.35f };
            Vec2 g_bar_l = { -r * 0.55f,      r * 0.10f };
            Vec2 g_bar_r = {  r * 0.55f,      r * 0.10f };
            Line g_lines[4] = {
                { g_tip,   g_left  },
                { g_tip,   g_right },
                { g_left,  g_right },
                { g_bar_l, g_bar_r }
            };
            Shape g_shape = { g_lines, 4, ghost_col };
            vg_draw_shape(&g_shape, draw_pos, draw_angle, 1.0f);
        }

        /* Vector Stress-Cracking: draw glowing fracture lines on damaged
         * metal/crystal asteroids, radiating from the interior outward. */
        if (asteroids[i].hit_count > 0 && asteroids[i].material > 0) {
            SDL_Color crack_col = (asteroids[i].material == 2)
                ? (SDL_Color){60, 220, 180, 160}   /* crystal: teal-ice  */
                : (SDL_Color){220, 160, 40, 160};  /* metal:   amber     */
            float r  = asteroids[i].radius * 0.55f;
            float ag = asteroids[i].angle;
            /* Four crack segments at roughly 90° intervals, slightly skewed */
            static const float offsets[4] = { 0.4f, 1.97f, 3.54f, 5.11f };
            for (int ci = 0; ci < 4; ci++) {
                float ca   = ag + offsets[ci];
                float inner = r * 0.18f;
                float outer = r * 0.58f + r * 0.22f * (float)(ci % 2);
                Vec2 p1 = { cosf(ca) * inner, sinf(ca) * inner };
                Vec2 p2 = { cosf(ca) * outer, sinf(ca) * outer };
                Line cl  = {p1, p2};
                Shape cs = {&cl, 1, crack_col};
                vg_draw_shape(&cs, asteroids[i].pos, 0.0f, 1.0f);
            }
        }
    }

    /* ── Player bullets ─────────────────────────────────────────── */
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].active) continue;
        float vx  = bullets[i].vel.x;
        float vy  = bullets[i].vel.y;
        float len = sqrtf(vx * vx + vy * vy);
        Vec2  p1  = {0, 0};
        Vec2  p2  = {(vx / len) * 4.0f, (vy / len) * 4.0f};
        Line  l   = {p1, p2};
        Shape s   = {&l, 1, bullets[i].color};
        vg_draw_shape_trail(&s, bullets[i].trail_pos, bullets[i].trail_ang,
                            PHOS_TRAIL_LEN, bullets[i].trail_head,
                            1.0f, 0.7f, 0.9f);
        vg_draw_shape(&s, bullets[i].pos, 0.0f, 1.0f);
    }

    /* ── Enemy bullets ──────────────────────────────────────────── */
    for (int i = 0; i < MAX_UFO_BULLETS; i++) {
        if (!ufo_bullets[i].active) continue;
        float vx  = ufo_bullets[i].vel.x;
        float vy  = ufo_bullets[i].vel.y;
        float len = sqrtf(vx * vx + vy * vy);
        Vec2  p1  = {0, 0};
        Vec2  p2  = {(vx / len) * 4.0f, (vy / len) * 4.0f};
        Line  l   = {p1, p2};
        Shape s   = {&l, 1, ufo_bullets[i].color};
        vg_draw_shape_trail(&s, ufo_bullets[i].trail_pos, ufo_bullets[i].trail_ang,
                            PHOS_TRAIL_LEN, ufo_bullets[i].trail_head,
                            1.0f, 0.7f, 0.9f);
        vg_draw_shape(&s, ufo_bullets[i].pos, 0.0f, 1.0f);
    }

    /* ── Enemy (UFO) ────────────────────────────────────────────── */
    if (ufo.active) {
        Shape s;
        s.lines      = ufo.lines;
        s.line_count = ufo.line_count;
        s.color      = (SDL_Color){255, 180, 180, 255};
        /*
         * Scale enemies to be roughly player-ship-sized (~16-22 px at 1.0).
         * Kamikaze (size 3) and Eldritch Tendril (6) / Daemon Sigil (7)
         * rotate to face their movement direction.
         */
        float scale, draw_angle = 0.0f;
        switch (ufo.size) {
            case 1:  scale = 0.7f; break;                          /* Small saucer  ~22 px */
            case 2:  scale = 1.2f; break;                          /* Large saucer  ~38 px */
            case 3:  scale = 1.0f; draw_angle = ufo.angle; break;  /* Kamikaze, rotates    */
            case 4:  scale = 0.9f; break;                          /* Bomber        ~36 px */
            case 5:  scale = 1.5f; break;                          /* Eye of Void   ~48 px */
            case 6:  scale = 1.1f; draw_angle = ufo.angle; break;  /* Eldritch Tendril     */
            case 7:  scale = 1.2f; draw_angle = ufo.angle; break;  /* Daemon Sigil         */
            default: scale = 1.0f; break;
        }
        vg_draw_shape_trail(&s, ufo.trail_pos, ufo.trail_ang,
                            PHOS_TRAIL_LEN, ufo.trail_head,
                            scale, 0.4f, 0.75f);
        vg_draw_shape(&s, ufo.pos, draw_angle, scale);
    }

    /* ── Rifts ──────────────────────────────────────────────────── */
    if (player_rift.active) {
        Shape rs;
        rs.lines      = ufo_lines;
        rs.line_count = 10;
        rs.color      = (SDL_Color){255, 50, 255, 255};
        vg_draw_shape(&rs, player_rift.pos, player_rift.angle1,
                      player_rift.radius / 16.0f);
        vg_draw_shape(&rs, player_rift.pos, player_rift.angle2,
                      (player_rift.radius / 16.0f) * 0.8f);
    }
    if (rift.active) {
        Shape rs;
        rs.lines      = ufo_lines;
        rs.line_count = 10;
        rs.color      = (SDL_Color){180, 50, 255, 180};
        vg_draw_shape(&rs, rift.pos, rift.angle1, rift.radius / 16.0f);
        vg_draw_shape(&rs, rift.pos, rift.angle2,
                      (rift.radius / 16.0f) * 0.8f);
    }

    /*  Gravity Wells  */
    for (int i = 0; i < MAX_GRAVITY_WELLS; i++) {
        if (gravity_wells[i].active) {
            Shape bh;
            bh.lines      = ufo_lines;
            bh.line_count = 10;
            bh.color      = (SDL_Color){30, 0, 80, 255};
            vg_draw_shape(&bh, gravity_wells[i].pos, game_time * 2.0f, gravity_wells[i].radius / 16.0f);
            vg_draw_shape(&bh, gravity_wells[i].pos, -game_time * 1.5f, (gravity_wells[i].radius / 16.0f) * 0.6f);
        }
    }

    /* ── Shockwaves ─────────────────────────────────────────────── */
    {
        static Line slines[16];
        static int  s_init = 0;
        if (!s_init) {
            s_init = 1;
            for (int j = 0; j < 16; j++) {
                float a1 = j       * (float)M_PI * 2.0f / 16.0f;
                float a2 = (j + 1) * (float)M_PI * 2.0f / 16.0f;
                slines[j].p1 = (Vec2){cosf(a1), sinf(a1)};
                slines[j].p2 = (Vec2){cosf(a2), sinf(a2)};
            }
        }
        for (int i = 0; i < 4; i++) {
            if (!shockwaves[i].active) continue;
            Shape ss = {slines, 16, (SDL_Color){100, 255, 255, 150}};
            vg_draw_shape(&ss, shockwaves[i].pos, 0.0f, shockwaves[i].radius);
        }
    }

    /* ── Death ghost (Autodyne outline pulses at death position) ── */
    if (!player.active && lives > 0) {
        /* g_ghost_t incremented in game_render() before this call */
        if (respawn_phase == 1) respawn_blink += 0.016f;
        float ga = 0.5f + 0.5f * sinf(g_ghost_t * 6.0f);
        SDL_Color gc = {255, 80, 80, (Uint8)(ga * 120)};
        Shape gs = {ship_lines, sizeof(ship_lines) / sizeof(Line), gc};
        vg_draw_shape(&gs, player_death_pos, player_death_angle, 1.0f);
    }

    /* ── Autodyne (player ship) ─────────────────────────────────── */
    if (player.active) {
        int visible = 1;
        if (player.invuln_timer > 0.0f)
            visible = ((int)(player.invuln_timer * 10) % 2 == 0);

        if (visible) {
            Shape s = {player.lines, player.line_count, main_color};
            vg_draw_shape_trail(&s, player.trail_pos, player.trail_ang,
                                PHOS_TRAIL_LEN, player.trail_head,
                                1.0f, 0.4f, 0.7f);
            vg_draw_shape(&s, player.pos, player.angle, 1.0f);

            /* Thruster flame — animated multi-segment flicker */
            if (is_thrusting) {
                /* g_flame_t incremented in game_render() */
                float fl = 10.0f + 6.0f * sinf(g_flame_t * 3.7f)
                                 + 3.0f * sinf(g_flame_t * 7.3f);
                float fw = 2.5f + 1.5f * sinf(g_flame_t * 5.1f);
                Line flines[3] = {
                    {{-fw,        6.0f}, {0.0f, 6.0f + fl}},
                    {{ 0.0f, 6.0f + fl}, { fw,  6.0f}},
                    {{-fw * 0.5f, 6.0f}, { fw * 0.5f, 6.0f}}
                };
                float fade = 0.6f + 0.4f * sinf(g_flame_t * 11.0f);
                SDL_Color fc = {
                    255,
                    (Uint8)(100 + 100 * fade),
                    (Uint8)(20  * (1.0f - fade)),
                    255
                };
                Shape fs = {flines, 3, fc};
                vg_draw_shape(&fs, player.pos, player.angle, 1.0f);
            }

            /* Ether shroud shield ring */
            if (player_upgrades.shield_active) {
                /* g_shield_pulse incremented in game_render() */
                static Line shield_lines[8];
                static int  shield_init = 0;
                if (!shield_init) {
                    shield_init = 1;
                    for (int i = 0; i < 8; i++) {
                        float a1 = i       * (float)M_PI / 4.0f;
                        float a2 = (i + 1) * (float)M_PI / 4.0f;
                        shield_lines[i].p1 = (Vec2){cosf(a1) * 16.0f,
                                                     sinf(a1) * 16.0f};
                        shield_lines[i].p2 = (Vec2){cosf(a2) * 16.0f,
                                                     sinf(a2) * 16.0f};
                    }
                }
                Shape ss = {shield_lines, 8, (SDL_Color){100, 180, 255, 180}};
                vg_draw_shape(&ss, player.pos, g_shield_pulse,
                              1.3f + 0.1f * sinf(g_shield_pulse * 2.0f));
            }
        }
    }

    /* ── Wraith twin (mirror image) ─────────────────────────────── */
    if (mirror_active_flag && player.active) {
        SDL_Color mc = {100, 180, 255, 120};
        Shape ms = {ship_lines, 5, mc};
        vg_draw_shape(&ms, mirror_pos, mirror_angle,
                      player.radius * player_upgrades.size_mult * 0.9f);
    }

    /* ── Home Station ───────────────────────────────────────────── */
    {
        Shape hs = {home_station_lines,
                    sizeof(home_station_lines) / sizeof(Line),
                    (SDL_Color){80, 220, 255, 220}};
        vg_draw_shape(&hs, (Vec2){0.0f, 0.0f}, home_station_angle, 1.0f);
    }

    /* ── NPCs ───────────────────────────────────────────────────── */
    for (int i = 0; i < MAX_NPC; i++) {
        if (!npcs[i].active) continue;
        SDL_Color nc = npcs[i].following
                       ? (SDL_Color){ 80, 255, 120, 255}
                       : (SDL_Color){ 60, 200,  80, 200};
        Shape ns = {npc_drone_lines,
                    sizeof(npc_drone_lines) / sizeof(Line), nc};
        vg_draw_shape(&ns, npcs[i].pos, npcs[i].angle, 0.9f);
    }

    /* ── Item 27: pet shield-drone chatter floats ─────────────────── */
    /* vf_draw_string uses raw screen coords; drone_chatter does its own
     * world→screen subtraction using camera_pos. Called after NPC shapes
     * so floats render on top of the drone diamond glyphs.              */
    drone_chatter_render(g_renderer, camera_pos.x, camera_pos.y);

    /* ── Item 23: rust-weaver drones + corrosive spit ─────────────── */
    /* Render after NPCs so hostile drones don't get hidden behind the
     * friendly drones' diamond glyphs.  Module uses raw SDL_RenderDrawLineF
     * (bypasses vg_set_camera) so it needs the world→screen offset directly. */
    rustweaver_render(g_renderer, camera_pos.x, camera_pos.y);

    /* ── Item 21: Ascian polygon-patrol interceptors + volley bolts ─ */
    /* Same world→screen offset convention as rustweaver_render: this
     * module also uses raw SDL_RenderDrawLineF calls.  Magenta triangles
     * + magenta bolt streaks read as a clean geometric contrast to the
     * rust-weaver's organic green stars. */
    ascian_render(g_renderer, camera_pos.x, camera_pos.y);

    /* ── Item 22: Lictor elite pursuit interceptors + aimed bolts ─── */
    /* Cinnabar narrow delta silhouette + amber bolt streaks — visually
     * distinct from Ascian magenta and Rust-Weaver acid-green so the
     * player can tell elite enemy types apart at a glance.  Same
     * world→screen offset convention as the other enemy modules. */
    lictor_render(g_renderer, camera_pos.x, camera_pos.y);

    /* ── Item 25: EMP Sentinels — purple/cyan status-disruption units ─ */
    /* Hex purple body with bright cyan pupil, plus an expanding pulse
     * ring during the PULSE phase.  Drawn last so the ring overlays
     * other enemies — it's the gameplay-critical telegraph. */
    emp_sentinel_render(g_renderer, camera_pos.x, camera_pos.y);

    /* ── Item 24: Scavenger Probes — amber bow-tie + dashed cyan beam ─ */
    /* Probes draw bow-tie silhouettes, dashed cyan tractor beams while
     * siphoning, and a shrinking amber ring during the WARP-out phase.
     * Drawn after the EMP ring so the beam isn't visually swallowed by
     * a coincident pulse — the scavenger beam IS the gameplay tell.   */
    scavenger_render(g_renderer, camera_pos.x, camera_pos.y,
                     player.pos.x, player.pos.y);
}

/* ────────────────────────────────────────────────────────────────────── */

/**
 * @brief Renders the HUD: score, lives, fuel bar, combo indicator,
 *        XP bar, resource readout, and active-upgrade icon strip.
 *
 * Camera must be reset to (0,0) before this call.
 */
static void render_hud(void)
{
    char hud_text[64];

    /* Resolve zone color for border accents */
    SDL_Color zone_color = ui_zone_color(player_zone);

    /* ── Atmosphere passes — drawn before all panels ── */
    if (g_settings.graphics.vignette)
        ui_vignette(g_renderer);
    if (g_settings.graphics.particle_count > 0)
        ui_particle_drift(g_renderer, game_time, g_settings.graphics.particle_count);

    /* PLACEHOLDER: old panel rendering follows — will be replaced by angled panels */
    ui_particle_drift(g_renderer, game_time, 0); /* no-op, kept for branch parity */

    /* ================================================================
     * TOP-LEFT PANEL — [SCORE] / ZONE / COMBO  (terminal style)
     * ================================================================ */
    {
        float px = HUD_TL_X, py = HUD_TL_Y, pw = HUD_TL_W, ph = HUD_TL_H;
        float pad = HUD_PAD_INNER;

        ui_panel_terminal(g_renderer, px, py, pw, ph, zone_color);

        /* Row 1: [SCORE] label + score value */
        float row1_y = py + pad;
        vf_draw_string("[SCORE]", px + pad, row1_y, 9, HUD_TEXT_DIM);
        sprintf(hud_text, "%08d", score);
        {
            float tw = (float)strlen(hud_text) * 9.0f * 1.1f;
            vf_draw_string(hud_text, px + pw - pad - tw, row1_y, 16, HUD_TEXT_GOLD);
        }

        /* Separator */
        float sep_y = row1_y + HUD_ROW_H + 4.0f;
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer,
            HUD_BORDER_MID.r, HUD_BORDER_MID.g, HUD_BORDER_MID.b, 80);
        SDL_RenderDrawLineF(g_renderer, px + pad, sep_y, px + pw - pad, sep_y);

        /* Row 2: ZONE: name | LVL: N */
        static const char *tl_zone_names[] = {
            "HOME SPACE", "INNER BELT", "DEEP VOID", "THE ABYSS", "DEEP DRIFT"
        };
        float row2_y = sep_y + 5.0f;
        vf_draw_string("ZONE:", px + pad, row2_y, 9, HUD_TEXT_DIM);
        vf_draw_string(tl_zone_names[player_zone],
                       px + pad + 42.0f, row2_y, 9, zone_color);
        sprintf(hud_text, "LVL:%d", player_level);
        vf_draw_string(hud_text, px + pw - pad - 42.0f, row2_y, 9, HUD_TEXT_DIM);

        /* Row 3: COMBO */
        float row3_y = row2_y + HUD_ROW_H + 2.0f;
        ui_combo_multiplier(g_renderer, px + pad, row3_y, combo_count, combo_timer);
    }

    /* ================================================================
     * BOTTOM-LEFT PANEL — [HULL] / [CHRON] / [LIVES]  (terminal style)
     * ================================================================ */
    {
        float px = HUD_BL_X, py = HUD_BL_Y, pw = HUD_BL_W, ph = HUD_BL_H;
        float pad = HUD_PAD_INNER;

        ui_panel_terminal(g_renderer, px, py, pw, ph, zone_color);

        float bar_x = px + pad + 48.0f;
        float bar_w = pw - (bar_x - px) - pad;

        /* Row 1: [HULL] — placeholder solid bar (no HP struct yet) */
        float row1_y = py + pad;
        vf_draw_string("[HULL]", px + pad, row1_y, 9, HUD_TEXT_DIM);
        ui_bar(g_renderer, bar_x, row1_y, bar_w, 8.0f, 1.0f, 1.0f, HUD_TEAL);

        /* Row 2: [CHRON] segmented XP bar */
        float row2_y = row1_y + HUD_ROW_H + 4.0f;
        vf_draw_string("[CHRON]", px + pad, row2_y, 9, HUD_TEXT_DIM);
        SDL_Color xp_fill = HUD_GOLD_BAR;
        if (xp_flash_timer > 0.0f && ((int)(xp_flash_timer * 20) % 2 == 0))
            xp_fill = HUD_TEXT_PRIMARY;
        ui_bar_segmented(g_renderer, bar_x, row2_y, bar_w, 8.0f,
                         (float)player_xp, (float)xp_threshold, 2, xp_fill);
        sprintf(hud_text, "%dXP", player_xp);
        {
            float tw = (float)strlen(hud_text) * 7.0f * 1.1f;
            vf_draw_string(hud_text, px + pw - pad - tw, row2_y, 7, HUD_TEXT_DIM);
        }

        /* Row 3: [LIVES] — ship glyphs */
        float row3_y = row2_y + HUD_ROW_H + 4.0f;
        vf_draw_string("[LIVES]", px + pad, row3_y, 9, HUD_TEXT_DIM);
        {
            SDL_Color ship_col = HUD_TEXT_CYAN;
            Shape s = {ship_lines, sizeof(ship_lines) / sizeof(Line), ship_col};
            int icon_count = lives - 1;
            if (icon_count < 0) icon_count = 0;
            if (icon_count > 6) icon_count = 6;
            for (int i = 0; i < icon_count; i++) {
                Vec2 pos = {bar_x + i * 14.0f, row3_y + 4.0f};
                vg_draw_shape(&s, pos, 0.0f, 0.45f);
            }
        }
    }

    /* ================================================================
     * BOTTOM-RIGHT PANEL — [FUEL] / [ZONE] / [AMMO]  (terminal style — Qud/Noctis)
     * ================================================================ */
    {
        float px = HUD_BR_X, py = HUD_BR_Y, pw = HUD_BR_W, ph = HUD_BR_H;
        float pad = HUD_PAD_INNER;

        ui_panel_terminal(g_renderer, px, py, pw, ph, zone_color);

        float bar_x = px + pad + 48.0f;
        float bar_w = pw - (bar_x - px) - pad;

        /* Row 1: [FUEL] bar + percent */
        float row1_y = py + pad;
        float fpct = (fuel_max > 0.0f) ? (fuel_current / fuel_max) : 0.0f;
        SDL_Color fc = ui_fuel_color(fpct);
        vf_draw_string("[FUEL]", px + pad, row1_y, 9, HUD_TEXT_DIM);
        ui_bar(g_renderer, bar_x, row1_y, bar_w, 8.0f, fuel_current, fuel_max, fc);
        sprintf(hud_text, "%d%%", (int)(fpct * 100.0f));
        {
            float tw = (float)strlen(hud_text) * 7.0f * 1.1f;
            vf_draw_string(hud_text, px + pw - pad - tw, row1_y, 7, fc);
        }

        /* Row 2: [ZONE] — zone name in zone accent color */
        static const char *br_zone_names[] = {
            "HOME SPACE", "INNER BELT", "DEEP VOID", "THE ABYSS", "DEEP DRIFT"
        };
        float row2_y = row1_y + HUD_ROW_H + 4.0f;
        vf_draw_string("[ZONE]", px + pad, row2_y, 9, HUD_TEXT_DIM);
        vf_draw_string(br_zone_names[player_zone], bar_x, row2_y, 9, zone_color);

        /* Row 3: [AMMO] block bar */
        float row3_y = row2_y + HUD_ROW_H + 4.0f;
        vf_draw_string("[AMMO]", px + pad, row3_y, 9, HUD_TEXT_DIM);
        ui_bar_block(g_renderer, bar_x, row3_y,
                     (float)res_ammo, 12.0f, 12, HUD_AMBER);

        /* Contraband warning */
        if (res_contraband > 0) {
            SDL_Color cb_warn = ui_pulse(HUD_CINNABAR, game_time, 2.0f, 0.4f);
            vf_draw_string("CONTRABAND", px + pad, row3_y, 7, cb_warn);
        }
    }

    /* ================================================================
     * RELIC STRIP — upgrade badges along right edge (preserved)
     * ================================================================ */
    if (game_state == STATE_PLAYING || game_state == STATE_ATTRACT_GAMEPLAY) {
        float ix = (float)(SCREEN_WIDTH - 45);
        float iy = HUD_UPGRADE_Y0;
        SDL_Color ic = HUD_TEXT_CYAN;

        if (player_upgrades.triple_shot)
            { vf_draw_string("3X",  ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.max_bounces > 0)
            { vf_draw_string("BNC", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.shield_active)
            { vf_draw_string("SHD", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.piercing)
            { vf_draw_string("PRC", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.homing)
            { vf_draw_string("HOM", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.rear_gun)
            { vf_draw_string("RRG", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.thermal_hull)
            { vf_draw_string("RAM", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.singularity_displacer)
            { vf_draw_string("WRP", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.split_shot)
            { vf_draw_string("SPL", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.resonance_cascade)
            { vf_draw_string("RES", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.mirror_image)
            { vf_draw_string("TWN", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.phase_shift)
            { vf_draw_string("PHS", ix, iy, 9, HUD_AMBER); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.singularity_whip)
            { vf_draw_string("BWP", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.nova_explosion)
            { vf_draw_string("NOV", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        (void)iy;
    }

    /* ── Cugel-9 board computer log panel ─────────────────────────────
     * Shows the most recently posted message in a terminal panel strip
     * positioned just above the bottom HUD panels.
     * Fade-in over CUGEL9_FADE_DUR, stay for CUGEL9_SHOW_DUR, fade out. */
    if (game_state == STATE_PLAYING || game_state == STATE_ATTRACT_GAMEPLAY) {
        /* Find the message with the most remaining timer (= most recent) */
        int   best   = -1;
        float best_t = 0.0f;
        for (int ci = 0; ci < CUGEL9_MAX_MSGS; ci++) {
            if (cugel9_buf[ci].timer > best_t) {
                best_t = cugel9_buf[ci].timer;
                best   = ci;
            }
        }
        if (best >= 0) {
            Cugel9Msg *cm   = &cugel9_buf[best];
            float      total = CUGEL9_SHOW_DUR + 2.0f * CUGEL9_FADE_DUR;
            float      age   = total - cm->timer;  /* 0 at post → grows */
            float      alpha;
            if      (age    < CUGEL9_FADE_DUR) alpha = age / CUGEL9_FADE_DUR;
            else if (cm->timer < CUGEL9_FADE_DUR) alpha = cm->timer / CUGEL9_FADE_DUR;
            else                                   alpha = 1.0f;
            if (alpha < 0.0f) alpha = 0.0f;
            if (alpha > 1.0f) alpha = 1.0f;

            /* Panel geometry — centered strip just above bottom panels */
            float cpw = 580.0f;
            float cpx = (SCREEN_WIDTH  - cpw) * 0.5f;
            float cph = 32.0f;
            float cpy = (float)(SCREEN_HEIGHT - 90 - HUD_MARGIN_Y) - cph - 10.0f;

            SDL_Color cac = HUD_TEXT_CYAN;
            cac.a = (Uint8)(180.0f * alpha);
            ui_panel_terminal(g_renderer, cpx, cpy, cpw, cph, cac);

            /* Row: "[CUGEL-9]:" dim label + message body in primary white */
            float rly = cpy + (cph - 10.0f) * 0.5f;
            SDL_Color clc = HUD_TEXT_DIM;     clc.a = (Uint8)(200.0f * alpha);
            SDL_Color cbc = HUD_TEXT_PRIMARY; cbc.a = (Uint8)(230.0f * alpha);
            SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
            vf_draw_string("[CUGEL-9]:", cpx + 8.0f,   rly, 8, clc);
            vf_draw_string(cm->text,     cpx + 104.0f,  rly, 8, cbc);
        }
    }

    /* FPS counter */
    if (settings_show_fps) {
        sprintf(hud_text, "%d FPS", fps_display_val);
        vf_draw_string(hud_text, 8, 8, 12, HUD_GREEN);
    }
}

/* ────────────────────────────────────────────────────────────────────── */

/**
 * @brief Renders the minimap in the upper-right corner showing relative
 *        entity positions: player (white), asteroids (grey), enemies
 *        (red), NPCs (green), home station (cyan), enemy bullets (red).
 *        Also draws the off-screen home arrow when the station is not
 *        visible in the viewport.
 */
static void render_minimap(void)
{
    if (!minimap_visible) return;

    /* Panel frame — rectangular (minimap uses CoQ rectangle style) */
    float panel_x = (float)HUD_TR_X;
    float panel_y = (float)HUD_TR_Y;
    float panel_w = (float)HUD_TR_W;
    float panel_h = (float)HUD_TR_H;
    SDL_Color mm_zone_col = ui_zone_color(player_zone);

    /* ui_panel_terminal already draws interior scanlines — no duplicate pass.
     * Border bumped to ACTIVE (full cyan) for stronger minimap visibility. */
    ui_panel_terminal(g_renderer, panel_x, panel_y, panel_w, panel_h, HUD_BORDER_ACTIVE);

    /* ── Sensor Static (Blindness) override ─────────────────────────────
     * While the blackout timer is active, replace the minimap interior
     * with sparse cyan/cinnabar pixel noise + a "[NO SIGNAL]" label.
     * The panel frame still draws above so the readout location stays
     * recognisable. Reads as "raster snow" rather than a missing UI. */
    if (player.sensor_static_timer > 0.0f) {
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        /* Confine noise to the panel interior */
        float nx0 = panel_x + 4.0f;
        float ny0 = panel_y + 4.0f;
        float nw  = panel_w - 8.0f;
        float nh  = panel_h - 8.0f;
        /* Density and seed shift each frame for shimmer */
        int   speckles = (int)(nw * nh * 0.018f);
        for (int s = 0; s < speckles; s++) {
            int rx = rand() % (int)nw;
            int ry = rand() % (int)nh;
            int hue = rand() & 7;
            Uint8 a = 90 + (rand() & 0x3F);
            SDL_Color c = (hue < 5) ? HUD_TEXT_CYAN : HUD_CINNABAR;
            SDL_SetRenderDrawColor(g_renderer, c.r, c.g, c.b, a);
            SDL_RenderDrawPoint(g_renderer, (int)(nx0 + rx), (int)(ny0 + ry));
        }
        /* Pulsed "[NO SIGNAL]" overlay centred in the panel */
        float pulse = (sinf(game_time * 14.0f) + 1.0f) * 0.5f;
        SDL_Color label = HUD_CINNABAR;
        label.a = (Uint8)(140.0f + 90.0f * pulse);
        vf_draw_string_centered("[NO SIGNAL]",
                                panel_x + panel_w * 0.5f,
                                panel_y + panel_h * 0.5f - 6.0f,
                                9, label);
        return; /* skip the live blip rendering below */
    }

    /* Header */
    vf_draw_string("[MINIMAP]", panel_x + HUD_PAD_INNER,
                   panel_y + HUD_PAD_INNER, 9, HUD_TEXT_DIM);

    /* Separator */
    float mm_sep_y = panel_y + HUD_PAD_INNER + HUD_ROW_H + 2.0f;
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer,
        HUD_BORDER_MID.r, HUD_BORDER_MID.g, HUD_BORDER_MID.b, 80);
    SDL_RenderDrawLineF(g_renderer,
        panel_x + 4.0f, mm_sep_y, panel_x + panel_w - 4.0f, mm_sep_y);

    /* Map area coordinates */
    float mmx  = panel_x + 4.0f;
    float mmy  = mm_sep_y + 4.0f;
    float mmw  = panel_w - 8.0f;
    float mmh  = panel_y + panel_h - mmy - 4.0f;
    float range = 3500.0f;
    float scx  = mmw / (range * 2.0f);
    float scy  = mmh / (range * 2.0f);
    float mcx  = mmx + mmw * 0.5f;
    float mcy  = mmy + mmh * 0.5f;

    /* Draw grid cell background */
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer,
        HUD_PANEL_DEEP.r, HUD_PANEL_DEEP.g, HUD_PANEL_DEEP.b, HUD_PANEL_DEEP.a);
    {
        int cell_sz = 5;
        int cols = (int)(mmw / cell_sz);
        int rows_mm = (int)(mmh / cell_sz);
        for (int rr = 0; rr < rows_mm; rr++) {
            for (int cc = 0; cc < cols; cc++) {
                SDL_FRect cell = { mmx + cc * cell_sz, mmy + rr * cell_sz, 4.0f, 4.0f };
                SDL_RenderFillRectF(g_renderer, &cell);
            }
        }
    }
    (void)mm_zone_col;

    /* Home station — cyan cross */
    {
        float hx = mcx + (0.0f - player.pos.x) * scx;
        float hy = mcy + (0.0f - player.pos.y) * scy;
        if (hx >= mmx && hx <= mmx + mmw && hy >= mmy && hy <= mmy + mmh) {
            Line hl[2] = {{{hx - 6, hy}, {hx + 6, hy}},
                          {{hx, hy - 6}, {hx, hy + 6}}};
            for (int i = 0; i < 2; i++) {
                Shape s = {&hl[i], 1, (SDL_Color){100, 255, 255, 255}};
                vg_draw_shape(&s, (Vec2){0, 0}, 0.0f, 1.0f);
            }
        }
    }

    /* Asteroids - White radar blips */
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) continue;
        float ax = mcx + (asteroids[i].pos.x - player.pos.x) * scx;
        float ay = mcy + (asteroids[i].pos.y - player.pos.y) * scy;
        if (ax < mmx || ax > mmx + mmw || ay < mmy || ay > mmy + mmh) continue;
        ui_minimap_blip(g_renderer, ax, ay, (SDL_Color){255, 255, 255, 255}, 1.0f);
    }

    /* Enemy (UFO) - Red radar blips */
    if (ufo.active) {
        float ux = mcx + (ufo.pos.x - player.pos.x) * scx;
        float uy = mcy + (ufo.pos.y - player.pos.y) * scy;
        if (ux >= mmx && ux <= mmx + mmw && uy >= mmy && uy <= mmy + mmh) {
            ui_minimap_blip(g_renderer, ux, uy, (SDL_Color){255, 0, 0, 255}, 2.0f);
        }
    }

    /* Anomalies/Rifts - Purple radar blips */
    if (rift.active) {
        float rx = mcx + (rift.pos.x - player.pos.x) * scx;
        float ry = mcy + (rift.pos.y - player.pos.y) * scy;
        if (rx >= mmx && rx <= mmx + mmw && ry >= mmy && ry <= mmy + mmh) {
            ui_minimap_blip(g_renderer, rx, ry, (SDL_Color){160, 32, 240, 255}, 3.0f);
        }
    }

    /* NPCs - Green radar blips */
    for (int i = 0; i < MAX_NPC; i++) {
        if (!npcs[i].active) continue;
        float nx = mcx + (npcs[i].pos.x - player.pos.x) * scx;
        float ny = mcy + (npcs[i].pos.y - player.pos.y) * scy;
        if (nx < mmx || nx > mmx + mmw || ny < mmy || ny > mmy + mmh) continue;
        ui_minimap_blip(g_renderer, nx, ny, (SDL_Color){20, 180, 20, 255}, 2.0f);
    }

    /* Player — bright cyan 3×3 rect at map centre */
    SDL_SetRenderDrawColor(g_renderer,
        HUD_TEXT_CYAN.r, HUD_TEXT_CYAN.g, HUD_TEXT_CYAN.b, 255);
    {
        SDL_FRect pdot = {mcx - 1.5f, mcy - 1.5f, 3.0f, 3.0f};
        SDL_RenderFillRectF(g_renderer, &pdot);
    }

    /* Zone label beneath map */
    static const char *mm_zn[] = {
        "HOME SPACE", "INNER BELT", "DEEP VOID", "THE ABYSS", "DEEP DRIFT"
    };
    vf_draw_string_centered(mm_zn[player_zone],
                            panel_x + panel_w * 0.5f,
                            mmy + mmh + 6.0f, 7,
                            ui_zone_color(player_zone));

    /* Off-screen home arrow (preserved) */
    if (player.active) {
        float home_sx = 0.0f - camera_pos.x;
        float home_sy = 0.0f - camera_pos.y;
        int on_screen = (home_sx > 30 && home_sx < SCREEN_WIDTH  - 30 &&
                         home_sy > 30 && home_sy < SCREEN_HEIGHT - 30);
        if (!on_screen) {
            float ddx = home_sx - SCREEN_WIDTH  * 0.5f;
            float ddy = home_sy - SCREEN_HEIGHT * 0.5f;
            float dd  = sqrtf(ddx * ddx + ddy * ddy);
            if (dd > 1.0f) {
                ddx /= dd; ddy /= dd;
                float t = 1e9f;
                if (ddx >  0.001f) t = fminf(t, (SCREEN_WIDTH  - 45.0f - SCREEN_WIDTH  * 0.5f) / ddx);
                if (ddx < -0.001f) t = fminf(t, (45.0f         - SCREEN_WIDTH  * 0.5f) / ddx);
                if (ddy >  0.001f) t = fminf(t, (SCREEN_HEIGHT - 45.0f - SCREEN_HEIGHT * 0.5f) / ddy);
                if (ddy < -0.001f) t = fminf(t, (45.0f         - SCREEN_HEIGHT * 0.5f) / ddy);
                float arx = SCREEN_WIDTH  * 0.5f + ddx * t;
                float ary = SCREEN_HEIGHT * 0.5f + ddy * t;
                float arpx = -ddy, arpy = ddx;
                float as = 11.0f;
                Line arrow[3] = {
                    {{arx + ddx * as,           ary + ddy * as},
                     {arx - ddx * 8 + arpx * 7, ary - ddy * 8 + arpy * 7}},
                    {{arx + ddx * as,           ary + ddy * as},
                     {arx - ddx * 8 - arpx * 7, ary - ddy * 8 - arpy * 7}},
                    {{arx - ddx * 8 + arpx * 7, ary - ddy * 8 + arpy * 7},
                     {arx - ddx * 8 - arpx * 7, ary - ddy * 8 - arpy * 7}}
                };
                SDL_Color ac = HUD_TEXT_CYAN;
                ac.a = 200;
                for (int i = 0; i < 3; i++) {
                    Shape s = {&arrow[i], 1, ac};
                    vg_draw_shape(&s, (Vec2){0, 0}, 0.0f, 1.0f);
                }
                vf_draw_string_centered("HOME", arx, ary - 18.0f, 9,
                                        (SDL_Color){100, 255, 200, 160});
            }
        }
    }
}

/* ────────────────────────────────────────────────────────────────────── */

/**
 * @brief Renders screen-space overlays: score pop-floats, edge flash,
 *        shop overlay, and warp-menu overlay.
 *
 * Camera is in HUD (0,0) space when this returns; caller restores it.
 */
static void render_overlays(void)
{
    /* Score pop-floats (world coordinates — rendered while world camera is set) */
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < MAX_SCORE_FLOATS; i++) {
        if (!score_floats[i].active) continue;
        float t     = score_floats[i].life;
        Uint8 alpha = (t > 0.8f) ? 255 : (Uint8)(t / 0.8f * 255);
        SDL_Color fc = HUD_TEXT_GOLD;  fc.a = alpha;
        if (score_floats[i].value >= 200) { fc = HUD_AMBER;    fc.a = alpha; }
        if (score_floats[i].value >= 400) { fc = HUD_CINNABAR; fc.a = alpha; }
        char temp_text[32];
        sprintf(temp_text, "+%d", score_floats[i].value);
        vf_draw_string_centered(temp_text,
                                score_floats[i].x, score_floats[i].y, 14, fc);
    }

    /* Event text floaters (CRIT!, HIT, VENT!, etc.) — world space, scale pulse */
    for (int i = 0; i < MAX_EVENT_FLOATS; i++) {
        if (!event_floats[i].active) continue;
        float t      = event_floats[i].life / event_floats[i].max_life;
        Uint8 alpha  = (t > 0.6f) ? 255 : (Uint8)(t / 0.6f * 255);
        float sz     = 12.0f + 4.0f * t;
        SDL_Color ec = event_floats[i].color; ec.a = alpha;
        vf_draw_string_centered(event_floats[i].label,
                                event_floats[i].x, event_floats[i].y,
                                sz, ec);
    }

    /* Switch to screen space for all remaining overlays */
    vg_set_camera((Vec2){0.0f, 0.0f});

    /* ── Screen-edge vignette + zone rust tint ──────────────────────────
     * Base black vignette (ui_vignette) darkens all four edges every frame.
     * When the player is in a non-HOME zone, a PHOTIC RUST (#B22222) tinted
     * pass overlays the edges with intensity proportional to zone depth
     * (zone 1 = subtle, zone 3 = heavy).  A 2 Hz sine wave approximates the
     * audio beat peak, blending the tint toward CINNABAR (227,66,52) and
     * briefly boosting alpha, making the screen pulse with zone menace.   */
    if (game_state == STATE_PLAYING || game_state == STATE_PAUSED ||
        game_state == STATE_ATTRACT_GAMEPLAY) {

        ui_vignette(g_renderer);   /* base black edge darkening — always on */

        if (player_zone > 0) {
            float zone_f = (float)player_zone / 4.0f;          /* 0→1 over 5 zones */
            /* 2 Hz beat pulse: 0→1 sine, peaks once per half-second */
            float beat   = (sinf(game_time * 12.5664f) + 1.0f) * 0.5f;
            /* Colour: blend PHOTIC RUST → CINNABAR on beat in deep zones */
            Uint8 vr = (Uint8)(178.0f + beat * zone_f * 49.0f); /* 178→227 */
            Uint8 vg = (Uint8)( 34.0f + beat * zone_f * 32.0f); /* 34→66   */
            Uint8 vb = (Uint8)( 34.0f + beat * zone_f * 18.0f); /* 34→52   */
            /* Alpha: 0 at HOME, up to 85+40=125 at DEEP DRIFT on beat peak */
            Uint8 aa = (Uint8)(zone_f * (85.0f + beat * zone_f * 40.0f));
            SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
            /* Outer pass — 60px band, 1/3 alpha */
            SDL_SetRenderDrawColor(g_renderer, vr, vg, vb, aa / 3);
            {
                SDL_Rect vig_o[4] = {
                    {0,                0,                SCREEN_WIDTH,  60},
                    {0,                SCREEN_HEIGHT-60, SCREEN_WIDTH,  60},
                    {0,                0,                60,            SCREEN_HEIGHT},
                    {SCREEN_WIDTH-60,  0,                60,            SCREEN_HEIGHT}
                };
                for (int vi = 0; vi < 4; vi++)
                    SDL_RenderFillRect(g_renderer, &vig_o[vi]);
            }
            /* Inner pass — 30px band, 2/3 alpha (stronger near edge) */
            SDL_SetRenderDrawColor(g_renderer, vr, vg, vb, (2 * aa) / 3);
            {
                SDL_Rect vig_i[4] = {
                    {0,                0,                SCREEN_WIDTH,  30},
                    {0,                SCREEN_HEIGHT-30, SCREEN_WIDTH,  30},
                    {0,                0,                30,            SCREEN_HEIGHT},
                    {SCREEN_WIDTH-30,  0,                30,            SCREEN_HEIGHT}
                };
                for (int vi = 0; vi < 4; vi++)
                    SDL_RenderFillRect(g_renderer, &vig_i[vi]);
            }
        }
    }

    /* ── CRT Scanline Shimmer ────────────────────────────────────────────────────────────────
     * Applies only to the gameplay viewport (not menus).
     * Two passes:
     *   1. Static scanline grid -- 1px black lines every 4px at ~8% opacity,
     *      giving the screen a classic CRT interlace texture.
     *   2. Periodic phosphor sweep -- a bright teal band rolls from top to
     *      bottom of the screen every 4 seconds, simulating the CRT electron
     *      beam refresh. Alpha peaks at the sweep centre and falls off above
     *      and below.
     * Toggled by g_settings.graphics.scanlines.                            */
    if (g_settings.graphics.scanlines &&
        (game_state == STATE_PLAYING ||
         game_state == STATE_PAUSED  ||
         game_state == STATE_ATTRACT_GAMEPLAY)) {

        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

        /* Pass 1: static scanline grid */
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 20);   /* ~8% black */
        for (int sl = 0; sl < SCREEN_HEIGHT; sl += 4) {
            SDL_RenderDrawLine(g_renderer, 0, sl, SCREEN_WIDTH, sl);
        }

        /* Pass 2: rolling phosphor sweep
         * Period = 4.0s; sweep_y travels 0->SCREEN_HEIGHT over one period. */
        {
            float period  = 4.0f;
            float phase   = fmodf(game_time, period) / period;   /* 0->1 */
            float sweep_y = phase * (float)SCREEN_HEIGHT;

            /* Five stacked lines -- bright centre, dim halos above/below */
            static const struct { int dy; Uint8 alpha; } sweep_passes[] = {
                { -2,  8 },   /* upper halo   */
                { -1, 22 },   /* upper fringe */
                {  0, 42 },   /* centre -- brightest */
                {  1, 22 },   /* lower fringe */
                {  2,  8 },   /* lower halo   */
            };
            for (int sp = 0; sp < 5; sp++) {
                int ly = (int)sweep_y + sweep_passes[sp].dy;
                if (ly < 0 || ly >= SCREEN_HEIGHT) continue;
                /* Teal phosphor tint -- matches the vector screen colour */
                SDL_SetRenderDrawColor(g_renderer,
                    0, 210, 230, sweep_passes[sp].alpha);
                SDL_RenderDrawLine(g_renderer, 0, ly, SCREEN_WIDTH, ly);
            }
        }
    }

    /* Edge-wrap flash -- bright border momentarily when ship wraps */
    if (edge_flash_timer > 0.0f) {
        float alpha = edge_flash_timer / 0.15f;
        SDL_Color ef = HUD_BORDER_ACTIVE; ef.a = (Uint8)(alpha * 180);
        float m = 4.0f;
        Line el[4] = {
            {{m,                m},               {SCREEN_WIDTH - m, m}},
            {{SCREEN_WIDTH - m, m},               {SCREEN_WIDTH - m, SCREEN_HEIGHT - m}},
            {{SCREEN_WIDTH - m, SCREEN_HEIGHT - m},{m,               SCREEN_HEIGHT - m}},
            {{m,                SCREEN_HEIGHT - m},{m,               m}}
        };
        Shape es = {el, 4, ef};
        vg_draw_shape(&es, (Vec2){0, 0}, 0.0f, 1.0f);
    }

    /* ── Warp-Drive Singularity Exit Effect ──────────────────────────
     * Fires on every warp jump (todo §8 / action-list #10, "vector
     * expansion" default variant). A CRT bloom flash washes the screen
     * while three cyan vector rings expand outward from centre, reading
     * as the Autodyne tearing back into realspace. Screen-space, additive.
     * The 3-way unlockable FX progression in the spec is descoped to this
     * single default variant for now. */
    if (warp_flash_timer > 0.0f) {
        float t    = warp_flash_timer / WARP_FLASH_DUR;   /* 1 -> 0 over effect */
        float cx   = SCREEN_WIDTH  * 0.5f;
        float cy   = SCREEN_HEIGHT * 0.5f;
        float maxr = sqrtf(cx * cx + cy * cy);

        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_ADD);

        /* CRT bloom flash — brightest at the instant of arrival, eases out */
        Uint8 fa = (Uint8)(t * t * 150.0f);
        SDL_SetRenderDrawColor(g_renderer, 150, 230, 255, fa);
        SDL_Rect warp_full = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
        SDL_RenderFillRect(g_renderer, &warp_full);

        /* Expanding vector rings — staggered radii, fade as they reach edge */
        for (int ring = 0; ring < 3; ring++) {
            float prog = (1.0f - t) + (float)ring * 0.16f;
            if (prog <= 0.0f || prog > 1.0f) continue;
            float r  = prog * maxr;
            Uint8 ra = (Uint8)((1.0f - prog) * 210.0f);
            SDL_Color rc = HUD_TEXT_CYAN; rc.a = ra;
            SDL_SetRenderDrawColor(g_renderer, rc.r, rc.g, rc.b, rc.a);
            const int N = 48;
            float px = cx + r, py = cy;     /* angle 0 */
            for (int s = 1; s <= N; s++) {
                float a  = (float)s / (float)N * 6.28318530718f;
                float nx = cx + cosf(a) * r;
                float ny = cy + sinf(a) * r;
                SDL_RenderDrawLine(g_renderer, (int)px, (int)py, (int)nx, (int)ny);
                px = nx; py = ny;
            }
        }

        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    }

    /* Autarch (god) mode banner */
    if (god_mode) {
        SDL_Color gm_col = ui_pulse(HUD_GOLD_BAR, game_time, 2.0f, 0.4f);
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        ui_panel_terminal(g_renderer, SCREEN_WIDTH / 2.0f - 200.0f, 4.0f, 400.0f, 28.0f,
                 gm_col);
        vf_draw_string_centered("AUTARCH MODE", SCREEN_WIDTH / 2.0f, 10.0f,
                                14, gm_col);
    }

    /* Critical hull warning — show when on last life */
    if (game_state == STATE_PLAYING && lives <= 1) {
        SDL_Color warn = ui_pulse(HUD_CINNABAR, game_time, 3.0f, 0.5f);
        ui_warning_chevrons(g_renderer, SCREEN_WIDTH / 2.0f - 28.0f,
                            SCREEN_HEIGHT - 30.0f, warn);
    }

    /* ── Zone Transition Banner ──────────────────────────────────────
     * Slides in at screen-centre when the player crosses a zone boundary.
     * 0.4 s fade-in  →  1.7 s hold  →  0.4 s fade-out  (total 2.5 s).
     * Uses the zone's canonical colour so the banner reads at a glance. */
    if (zone_banner_timer > 0.0f) {
        static const char *zone_banner_names[] = {
            ">>> HOME SPACE <<<",
            ">>> INNER BELT <<<",
            ">>> DEEP  VOID <<<",
            ">>> THE  ABYSS <<<",
            ">>> DEEP DRIFT <<<"
        };
        float t = zone_banner_timer;
        float alpha_f;
        if      (t > 2.1f) alpha_f = (2.5f - t) / 0.4f;   /* fade in  */
        else if (t < 0.4f) alpha_f = t / 0.4f;             /* fade out */
        else               alpha_f = 1.0f;                  /* hold     */
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_Color bc = ui_zone_color(player_zone);
        bc.a = (Uint8)(alpha_f * 220.0f);
        float by = SCREEN_HEIGHT * 0.42f;
        vf_draw_string_centered(zone_banner_names[player_zone],
                                SCREEN_WIDTH / 2.0f, by, 18, bc);
        /* thin decorative dividers flanking the text */
        SDL_Color dc = HUD_BORDER_ACTIVE; dc.a = bc.a / 2;
        vf_draw_string_centered("---         ---",
                                SCREEN_WIDTH / 2.0f, by + 26.0f, 10, dc);
    }

    /* ── Proximity Danger Alert ──────────────────────────────────────
     * Pulses ">>> DANGER <<<" in Cinnabar at screen-bottom when any
     * active UFO is within 520 world-units of the player. Suppressed
     * while Sensor Static is active — proximity data lives on the same
     * sensor stack that the blackout takes offline. */
    if (game_state == STATE_PLAYING && ufo.active && player.active
        && player.sensor_static_timer <= 0.0f) {
        float edx  = ufo.pos.x - player.pos.x;
        float edy  = ufo.pos.y - player.pos.y;
        float edist = sqrtf(edx * edx + edy * edy);
        if (edist < 520.0f) {
            float pulse = (sinf(game_time * 10.0f) + 1.0f) * 0.5f;
            SDL_Color dc = HUD_CINNABAR;
            dc.a = (Uint8)(140.0f + 115.0f * pulse);
            SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
            vf_draw_string_centered(">>> DANGER <<<",
                                    SCREEN_WIDTH / 2.0f,
                                    SCREEN_HEIGHT - 54.0f, 14, dc);
        }
    }

    /* Combo pop text */
    if (combo_timer > 0.0f && combo_count >= 4 && g_settings.hud.show_combo) {
        float ct = combo_timer;
        Uint8 ca  = (ct > 0.3f) ? 255 : (Uint8)(ct / 0.3f * 255);
        SDL_Color cc = HUD_AMBER; cc.a = ca;
        char combuf[32];
        sprintf(combuf, "x%d", combo_count);
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        vf_draw_string_centered(combuf, SCREEN_WIDTH / 2.0f,
                                SCREEN_HEIGHT / 2.0f - 60.0f, 28, cc);
    }

    /* Shop overlay */
    if (game_state == STATE_SHOP) {
        ui_panel_terminal(g_renderer, 50, 30, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 60,
                 HUD_BORDER_MAIN);
        vf_draw_string_centered("HOME STATION EXCHANGE",
                                SCREEN_WIDTH / 2.0f, 60, 20, HUD_TEXT_CYAN);
        vf_draw_string_centered("[ ESC ] CLOSE",
                                SCREEN_WIDTH / 2.0f, 88, 11,
                                (SDL_Color){120, 120, 140, 200});
        static const char *shop_names[] = {
            "FUEL CELLS (25u)",   "FULL REFUEL",        "THRUSTER UPGRADE",
            "CANNON UPGRADE",     "HULL PLATING",        "EMERGENCY LIFE",
            "ROCKET PACK (+5)",   "FIGHTER DRONE",       "REPAIR DRONE",
            "WARP RANGE +1000u",  "FUEL TANK UPGRADE",  "AMMO RESUPPLY",
            "VOID STEEL SMELTER", "AUTODYNE SCANNER",    "CHROME: TRISKELION",
            "CHROME: ETHER SHROUD"
        };
        #define SHOP_TOTAL 16
        for (int si = 0; si < SHOP_TOTAL && si < SHOP_ITEMS_PER_PAGE; si++) {
            float iy = 115.0f + si * 42.0f;
            SDL_Color ic = (si == shop_sel) ? HUD_TEXT_GOLD : HUD_TEXT_PRIMARY;
            if (si == shop_sel) {
                ui_corner_brackets(g_renderer,
                                   80, iy - 4, SCREEN_WIDTH - 160, 36,
                                   HUD_TEXT_GOLD, 10.0f);
                ui_cursor_chevron(g_renderer, 70.0f, iy, HUD_TEXT_CYAN);
            }
            vf_draw_string(shop_names[si], 110.0f, iy, 13, ic);
        }
        #undef SHOP_TOTAL
        char inv[128];
        sprintf(inv, "INV: VS:%d AF:%d HX:%d IS:%d AM:%d RK:%d CHR:%d",
                res_void_steel, res_autodyne_frags, res_hex_modules,
                res_isotopes, res_ammo, res_rockets, chrome);
        vf_draw_string_centered(inv, SCREEN_WIDTH / 2.0f,
                                SCREEN_HEIGHT - 70, 10, HUD_TEXT_DIM);
    }

    /* Warp-menu overlay */
    if (game_state == STATE_WARP_MENU) {
        ui_panel_terminal(g_renderer, 190, 140, SCREEN_WIDTH - 380, SCREEN_HEIGHT - 280,
                 HUD_BORDER_MAIN);
        vf_draw_string_centered("WARP DRIVE \xe2\x80\x94 SAVED LOCI",
                                SCREEN_WIDTH / 2.0f, 165, 16, HUD_TEXT_CYAN);
        if (warp_loc_count == 0) {
            vf_draw_string_centered("NO LOCI SAVED",
                                    SCREEN_WIDTH / 2.0f, 230, 11,
                                    HUD_TEXT_DIM);
        } else {
            for (int wi = 0; wi < warp_loc_count; wi++) {
                float wy   = 200.0f + wi * 40.0f;
                float wdx  = warp_locs[wi].pos.x - player.pos.x;
                float wdy  = warp_locs[wi].pos.y - player.pos.y;
                float wdist = sqrtf(wdx * wdx + wdy * wdy);
                int in_range = (wdist <= warp_drive_range) &&
                               (fuel_current >= 20.0f);
                SDL_Color wc = (wi == warp_menu_sel)
                               ? HUD_TEXT_GOLD
                               : in_range
                                 ? HUD_GREEN
                                 : HUD_CINNABAR;
                char wbuf[80];
                sprintf(wbuf, "%s  [%.0fu]%s",
                        warp_locs[wi].label, wdist,
                        in_range ? "" : " OUT OF RANGE");
                if (wi == warp_menu_sel)
                    ui_cursor_chevron(g_renderer, 200.0f, wy, HUD_TEXT_CYAN);
                vf_draw_string_centered(wbuf, SCREEN_WIDTH / 2.0f, wy, 13, wc);
            }
        }
        vf_draw_string_centered("ENTER=WARP  ESC=CANCEL  (20 FUEL)",
                                SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 165, 11,
                                HUD_TEXT_DIM);
    }

    /* Crosshair — drawn last so it sits on top of everything */
    if (settings_mouse_aim && settings_crosshair_style > 0 &&
        (game_state == STATE_PLAYING ||
         game_state == STATE_ATTRACT_GAMEPLAY)) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        SDL_Color xhc = HUD_TEXT_CYAN; xhc.a = 200;
        float fx = (float)mx, fy = (float)my;
        if (settings_crosshair_style == 1) {   /* Cross with centre gap */
            Line cl[4] = {
                {{fx - 14, fy}, {fx -  5, fy}},
                {{fx +  5, fy}, {fx + 14, fy}},
                {{fx, fy - 14}, {fx, fy -  5}},
                {{fx, fy +  5}, {fx, fy + 14}}
            };
            for (int ci = 0; ci < 4; ci++) {
                Shape cs = {&cl[ci], 1, xhc};
                vg_draw_shape(&cs, (Vec2){0, 0}, 0.0f, 1.0f);
            }
        } else {                                /* Dot */
            Line cl[2] = {
                {{fx - 4, fy}, {fx + 4, fy}},
                {{fx, fy - 4}, {fx, fy + 4}}
            };
            for (int ci = 0; ci < 2; ci++) {
                Shape cs = {&cl[ci], 1, xhc};
                vg_draw_shape(&cs, (Vec2){0, 0}, 0.0f, 1.0f);
            }
        }
    }

    /* ── Viewport-wide CRT scanline shimmer ─────────────────────────────
     * A single bright sweep line descends across the full gameplay viewport
     * every 3-5 seconds when g_settings.graphics.scanlines is enabled.
     * Runs in screen space (camera reset to origin).                      */
    if (g_settings.graphics.scanlines) {
        static float scan_timer     = 0.0f;   /* time until next sweep    */
        static float scan_y         = -1.0f;  /* current sweep position   */
        static float scan_period    = 4.0f;   /* seconds between sweeps   */
        static int   scan_sweeping  = 0;       /* 1 while sweep is active  */

        float dt = 0.016f;   /* fixed step — same as g_ghost_t increment  */

        if (!scan_sweeping) {
            scan_timer -= dt;
            if (scan_timer <= 0.0f) {
                scan_sweeping = 1;
                scan_y        = 0.0f;
                /* Randomise next period in [3, 5] seconds */
                scan_period = 3.0f + ((float)(rand() % 2000) / 1000.0f);
                scan_timer  = scan_period;
            }
        } else {
            /* Sweep at 220 px/s so it crosses a 720p viewport in ~3.3 s */
            scan_y += 220.0f * dt;
            if (scan_y > (float)SCREEN_HEIGHT) {
                scan_sweeping = 0;
                scan_y        = -1.0f;
            }
        }

        if (scan_sweeping && scan_y >= 0.0f) {
            /* Switch to screen space for the scanline */
            vg_set_camera((Vec2){0.0f, 0.0f});
            SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

            /* Primary bright line ~7% opacity */
            SDL_SetRenderDrawColor(g_renderer, 200, 220, 255, 18);
            SDL_RenderDrawLineF(g_renderer,
                                0.0f, scan_y,
                                (float)SCREEN_WIDTH, scan_y);

            /* Soft glow: two flanking lines at ~4% opacity */
            SDL_SetRenderDrawColor(g_renderer, 200, 220, 255, 10);
            SDL_RenderDrawLineF(g_renderer,
                                0.0f, scan_y - 1.0f,
                                (float)SCREEN_WIDTH, scan_y - 1.0f);
            SDL_RenderDrawLineF(g_renderer,
                                0.0f, scan_y + 1.0f,
                                (float)SCREEN_WIDTH, scan_y + 1.0f);
        }
    }

}

/* ────────────────────────────────────────────────────────────────────── */

/**
 * @brief Renders the currently active menu or UI screen based on
 *        game_state.  World-rendering helpers have already run; camera
 *        is in screen space (0,0) at entry.
 */
static void render_menus(void)
{
    /* =========== STATE: TITLE =========== */
    if (game_state == STATE_TITLE) {
        static float title_timer = 0.0f;
        title_timer += 0.016f;  /* visual-only timer, frame-rate dependence acceptable */

        if (g_boot_timer < 3.0f) {
            /* Centered boot splash — 5 lines × 30px = 150px stack, centered
             * vertically in 960px screen → start_y = 405.  Center-x = 640. */
            SDL_Color term_green = {50, 255, 50, 255};
            const float cx      = (float)SCREEN_WIDTH / 2.0f;   /* 640 */
            float       start_y = ((float)SCREEN_HEIGHT - 150.0f) / 2.0f; /* 405 */

            if (g_boot_timer > 0.1f) {
                vf_draw_string_centered("NOCTIS OS V4.0 LOADED...", cx, start_y, 16.0f, term_green);
                start_y += 30.0f;
            }
            if (g_boot_timer > 0.6f) {
                vf_draw_string_centered("MEM CHECK OK...", cx, start_y, 16.0f, term_green);
                start_y += 30.0f;
            }
            if (g_boot_timer > 1.2f) {
                vf_draw_string_centered("INITIALIZING SENSORS...", cx, start_y, 16.0f, term_green);
                start_y += 30.0f;
            }
            if (g_boot_timer > 1.8f) {
                vf_draw_string_centered("LOADING FULIGIN PROTOCOL...", cx, start_y, 16.0f, term_green);
                start_y += 30.0f;
            }
            if (g_boot_timer > 2.5f) {
                vf_draw_string_centered("SYSTEM READY.", cx, start_y, 16.0f, term_green);
            }
            return;
        }

        /* Background atmosphere */
        ui_particle_drift(g_renderer, game_time, 40);

        /* Central hero panel */
        ui_panel_terminal(g_renderer, 340, 200, 600, 520, HUD_BORDER_MAIN);

        float panel_cx = 340.0f + 600.0f / 2.0f;  /* 640 */

        /* Pulsing title */
        float title_sz = 52.0f * (1.0f + 0.04f * sinf(title_timer * 2.0f));
        SDL_Color title_col = ui_pulse(HUD_TEXT_CYAN, game_time, 0.8f, 0.15f);
        vf_draw_string_centered("FULIGIN", panel_cx, 260, title_sz, title_col);

        /* Subtitle — dim cyan */
        SDL_Color dim_cyan = {0, 180, 180, 180};
        vf_draw_string_centered(
            "D R I F T I N G   A T   T H E   E D G E   O F   U R T H",
            panel_cx, 340, 11, dim_cyan);

        /* Thin separator at y=370 */
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 26, 26, 46, 140);
        SDL_RenderDrawLine(g_renderer, 350, 370, 930, 370);

        /* Menu items */
        const char *menu_labels[] = {"BEGIN DRIFT", "DRIFTER ANNALS", "SETTINGS"};
        float item_y_start = 400.0f;
        float item_step    = 60.0f;
        for (int i = 0; i < 3; i++) {
            int   is_sel = (menu_selection == i);
            SDL_Color ic = is_sel ? HUD_TEXT_GOLD : HUD_TEXT_PRIMARY;
            float iy     = item_y_start + i * item_step;
            vf_draw_string_centered(menu_labels[i], panel_cx, iy, 20, ic);
            if (is_sel) {
                ui_cursor_chevron(g_renderer, panel_cx - 130.0f, iy, HUD_TEXT_CYAN);
                ui_corner_brackets(g_renderer,
                                   panel_cx - 120.0f, iy - 6.0f,
                                   240.0f, 30.0f,
                                   HUD_TEXT_GOLD, 12.0f);
            }
        }

    /* =========== STATE: PAUSED =========== */
    } else if (game_state == STATE_PAUSED) {
        /* Dark semi-transparent overlay */
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 160);
        SDL_Rect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(g_renderer, &overlay);

        /* Center panel */
        ui_panel_terminal(g_renderer, 200, 40, 880, SCREEN_HEIGHT - 80, HUD_BORDER_MAIN);

        float panel_cx = 200.0f + 880.0f / 2.0f;  /* 640 */

        /* Header */
        vf_draw_string_centered("STASIS", panel_cx, 65, 32, HUD_TEXT_PRIMARY);

        /* Pause options section */
        ui_panel_header(g_renderer, 200, 105, 880, "PAUSE OPTIONS", HUD_TEXT_CYAN);

        const char *pause_opts[] = {
            "S: SAVE GAME",
            "L: LOAD GAME",
            "Q: QUIT TO TITLE",
            "ESC/SPACE: RESUME"
        };
        float opt_y = 125.0f;
        for (int i = 0; i < 4; i++) {
            vf_draw_string_centered(pause_opts[i], panel_cx, opt_y, 17, HUD_TEXT_PRIMARY);
            opt_y += 32.0f;
        }

        /* Separator at y=255 */
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 26, 26, 46, 160);
        SDL_RenderDrawLine(g_renderer, 210, 255, 1070, 255);

        /* Relics section */
        ui_panel_header(g_renderer, 200, 265, 880, "RELIQUARIES CARRIED", HUD_TEXT_CYAN);

        /* ── Relic log: map active upgrades to name+description rows ── */
        typedef struct { const char *name; const char *desc; } RelicEntry;
        RelicEntry relics[30];
        int relic_count = 0;
        #define ADD_RELIC(up, nm, dc) \
            if (up) { relics[relic_count].name = (nm); \
                      relics[relic_count].desc = (dc); \
                      relic_count++; }
        ADD_RELIC(player_upgrades.triple_shot,
                  "TRISKELION BURST",   "Three bolts spread from prow  [ fires with FIRE key ]")
        ADD_RELIC(player_upgrades.max_bounces > 0,
                  "VOID RICOCHET",      "Bolts rebound off void edge   [ passive ]")
        ADD_RELIC(player_upgrades.shield_active,
                  "ETHER SHROUD",       "Absorbs one asteroid hit      [ passive ]")
        ADD_RELIC(player_upgrades.piercing,
                  "ICHOROUS ROUNDS",    "Bolts pierce through enemies  [ passive ]")
        ADD_RELIC(player_upgrades.homing,
                  "SEEKER ICHORS",      "Bolts home to nearest stone   [ passive ]")
        ADD_RELIC(player_upgrades.rear_gun,
                  "AFT CANNON",         "Fires bolt from the aft too   [ fires with FIRE key ]")
        ADD_RELIC(player_upgrades.split_shot,
                  "FISSION SHOT",       "Bolts split on first impact   [ passive ]")
        ADD_RELIC(player_upgrades.mirror_image,
                  "WRAITH TWIN",        "Phantom twin orbits behind    [ passive ]")
        ADD_RELIC(player_upgrades.phase_shift,
                  "PHASE SHIFT",        "Pass through one killing blow [ passive, one-time ]")
        ADD_RELIC(player_upgrades.thermal_hull,
                  "THERMAL HULL",       "Ram at speed to destroy stone [ THRUST into enemy ]")
        ADD_RELIC(player_upgrades.singularity_displacer,
                  "RIFT DISPLACER",     "Double-tap THRUST to rift-jump [ tap THRUST twice ]")
        ADD_RELIC(player_upgrades.singularity_whip,
                  "BANE WHIP",          "Thrust trail scorches drift   [ hold THRUST ]")
        ADD_RELIC(player_upgrades.resonance_cascade,
                  "RESONANCE CASCADE",  "Bolts unleash shockwaves      [ fires with FIRE key ]")
        #undef ADD_RELIC

        if (relic_count > 0) {
            float ry    = 285.0f;
            int   shown = relic_count < 12 ? relic_count : 12;
            float row_h = (float)(SCREEN_HEIGHT - 310) / shown;
            if (row_h > 52.0f) row_h = 52.0f;
            /* Icon col x=220, name col x=285, desc col x=560 */
            for (int i = 0; i < shown; i++) {
                char icon[4] = {relics[i].name[0], relics[i].name[1],
                                relics[i].name[2], '\0'};
                vf_draw_string(icon,            220, ry, 11, HUD_TEXT_CYAN);
                vf_draw_string(relics[i].name,  285, ry, 11, HUD_TEXT_CYAN);
                vf_draw_string(relics[i].desc,  560, ry,  9, HUD_TEXT_DIM);
                ry += row_h;
            }
        } else {
            vf_draw_string_centered("NO RELIQUARIES CARRIED",
                                    panel_cx, 310, 14, HUD_TEXT_DIM);
        }

    /* =========== STATE: SETTINGS =========== */
    } else if (game_state == STATE_SETTINGS) {
        /* Tab names for the 9-category settings menu */
        const char *tab_names[] = {
            "VIDEO", "GRAPHICS", "AUDIO", "CONTROLS",
            "HUD", "ACCESS", "GAMEPLAY", "WORLD", "SYSTEM"
        };
        const int tab_count = 9;

        /* Outer 800×640 center panel */
        float sp_x = (SCREEN_WIDTH  - 800) / 2.0f;   /* 240 */
        float sp_y = (SCREEN_HEIGHT - 640) / 2.0f;   /* 160 */
        float sp_w = 800.0f, sp_h = 640.0f;
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        ui_panel_terminal(g_renderer, sp_x, sp_y, sp_w, sp_h, HUD_BORDER_MAIN);

        float panel_cx = sp_x + sp_w / 2.0f;  /* 640 */

        /* Header */
        vf_draw_string_centered("AUTODYNE CONFIGURATION", panel_cx,
                                sp_y + 22.0f, 18, HUD_TEXT_CYAN);

        /* Tab strip — two rows of angled mini-tabs, 5 on top row, 4 on second */
        float tab_w = 140.0f, tab_h = 26.0f;
        float tab_row1_y = sp_y + 48.0f;
        float tab_row2_y = tab_row1_y + tab_h + 4.0f;
        float tab_row1_x = sp_x + 10.0f;
        float tab_row2_x = sp_x + 10.0f + (tab_w + 6.0f) * 0.5f;  /* offset 2nd row */
        for (int t = 0; t < tab_count; t++) {
            int   row   = (t < 5) ? 0 : 1;
            int   col   = (t < 5) ? t : t - 5;
            float tx    = (row == 0 ? tab_row1_x : tab_row2_x) + col * (tab_w + 6.0f);
            float ty    = (row == 0) ? tab_row1_y : tab_row2_y;
            int   is_sel = (t == settings_tab);
            SDL_Color border = is_sel ? HUD_BORDER_ACTIVE : HUD_BORDER_DIM;
            ui_panel_angled(g_renderer, tx, ty, tab_w, tab_h, 6.0f, border);
            SDL_Color tc = is_sel ? HUD_TEXT_GOLD : HUD_TEXT_DIM;
            vf_draw_string_centered(tab_names[t], tx + tab_w / 2.0f,
                                    ty + 7.0f, 11, tc);
        }

        /* Content area starts below tabs */
        float base_y = tab_row2_y + tab_h + 16.0f;
        float step   = 44.0f;
        float row_x  = sp_x + 40.0f;

        /* Helper macro: draw one settings row at index i with label and value */
        #define DRAW_SETTINGS_ROW(idx, label, valstr) \
        do { \
            int   _sel = (settings_row == (idx)); \
            float _y   = base_y + (idx) * step; \
            SDL_Color _lc = _sel ? HUD_TEXT_CYAN  : HUD_TEXT_DIM; \
            SDL_Color _vc = _sel ? HUD_TEXT_GOLD  : HUD_TEXT_PRIMARY; \
            if (_sel) ui_cursor_chevron(g_renderer, row_x - 16.0f, _y, HUD_TEXT_CYAN); \
            vf_draw_string((label),   row_x,         _y, 13, _lc); \
            vf_draw_string((valstr),  row_x + 340.0f, _y, 13, _vc); \
            if (_sel) { \
                float _bw = sp_w - 80.0f; \
                ui_corner_brackets(g_renderer, row_x - 4.0f, _y - 6.0f, \
                                   _bw, 22.0f, HUD_BORDER_ACTIVE, 8.0f); \
            } \
        } while (0)

        const char *on_off[]     = {"OFF", "ON"};
        const char *glow_names[] = {"OFF", "LOW", "MED", "HIGH", "MAX"};
        const char *dz_names[]   = {"LOW", "MED", "HIGH"};
        const char *ch_names[]   = {"NONE", "DOT", "CROSS", "CHEVRON"};
        const char *cb_names[]   = {"NONE", "DEUTERANOPIA", "PROTANOPIA", "TRITANOPIA"};
        const char *diff_names[] = {"AUTODYNE", "STANDARD", "CACOGEN"};
        char _vbuf[64];

        /* ── VIDEO tab ── */
        if (settings_tab == 0) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "DISPLAY MODE",
                g_settings.video.fullscreen ? "FULLSCREEN" : "WINDOWED");
            sprintf(_vbuf, "%d", g_settings.video.refresh_rate);
            DRAW_SETTINGS_ROW(r++, "REFRESH RATE", _vbuf);
            DRAW_SETTINGS_ROW(r++, "VSYNC",
                on_off[g_settings.video.vsync]);

        /* ── GRAPHICS tab ── */
        } else if (settings_tab == 1) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "PHOSPHOR GLOW",
                glow_names[g_settings.graphics.glow_intensity]);
            sprintf(_vbuf, "%d", g_settings.graphics.particle_count);
            DRAW_SETTINGS_ROW(r++, "PARTICLE DRIFT", _vbuf);
            DRAW_SETTINGS_ROW(r++, "CRT SCANLINES",
                on_off[g_settings.graphics.scanlines]);
            DRAW_SETTINGS_ROW(r++, "VIGNETTE",
                on_off[g_settings.graphics.vignette]);
            DRAW_SETTINGS_ROW(r++, "MOTION BLUR",
                on_off[g_settings.graphics.motion_blur]);
            DRAW_SETTINGS_ROW(r++, "SCREEN SHAKE",
                on_off[g_settings.graphics.screen_shake]);

        /* ── AUDIO tab ── */
        } else if (settings_tab == 2) {
            int r = 0;
            sprintf(_vbuf, "%d%%", g_settings.audio.master_vol);
            DRAW_SETTINGS_ROW(r++, "MASTER VOLUME", _vbuf);
            sprintf(_vbuf, "%d%%", g_settings.audio.music_vol);
            DRAW_SETTINGS_ROW(r++, "MUSIC VOLUME",  _vbuf);
            sprintf(_vbuf, "%d%%", g_settings.audio.sfx_vol);
            DRAW_SETTINGS_ROW(r++, "SFX VOLUME",    _vbuf);
            sprintf(_vbuf, "%d%%", g_settings.audio.ui_vol);
            DRAW_SETTINGS_ROW(r++, "UI VOLUME",     _vbuf);
            DRAW_SETTINGS_ROW(r++, "STREAMER MODE",
                on_off[g_settings.audio.streamer_mode]);

        /* ── CONTROLS tab ── */
        } else if (settings_tab == 3) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "MOUSE AIM",
                on_off[g_settings.controls.mouse_aim]);
            sprintf(_vbuf, "%d", g_settings.controls.mouse_sensitivity);
            DRAW_SETTINGS_ROW(r++, "MOUSE SENSITIVITY", _vbuf);
            DRAW_SETTINGS_ROW(r++, "AUTOFIRE",
                on_off[g_settings.controls.autofire]);
            DRAW_SETTINGS_ROW(r++, "AIM ASSIST",
                on_off[g_settings.controls.aim_assist]);
            DRAW_SETTINGS_ROW(r++, "CTRL DEADZONE",
                dz_names[g_settings.controls.ctrl_deadzone]);
            DRAW_SETTINGS_ROW(r++, "INVERT Y",
                on_off[g_settings.controls.invert_y]);
            DRAW_SETTINGS_ROW(r++, "MOUSE FIRE BTN",
                mouse_btn_label(g_settings.controls.mouse_fire_btn));
            DRAW_SETTINGS_ROW(r++, "MOUSE THRUST BTN",
                mouse_btn_label(g_settings.controls.mouse_thrust_btn));
            DRAW_SETTINGS_ROW(r++, "MOUSE HYPER BTN",
                mouse_btn_label(g_settings.controls.mouse_hyper_btn));
            DRAW_SETTINGS_ROW(r++, "KEYBINDS", ">");
            if (g_controller)
                vf_draw_string_centered("CONTROLLER CONNECTED",
                                        panel_cx, sp_y + sp_h - 40.0f,
                                        11, HUD_GREEN);

        /* ── HUD tab ── */
        } else if (settings_tab == 4) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "SHOW FPS",
                on_off[g_settings.hud.show_fps]);
            DRAW_SETTINGS_ROW(r++, "SHOW MINIMAP",
                on_off[g_settings.hud.show_minimap]);
            DRAW_SETTINGS_ROW(r++, "CROSSHAIR",
                ch_names[g_settings.hud.crosshair]);
            sprintf(_vbuf, "%d%%", g_settings.hud.hud_scale);
            DRAW_SETTINGS_ROW(r++, "HUD SCALE",   _vbuf);
            DRAW_SETTINGS_ROW(r++, "SHOW COMBO",
                on_off[g_settings.hud.show_combo]);
            DRAW_SETTINGS_ROW(r++, "SHOW ZONE NAME",
                on_off[g_settings.hud.show_zone_name]);

        /* ── ACCESSIBILITY tab ── */
        } else if (settings_tab == 5) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "COLORBLIND MODE",
                cb_names[g_settings.accessibility.colorblind]);
            DRAW_SETTINGS_ROW(r++, "HIGH CONTRAST",
                on_off[g_settings.accessibility.high_contrast]);
            sprintf(_vbuf, "%+d", g_settings.accessibility.font_size_delta);
            DRAW_SETTINGS_ROW(r++, "FONT SIZE DELTA", _vbuf);
            DRAW_SETTINGS_ROW(r++, "REDUCE MOTION",
                on_off[g_settings.accessibility.reduce_motion]);

        /* ── GAMEPLAY tab ── */
        } else if (settings_tab == 6) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "DIFFICULTY",
                diff_names[g_settings.gameplay.difficulty]);
            sprintf(_vbuf, "%d", g_settings.gameplay.starting_lives);
            DRAW_SETTINGS_ROW(r++, "STARTING LIVES", _vbuf);

        /* ── WORLD tab ── */
        } else if (settings_tab == 7) {
            int r = 0;
            sprintf(_vbuf, "%u", (unsigned)g_settings.world.seed);
            DRAW_SETTINGS_ROW(r++, "WORLD SEED",        _vbuf);
            sprintf(_vbuf, "%.2f", g_settings.world.asteroid_density);
            DRAW_SETTINGS_ROW(r++, "ASTEROID DENSITY",  _vbuf);
            sprintf(_vbuf, "%.2f", g_settings.world.enemy_density);
            DRAW_SETTINGS_ROW(r++, "ENEMY DENSITY",     _vbuf);
            sprintf(_vbuf, "%.2f", g_settings.world.loot_multiplier);
            DRAW_SETTINGS_ROW(r++, "LOOT MULTIPLIER",   _vbuf);
            sprintf(_vbuf, "%d",   g_settings.world.zone_sharpness);
            DRAW_SETTINGS_ROW(r++, "ZONE SHARPNESS",    _vbuf);
            sprintf(_vbuf, "%d",   g_settings.world.starting_zone);
            DRAW_SETTINGS_ROW(r++, "STARTING ZONE",     _vbuf);

        /* ── SYSTEM tab ── */
        } else if (settings_tab == 8) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "SHOW INTRO",
                on_off[g_settings.show_intro]);
            DRAW_SETTINGS_ROW(r++, "RESET DEFAULTS", "ENTER");
            DRAW_SETTINGS_ROW(r++, "SAVE & QUIT",    "ENTER");
        }

        #undef DRAW_SETTINGS_ROW

        /* Bottom instructions */
        vf_draw_string_centered(
            "[ Q / E ]  PREV/NEXT TAB    [ UP / DN ]  SELECT ROW    "
            "[ LT / RT ]  CHANGE VALUE    [ ESC ]  BACK",
            panel_cx, sp_y + sp_h - 20.0f, 9, HUD_TEXT_DARK);

    /* =========== STATE: KEYBINDS =========== */
    } else if (game_state == STATE_KEYBINDS) {
        ui_panel_terminal(g_renderer, 80, 20, SCREEN_WIDTH - 160, SCREEN_HEIGHT - 40,
                 HUD_BORDER_MAIN);

        float panel_cx = 80.0f + (SCREEN_WIDTH - 160) / 2.0f;  /* 640 */

        SDL_Color kb_tc = (keybind_page == 0) ? HUD_TEXT_GOLD : HUD_TEXT_DIM;
        SDL_Color ct_tc = (keybind_page == 1) ? HUD_TEXT_GOLD : HUD_TEXT_DIM;

        vf_draw_string_centered("KEYBINDS", panel_cx, 35, 22, HUD_TEXT_CYAN);
        vf_draw_string("KEYBOARD",   panel_cx - 200.0f, 70, 16, kb_tc);
        vf_draw_string("CONTROLLER", panel_cx +  40.0f, 70, 16, ct_tc);

        vf_draw_string_centered("Q / E   TO   SWITCH   PAGES",
                                panel_cx, SCREEN_HEIGHT - 60, 11, HUD_TEXT_DARK);
        vf_draw_string_centered("ESC TO RETURN",
                                panel_cx, SCREEN_HEIGHT - 38, 11, HUD_TEXT_DARK);

        if (rebinding_action >= 0) {
            vf_draw_string_centered("PRESS ANY KEY",
                                    panel_cx,
                                    SCREEN_HEIGHT / 2.0f - 20, 28, HUD_AMBER);
            vf_draw_string_centered("ESC TO CANCEL",
                                    panel_cx,
                                    SCREEN_HEIGHT / 2.0f + 30, 16, HUD_TEXT_DIM);
        } else if (ctrl_rebinding_action >= 0) {
            vf_draw_string_centered("PRESS CONTROLLER BUTTON",
                                    panel_cx,
                                    SCREEN_HEIGHT / 2.0f - 20, 24, HUD_AMBER);
            vf_draw_string_centered("START/BACK RESERVED   ESC TO CANCEL",
                                    panel_cx,
                                    SCREEN_HEIGHT / 2.0f + 30, 14, HUD_TEXT_DIM);
        } else if (keybind_page == 0) {
            /* Keyboard binds list */
            vf_draw_string_centered("ENTER TO REBIND",
                                    panel_cx, 100, 12, HUD_TEXT_DARK);
            float kb_row_h = (float)(SCREEN_HEIGHT - 160) / KB_COUNT;
            if (kb_row_h > 44.0f) kb_row_h = 44.0f;
            for (int i = 0; i < KB_COUNT; i++) {
                int   is_sel = (keybind_selection == i);
                SDL_Color ic = is_sel ? HUD_TEXT_GOLD : HUD_TEXT_PRIMARY;
                const char *kname = SDL_GetScancodeName(keybinds[i]);
                char row[64];
                sprintf(row, "%-12s  %s", kb_action_names[i], kname);
                float y = 120.0f + i * kb_row_h;
                if (is_sel)
                    ui_cursor_chevron(g_renderer, panel_cx - 200.0f, y,
                                      HUD_TEXT_CYAN);
                vf_draw_string_centered(row, panel_cx, y, 16, ic);
                if (is_sel) {
                    float bw = (float)strlen(row) * 16 * 0.7f + 20.0f;
                    ui_corner_brackets(g_renderer,
                                       panel_cx - bw / 2.0f, y - 8.0f,
                                       bw, 24.0f, HUD_TEXT_GOLD, 10.0f);
                }
            }
        } else {
            /* Controller binds diagram page */
            float cx = SCREEN_WIDTH  / 2.0f;
            float cy = SCREEN_HEIGHT / 2.0f;
            render_controller_binds_page(cx, cy, keybind_selection,
                                         ctrl_binds, ct_action_names);
            if (!g_controller) {
                vf_draw_string_centered("NO CONTROLLER DETECTED",
                                        panel_cx,
                                        SCREEN_HEIGHT - 90, 14,
                                        HUD_CINNABAR);
            }
        }

    /* =========== STATE: HIGH SCORES (DRIFTER ANNALS) =========== */
    } else if (game_state == STATE_HIGHSCORES) {
        /* Background particles */
        ui_particle_drift(g_renderer, game_time, 25);

        /* Panel */
        ui_panel_terminal(g_renderer, 280, 80, 720, 750, HUD_GOLD_BAR);

        float panel_cx = 280.0f + 720.0f / 2.0f;  /* 640 */

        /* Header */
        vf_draw_string_centered("THE FALLEN DRIFTERS", panel_cx, 115, 28,
                                HUD_TEXT_GOLD);

        /* Score entries */
        for (int i = 0; i < 5; i++) {
            float ey = 210.0f + i * 90.0f;
            char rank_buf[4];
            sprintf(rank_buf, "%d.", i + 1);
            vf_draw_string_centered(rank_buf,                  panel_cx - 160.0f, ey, 20, HUD_TEXT_DIM);
            vf_draw_string_centered(high_scores[i].initials,   panel_cx -  40.0f, ey, 20, HUD_TEXT_CYAN);
            char score_buf[16];
            sprintf(score_buf, "%05d", high_scores[i].score);
            vf_draw_string_centered(score_buf,                 panel_cx + 120.0f, ey, 20, HUD_TEXT_PRIMARY);
            /* AUTARCH label above top entry */
            if (i == 0) {
                vf_draw_string_centered("AUTARCH", panel_cx, ey - 20.0f, 9,
                                        HUD_TEXT_GOLD);
            }
        }

        /* Instruction */
        vf_draw_string_centered("PRESS SPACE TO RETURN TO THE DRIFT",
                                panel_cx, SCREEN_HEIGHT - 80, 12, HUD_TEXT_DIM);

    /* =========== STATE: GAME OVER =========== */
    } else if (game_state == STATE_GAMEOVER) {
        /* Background particles with cinnabar tint */
        ui_particle_drift(g_renderer, game_time, 50);

        /* Panel */
        ui_panel_terminal(g_renderer, 290, 280, 700, 340, HUD_CINNABAR);

        float panel_cx = 290.0f + 700.0f / 2.0f;  /* 640 */

        SDL_Color go_col = ui_pulse(HUD_CINNABAR, game_time, 1.5f, 0.3f);
        vf_draw_string_centered("DRIFT ENDS", panel_cx, 315, 42, go_col);

        vf_draw_string_centered("THE AUTODYNE IS LOST TO THE VOID",
                                panel_cx, 380, 13, HUD_TEXT_DIM);

        char final_score[48];
        sprintf(final_score, "FINAL CHRONICLE: %d", score);
        vf_draw_string_centered(final_score, panel_cx, 420, 18, HUD_TEXT_PRIMARY);

        vf_draw_string_centered("PRESS ENTER TO DRIFT AGAIN",
                                panel_cx, 475, 16, HUD_AMBER);

    /* =========== STATE: NEW HIGH SCORE =========== */
    } else if (game_state == STATE_NEW_HIGHSCORE) {
        /* Panel */
        ui_panel_terminal(g_renderer, 340, 280, 600, 340, HUD_GOLD_BAR);

        float panel_cx = 340.0f + 600.0f / 2.0f;  /* 640 */

        vf_draw_string_centered("NEW DRIFTER RECORD!", panel_cx, 310, 24,
                                HUD_TEXT_GOLD);

        char score_buf[32];
        sprintf(score_buf, "%d", score);
        vf_draw_string_centered(score_buf, panel_cx, 350, 16, HUD_TEXT_PRIMARY);

        vf_draw_string_centered("INSCRIBE YOUR MARK:", panel_cx, 390, 18,
                                HUD_TEXT_PRIMARY);

        char di[8];
        sprintf(di, "%c %c %c",
                temp_initials[0], temp_initials[1], temp_initials[2]);
        vf_draw_string_centered(di, panel_cx, 440, 28, HUD_TEXT_CYAN);

    /* =========== STATE: UPGRADE SELECT (RELIC CHOICE) =========== */
    } else if (game_state == STATE_UPGRADE_SELECT) {
        static float upgrade_pulse = 0.0f;
        upgrade_pulse += 0.04f; /* visual-only timer */

        /* Background particles */
        ui_particle_drift(g_renderer, game_time, 30);

        /* Pulsing header */
        float sz = 24.0f + 2.0f * sinf(upgrade_pulse * 3.0f);
        SDL_Color surge_col = ui_pulse(HUD_GREEN, game_time, 2.0f, 0.3f);
        vf_draw_string_centered("CHRONICLE SURGE!",
                                SCREEN_WIDTH / 2.0f, 80, sz, surge_col);
        vf_draw_string_centered("CHOOSE YOUR RELIC",
                                SCREEN_WIDTH / 2.0f, 130, 14, HUD_TEXT_DIM);
        vf_draw_string_centered("WASD / ARROWS + ENTER",
                                SCREEN_WIDTH / 2.0f, 155, 11, HUD_TEXT_DARK);

        /* 3 relic cards in a row:
         * Card w=340, h=180, spacing=20
         * Total = 340*3 + 20*2 = 1060, start_x = (1280-1060)/2 = 110 */
        float card_w   = 340.0f;
        float card_h   = 180.0f;
        float card_gap = 20.0f;
        float card_sx  = 110.0f;
        float card_y   = 200.0f;

        for (int i = 0; i < 3; i++) {
            int   is_sel     = (i == selected_option);
            float cx         = card_sx + i * (card_w + card_gap);
            SDL_Color border = is_sel ? HUD_BORDER_ACTIVE : HUD_PANEL_DEEP;

            ui_panel_terminal(g_renderer, cx, card_y, card_w, card_h, border);

            if (is_sel) {
                ui_corner_brackets(g_renderer, cx, card_y, card_w, card_h,
                                   HUD_TEXT_GOLD, 16.0f);
            }

            float card_cx = cx + card_w / 2.0f;
            float name_sz = is_sel
                            ? (18.0f * (1.0f + 0.05f * sinf(upgrade_pulse * 5.0f + i)))
                            : 14.0f;
            SDL_Color nc  = is_sel ? HUD_TEXT_GOLD : HUD_TEXT_DIM;

            vf_draw_string_centered(upgrade_names[upgrade_options[i]],
                                    card_cx, 250, name_sz, nc);

            /* Separator line at y=280 */
            SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_renderer, 26, 26, 46, 120);
            SDL_RenderDrawLine(g_renderer, (int)(cx + 4), 280,
                               (int)(cx + card_w - 4), 280);

            /* Description */
            vf_draw_string_centered(upgrade_descs[upgrade_options[i]],
                                    card_cx, 295, 10, HUD_TEXT_DARK);

            if (is_sel) {
                SDL_Color dim_amber = HUD_AMBER; dim_amber.a = 160;
                vf_draw_string_centered("PRESS ENTER", card_cx, 355, 9, dim_amber);
            }
        }
    }
}

/* =========== CRT GLASS CURVATURE OVERLAY =========== */

/**
 * @brief Render a CRT glass faceplate overlay when settings_crt_curve is on.
 *
 * Draws 14 edge-darkening strips (quadratic alpha) and 40 per-corner glare
 * diagonals using plain SDL2 draw calls on g_renderer.  This runs entirely
 * in game.c so vector_graphics.c does not need to be touched.
 *
 * Call this after all game/HUD/menu layers, immediately before vg_present().
 */
static void render_crt_glass(void)
{
    const int W = SCREEN_WIDTH;
    const int H = SCREEN_HEIGHT;

    SDL_BlendMode old_blend;
    SDL_GetRenderDrawBlendMode(g_renderer, &old_blend);
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);

    /* --- Edge-darkening vignette: 14 strips, quadratic fade inward --- */
    for (int i = 0; i < 14; i++) {
        float t   = 1.0f - (float)i / 14.0f;
        Uint8 a   = (Uint8)(160.0f * t * t);   /* quadratic — heaviest at edge */
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, a);
        int d = i * 3;                          /* 3 px between strips → 42 px zone */
        SDL_Rect top = { 0,       d,       W, 1 };
        SDL_Rect bot = { 0,       H-1-d,   W, 1 };
        SDL_Rect lft = { d,       0,       1, H };
        SDL_Rect rgt = { W-1-d,   0,       1, H };
        SDL_RenderFillRect(g_renderer, &top);
        SDL_RenderFillRect(g_renderer, &bot);
        SDL_RenderFillRect(g_renderer, &lft);
        SDL_RenderFillRect(g_renderer, &rgt);
    }

    /* --- Corner glare: 40 diagonal lines fading outward from each corner --- */
    for (int i = 0; i < 40; i++) {
        Uint8 a = (Uint8)(28.0f * (1.0f - (float)i / 40.0f));
        SDL_SetRenderDrawColor(g_renderer, 220, 235, 255, a);
        int s = i * 4;
        SDL_RenderDrawLine(g_renderer, 0,     s,     s,     0    ); /* top-left    */
        SDL_RenderDrawLine(g_renderer, W-1-s, 0,     W-1,   s    ); /* top-right   */
        SDL_RenderDrawLine(g_renderer, 0,     H-1-s, s,     H-1  ); /* bottom-left */
        SDL_RenderDrawLine(g_renderer, W-1-s, H-1,   W-1,   H-1-s); /* bottom-right*/
    }

    SDL_SetRenderDrawBlendMode(g_renderer, old_blend);
}

/* =========== MAIN RENDER ENTRY POINT =========== */

/**
 * @brief Main render entry point.
 *
 * Applies the phosphor persistence fade, draws the world (entities,
 * overlays, HUD, minimap) when in gameplay states, then composites menus
 * on top before presenting.
 */
void game_render(void)
{
    /*
     * Persistence table mirrors the five settings_glow levels (0-4).
     * 1.0 = no fade (infinite persistence); lower = faster phosphor decay.
     */
    static const float persistence_levels[] = {1.0f, 0.95f, 0.90f, 0.82f, 0.70f};
    vg_apply_persistence(persistence_levels[settings_glow]);

    /* Advance visual-effect timers. Fixed-step accumulation per frame.
     * Frame-rate dependence is acceptable here -- these drive purely
     * aesthetic flicker; exact timing is not gameplay-relevant. */
    g_ghost_t      += 0.016f;
    g_flame_t      += 0.22f;
    g_shield_pulse += 0.05f;

    if (game_state == STATE_PLAYING  ||
        game_state == STATE_PAUSED   ||
        game_state == STATE_ATTRACT_GAMEPLAY) {

        /* World space -- entities, score floats, world-space overlays */
        vg_set_camera(camera_pos);
        render_entities();
        render_overlays();   /* resets camera to (0,0) internally */

        /* Screen space -- HUD and minimap */
        vg_set_camera((Vec2){0.0f, 0.0f});
        render_hud();
        render_minimap();
    }

    /* Menus and full-screen states -- always screen space */
    vg_set_camera((Vec2){0.0f, 0.0f});
    render_menus();
    vg_draw_hyperjump_flash(hyperjump_flash_timer);
    /* CRT glass curvature faceplate overlay — renders last, above all layers */
    if (settings_crt_curve) render_crt_glass();
    vg_present();
}
