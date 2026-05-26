#ifndef ENTITIES_H
#define ENTITIES_H

#include <SDL2/SDL.h>
#include "vector_graphics.h"

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
#define FUEL_REGEN_RADIUS       600.0f    /* distance from home at which fuel regenerates */
#define FUEL_PASSIVE_BASE_RATE    0.08f   /* idle reactor drain even without thrusting */
#define FUEL_RELIC_RATE           0.04f   /* extra drain per active relic/weapon system */
#define FUEL_DRIFT_PENALTY_TIME  10.0f   /* seconds adrift before emergency hull breach */
#define FUEL_DRIFT_RESERVE        0.20f   /* fuel fraction restored on emergency refill */
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
#define ZONE_HOME_RADIUS         1000.0f  /* home zone — safe area near station */
#define ZONE_INNER_RADIUS        3500.0f  /* inner belt — mid-difficulty band */
#define ZONE_VOID_RADIUS         8000.0f  /* void zone — outer ring */
#define ZONE_ABYSS_RADIUS       14000.0f  /* abyss outer boundary — beyond this is DEEP DRIFT (zone 4) */

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


#endif
