#include "game.h"
#include "vector_graphics.h"
#include "vector_font.h"
#include "audio.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

extern SDL_Window *g_window;

// Constants
#define MAX_ASTEROIDS 32
#define MAX_BULLETS 48
#define MAX_UFO_BULLETS 32
#define MAX_PARTICLES 128
#define MAX_ORBS 100
#define MAX_TRAIL_POINTS 40
#define PHOS_TRAIL_LEN 14
#define FRICTION 0.99f
#define ROTATION_SPEED 4.5f
#define THRUST_FORCE 350.0f
#define BULLET_SPEED 600.0f
#define BULLET_LIFE 1.2f

#define MAX_MINES 8
#define MAX_POWERUPS 4
#define MAX_NPC 4
#define MAX_STRUCTURE 6

typedef enum {
    DIFFICULTY_EASY,
    DIFFICULTY_NORMAL,
    DIFFICULTY_HARD,
    DIFFICULTY_ACE
} Difficulty;

typedef enum {
    STATE_TITLE,
    STATE_PLAYING,
    STATE_PAUSED,
    STATE_GAMEOVER,
    STATE_NEW_HIGHSCORE,
    STATE_UPGRADE_SELECT,
    STATE_HIGHSCORES,
    STATE_SETTINGS,
    STATE_KEYBINDS,
    STATE_ATTRACT_INSTRUCTIONS,
    STATE_ATTRACT_GAMEPLAY
} GameState;

// Cheat tracking
static int god_mode = 0;

typedef struct {
    char initials[4];
    int score;
} HighScore;

typedef struct {
    Vec2 pos;
    Vec2 vel;
    float angle;
    float rot_speed;
    float scale;
    float life;
    float max_life;
    SDL_Color color;
} Particle;

typedef struct {
    Vec2 pos;
    float alpha;
    float size;
} TrailPoint;

// Entities
typedef struct {
    int active;
    Vec2 pos;
    Vec2 vel;
    float angle;
    float rot_speed;
    float radius;
    int size; // 3 = Large, 2 = Medium, 1 = Small
    Line lines[16];
    int line_count;
    float bullet_cooldown;
    float invuln_timer;
    Vec2 trail_pos[PHOS_TRAIL_LEN];
    float trail_ang[PHOS_TRAIL_LEN];
    int trail_head;
} ShipEntity;

typedef struct {
    int active;
    Vec2 pos;
    Vec2 vel;
    float angle;
    float rot_speed;
    float radius;
    int size; // 3, 2, 1
    Line lines[16];
    int line_count;
    int has_shield; // 1 = shielded asteroid (Level >= 3)
    Vec2 trail_pos[PHOS_TRAIL_LEN];
    float trail_ang[PHOS_TRAIL_LEN];
    int trail_head;
} AsteroidEntity;

typedef struct {
    int active;
    Vec2 pos;
    Vec2 vel;
    float life;
    SDL_Color color;
    int bounces;
    int pierces;
    int is_homing;
    Vec2 trail_pos[PHOS_TRAIL_LEN];
    float trail_ang[PHOS_TRAIL_LEN];
    int trail_head;
} BulletEntity;

typedef struct {
    int active;
    Vec2 pos;
    Vec2 vel;
    int size;
    float radius;
    float angle;       /* Current facing angle (radians) -- used by kamikaze */
    Line lines[16];
    int line_count;
    float fire_timer;
    float change_dir_timer;
    float target_y;
    float scream_timer;
    Vec2 trail_pos[PHOS_TRAIL_LEN];
    float trail_ang[PHOS_TRAIL_LEN];
    int trail_head;
} UfoEntity;

typedef struct {
    int active;
    int type;        /* 0=home_station */
    Vec2 pos;
    float angle;
    float rot_speed;
    float radius;
} StructureEntity;

typedef struct {
    int active;
    int following;       /* 0=idle, 1=following player */
    Vec2 pos;
    Vec2 vel;
    float angle;
    float radius;
    float orbit_angle;   /* current orbit angle around player */
    float contact_timer; /* time player has been close */
} NpcEntity;

typedef struct {
    int active;
    Vec2 pos;
    Vec2 vel;
    float radius;
    float fuse_timer;
    float flash_timer;
    int flash_state;
} MineEntity;

typedef struct {
    int active;
    Vec2 pos;
    Vec2 vel;
    float radius;
    float angle;
    float rot_speed;
    float life; // expires after 8 seconds
} PowerupEntity;

typedef struct {
    int active;
    Vec2 pos;
    float radius;
    float angle1;
    float angle2;
    float pulse_timer;
    float spawn_timer;
} RiftEntity;

typedef struct {
    int active;
    Vec2 pos;
    Vec2 vel;
    int value;
    float life;
    Vec2 trail_pos[PHOS_TRAIL_LEN];
    float trail_ang[PHOS_TRAIL_LEN];
    int trail_head;
} OrbEntity;

typedef struct {
    int active;
    Vec2 pos;
    float radius;
    float max_radius;
    float thickness;
    float life;
} ShockwaveEntity;

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
    UPGRADE_COUNT // Always at the end
} UpgradeType;

typedef struct {
    int triple_shot;
    int max_bounces;
    float fire_cooldown_mult;
    float bullet_speed_mult;
    int shield_active;
    float speed_mult;
    int piercing;
    float size_mult;
    int homing;
    float magnet_radius;
    int rear_gun;
    int split_shot;
    float xp_mult;
    int mirror_image;
    int phase_shift;
    int thermal_hull;
    int singularity_displacer;
    int singularity_whip;
    int resonance_cascade;
} PlayerUpgrades;

// Game Global Variables
static GameState game_state = STATE_TITLE;
static Difficulty difficulty = DIFFICULTY_NORMAL;
static HighScore high_scores[5];
static int new_high_score_idx = -1;
static char temp_initials[4] = "A  ";
static int cur_initial_char = 0;

static ShipEntity player;
static AsteroidEntity asteroids[MAX_ASTEROIDS];
static BulletEntity bullets[MAX_BULLETS];
static BulletEntity ufo_bullets[MAX_UFO_BULLETS];
static UfoEntity ufo;
static Particle particles[MAX_PARTICLES];
static OrbEntity orbs[MAX_ORBS];
static float fire_cooldown_timer = 0.0f;
static int is_thrusting = 0;
static PlayerUpgrades player_upgrades;
static MineEntity mines[MAX_MINES];
static PowerupEntity powerups[MAX_POWERUPS];
static RiftEntity rift;
static RiftEntity player_rift;
static ShockwaveEntity shockwaves[4];
static StructureEntity structures[MAX_STRUCTURE];
static NpcEntity npcs[MAX_NPC];
static int   minimap_visible  = 1;
static int   player_zone      = 0;
static float player_dist_origin = 0.0f;
static float home_station_angle = 0.0f;
static UpgradeType upgrade_options[3];
static int selected_option = 0;
static float screen_shake_timer = 0.0f;
static float screen_shake_intensity = 0.0f;

static int menu_selection = 0; // 0: Start, 1: High Scores, 2: Settings
static int settings_volume = 100;
static int settings_fullscreen = 0;
static int settings_glow = 3; // 0=OFF, 1=LOW, 2=MED, 3=HIGH, 4=MAX

// Extended settings
static int settings_tab = 0;       // 0=VIDEO 1=AUDIO 2=GAMEPLAY 3=CONTROLS
static int settings_sfx_vol = 100;
static int settings_music_vol = 100;
static int settings_screen_shake = 1;
static int settings_show_fps = 0;
static int settings_mouse_aim = 1;
static int settings_crosshair_style = 1; // 0=OFF 1=CROSS 2=DOT
static int settings_autofire = 0;
static int settings_controller_deadzone = 1; // 0=LOW 1=MED 2=HIGH
static int settings_invert_y = 0;
static int settings_control_scheme = 0; // 0=ARCADE  1=TWIN_STICK

// FPS counter
static float fps_accum = 0.0f;
static int fps_frame_count = 0;
static int fps_display_val = 0;

// Controller
static SDL_GameController *g_controller = NULL;

// Keyboard binds
typedef enum {
    KB_ROTATE_LEFT=0, KB_ROTATE_RIGHT, KB_THRUST, KB_FIRE,
    KB_PAUSE, KB_HYPERSPACE,
    KB_ABILITY1, KB_ABILITY2, KB_ABILITY3, KB_ABILITY4,
    KB_MINIMAP,
    KB_COUNT
} KeyAction;
static SDL_Scancode keybinds[KB_COUNT] = {
    SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_UP, SDL_SCANCODE_SPACE,
    SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RETURN,
    SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
    SDL_SCANCODE_TAB
};
static const char* kb_action_names[KB_COUNT] = {
    "ROTATE LEFT", "ROTATE RIGHT", "THRUST", "FIRE",
    "PAUSE", "HYPERSPACE",
    "ABILITY 1", "ABILITY 2", "ABILITY 3", "ABILITY 4",
    "MINIMAP"
};
static int rebinding_action = -1; // -1 = not rebinding
static int keybind_selection = 0;
static int keybind_page = 0; // 0=keyboard  1=controller

// Controller button binds
typedef enum {
    CT_THRUST=0, CT_FIRE, CT_HYPERSPACE,
    CT_ABILITY1, CT_ABILITY2, CT_ABILITY3, CT_ABILITY4,
    CT_COUNT
} CtrlAction;
static SDL_GameControllerButton ctrl_binds[CT_COUNT] = {
    SDL_CONTROLLER_BUTTON_A,            // THRUST
    SDL_CONTROLLER_BUTTON_X,            // FIRE
    SDL_CONTROLLER_BUTTON_Y,            // HYPERSPACE
    SDL_CONTROLLER_BUTTON_B,            // ABILITY 1
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER, // ABILITY 2
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,// ABILITY 3
    SDL_CONTROLLER_BUTTON_MAX           // ABILITY 4 (unbound)
};
static const char* ct_action_names[CT_COUNT] = {
    "THRUST", "FIRE", "HYPERSPACE", "ABILITY 1", "ABILITY 2", "ABILITY 3", "ABILITY 4"
};
static int ctrl_rebinding_action = -1;

// Twin-stick fire direction override
static float twin_stick_fire_angle = 0.0f;
static int   twin_stick_fire_active = 0;

static float god_mode_msg_timer = 0.0f;
static int is_attract_ai = 0;
static float idle_timer = 0.0f;
static Vec2 mirror_pos = {0, 0};
static float mirror_angle = 0.0f;
static int mirror_active_flag = 0;
#define ATTRACT_DELAY 10.0f

static int score = 0;
static int lives = 3;
static int level = 1;
static int total_asteroids_destroyed = 0;

// Combo system
static int combo_count = 0;
static float combo_timer = 0.0f;
#define COMBO_WINDOW 1.8f

// Score pop floaters
typedef struct { float x, y, vy; int value; float life; int active; } ScoreFloat;
#define MAX_SCORE_FLOATS 24
static ScoreFloat score_floats[MAX_SCORE_FLOATS];



// XP bar flash
static float xp_flash_timer = 0.0f;

// Edge flash – warn player they're near wrapping
static float edge_flash_timer = 0.0f;

// Near-miss adrenaline XP
static float near_miss_cooldown = 0.0f;

static Vec2 camera_pos = {0.0f, 0.0f};
static int wave_asteroids_destroyed = 0;
static int wave_cleared_pending = 0;

static int player_level = 1;
static int player_xp = 0;
static int xp_threshold = 100;

static const char* upgrade_names[] = {
    "TRISKELION BURST", "VOID RICOCHET", "ETHER SHROUD", "OVERCLOCK", "ICHOROUS ROUNDS", "AUTODYNE BOOST",
    "SMALL FORM", "SEEKER ICHORS", "PALE SIGHT", "CORRUPTION ROUNDS", "TEMPORAL FOLD", "ORB MAGNET", "SOLAR FURY",
    "WRAITH TWIN", "AFT CANNON", "ORBIT MINE", "CRIT CHANCE", "RESTORED VIGOR", "BANE TRAIL", "FISSION SHOT",
    "VORTEX GRENADE", "PHASE SHIFT", "AUTO TURRET", "NOVA SHELL", "CHRONICLE BOOST", "OSSIFIED HULL",
    "THERMAL HULL", "RIFT DISPLACER", "BANE WHIP", "RESONANCE CASCADE"
};

static const char* difficulty_names[] = {
    "EASY", "NORMAL", "HARD", "ACE"
};

static const char* upgrade_descs[] = {
    "Three-bolt spread from the prow", "Bolts rebound off the void's edge", "One absorption per life", "30% faster discharge",
    "Bolts pierce through Void Stones", "20% faster Autodyne thrust", "Reduce ship collision radius", "Bolts seek the nearest stone",
    "Reveal hidden Remnant threats", "Bolts corrode on contact", "Briefly slow the drift of time", "Draw chronicle orbs to you",
    "Rage boils when hull is intact", "A phantom twin follows your path", "Discharge from the aft thruster", "Mines orbit the hull",
    "Random bolts strike twice", "The Autarch grants another life", "Thruster wash scorches Void Stones", "Bolts split on first impact",
    "Pulls Void Stones into ruin", "Pass through one killing blow", "An ancient turret follows you", "Hull detonates on impact",
    "Harvest 50% more chronicle", "Ancient plating resists damage",
    "Ram Void Stones at full thrust", "Double-tap thrust to rift-jump", "Thruster trail scorches the drift", "Bolts unleash shockwaves"
};


static float level_start_timer = 2.0f;
static float ufo_spawn_timer = 15.0f;
static float beat_timer = 1.0f;
static int cur_beat = 0;

static float thrust_timer = 0.0f;

// Preset Ship Lines
static Line ship_lines[] = {
    {{0.0f, -12.0f}, {-8.0f, 10.0f}},
    {{-8.0f, 10.0f}, {-3.0f, 6.0f}},
    {{-3.0f, 6.0f}, {3.0f, 6.0f}},
    {{3.0f, 6.0f}, {8.0f, 10.0f}},
    {{8.0f, 10.0f}, {0.0f, -12.0f}}
};




// Preset UFO Lines
static Line ufo_lines[] = {
    {{-16.0f, 0.0f}, {16.0f, 0.0f}},
    {{-16.0f, 0.0f}, {-8.0f, 5.0f}},
    {{-8.0f, 5.0f}, {8.0f, 5.0f}},
    {{8.0f, 5.0f}, {16.0f, 0.0f}},
    {{-16.0f, 0.0f}, {-8.0f, -5.0f}},
    {{-8.0f, -5.0f}, {8.0f, -5.0f}},
    {{8.0f, -5.0f}, {16.0f, 0.0f}},
    {{-8.0f, -5.0f}, {-4.0f, -10.0f}},
    {{-4.0f, -10.0f}, {4.0f, -10.0f}},
    {{4.0f, -10.0f}, {8.0f, -6.0f}}
};

static Line kamikaze_lines[] = {
    {{12.0f, 0.0f}, {-8.0f, -8.0f}},
    {{-8.0f, -8.0f}, {-4.0f, 0.0f}},
    {{-4.0f, 0.0f}, {-8.0f, 8.0f}},
    {{-8.0f, 8.0f}, {12.0f, 0.0f}}
};

static Line bomber_lines[] = {
    {{-20.0f, 0.0f}, {20.0f, 0.0f}},
    {{-20.0f, 0.0f}, {-10.0f, 7.0f}},
    {{-10.0f, 7.0f}, {10.0f, 7.0f}},
    {{10.0f, 7.0f}, {20.0f, 0.0f}},
    {{-20.0f, 0.0f}, {-12.0f, -6.0f}},
    {{-12.0f, -6.0f}, {12.0f, -6.0f}},
    {{12.0f, -6.0f}, {20.0f, 0.0f}},
    {{-12.0f, -6.0f}, {-6.0f, -12.0f}},
    {{-6.0f, -12.0f}, {6.0f, -12.0f}},
    {{6.0f, -12.0f}, {12.0f, -6.0f}}
};

/* Home space station (octagon + cross + inner box) */
static Line home_station_lines[] = {
    {{-22.0f,-52.0f},{22.0f,-52.0f}},
    {{22.0f,-52.0f},{52.0f,-22.0f}},
    {{52.0f,-22.0f},{52.0f,22.0f}},
    {{52.0f,22.0f},{22.0f,52.0f}},
    {{22.0f,52.0f},{-22.0f,52.0f}},
    {{-22.0f,52.0f},{-52.0f,22.0f}},
    {{-52.0f,22.0f},{-52.0f,-22.0f}},
    {{-52.0f,-22.0f},{-22.0f,-52.0f}},
    {{-52.0f,0.0f},{-24.0f,0.0f}},
    {{52.0f,0.0f},{24.0f,0.0f}},
    {{0.0f,-52.0f},{0.0f,-24.0f}},
    {{0.0f,52.0f},{0.0f,24.0f}},
    {{-12.0f,-12.0f},{12.0f,-12.0f}},
    {{12.0f,-12.0f},{12.0f,12.0f}},
    {{12.0f,12.0f},{-12.0f,12.0f}},
    {{-12.0f,12.0f},{-12.0f,-12.0f}}
};

/* Eldritch Tendril (Zone 2+ enemy) — asymmetric tentacle shape */
static Line eldritch_lines[] = {
    {{0.0f,-16.0f},{10.0f,-6.0f}},
    {{10.0f,-6.0f},{16.0f,4.0f}},
    {{16.0f,4.0f},{8.0f,16.0f}},
    {{0.0f,-16.0f},{-10.0f,-6.0f}},
    {{-10.0f,-6.0f},{-16.0f,4.0f}},
    {{-16.0f,4.0f},{-8.0f,16.0f}},
    {{8.0f,16.0f},{0.0f,10.0f}},
    {{-8.0f,16.0f},{0.0f,10.0f}},
    {{0.0f,-16.0f},{0.0f,-8.0f}},
    {{-4.0f,0.0f},{4.0f,0.0f}}
};

/* Daemon Sigil (Zone 3+ enemy) — angular chaos symbol */
static Line daemon_lines[] = {
    {{0.0f,-20.0f},{12.0f,-8.0f}},
    {{12.0f,-8.0f},{20.0f,0.0f}},
    {{20.0f,0.0f},{12.0f,12.0f}},
    {{12.0f,12.0f},{0.0f,20.0f}},
    {{0.0f,20.0f},{-12.0f,12.0f}},
    {{-12.0f,12.0f},{-20.0f,0.0f}},
    {{-20.0f,0.0f},{-12.0f,-8.0f}},
    {{-12.0f,-8.0f},{0.0f,-20.0f}},
    {{-8.0f,-8.0f},{8.0f,-8.0f}},
    {{8.0f,-8.0f},{8.0f,8.0f}},
    {{8.0f,8.0f},{-8.0f,8.0f}},
    {{-8.0f,8.0f},{-8.0f,-8.0f}},
    {{0.0f,-12.0f},{0.0f,12.0f}},
    {{-12.0f,0.0f},{12.0f,0.0f}},
    {{-6.0f,-6.0f},{6.0f,6.0f}},
    {{6.0f,-6.0f},{-6.0f,6.0f}}
};

/* Friendly NPC shield drone — small diamond with orbit ring hint */
static Line npc_drone_lines[] = {
    {{0.0f,-10.0f},{10.0f,0.0f}},
    {{10.0f,0.0f},{0.0f,10.0f}},
    {{0.0f,10.0f},{-10.0f,0.0f}},
    {{-10.0f,0.0f},{0.0f,-10.0f}},
    {{-5.0f,-5.0f},{5.0f,-5.0f}},
    {{5.0f,-5.0f},{5.0f,5.0f}},
    {{5.0f,5.0f},{-5.0f,5.0f}}
};

// Helpers
typedef struct {
    int wrapped;
    Vec2 entry_pos;
    Vec2 exit_pos;
} WrapEvent;

static WrapEvent wrap_position_ext(Vec2 *pos, float padding) {
    WrapEvent ev = {0, *pos, *pos};
    if (pos->x < -padding) { pos->x = SCREEN_WIDTH + padding; ev.wrapped = 1; }
    if (pos->x > SCREEN_WIDTH + padding) { pos->x = -padding; ev.wrapped = 1; }
    if (pos->y < -padding) { pos->y = SCREEN_HEIGHT + padding; ev.wrapped = 1; }
    if (pos->y > SCREEN_HEIGHT + padding) { pos->y = -padding; ev.wrapped = 1; }
    ev.exit_pos = *pos;
    return ev;
}

static Vec2 calculate_external_forces(Vec2 pos) {
    Vec2 force = {0.0f, 0.0f};
    
    if (rift.active) {
        float dx = rift.pos.x - pos.x;
        float dy = rift.pos.y - pos.y;
        float dist_sq = dx*dx + dy*dy;
        if (dist_sq > 1.0f && dist_sq < 800.0f * 800.0f) {
            float dist = sqrtf(dist_sq);
            float pull_str = 200000.0f / dist_sq;
            if (pull_str > 400.0f) pull_str = 400.0f;
            force.x += (dx / dist) * pull_str;
            force.y += (dy / dist) * pull_str;
        }
    }
    
    if (player_rift.active) {
        float dx = player_rift.pos.x - pos.x;
        float dy = player_rift.pos.y - pos.y;
        float dist_sq = dx*dx + dy*dy;
        if (dist_sq > 1.0f && dist_sq < 600.0f * 600.0f) {
            float dist = sqrtf(dist_sq);
            float pull_str = 300000.0f / dist_sq;
            if (pull_str > 500.0f) pull_str = 500.0f;
            force.x += (dx / dist) * pull_str;
            force.y += (dy / dist) * pull_str;
        }
    }
    
    if (ufo.active && ufo.size == 5) {
        float dx = ufo.pos.x - pos.x;
        float dy = ufo.pos.y - pos.y;
        float dist_sq = dx*dx + dy*dy;
        if (dist_sq > 1.0f && dist_sq < 1000.0f * 1000.0f) {
            float dist = sqrtf(dist_sq);
            float pull_str = 150000.0f / dist_sq;
            if (pull_str > 250.0f) pull_str = 250.0f;
            force.x += (dx / dist) * pull_str;
            force.y += (dy / dist) * pull_str;
        }
    }
    
    return force;
}

static WrapEvent update_physics(Vec2 *pos, Vec2 *vel, float delta_time, float padding, float friction) {
    Vec2 ext_f = calculate_external_forces(*pos);
    vel->x += ext_f.x * delta_time;
    vel->y += ext_f.y * delta_time;
    
    if (friction < 1.0f) {
        vel->x *= powf(friction, delta_time * 60.0f);
        vel->y *= powf(friction, delta_time * 60.0f);
    }
    
    pos->x += vel->x * delta_time;
    pos->y += vel->y * delta_time;
    
    return wrap_position_ext(pos, padding);
}

// Forward declarations
static void spawn_particles(Vec2 pos, int count, SDL_Color color);
static void spawn_asteroid(Vec2 pos, int size);
static void spawn_orb(Vec2 pos, int value);

// --- Component Hooks ---
static void on_weapon_fire(Vec2 pos, Vec2 vel) {
    // Resonance Cascade could hook here or in bullet collision
}

// Return 1 to cancel default collision logic
static int on_collision(int is_player_asteroid, int asteroid_idx) {
    if (is_player_asteroid && player_upgrades.thermal_hull) {
        float speed_sq = player.vel.x * player.vel.x + player.vel.y * player.vel.y;
        if (speed_sq > 120.0f * 120.0f) {
            asteroids[asteroid_idx].active = 0;
            wave_asteroids_destroyed++;
            int next_size = asteroids[asteroid_idx].size - 1;
            spawn_particles(asteroids[asteroid_idx].pos, 25, (SDL_Color){255, 100, 50, 255});
            if (next_size >= 1) {
                spawn_asteroid(asteroids[asteroid_idx].pos, next_size);
                spawn_asteroid(asteroids[asteroid_idx].pos, next_size);
            }
            int orb_val = (asteroids[asteroid_idx].size == 3) ? 20 : ((asteroids[asteroid_idx].size == 2) ? 10 : 4);
            spawn_orb(asteroids[asteroid_idx].pos, orb_val);
            score += 200;
            audio_play(SFX_EXPLOSION_LG);
            
            player.vel.x *= -0.5f;
            player.vel.y *= -0.5f;
            player.invuln_timer = 0.5f;
            return 1;
        }
    }
    return 0;
}

static void on_thrust(Vec2 pos, Vec2 vel) {
    if (player_upgrades.singularity_whip) {
        // Handled via the trail system doing damage in game_update
    }
}

static void on_screen_wrap(WrapEvent event) {
    if (player_upgrades.singularity_displacer) {
        player_rift.active = 1;
        player_rift.pos = event.entry_pos;
        player_rift.radius = 20.0f;
        player_rift.pulse_timer = 0.0f;
        player_rift.spawn_timer = 3.0f; // Lasts 3 seconds
    }
}

static int check_collision(Vec2 p1, float r1, Vec2 p2, float r2) {
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    float dist_sq = dx*dx + dy*dy;
    float sum_r = r1 + r2;
    return dist_sq < (sum_r * sum_r);
}

static void load_high_scores() {
    FILE *f = fopen("highscores.dat", "rb");
    if (f) {
        fread(high_scores, sizeof(HighScore), 5, f);
        fclose(f);
    } else {
        // Default high scores
        strcpy(high_scores[0].initials, "PDR"); high_scores[0].score = 10000;
        strcpy(high_scores[1].initials, "AUT"); high_scores[1].score = 8000;
        strcpy(high_scores[2].initials, "SIB"); high_scores[2].score = 6000;
        strcpy(high_scores[3].initials, "VEX"); high_scores[3].score = 4000;
        strcpy(high_scores[4].initials, "ORM"); high_scores[4].score = 2000;
    }
}

static void save_high_scores() {
    FILE *f = fopen("highscores.dat", "wb");
    if (f) {
        fwrite(high_scores, sizeof(HighScore), 5, f);
        fclose(f);
    }
}

static void save_game() {
    FILE *f = fopen("savegame.dat", "wb");
    if (!f) return;

    fwrite(&score, sizeof(int), 1, f);
    fwrite(&lives, sizeof(int), 1, f);
    fwrite(&level, sizeof(int), 1, f);
    fwrite(&player_level, sizeof(int), 1, f);
    fwrite(&player_xp, sizeof(int), 1, f);
    fwrite(&xp_threshold, sizeof(int), 1, f);
    fwrite(&player_upgrades, sizeof(PlayerUpgrades), 1, f);
    fwrite(&player.pos, sizeof(Vec2), 1, f);
    fwrite(&difficulty, sizeof(Difficulty), 1, f);

    fclose(f);
}

static void load_game() {
    FILE *f = fopen("savegame.dat", "rb");
    if (!f) return;

    fread(&score, sizeof(int), 1, f);
    fread(&lives, sizeof(int), 1, f);
    fread(&level, sizeof(int), 1, f);
    fread(&player_level, sizeof(int), 1, f);
    fread(&player_xp, sizeof(int), 1, f);
    fread(&xp_threshold, sizeof(int), 1, f);
    fread(&player_upgrades, sizeof(PlayerUpgrades), 1, f);
    fread(&player.pos, sizeof(Vec2), 1, f);
    fread(&difficulty, sizeof(Difficulty), 1, f);

    fclose(f);
    
    player.active = 1;
    game_state = STATE_PLAYING;
}

static float distance_sq(Vec2 p1, Vec2 p2) {
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return dx*dx + dy*dy;
}



static void spawn_particles(Vec2 pos, int count, SDL_Color color) {
    int spawned = 0;
    for (int i = 0; i < MAX_PARTICLES && spawned < count; i++) {
        if (particles[i].life <= 0.0f) {
            particles[i].pos = pos;
            float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float speed = 50.0f + 100.0f * ((float)rand() / RAND_MAX);
            particles[i].vel.x = cosf(angle) * speed;
            particles[i].vel.y = sinf(angle) * speed;
            particles[i].angle = angle;
            particles[i].rot_speed = -5.0f + 10.0f * ((float)rand() / RAND_MAX);
            particles[i].scale = 2.0f + 5.0f * ((float)rand() / RAND_MAX);
            particles[i].max_life = 0.5f + 0.8f * ((float)rand() / RAND_MAX);
            particles[i].life = particles[i].max_life;
            particles[i].color = color;
            spawned++;
        }
    }
}

static void init_asteroid_shape(AsteroidEntity *ast, int size) {
    ast->size = size;
    ast->radius = (size == 3) ? 35.0f : ((size == 2) ? 18.0f : 9.0f);
    
    int num_points = 8 + rand() % 5; // 8 to 12 vertices
    ast->line_count = num_points;
    
    Vec2 points[16];
    for (int i = 0; i < num_points; i++) {
        float angle = i * 2.0f * (float)M_PI / num_points;
        float r = ast->radius * (0.8f + 0.4f * ((float)rand() / RAND_MAX));
        points[i].x = r * cosf(angle);
        points[i].y = r * sinf(angle);
    }
    
    for (int i = 0; i < num_points; i++) {
        ast->lines[i].p1 = points[i];
        ast->lines[i].p2 = points[(i + 1) % num_points];
    }
}

static void spawn_asteroid(Vec2 pos, int size) {
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) {
            asteroids[i].active = 1;
            asteroids[i].pos = pos;

            float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float speed_scale = 1.0f + (difficulty * 0.3f);
            float speed = (30.0f + 50.0f * (4 - size) + 10.0f * level) * speed_scale;
            asteroids[i].vel.x = cosf(angle) * speed;
            asteroids[i].vel.y = sinf(angle) * speed;
            asteroids[i].angle = angle;
            asteroids[i].rot_speed = (-1.5f + 3.0f * ((float)rand() / RAND_MAX)) * speed_scale;
            init_asteroid_shape(&asteroids[i], size);
            asteroids[i].has_shield = (level >= 3 && rand() % 100 < (15 + difficulty * 10));
            break;
        }
    }
}
static void reset_player() {
    player.active = 1;
    /* Respawn at random position within the current visible screen area */
    float margin = 80.0f;
    player.pos.x = camera_pos.x + margin + ((float)rand() / RAND_MAX) * (SCREEN_WIDTH  - margin * 2.0f);
    player.pos.y = camera_pos.y + margin + ((float)rand() / RAND_MAX) * (SCREEN_HEIGHT - margin * 2.0f);
    player.vel = (Vec2){0.0f, 0.0f};
    player.angle = 0.0f;
    player.invuln_timer = 2.0f; /* 2 seconds invulnerability */
}

/* Zone 0=Home(<1000) Zone1=Belt(<3500) Zone2=Void(<8000) Zone3=Abyss */
static int get_zone(Vec2 pos) {
    float d = sqrtf(pos.x * pos.x + pos.y * pos.y);
    if (d < 1000.0f) return 0;
    if (d < 3500.0f) return 1;
    if (d < 8000.0f) return 2;
    return 3;
}

/* Spawn a friendly shield-drone NPC at a given world position */
static void spawn_npc(Vec2 pos) {
    for (int i = 0; i < MAX_NPC; i++) {
        if (!npcs[i].active) {
            npcs[i].active = 1;
            npcs[i].following = 0;
            npcs[i].pos = pos;
            npcs[i].vel = (Vec2){0.0f, 0.0f};
            npcs[i].angle = 0.0f;
            npcs[i].radius = 10.0f;
            npcs[i].orbit_angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            npcs[i].contact_timer = 0.0f;
            return;
        }
    }
}

/* Init home station + zone NPCs */
static void init_home_area() {
    /* Home station structure at world origin */
    structures[0].active = 1;
    structures[0].type   = 0;
    structures[0].pos    = (Vec2){0.0f, 0.0f};
    structures[0].radius = 55.0f;
    structures[0].rot_speed = 0.004f;
    structures[0].angle  = 0.0f;
    /* Spawn a couple of friendly drones near home */
    Vec2 d1 = { 200.0f + ((float)rand()/RAND_MAX)*100.0f,  80.0f};
    Vec2 d2 = {-180.0f - ((float)rand()/RAND_MAX)*100.0f, -90.0f};
    spawn_npc(d1);
    spawn_npc(d2);
}

static void spawn_ufo() {
    ufo.active = 1;
    
    /* Zone + level based enemy type selection */
    int r = rand() % 100;
    if (player_zone >= 3 && r < 15) {
        ufo.size = 7;  /* Daemon Sigil (Zone 3+) */
        ufo.radius = 22.0f;
    } else if (player_zone >= 2 && r < 30) {
        ufo.size = 6;  /* Eldritch Tendril (Zone 2+) */
        ufo.radius = 18.0f;
    } else if (level >= 4 && r < 45) {
        ufo.size = 5;  /* Eye of the Void */
        ufo.radius = 24.0f;
    } else if (level >= 3 && r < 60) {
        ufo.size = 4;  /* Bomber */
        ufo.radius = 20.0f;
    } else if (level >= 2 && r < 75) {
        ufo.size = 3;  /* Kamikaze */
        ufo.radius = 12.0f;
    } else {
        ufo.size = (rand() % 100 < 40 + level * 5) ? 1 : 2;
        ufo.radius = (ufo.size == 2) ? 16.0f : 8.0f;
    }
    
    // Choose side to spawn
    if (rand() % 2 == 0) {
        ufo.pos.x = -ufo.radius;
        ufo.vel.x = 100.0f + 25.0f * level;
    } else {
        ufo.pos.x = SCREEN_WIDTH + ufo.radius;
        ufo.vel.x = -(100.0f + 25.0f * level);
    }
    
    /* Speed adjustments per type */
    if (ufo.size == 3) {
        ufo.vel.x *= 1.4f;
    } else if (ufo.size == 4) {
        ufo.vel.x *= 0.6f;
    } else if (ufo.size == 6) {
        ufo.vel.x *= 0.5f; /* Tendril drifts slow */
    } else if (ufo.size == 7) {
        ufo.vel.x *= 0.7f; /* Daemon moderate speed */
    }
    
    ufo.pos.y = 80.0f + ((float)rand() / RAND_MAX) * (SCREEN_HEIGHT - 160.0f);
    if (ufo.size == 4) {
        ufo.pos.y = (rand() % 2 == 0) ? 50.0f : SCREEN_HEIGHT - 50.0f;
        ufo.target_y = ufo.pos.y;
    } else {
        ufo.target_y = ufo.pos.y;
    }
    ufo.vel.y = 0.0f;
    ufo.fire_timer = 1.0f;
    ufo.change_dir_timer = 1.0f;
    ufo.scream_timer = 2.0f;
    
    // Setup model lines
    if (ufo.size == 3) {
        ufo.line_count = sizeof(kamikaze_lines) / sizeof(Line);
        for (int i = 0; i < ufo.line_count; i++) {
            ufo.lines[i] = kamikaze_lines[i];
        }
    } else if (ufo.size == 4) {
        ufo.line_count = sizeof(bomber_lines) / sizeof(Line);
        for (int i = 0; i < ufo.line_count; i++) {
            ufo.lines[i] = bomber_lines[i];
        }
    } else if (ufo.size == 5) {
        ufo.line_count = 0; /* custom procedural rendering */
    } else if (ufo.size == 6) {
        ufo.line_count = sizeof(eldritch_lines) / sizeof(Line);
        for (int i = 0; i < ufo.line_count; i++) ufo.lines[i] = eldritch_lines[i];
    } else if (ufo.size == 7) {
        ufo.line_count = sizeof(daemon_lines) / sizeof(Line);
        for (int i = 0; i < ufo.line_count; i++) ufo.lines[i] = daemon_lines[i];
    } else {
        ufo.line_count = sizeof(ufo_lines) / sizeof(Line);
        for (int i = 0; i < ufo.line_count; i++) ufo.lines[i] = ufo_lines[i];
    }
    
    audio_play(SFX_UFO_LOOP);
}

static void trigger_hyperspace() {
    if (!player.active) return;
    
    spawn_particles(player.pos, 10, (SDL_Color){150, 180, 255, 255});
    audio_play(SFX_EXPLOSION_SM);
    
    // Instant boom chance (classic 1.5% chance)
    if (((float)rand() / RAND_MAX) < 0.015f) {
        player.active = 0;
        audio_play(SFX_EXPLOSION_LG);
        spawn_particles(player.pos, 30, (SDL_Color){255, 100, 100, 255});
        lives--;
        if (lives <= 0) {
            game_state = STATE_GAMEOVER;
            audio_stop(SFX_THRUST);
            audio_stop(SFX_UFO_LOOP);
        }
        return;
    }
    
    /* Hyperspace lands within current visible screen area (not world origin) */
    float margin = 80.0f;
    player.pos.x = camera_pos.x + margin + ((float)rand() / RAND_MAX) * (SCREEN_WIDTH  - margin * 2.0f);
    player.pos.y = camera_pos.y + margin + ((float)rand() / RAND_MAX) * (SCREEN_HEIGHT - margin * 2.0f);
    player.vel = (Vec2){0.0f, 0.0f};
    player.invuln_timer = 0.5f;
}

static void spawn_asteroid(Vec2 pos, int size);

static void spawn_orb(Vec2 pos, int value) {
    for (int i = 0; i < MAX_ORBS; i++) {
        if (!orbs[i].active) {
            orbs[i].active = 1;
            orbs[i].pos = pos;
            float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float speed = 20.0f + 30.0f * ((float)rand() / RAND_MAX);
            orbs[i].vel.x = cosf(angle) * speed;
            orbs[i].vel.y = sinf(angle) * speed;
            orbs[i].value = value;
            orbs[i].life = 10.0f;
            break;
        }
    }
}

static void spawn_upgrade_options() {
    audio_stop(SFX_THRUST);
    audio_stop(SFX_UFO_LOOP);
    UpgradeType all_upgrades[30];
    for (int i = 0; i < 30; i++) all_upgrades[i] = (UpgradeType)i;
    int total_available = 30;

    // Shuffle and pick 3
    for (int i = 0; i < 3; i++) {
        int idx = i + rand() % (total_available - i);
        UpgradeType temp = all_upgrades[i];
        all_upgrades[i] = all_upgrades[idx];
        all_upgrades[idx] = temp;
        upgrade_options[i] = all_upgrades[i];
    }
    selected_option = 0;
}

static void apply_upgrade(UpgradeType type) {
    switch (type) {
        case UPGRADE_TRIPLE_SHOT: player_upgrades.triple_shot = 1; break;
        case UPGRADE_BOUNCE: player_upgrades.max_bounces += 1; break;
        case UPGRADE_SHIELD: player_upgrades.shield_active = 1; break;
        case UPGRADE_RAPID_FIRE: player_upgrades.fire_cooldown_mult *= 0.7f; break;
        case UPGRADE_PIERCING: player_upgrades.piercing = 1; break;
        case UPGRADE_SPEED: player_upgrades.speed_mult *= 1.2f; break;
        case UPGRADE_SIZE_DOWN: player_upgrades.size_mult *= 0.8f; player.radius *= 0.8f; break;
        case UPGRADE_HOMING: player_upgrades.homing = 1; break;
        case UPGRADE_ORB_MAGNET: player_upgrades.magnet_radius += 50.0f; break;
        case UPGRADE_REAR_GUN: player_upgrades.rear_gun = 1; break;
        case UPGRADE_SPLIT_SHOT: player_upgrades.split_shot = 1; break;
        case UPGRADE_XP_BOOST: player_upgrades.xp_mult += 0.5f; break;
        case UPGRADE_HEALTH_BOOST: lives++; break;
        case UPGRADE_MIRROR_IMAGE: player_upgrades.mirror_image = 1; break;
        case UPGRADE_PHASE_SHIFT: player_upgrades.phase_shift = 1; break;
        case UPGRADE_THERMAL_HULL: player_upgrades.thermal_hull = 1; break;
        case UPGRADE_SINGULARITY_DISPLACER: player_upgrades.singularity_displacer = 1; break;
        case UPGRADE_SINGULARITY_WHIP: player_upgrades.singularity_whip = 1; break;
        case UPGRADE_RESONANCE_CASCADE: player_upgrades.resonance_cascade = 1; break;
        // Remaining 10+ buffs could be implemented with more complex logic, 
        // for now we'll ensure the player_upgrades struct covers the main ones requested.
        default: break; 
    }
}

static void start_next_level() {
    level++;
    int count = 4 + level + (difficulty * 2); 
    if (count > MAX_ASTEROIDS - 4) count = MAX_ASTEROIDS - 4;

    // Deactivate all bullets & particles
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = 0;
    for (int i = 0; i < MAX_UFO_BULLETS; i++) ufo_bullets[i].active = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].life = 0.0f;

    // Deactivate mines and powerups
    for (int i = 0; i < MAX_MINES; i++) mines[i].active = 0;
    for (int i = 0; i < MAX_POWERUPS; i++) powerups[i].active = 0;

    ufo.active = 0;
    audio_stop(SFX_UFO_LOOP);

    // Spawn new asteroids at distance from player
    for (int i = 0; i < count; i++) {
        float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
        float dist = 450.0f + ((float)rand() / RAND_MAX) * 300.0f;
        Vec2 pos = {
            player.pos.x + cosf(angle) * dist,
            player.pos.y + sinf(angle) * dist
        };
        spawn_asteroid(pos, 3);
    }
    // Spawn Anomalous Void Rift in level >= 4
    if (level >= 4) {
        rift.active = 1;
        float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
        float dist = 450.0f + ((float)rand() / RAND_MAX) * 300.0f;
        rift.pos.x = player.pos.x + cosf(angle) * dist;
        rift.pos.y = player.pos.y + sinf(angle) * dist;
        rift.radius = 35.0f;
        rift.angle1 = 0.0f;
        rift.angle2 = 0.0f;
        rift.pulse_timer = 0.0f;
        rift.spawn_timer = 5.0f;
    } else {
        rift.active = 0;
    }
    
    level_start_timer = 1.5f;
    ufo_spawn_timer = 15.0f + ((float)rand() / RAND_MAX) * 15.0f;
}

static void start_new_game() {
    score = 0;
    lives = 3;
    level = 0; // Will become 1 when we call start_next_level
    total_asteroids_destroyed = 0;
    player_level = 1;
    player_xp = 0;
    xp_threshold = 100;

    // Clear all
    for (int i = 0; i < MAX_ASTEROIDS; i++) asteroids[i].active = 0;
    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = 0;
    for (int i = 0; i < MAX_UFO_BULLETS; i++) ufo_bullets[i].active = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) particles[i].life = 0.0f;
    for (int i = 0; i < MAX_ORBS; i++) orbs[i].active = 0;
    for (int i = 0; i < MAX_SCORE_FLOATS; i++) score_floats[i].active = 0;
    combo_count = 0; combo_timer = 0.0f;
    ufo.active = 0;
    wave_asteroids_destroyed = 0;
    wave_cleared_pending = 0;
    camera_pos = (Vec2){0.0f, 0.0f};
    /* Init zone structures and NPCs */
    for (int i = 0; i < MAX_STRUCTURE; i++) structures[i].active = 0;
    for (int i = 0; i < MAX_NPC; i++) npcs[i].active = 0;
    init_home_area();

    // Reset player upgrades
    player_upgrades.triple_shot = 0;
    player_upgrades.max_bounces = 0;
    player_upgrades.fire_cooldown_mult = 1.0f;
    player_upgrades.bullet_speed_mult = 1.0f;
    player_upgrades.shield_active = 0;
    player_upgrades.speed_mult = 1.0f;
    player_upgrades.piercing = 0;
    player_upgrades.size_mult = 1.0f;
    player_upgrades.homing = 0;
    player_upgrades.magnet_radius = 60.0f;
    player_upgrades.rear_gun = 0;
    player_upgrades.split_shot = 0;
    player_upgrades.xp_mult = 1.0f;
    player_upgrades.mirror_image = 0;
    player_upgrades.phase_shift = 0;
    mirror_active_flag = 0;

    // Clear mines, powerups, rift, screen shake
    for (int i = 0; i < MAX_MINES; i++) mines[i].active = 0;
    for (int i = 0; i < MAX_POWERUPS; i++) powerups[i].active = 0;
    rift.active = 0;
    screen_shake_timer = 0.0f;
    screen_shake_intensity = 0.0f;
    vg_set_shake(0, 0);

    audio_stop(SFX_THRUST);
    audio_stop(SFX_UFO_LOOP);

    reset_player();
    start_next_level();
    game_state = STATE_PLAYING;
}

void game_init() {
    load_high_scores();
    vf_init();

    // Setup player static shape
    player.line_count = sizeof(ship_lines) / sizeof(Line);
    for (int i = 0; i < player.line_count; i++) {
        player.lines[i] = ship_lines[i];
    }
    player.radius = 8.0f;
    player.active = 0;
    player.trail_head = 0;
    for (int i = 0; i < PHOS_TRAIL_LEN; i++) {
        player.trail_pos[i] = (Vec2){0.0f, 0.0f};
        player.trail_ang[i] = 0.0f;
    }
}

void game_set_paused(int paused) {
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

void game_handle_input(SDL_Event *event) {
    idle_timer = 0.0f; // Reset idle timer on any input
    if (game_state == STATE_ATTRACT_INSTRUCTIONS || game_state == STATE_ATTRACT_GAMEPLAY) {
        game_state = STATE_TITLE;
        return;
    }

    if (event->type == SDL_CONTROLLERDEVICEADDED) {
        if (!g_controller) g_controller = SDL_GameControllerOpen(event->cdevice.which);
        return;
    }
    if (event->type == SDL_CONTROLLERDEVICEREMOVED) {
        if (g_controller) { SDL_GameControllerClose(g_controller); g_controller = NULL; }
        return;
    }

    if (event->type == SDL_CONTROLLERBUTTONDOWN) {
        SDL_GameControllerButton btn = event->cbutton.button;
        if (game_state == STATE_TITLE) {
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP)    menu_selection = (menu_selection + 2) % 3;
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN)  menu_selection = (menu_selection + 1) % 3;
            if (btn == SDL_CONTROLLER_BUTTON_A || btn == SDL_CONTROLLER_BUTTON_START) {
                if (menu_selection == 0) start_new_game();
                else if (menu_selection == 1) game_state = STATE_HIGHSCORES;
                else if (menu_selection == 2) { game_state = STATE_SETTINGS; menu_selection = 0; settings_tab = 0; }
            }
            if (btn == SDL_CONTROLLER_BUTTON_B) { SDL_Event qe; qe.type = SDL_QUIT; SDL_PushEvent(&qe); }
        } else if (game_state == STATE_PLAYING) {
            if (btn == SDL_CONTROLLER_BUTTON_START) game_set_paused(1);
            if (btn == ctrl_binds[CT_HYPERSPACE]) trigger_hyperspace();
        } else if (game_state == STATE_PAUSED) {
            if (btn == SDL_CONTROLLER_BUTTON_START || btn == SDL_CONTROLLER_BUTTON_B) game_set_paused(0);
        } else if (game_state == STATE_SETTINGS) {
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP)    menu_selection = (menu_selection == 0) ? 0 : menu_selection - 1;
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN)  menu_selection++;
            if (btn == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)  settings_tab = (settings_tab + 3) % 4;
            if (btn == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) settings_tab = (settings_tab + 1) % 4;
            if (btn == SDL_CONTROLLER_BUTTON_B || btn == SDL_CONTROLLER_BUTTON_START) { game_state = STATE_TITLE; menu_selection = 0; }
        } else if (game_state == STATE_KEYBINDS) {
            if (ctrl_rebinding_action >= 0) {
                // Assign this button to the action (don't allow START/BACK to avoid trapping)
                if (btn != SDL_CONTROLLER_BUTTON_START && btn != SDL_CONTROLLER_BUTTON_BACK) {
                    ctrl_binds[ctrl_rebinding_action] = btn;
                }
                ctrl_rebinding_action = -1;
            } else {
                if (btn == SDL_CONTROLLER_BUTTON_B) { game_state = STATE_SETTINGS; menu_selection = 0; }
            }
        } else if (game_state == STATE_GAMEOVER) {
            if (btn == SDL_CONTROLLER_BUTTON_A || btn == SDL_CONTROLLER_BUTTON_START) {
                int is_hs = 0;
                for (int i = 0; i < 5; i++) if (score > high_scores[i].score) { is_hs = 1; new_high_score_idx = i; break; }
                if (is_hs) { game_state = STATE_NEW_HIGHSCORE; strcpy(temp_initials, "A  "); cur_initial_char = 0; }
                else game_state = STATE_HIGHSCORES;
            }
        } else if (game_state == STATE_HIGHSCORES) {
            if (btn == SDL_CONTROLLER_BUTTON_B || btn == SDL_CONTROLLER_BUTTON_START) game_state = STATE_TITLE;
        } else if (game_state == STATE_UPGRADE_SELECT) {
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT || btn == SDL_CONTROLLER_BUTTON_DPAD_UP) selected_option = (selected_option + 2) % 3;
            if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT || btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) selected_option = (selected_option + 1) % 3;
            if (btn == SDL_CONTROLLER_BUTTON_A) {
                apply_upgrade(upgrade_options[selected_option]);
                if (wave_cleared_pending) { wave_cleared_pending = 0; start_next_level(); }
                game_state = is_attract_ai ? STATE_ATTRACT_GAMEPLAY : STATE_PLAYING;
                if (ufo.active) audio_play(SFX_UFO_LOOP);
            }
        }
        return;
    }

    if (event->type == SDL_KEYDOWN) {
        if (game_state == STATE_ATTRACT_GAMEPLAY) {
            game_state = STATE_TITLE;
            audio_stop(SFX_THRUST);
            audio_stop(SFX_UFO_LOOP);
            return;
        }

        // Handle Cheat Code (Ctrl+F8)
        if (event->key.keysym.sym == SDLK_F8 && (SDL_GetModState() & KMOD_CTRL)) {
            god_mode = !god_mode;
            god_mode_msg_timer = 2.0f;
            audio_play(SFX_EXPLOSION_LG); // feedback
        }

        if (game_state == STATE_TITLE) {
            if (event->key.keysym.sym == SDLK_UP || event->key.keysym.sym == SDLK_w) menu_selection = (menu_selection + 2) % 3;
            if (event->key.keysym.sym == SDLK_DOWN || event->key.keysym.sym == SDLK_s) menu_selection = (menu_selection + 1) % 3;
            if (event->key.keysym.sym == SDLK_SPACE || event->key.keysym.sym == SDLK_RETURN) {
                if (menu_selection == 0) start_new_game();
                else if (menu_selection == 1) game_state = STATE_HIGHSCORES;
                else if (menu_selection == 2) { game_state = STATE_SETTINGS; menu_selection = 0; settings_tab = 0; }
            }
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                SDL_Event qe; qe.type = SDL_QUIT; SDL_PushEvent(&qe);
            }
        } else if (game_state == STATE_PLAYING) {
            if (event->key.keysym.sym == SDLK_ESCAPE) {
                game_set_paused(1);
            } else if (event->key.keysym.scancode == keybinds[KB_MINIMAP]) {
                minimap_visible = !minimap_visible;
            } else if ((event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_DOWN) && player.active) {
                trigger_hyperspace();
            }
        } else if (game_state == STATE_PAUSED) {
            if (event->key.keysym.sym == SDLK_ESCAPE || event->key.keysym.sym == SDLK_SPACE || event->key.keysym.sym == SDLK_RETURN) {
                game_set_paused(0);
            } else if (event->key.keysym.sym == SDLK_s) {
                save_game();
            } else if (event->key.keysym.sym == SDLK_l) {
                load_game();
            } else if (event->key.keysym.sym == SDLK_q) {
                game_state = STATE_TITLE;
                audio_stop(SFX_THRUST);
                audio_stop(SFX_UFO_LOOP);
            }
        } else if (game_state == STATE_KEYBINDS) {
            if (rebinding_action >= 0) {
                // Any key press assigns it (keyboard page)
                SDL_Scancode sc = event->key.keysym.scancode;
                if (sc != SDL_SCANCODE_ESCAPE) keybinds[rebinding_action] = sc;
                rebinding_action = -1;
            } else {
                int page_count = (keybind_page == 0) ? KB_COUNT : CT_COUNT;
                if (event->key.keysym.sym == SDLK_UP || event->key.keysym.sym == SDLK_w)
                    keybind_selection = (keybind_selection + page_count - 1) % page_count;
                if (event->key.keysym.sym == SDLK_DOWN || event->key.keysym.sym == SDLK_s)
                    keybind_selection = (keybind_selection + 1) % page_count;
                if (event->key.keysym.sym == SDLK_q || event->key.keysym.sym == SDLK_e)
                    { keybind_page = 1 - keybind_page; keybind_selection = 0; rebinding_action = -1; ctrl_rebinding_action = -1; }
                if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_SPACE) {
                    if (keybind_page == 0) rebinding_action = keybind_selection;
                    // controller rebinding happens via controller button event
                    else ctrl_rebinding_action = keybind_selection;
                }
                if (event->key.keysym.sym == SDLK_ESCAPE)
                    { game_state = STATE_SETTINGS; menu_selection = 0; settings_tab = 3; rebinding_action = -1; ctrl_rebinding_action = -1; }
            }
        } else if (game_state == STATE_SETTINGS) {
            // Tab switching with Q/E
            if (event->key.keysym.sym == SDLK_q) { settings_tab = (settings_tab + 3) % 4; menu_selection = 0; }
            if (event->key.keysym.sym == SDLK_e) { settings_tab = (settings_tab + 1) % 4; menu_selection = 0; }

            int tab_item_count[] = {4, 3, 3, 5}; // items per tab (CONTROLS tab now has 5)
            int n = tab_item_count[settings_tab];
            if (event->key.keysym.sym == SDLK_UP   || event->key.keysym.sym == SDLK_w) menu_selection = (menu_selection + n - 1) % n;
            if (event->key.keysym.sym == SDLK_DOWN  || event->key.keysym.sym == SDLK_s) menu_selection = (menu_selection + 1) % n;

            if (settings_tab == 0) { // VIDEO
                if (menu_selection == 0) { // Fullscreen
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a || event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) {
                        settings_fullscreen = !settings_fullscreen;
                        SDL_SetWindowFullscreen(g_window, settings_fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    }
                } else if (menu_selection == 1) { // Phosphor Glow
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a) settings_glow = (settings_glow + 4) % 5;
                    if (event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) settings_glow = (settings_glow + 1) % 5;
                } else if (menu_selection == 2) { // Show FPS
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a || event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) settings_show_fps = !settings_show_fps;
                } else if (menu_selection == 3) { // Screen Shake
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a || event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) settings_screen_shake = !settings_screen_shake;
                }
            } else if (settings_tab == 1) { // AUDIO
                if (menu_selection == 0) { // Master Volume
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a) { if (settings_volume > 0) { settings_volume -= 10; audio_set_volume(settings_volume); } }
                    if (event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) { if (settings_volume < 100) { settings_volume += 10; audio_set_volume(settings_volume); } }
                } else if (menu_selection == 1) { // SFX Volume
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a) { if (settings_sfx_vol > 0) settings_sfx_vol -= 10; }
                    if (event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) { if (settings_sfx_vol < 100) settings_sfx_vol += 10; }
                } else if (menu_selection == 2) { // Music Volume
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a) { if (settings_music_vol > 0) settings_music_vol -= 10; }
                    if (event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) { if (settings_music_vol < 100) settings_music_vol += 10; }
                }
            } else if (settings_tab == 2) { // GAMEPLAY
                if (menu_selection == 0) { // Difficulty
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a) difficulty = (difficulty + 3) % 4;
                    if (event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) difficulty = (difficulty + 1) % 4;
                } else if (menu_selection == 1) { // Autofire
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a || event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) settings_autofire = !settings_autofire;
                } else if (menu_selection == 2) { // Invert Y (mouse)
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a || event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) settings_invert_y = !settings_invert_y;
                }
            } else if (settings_tab == 3) { // CONTROLS
                if (menu_selection == 0) { // Mouse Aim
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a || event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) settings_mouse_aim = !settings_mouse_aim;
                } else if (menu_selection == 1) { // Crosshair
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a) settings_crosshair_style = (settings_crosshair_style + 2) % 3;
                    if (event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) settings_crosshair_style = (settings_crosshair_style + 1) % 3;
                } else if (menu_selection == 2) { // Controller deadzone
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a) settings_controller_deadzone = (settings_controller_deadzone + 2) % 3;
                    if (event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) settings_controller_deadzone = (settings_controller_deadzone + 1) % 3;
                } else if (menu_selection == 3) { // Control scheme
                    if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a || event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d) settings_control_scheme = !settings_control_scheme;
                } else if (menu_selection == 4) { // Keybinds
                    if (event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_SPACE) { game_state = STATE_KEYBINDS; keybind_selection = 0; keybind_page = 0; rebinding_action = -1; ctrl_rebinding_action = -1; }
                }
            }
            if (event->key.keysym.sym == SDLK_ESCAPE) { game_state = STATE_TITLE; menu_selection = 0; }
        } else if (game_state == STATE_GAMEOVER) {
            if (event->key.keysym.sym == SDLK_SPACE || event->key.keysym.sym == SDLK_RETURN) {
                // Check if high score achieved
                int is_hs = 0;
                for (int i = 0; i < 5; i++) {
                    if (score > high_scores[i].score) {
                        is_hs = 1;
                        new_high_score_idx = i;
                        break;
                    }
                }
                if (is_hs) {
                    game_state = STATE_NEW_HIGHSCORE;
                    strcpy(temp_initials, "A  ");
                    cur_initial_char = 0;
                } else {
                    game_state = STATE_HIGHSCORES; // Show scores anyway
                }
            }
        } else if (game_state == STATE_HIGHSCORES) {
            if (event->key.keysym.sym == SDLK_SPACE || event->key.keysym.sym == SDLK_RETURN || event->key.keysym.sym == SDLK_ESCAPE) {
                game_state = STATE_TITLE;
            }
        } else if (game_state == STATE_NEW_HIGHSCORE) {
            if (event->key.keysym.sym == SDLK_LEFT) {
                if (temp_initials[cur_initial_char] == ' ') {
                    temp_initials[cur_initial_char] = 'Z';
                } else if (temp_initials[cur_initial_char] == 'A') {
                    temp_initials[cur_initial_char] = ' ';
                } else {
                    temp_initials[cur_initial_char]--;
                }
            } else if (event->key.keysym.sym == SDLK_RIGHT) {
                if (temp_initials[cur_initial_char] == ' ') {
                    temp_initials[cur_initial_char] = 'A';
                } else if (temp_initials[cur_initial_char] == 'Z') {
                    temp_initials[cur_initial_char] = ' ';
                } else {
                    temp_initials[cur_initial_char]++;
                }
            } else if (event->key.keysym.sym == SDLK_SPACE || event->key.keysym.sym == SDLK_RETURN) {
                if (cur_initial_char < 2) {
                    cur_initial_char++;
                    temp_initials[cur_initial_char] = 'A';
                } else {
                    // Shift lower scores down
                    for (int i = 4; i > new_high_score_idx; i--) {
                        high_scores[i] = high_scores[i - 1];
                    }
                    // Insert new high score
                    strcpy(high_scores[new_high_score_idx].initials, temp_initials);
                    high_scores[new_high_score_idx].score = score;
                    save_high_scores();

                    game_state = STATE_HIGHSCORES;
                }
            }
        } else if (game_state == STATE_UPGRADE_SELECT) {
            if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_a || event->key.keysym.sym == SDLK_UP || event->key.keysym.sym == SDLK_w) {
                selected_option = (selected_option + 2) % 3;
            } else if (event->key.keysym.sym == SDLK_RIGHT || event->key.keysym.sym == SDLK_d || event->key.keysym.sym == SDLK_DOWN || event->key.keysym.sym == SDLK_s) {
                selected_option = (selected_option + 1) % 3;
            } else if (event->key.keysym.sym == SDLK_RETURN) {
                apply_upgrade(upgrade_options[selected_option]);
                if (wave_cleared_pending) {
                    wave_cleared_pending = 0;
                    start_next_level();
                }
                game_state = is_attract_ai ? STATE_ATTRACT_GAMEPLAY : STATE_PLAYING;
                if (ufo.active) {
                    audio_play(SFX_UFO_LOOP);
                }
            }
        }
    }

    if (event->type == SDL_MOUSEBUTTONDOWN && game_state == STATE_PLAYING && player.active) {
        if (event->button.button == SDL_BUTTON_MIDDLE) {
            trigger_hyperspace();
        }
    }
}


void game_update(float delta_time) {
    fps_accum += delta_time;
    fps_frame_count++;
    if (fps_accum >= 0.5f) {
        fps_display_val = (int)(fps_frame_count / fps_accum);
        fps_accum = 0.0f; fps_frame_count = 0;
    }

    // Hide cursor during play if mouse aim active
    SDL_ShowCursor((game_state == STATE_PLAYING && settings_mouse_aim) ? SDL_DISABLE : SDL_ENABLE);

    if (game_state == STATE_PLAYING || game_state == STATE_ATTRACT_GAMEPLAY) {
        if (player.active) {
            float target_cam_x = player.pos.x - SCREEN_WIDTH / 2.0f;
            float target_cam_y = player.pos.y - SCREEN_HEIGHT / 2.0f;
            camera_pos.x += (target_cam_x - camera_pos.x) * 6.0f * delta_time;
            camera_pos.y += (target_cam_y - camera_pos.y) * 6.0f * delta_time;
        }
    } else if (game_state != STATE_PAUSED && game_state != STATE_UPGRADE_SELECT) {
        camera_pos = (Vec2){0.0f, 0.0f};
    }

    if (game_state != STATE_PAUSED && game_state != STATE_UPGRADE_SELECT) {
        if (god_mode_msg_timer > 0.0f) {
            god_mode_msg_timer -= delta_time;
        }
    }

    if (game_state != STATE_PLAYING && game_state != STATE_ATTRACT_GAMEPLAY && game_state != STATE_PAUSED && game_state != STATE_UPGRADE_SELECT) {
        idle_timer += delta_time;
        if (idle_timer >= ATTRACT_DELAY) {
            idle_timer = 0.0f;
            if (game_state == STATE_TITLE) game_state = STATE_ATTRACT_INSTRUCTIONS;
            else if (game_state == STATE_ATTRACT_INSTRUCTIONS) game_state = STATE_HIGHSCORES;
            else if (game_state == STATE_HIGHSCORES) {
                is_attract_ai = 1;
                start_new_game();
                game_state = STATE_ATTRACT_GAMEPLAY;
            }
            else game_state = STATE_TITLE;
        }
    }

    if (game_state == STATE_UPGRADE_SELECT && is_attract_ai) {
        static float ai_upgrade_timer = 0.0f;
        ai_upgrade_timer += delta_time;
        if (ai_upgrade_timer >= 1.5f) {
            ai_upgrade_timer = 0.0f;
            selected_option = rand() % 3;
            apply_upgrade(upgrade_options[selected_option]);
            if (wave_cleared_pending) {
                wave_cleared_pending = 0;
                start_next_level();
            }
            game_state = STATE_ATTRACT_GAMEPLAY;
            if (ufo.active) {
                audio_play(SFX_UFO_LOOP);
            }
        }
    }

    int active_asteroids_count = 0;
    if (game_state != STATE_PAUSED && game_state != STATE_UPGRADE_SELECT) {
        // Update combo timer
        if (combo_timer > 0.0f) {
            combo_timer -= delta_time;
            if (combo_timer <= 0.0f) combo_count = 0;
        }

        // Update near-miss cooldown
        if (near_miss_cooldown > 0.0f) near_miss_cooldown -= delta_time;

        // Near-miss XP: if a UFO bullet passes very close to player
        if (player.active && near_miss_cooldown <= 0.0f) {
            for (int b = 0; b < MAX_UFO_BULLETS; b++) {
                if (!ufo_bullets[b].active) continue;
                float dx = ufo_bullets[b].pos.x - player.pos.x;
                float dy = ufo_bullets[b].pos.y - player.pos.y;
                float dist_sq = dx*dx + dy*dy;
                if (dist_sq < 35.0f * 35.0f && dist_sq > player.radius * player.radius) {
                    // Adrenaline bonus – small XP orb spawn
                    spawn_orb(player.pos, 3);
                    near_miss_cooldown = 1.0f;
                    break;
                }
            }
        }

        // Edge flash – detect if player is near wrap boundary at speed
        if (player.active) {
            float spd = sqrtf(player.vel.x*player.vel.x + player.vel.y*player.vel.y);
            if (spd > 120.0f) {
                float margin = 40.0f;
                if (player.pos.x < margin || player.pos.x > SCREEN_WIDTH - margin ||
                    player.pos.y < margin || player.pos.y > SCREEN_HEIGHT - margin) {
                    edge_flash_timer = 0.15f;
                }
            }
        }
        if (edge_flash_timer > 0.0f) edge_flash_timer -= delta_time;

        // Update score floaters
        for (int i = 0; i < MAX_SCORE_FLOATS; i++) {
            if (score_floats[i].active) {
                score_floats[i].y += score_floats[i].vy * delta_time;
                score_floats[i].life -= delta_time;
                if (score_floats[i].life <= 0.0f) score_floats[i].active = 0;
            }
        }

        // XP bar flash
        if (xp_flash_timer > 0.0f) xp_flash_timer -= delta_time;



        if (game_state == STATE_TITLE || game_state == STATE_ATTRACT_INSTRUCTIONS || game_state == STATE_ATTRACT_GAMEPLAY) {
            // Just rotate menu asteroids
            static float init_title_ast = 0.0f;
            if (init_title_ast == 0.0f) {
                init_title_ast = 1.0f;
                // Spawn a few background asteroids just to float around the title screen
                for (int i = 0; i < 4; i++) {
                    Vec2 pos = {((float)rand() / RAND_MAX) * SCREEN_WIDTH, ((float)rand() / RAND_MAX) * SCREEN_HEIGHT};
                    spawn_asteroid(pos, 3);
                }
            }
        }

        // --- Update Particles (always update, even in title/game over) ---
        for (int i = 0; i < MAX_PARTICLES; i++) {
            if (particles[i].life > 0.0f) {
                particles[i].life -= delta_time;
                particles[i].angle += particles[i].rot_speed * delta_time;
                particles[i].pos.x += particles[i].vel.x * delta_time;
                particles[i].pos.y += particles[i].vel.y * delta_time;
                if (player.active && distance_sq(particles[i].pos, player.pos) > 1300.0f * 1300.0f) {
                    particles[i].life = 0.0f;
                }
            }
        }

        // --- Update Asteroids (always update) ---
        for (int i = 0; i < MAX_ASTEROIDS; i++) {
            if (asteroids[i].active) {
                active_asteroids_count++;
                // Record trail before moving
                asteroids[i].trail_pos[asteroids[i].trail_head] = asteroids[i].pos;
                asteroids[i].trail_ang[asteroids[i].trail_head] = asteroids[i].angle;
                asteroids[i].trail_head = (asteroids[i].trail_head + 1) % PHOS_TRAIL_LEN;
                asteroids[i].angle += asteroids[i].rot_speed * delta_time;
                
                // Move asteroid without wrap-around:
                Vec2 ext_f = calculate_external_forces(asteroids[i].pos);
                asteroids[i].vel.x += ext_f.x * delta_time;
                asteroids[i].vel.y += ext_f.y * delta_time;
                asteroids[i].pos.x += asteroids[i].vel.x * delta_time;
                asteroids[i].pos.y += asteroids[i].vel.y * delta_time;

                // If asteroid is too far from player, reposition it!
                if (player.active) {
                    float dist_sq = distance_sq(asteroids[i].pos, player.pos);
                    if (dist_sq > 1200.0f * 1200.0f) {
                        float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
                        float dist = 850.0f + ((float)rand() / RAND_MAX) * 250.0f;
                        asteroids[i].pos.x = player.pos.x + cosf(angle) * dist;
                        asteroids[i].pos.y = player.pos.y + sinf(angle) * dist;
                        // Reset trail to prevent line artifacts stretching across the screen
                        for (int t = 0; t < PHOS_TRAIL_LEN; t++) {
                            asteroids[i].trail_pos[t] = asteroids[i].pos;
                        }
                    }
                }
            }
        }

        // Continual spawning: if active asteroids falls below target count, spawn new ones off-screen
        int target_asteroids = 6 + player_level + (difficulty * 2);
        if (target_asteroids > MAX_ASTEROIDS - 4) {
            target_asteroids = MAX_ASTEROIDS - 4;
        }
        if (player.active && active_asteroids_count < target_asteroids) {
            // Spawn a new large asteroid in an off-screen ring
            float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float dist = 850.0f + ((float)rand() / RAND_MAX) * 250.0f;
            Vec2 pos = {
                player.pos.x + cosf(angle) * dist,
                player.pos.y + sinf(angle) * dist
            };
            spawn_asteroid(pos, 3);
        }

        // Spawn/maintain Anomalous Void Rift off-screen if player is high enough level
        if (player.active && player_level >= 4 && !rift.active) {
            rift.active = 1;
            float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float dist = 850.0f + ((float)rand() / RAND_MAX) * 250.0f;
            rift.pos.x = player.pos.x + cosf(angle) * dist;
            rift.pos.y = player.pos.y + sinf(angle) * dist;
            rift.radius = 35.0f;
            rift.angle1 = 0.0f;
            rift.angle2 = 0.0f;
            rift.pulse_timer = 0.0f;
            rift.spawn_timer = 5.0f;
        }
    }

    if (game_state != STATE_PLAYING && game_state != STATE_ATTRACT_GAMEPLAY) return;

    // --- Sound Beat System ---
    if (active_asteroids_count > 0) {
        // Beat rate based on number of asteroids
        float speed_mult = (float)active_asteroids_count / (4 + level);
        if (speed_mult < 0.2f) speed_mult = 0.2f;
        if (speed_mult > 1.0f) speed_mult = 1.0f;
        float current_beat_delay = 0.25f + 0.75f * speed_mult;
        
        beat_timer -= delta_time;
        if (beat_timer <= 0.0f) {
            cur_beat = 1 - cur_beat;
            audio_play(cur_beat ? SFX_BEAT1 : SFX_BEAT2);
            beat_timer = current_beat_delay;
        }
    }

    // --- Update Player ---
    if (player.active) {
        int thrust_key_down = 0;
        int fire_key_down = 0;
        
        if (game_state == STATE_ATTRACT_GAMEPLAY) {
            // Very simple AI: find closest asteroid
            float closest_dist = 9999999.0f;
            int closest_idx = -1;
            for (int i = 0; i < MAX_ASTEROIDS; i++) {
                if (asteroids[i].active) {
                    float dx = asteroids[i].pos.x - player.pos.x;
                    float dy = asteroids[i].pos.y - player.pos.y;
                    float dist_sq = dx*dx + dy*dy;
                    if (dist_sq < closest_dist) {
                        closest_dist = dist_sq;
                        closest_idx = i;
                    }
                }
            }
            if (closest_idx != -1) {
                float dx = asteroids[closest_idx].pos.x - player.pos.x;
                float dy = asteroids[closest_idx].pos.y - player.pos.y;
                float target_angle = atan2f(dy, dx) + (float)M_PI / 2.0f;
                // Simple snapping
                player.angle = target_angle;
                
                if (closest_dist > 150.0f * 150.0f) {
                    thrust_key_down = 1;
                }
                if (closest_dist < 300.0f * 300.0f) {
                    fire_key_down = 1;
                }
            }
        } else {
            const Uint8 *keys = SDL_GetKeyboardState(NULL);
            if (keys[keybinds[KB_ROTATE_LEFT]] || keys[SDL_SCANCODE_A]) {
                player.angle -= ROTATION_SPEED * delta_time;
            }
            if (keys[keybinds[KB_ROTATE_RIGHT]] || keys[SDL_SCANCODE_D]) {
                player.angle += ROTATION_SPEED * delta_time;
            }

            // Controller input
            if (g_controller) {
                float deadzone = (float[]){0.1f, 0.2f, 0.35f}[settings_controller_deadzone] * 32767.0f;
                int16_t lt = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
                int16_t rt = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

                if (settings_control_scheme == 0) {
                    // ARCADE: left stick X = rotate
                    int16_t lx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
                    if (fabsf((float)lx) > deadzone)
                        player.angle += ((float)lx / 32767.0f) * ROTATION_SPEED * delta_time * 2.5f;
                    // LT = thrust, RT = fire (triggers)
                    if (lt > 8000 || SDL_GameControllerGetButton(g_controller, ctrl_binds[CT_THRUST])) thrust_key_down = 1;
                    if (rt > 8000 || SDL_GameControllerGetButton(g_controller, ctrl_binds[CT_FIRE]))   fire_key_down = 1;
                    twin_stick_fire_active = 0;
                } else {
                    // TWIN_STICK: left stick X/Y = rotate + forward thrust
                    int16_t lx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
                    int16_t ly = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);
                    if (fabsf((float)lx) > deadzone)
                        player.angle += ((float)lx / 32767.0f) * ROTATION_SPEED * delta_time * 4.0f;
                    // Push forward on left stick (ly negative = up) = thrust
                    if ((float)-ly > deadzone * 1.2f) thrust_key_down = 1;
                    // Right stick: aim direction + auto-fire
                    int16_t rx = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX);
                    int16_t ry = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY);
                    float rmag = sqrtf((float)rx*(float)rx + (float)ry*(float)ry);
                    if (rmag > deadzone * 1.2f) {
                        twin_stick_fire_angle  = atan2f((float)rx, -(float)ry);
                        twin_stick_fire_active = 1;
                        fire_key_down = 1; // auto-fire when right stick is pushed
                    } else {
                        twin_stick_fire_active = 0;
                    }
                    // LT still works as thrust, RT as fire
                    if (lt > 8000 || SDL_GameControllerGetButton(g_controller, ctrl_binds[CT_THRUST])) thrust_key_down = 1;
                    if (rt > 8000 || SDL_GameControllerGetButton(g_controller, ctrl_binds[CT_FIRE]))   fire_key_down = 1;
                }
            }

            int mx, my;
            Uint32 mouse_buttons = SDL_GetMouseState(&mx, &my);

            // Always recalculate — camera may have moved even if mouse didn't
            if (settings_mouse_aim) {
                float spx = player.pos.x - camera_pos.x;
                float spy = player.pos.y - camera_pos.y;
                float dx = (float)mx - spx;
                float dy = (float)my - spy;
                if (dx*dx + dy*dy > 9.0f) {
                    player.angle = atan2f(dy, dx) + (float)M_PI_2;
                }
            }

            thrust_key_down = (keys[keybinds[KB_THRUST]] || keys[SDL_SCANCODE_W] || (settings_mouse_aim && (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT))));
            fire_key_down   = (keys[keybinds[KB_FIRE]]   || keys[SDL_SCANCODE_SPACE] || (settings_mouse_aim && (mouse_buttons & SDL_BUTTON(SDL_BUTTON_RIGHT))));
        }

        static Uint32 last_thrust_tap = 0;
        static int thrust_key_was_down = 0;
        is_thrusting = thrust_key_down;
        
        if (thrust_key_down && !thrust_key_was_down) {
            Uint32 now = SDL_GetTicks();
            if (player_upgrades.singularity_displacer && (now - last_thrust_tap) < 300) {
                Vec2 old_pos = player.pos;
                player.pos.x += sinf(player.angle) * 400.0f;
                player.pos.y -= cosf(player.angle) * 400.0f;
                spawn_particles(old_pos, 20, (SDL_Color){100, 100, 255, 255});
                spawn_particles(player.pos, 20, (SDL_Color){100, 100, 255, 255});
                audio_play(SFX_EXPLOSION_MD);
            }
            last_thrust_tap = now;
        }
        thrust_key_was_down = thrust_key_down;

        if (thrust_key_down) {
            // Apply thrust in direction of nose
            player.vel.x += sinf(player.angle) * THRUST_FORCE * player_upgrades.speed_mult * delta_time;
            player.vel.y -= cosf(player.angle) * THRUST_FORCE * player_upgrades.speed_mult * delta_time;
            is_thrusting = 1;
            thrust_timer += delta_time;
            audio_play(SFX_THRUST);
            on_thrust(player.pos, player.vel);
        } else {
            is_thrusting = 0;
            audio_stop(SFX_THRUST);
        }

        // Auto-firing logic
        if (fire_cooldown_timer > 0.0f) {
            fire_cooldown_timer -= delta_time;
        }
        if (settings_autofire && fire_cooldown_timer <= 0.0f) fire_key_down = 1;

        // Record trail
        player.trail_pos[player.trail_head] = player.pos;
        player.trail_ang[player.trail_head] = player.angle;
        player.trail_head = (player.trail_head + 1) % PHOS_TRAIL_LEN;

        // Apply Physics (Friction, External Forces, Velocity)
        Vec2 ext_f = calculate_external_forces(player.pos);
        player.vel.x += ext_f.x * delta_time;
        player.vel.y += ext_f.y * delta_time;
        player.vel.x *= powf(FRICTION, delta_time * 60.0f);
        player.vel.y *= powf(FRICTION, delta_time * 60.0f);
        player.pos.x += player.vel.x * delta_time;
        player.pos.y += player.vel.y * delta_time;

        // Mirror image: phantom twin orbits 200px behind the player
        if (player_upgrades.mirror_image && player.active) {
            mirror_active_flag = 1;
            mirror_pos.x = player.pos.x - sinf(player.angle) * 200.0f;
            mirror_pos.y = player.pos.y + cosf(player.angle) * 200.0f;
            mirror_angle = player.angle;
        } else if (!player_upgrades.mirror_image) {
            mirror_active_flag = 0;
        }

        if (fire_key_down && fire_cooldown_timer <= 0.0f) {
            int bullets_to_fire = player_upgrades.triple_shot ? 3 : 1;
            float spread = 0.25f;
            // In twin-stick mode, bullets travel in right-stick direction
            float base_fire_angle = twin_stick_fire_active ? twin_stick_fire_angle : player.angle;

            for (int b = 0; b < bullets_to_fire; b++) {
                // Look for empty bullet slot
                for (int i = 0; i < MAX_BULLETS; i++) {
                    if (!bullets[i].active) {
                        bullets[i].active = 1;
                        bullets[i].life = BULLET_LIFE;
                        bullets[i].bounces = 0;
                        bullets[i].pierces = 0;

                        float angle = base_fire_angle;
                        if (bullets_to_fire == 3) {
                            angle += (b - 1) * spread;
                        }

                        // Bullet starts at ship nose
                        float nose_x = player.pos.x + sinf(angle) * 12.0f;
                        float nose_y = player.pos.y - cosf(angle) * 12.0f;
                        bullets[i].pos = (Vec2){nose_x, nose_y};
                        bullets[i].trail_head = 0;
                        for (int t = 0; t < PHOS_TRAIL_LEN; t++) {
                            bullets[i].trail_pos[t] = bullets[i].pos;
                            bullets[i].trail_ang[t] = 0.0f;
                        }
                        
                        // Add velocity
                        bullets[i].vel.x = sinf(angle) * BULLET_SPEED * player_upgrades.bullet_speed_mult;
                        bullets[i].vel.y = -cosf(angle) * BULLET_SPEED * player_upgrades.bullet_speed_mult;
                        
                        // Inherit some of ship's velocity
                        bullets[i].vel.x += player.vel.x * 0.3f;
                        bullets[i].vel.y += player.vel.y * 0.3f;
                        
                        bullets[i].color = (SDL_Color){255, 255, 255, 255};
                        bullets[i].is_homing = player_upgrades.homing ? 1 : 0;
                        
                        break;
                    }
                }
            }
            // Rear gun fires a bullet from the ship's aft
            if (player_upgrades.rear_gun) {
                for (int i = 0; i < MAX_BULLETS; i++) {
                    if (!bullets[i].active) {
                        bullets[i].active = 1;
                        bullets[i].life = BULLET_LIFE;
                        bullets[i].bounces = 0;
                        bullets[i].pierces = 0;
                        float rear_angle = base_fire_angle + 3.14159265f;
                        float rear_x = player.pos.x - sinf(base_fire_angle) * 12.0f;
                        float rear_y = player.pos.y + cosf(base_fire_angle) * 12.0f;
                        bullets[i].pos = (Vec2){rear_x, rear_y};
                        bullets[i].trail_head = 0;
                        for (int t = 0; t < PHOS_TRAIL_LEN; t++) { bullets[i].trail_pos[t] = bullets[i].pos; bullets[i].trail_ang[t] = 0.0f; }
                        bullets[i].vel.x = sinf(rear_angle) * BULLET_SPEED * player_upgrades.bullet_speed_mult + player.vel.x * 0.3f;
                        bullets[i].vel.y = -cosf(rear_angle) * BULLET_SPEED * player_upgrades.bullet_speed_mult + player.vel.y * 0.3f;
                        bullets[i].color = (SDL_Color){255, 180, 80, 255};
                        bullets[i].is_homing = 0;
                        break;
                    }
                }
            }
            audio_play(SFX_FIRE);
            on_weapon_fire(player.pos, player.vel);
            fire_cooldown_timer = 0.25f * player_upgrades.fire_cooldown_mult;
        }



        
        // God Mode overrides
        if (god_mode) {
            lives = 99;
            player_upgrades.triple_shot = 1;
            player_upgrades.fire_cooldown_mult = 0.2f;
            player_upgrades.magnet_radius = 500.0f;
        }

        // Invuln blink timer
        if (player.invuln_timer > 0.0f) {
            player.invuln_timer -= delta_time;
        }
    } else {
        // Wait, if player is dead and we have lives left, spawn them after 1.5 seconds
        static float spawn_delay = 1.5f;
        if (lives > 0) {
            spawn_delay -= delta_time;
            if (spawn_delay <= 0.0f) {
                reset_player();
                spawn_delay = 1.5f;
            }
        }
    }

    // --- Update Orbs ---
    for (int i = 0; i < MAX_ORBS; i++) {
        if (orbs[i].active) {
            // Record trail
            orbs[i].trail_pos[orbs[i].trail_head] = orbs[i].pos;
            orbs[i].trail_ang[orbs[i].trail_head] = 0.0f;
            orbs[i].trail_head = (orbs[i].trail_head + 1) % PHOS_TRAIL_LEN;
            // Magnet logic
            if (player.active) {
                float dx = player.pos.x - orbs[i].pos.x;
                float dy = player.pos.y - orbs[i].pos.y;
                float dist_sq = dx * dx + dy * dy;
                float mag_r = player_upgrades.magnet_radius + 40.0f; // More giving base radius
                if (dist_sq < mag_r * mag_r) {
                    float dist = sqrtf(dist_sq);
                    float pull = (god_mode) ? 1500.0f : 1200.0f;
                    orbs[i].vel.x *= (1.0f - 5.0f * delta_time);
                    orbs[i].vel.y *= (1.0f - 5.0f * delta_time);
                    orbs[i].vel.x += (dx / dist) * pull * delta_time;
                    orbs[i].vel.y += (dy / dist) * pull * delta_time;
                }
            }
            orbs[i].pos.x += orbs[i].vel.x * delta_time;
            orbs[i].pos.y += orbs[i].vel.y * delta_time;
            if (player.active && distance_sq(orbs[i].pos, player.pos) > 1300.0f * 1300.0f) {
                orbs[i].active = 0;
            }

            // Collection
            if (player.active && check_collision(player.pos, player.radius + 5.0f, orbs[i].pos, 6.0f)) {
                orbs[i].active = 0;
                int gained = (int)(orbs[i].value * player_upgrades.xp_mult);
                player_xp += gained;
                audio_play(SFX_EXPLOSION_SM);
                if (player_xp >= xp_threshold) {
                    player_xp -= xp_threshold;
                    player_level++;
                    level = player_level;
                    xp_threshold = (int)(xp_threshold * 1.6f);
                    xp_flash_timer = 0.4f;
                    spawn_upgrade_options();
                    game_state = STATE_UPGRADE_SELECT;
                }
            }
        }
    }


    // --- Update Bullets ---
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].trail_pos[bullets[i].trail_head] = bullets[i].pos;
            bullets[i].trail_ang[bullets[i].trail_head] = 0.0f;
            bullets[i].trail_head = (bullets[i].trail_head + 1) % PHOS_TRAIL_LEN;
            bullets[i].life -= delta_time;
            if (bullets[i].life <= 0.0f) {
                bullets[i].active = 0;
            } else {
                Vec2 ext_f = calculate_external_forces(bullets[i].pos);
                bullets[i].vel.x += ext_f.x * delta_time;
                bullets[i].vel.y += ext_f.y * delta_time;

                // Homing bullet steering
                if (bullets[i].is_homing) {
                    float min_dist_sq = 1e18f;
                    Vec2 home_target = bullets[i].pos;
                    for (int ha = 0; ha < MAX_ASTEROIDS; ha++) {
                        if (!asteroids[ha].active) continue;
                        float hdx = asteroids[ha].pos.x - bullets[i].pos.x;
                        float hdy = asteroids[ha].pos.y - bullets[i].pos.y;
                        float hd = hdx*hdx + hdy*hdy;
                        if (hd < min_dist_sq) { min_dist_sq = hd; home_target = asteroids[ha].pos; }
                    }
                    float hdx = home_target.x - bullets[i].pos.x;
                    float hdy = home_target.y - bullets[i].pos.y;
                    float hdist = sqrtf(hdx*hdx + hdy*hdy);
                    if (hdist > 1.0f) {
                        float homing_str = 280.0f;
                        bullets[i].vel.x += (hdx/hdist) * homing_str * delta_time;
                        bullets[i].vel.y += (hdy/hdist) * homing_str * delta_time;
                        float bspd = sqrtf(bullets[i].vel.x*bullets[i].vel.x + bullets[i].vel.y*bullets[i].vel.y);
                        float max_bspd = BULLET_SPEED * player_upgrades.bullet_speed_mult;
                        if (bspd > max_bspd) { bullets[i].vel.x = bullets[i].vel.x/bspd*max_bspd; bullets[i].vel.y = bullets[i].vel.y/bspd*max_bspd; }
                    }
                }

                bullets[i].pos.x += bullets[i].vel.x * delta_time;
                bullets[i].pos.y += bullets[i].vel.y * delta_time;

                // Bouncy bullets logic
                if (player_upgrades.max_bounces > 0 && bullets[i].bounces < player_upgrades.max_bounces) {
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
                    if (bounced) {
                        bullets[i].bounces++;
                    }
                } else {
                    if (player.active && distance_sq(bullets[i].pos, player.pos) > 1300.0f * 1300.0f) {
                        bullets[i].active = 0;
                    }
                }
            }
        }
    }

    // --- Update UFO ---
    if (ufo.active) {
        /* Kamikaze faces movement direction (nose at +X, atan2f(y,x) maps directly) */
        {
            float vspd = sqrtf(ufo.vel.x * ufo.vel.x + ufo.vel.y * ufo.vel.y);
            if (ufo.size == 3 && vspd > 1.0f)
                ufo.angle = atan2f(ufo.vel.y, ufo.vel.x);
        }
        /* Record trail */
        ufo.trail_pos[ufo.trail_head] = ufo.pos;
        ufo.trail_ang[ufo.trail_head] = ufo.angle;
        ufo.trail_head = (ufo.trail_head + 1) % PHOS_TRAIL_LEN;

        Vec2 ext_f = calculate_external_forces(ufo.pos);
        ufo.vel.x += ext_f.x * delta_time;
        ufo.vel.y += ext_f.y * delta_time;

        if (ufo.size == 3 && player.active) {
            // Vector Stalker (Kamikaze)
            float dx = player.pos.x - ufo.pos.x;
            float dy = player.pos.y - ufo.pos.y;
            float dist = sqrtf(dx*dx + dy*dy);
            float t = dist / 200.0f;
            Vec2 p_target = { player.pos.x + player.vel.x * t, player.pos.y + player.vel.y * t };
            
            float target_dx = p_target.x - ufo.pos.x;
            float target_dy = p_target.y - ufo.pos.y;
            float target_dist = sqrtf(target_dx*target_dx + target_dy*target_dy);
            if (target_dist > 0.1f) {
                ufo.vel.x = (target_dx / target_dist) * 200.0f;
                ufo.vel.y = (target_dy / target_dist) * 200.0f;
            }
            ufo.pos.x += ufo.vel.x * delta_time;
            ufo.pos.y += ufo.vel.y * delta_time;
        } else if (ufo.size == 4) {
            // Boundary Weaver (Bomber)
            ufo.pos.x += ufo.vel.x * delta_time;
            float dy = ufo.target_y - ufo.pos.y;
            ufo.pos.y += dy * 2.0f * delta_time;
            
            if ((ufo.vel.x > 0.0f && ufo.pos.x > camera_pos.x + SCREEN_WIDTH + ufo.radius) || 
                (ufo.vel.x < 0.0f && ufo.pos.x < camera_pos.x - ufo.radius)) {
                ufo.active = 0;
                audio_stop(SFX_UFO_LOOP);
                ufo_spawn_timer = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
            }
        } else if (ufo.size == 5) {
            // Eye of the Void (Gravitational Harvester)
            ufo.pos.x += ufo.vel.x * 0.3f * delta_time; 
            ufo.pos.y += sinf(SDL_GetTicks() / 500.0f) * 20.0f * delta_time; 
            
            if ((ufo.vel.x > 0.0f && ufo.pos.x > camera_pos.x + SCREEN_WIDTH + ufo.radius) || 
                (ufo.vel.x < 0.0f && ufo.pos.x < camera_pos.x - ufo.radius)) {
                ufo.active = 0;
                audio_stop(SFX_UFO_LOOP);
                ufo_spawn_timer = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
            }
        } else if (ufo.size == 6) {
            /* Eldritch Tendril: sinusoidal drift, approaches slowly */
            if (player.active) {
                float dx = player.pos.x - ufo.pos.x;
                float dy = player.pos.y - ufo.pos.y;
                float d  = sqrtf(dx*dx + dy*dy);
                if (d > 0.1f) { ufo.vel.x += (dx/d)*30.0f*delta_time; ufo.vel.y += (dy/d)*30.0f*delta_time; }
            }
            float t6 = SDL_GetTicks() / 800.0f;
            ufo.vel.x += cosf(t6 * 1.3f) * 40.0f * delta_time;
            ufo.vel.y += sinf(t6 * 0.9f) * 40.0f * delta_time;
            float spd6 = sqrtf(ufo.vel.x*ufo.vel.x + ufo.vel.y*ufo.vel.y);
            if (spd6 > 120.0f) { ufo.vel.x = ufo.vel.x/spd6*120.0f; ufo.vel.y = ufo.vel.y/spd6*120.0f; }
            ufo.pos.x += ufo.vel.x * delta_time;
            ufo.pos.y += ufo.vel.y * delta_time;
            ufo.angle = atan2f(ufo.vel.y, ufo.vel.x);
            if (!player.active || sqrtf((ufo.pos.x-player.pos.x)*(ufo.pos.x-player.pos.x)+(ufo.pos.y-player.pos.y)*(ufo.pos.y-player.pos.y)) > 1400.0f) {
                ufo.active = 0; audio_stop(SFX_UFO_LOOP);
                ufo_spawn_timer = 18.0f + ((float)rand()/RAND_MAX)*12.0f;
            }
        } else if (ufo.size == 7) {
            /* Daemon Sigil: erratic teleport-lurches + fast spin */
            ufo.angle += 3.0f * delta_time;
            ufo.change_dir_timer -= delta_time;
            if (ufo.change_dir_timer <= 0.0f) {
                ufo.change_dir_timer = 0.8f + ((float)rand()/RAND_MAX)*1.2f;
                float a7 = ((float)rand()/RAND_MAX) * 2.0f * (float)M_PI;
                float lurch = 80.0f + ((float)rand()/RAND_MAX)*80.0f;
                ufo.pos.x += cosf(a7)*lurch; ufo.pos.y += sinf(a7)*lurch;
                ufo.vel.x = cosf(a7)*160.0f; ufo.vel.y = sinf(a7)*160.0f;
            }
            ufo.pos.x += ufo.vel.x * delta_time;
            ufo.pos.y += ufo.vel.y * delta_time;
            ufo.vel.x *= (1.0f - 1.5f*delta_time);
            ufo.vel.y *= (1.0f - 1.5f*delta_time);
            if (!player.active || sqrtf((ufo.pos.x-player.pos.x)*(ufo.pos.x-player.pos.x)+(ufo.pos.y-player.pos.y)*(ufo.pos.y-player.pos.y)) > 1400.0f) {
                ufo.active = 0; audio_stop(SFX_UFO_LOOP);
                ufo_spawn_timer = 18.0f + ((float)rand()/RAND_MAX)*12.0f;
            }
        } else {
            // Normal UFO (1 or 2)
            ufo.pos.x += ufo.vel.x * delta_time;
            ufo.change_dir_timer -= delta_time;
            if (ufo.change_dir_timer <= 0.0f) {
                ufo.change_dir_timer = 1.0f + ((float)rand() / RAND_MAX) * 1.5f;
                ufo.vel.y = (-1.0f + 2.0f * (rand() % 2)) * 60.0f;
            }
            ufo.pos.y += ufo.vel.y * delta_time;
            
            if (ufo.pos.y < camera_pos.y + 50.0f) { ufo.pos.y = camera_pos.y + 50.0f; ufo.vel.y = -ufo.vel.y; }
            if (ufo.pos.y > camera_pos.y + SCREEN_HEIGHT - 50.0f) { ufo.pos.y = camera_pos.y + SCREEN_HEIGHT - 50.0f; ufo.vel.y = -ufo.vel.y; }
 
            if ((ufo.vel.x > 0.0f && ufo.pos.x > camera_pos.x + SCREEN_WIDTH + ufo.radius) || 
                (ufo.vel.x < 0.0f && ufo.pos.x < camera_pos.x - ufo.radius)) {
                ufo.active = 0;
                audio_stop(SFX_UFO_LOOP);
                ufo_spawn_timer = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
            }
        }

        // General distance deactivation for all UFOs in free roam
        if (player.active && distance_sq(ufo.pos, player.pos) > 1600.0f * 1600.0f) {
            ufo.active = 0;
            audio_stop(SFX_UFO_LOOP);
            ufo_spawn_timer = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
        }

        // Firing logic
        if (ufo.active) {
            ufo.fire_timer -= delta_time;
            if (ufo.fire_timer <= 0.0f && player.active) {
                /* Fire rate varies by type */
                if      (ufo.size == 6) ufo.fire_timer = 2.2f;
                else if (ufo.size == 7) ufo.fire_timer = 0.7f;
                else ufo.fire_timer = (ufo.size == 2) ? 1.5f : 1.0f;
                
                // Look for empty UFO bullet slot
                for (int i = 0; i < MAX_UFO_BULLETS; i++) {
                    if (!ufo_bullets[i].active) {
                        ufo_bullets[i].active = 1;
                        ufo_bullets[i].life = BULLET_LIFE;
                        ufo_bullets[i].pos = ufo.pos;
                        ufo_bullets[i].trail_head = 0;
                        for (int t = 0; t < PHOS_TRAIL_LEN; t++) {
                            ufo_bullets[i].trail_pos[t] = ufo.pos;
                            ufo_bullets[i].trail_ang[t] = 0.0f;
                        }
                        
                        float fire_angle = 0.0f;
                        if (ufo.size == 2) {
                            // Large UFO fires in complete random direction
                            fire_angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
                        } else {
                            // Small UFO targets player directly + minor scatter
                            float dx = player.pos.x - ufo.pos.x;
                            float dy = player.pos.y - ufo.pos.y;
                            fire_angle = atan2f(dx, -dy);
                            float accuracy_offset = -0.3f + 0.6f * ((float)rand() / RAND_MAX);
                            fire_angle += accuracy_offset * (4.0f / (3.0f + level)); // accuracy gets better in high levels
                        }
                        
                        ufo_bullets[i].vel.x = sinf(fire_angle) * (BULLET_SPEED - 150.0f);
                        ufo_bullets[i].vel.y = -cosf(fire_angle) * (BULLET_SPEED - 150.0f);
                        ufo_bullets[i].color = (SDL_Color){255, 120, 120, 255}; // Reddish UFO bullets
                        audio_play(SFX_UFO_FIRE);
                        break;
                    }
                }
            }
        }
    } else {
        ufo_spawn_timer -= delta_time;
        if (ufo_spawn_timer <= 0.0f) {
            spawn_ufo();
        }
    }


    /* ─── Update zone, structures, NPCs ─────────────────────────── */
    if (player.active) {
        player_dist_origin = sqrtf(player.pos.x*player.pos.x + player.pos.y*player.pos.y);
        player_zone = get_zone(player.pos);
    }

    /* Rotate home station */
    home_station_angle += 0.004f * delta_time * 60.0f;

    /* Update structures (indestructible home station for now) */
    (void)structures;

    /* Update friendly NPCs */
    for (int i = 0; i < MAX_NPC; i++) {
        if (!npcs[i].active) continue;
        float dx = player.pos.x - npcs[i].pos.x;
        float dy = player.pos.y - npcs[i].pos.y;
        float dist = sqrtf(dx*dx + dy*dy);

        if (!npcs[i].following) {
            /* Idle drift: gentle circles */
            npcs[i].orbit_angle += 0.4f * delta_time;
            npcs[i].vel.x += cosf(npcs[i].orbit_angle) * 20.0f * delta_time;
            npcs[i].vel.y += sinf(npcs[i].orbit_angle) * 20.0f * delta_time;
            /* Dampen */
            npcs[i].vel.x *= (1.0f - 2.0f * delta_time);
            npcs[i].vel.y *= (1.0f - 2.0f * delta_time);
            npcs[i].pos.x += npcs[i].vel.x * delta_time;
            npcs[i].pos.y += npcs[i].vel.y * delta_time;
            /* Check if player nearby */
            if (dist < 120.0f && player.active) {
                npcs[i].contact_timer += delta_time;
                if (npcs[i].contact_timer > 2.0f) {
                    npcs[i].following = 1;
                    npcs[i].orbit_angle = atan2f(dy, dx) + (float)M_PI;
                }
            } else {
                npcs[i].contact_timer *= 0.95f;
            }
        } else {
            /* Orbit player at ~85px */
            float target_dist = 85.0f;
            npcs[i].orbit_angle += 1.1f * delta_time;
            float tx = player.pos.x + cosf(npcs[i].orbit_angle) * target_dist;
            float ty = player.pos.y + sinf(npcs[i].orbit_angle) * target_dist;
            float ex = tx - npcs[i].pos.x;
            float ey = ty - npcs[i].pos.y;
            npcs[i].vel.x += ex * 8.0f * delta_time;
            npcs[i].vel.y += ey * 8.0f * delta_time;
            npcs[i].vel.x *= (1.0f - 3.0f * delta_time);
            npcs[i].vel.y *= (1.0f - 3.0f * delta_time);
            npcs[i].pos.x += npcs[i].vel.x * delta_time;
            npcs[i].pos.y += npcs[i].vel.y * delta_time;
            /* Lose following if player dies or gets very far */
            if (!player.active || dist > 400.0f)
                npcs[i].following = 0;
        }
        npcs[i].angle = atan2f(npcs[i].vel.y, npcs[i].vel.x);

        /* Check bullet collision */
        for (int b = 0; b < MAX_BULLETS; b++) {
            if (!bullets[b].active) continue;
            float bx = bullets[b].pos.x - npcs[i].pos.x;
            float by = bullets[b].pos.y - npcs[i].pos.y;
            if (sqrtf(bx*bx + by*by) < npcs[i].radius) {
                bullets[b].active = 0;
                npcs[i].active = 0; /* NPC dies if shot */
                spawn_particles(npcs[i].pos, 12, (SDL_Color){80,255,80,255});
                audio_play(SFX_EXPLOSION_SM);
                break;
            }
        }
    }
    // --- Update UFO Bullets ---
    for (int i = 0; i < MAX_UFO_BULLETS; i++) {
        if (ufo_bullets[i].active) {
            ufo_bullets[i].trail_pos[ufo_bullets[i].trail_head] = ufo_bullets[i].pos;
            ufo_bullets[i].trail_ang[ufo_bullets[i].trail_head] = 0.0f;
            ufo_bullets[i].trail_head = (ufo_bullets[i].trail_head + 1) % PHOS_TRAIL_LEN;

            ufo_bullets[i].life -= delta_time;
            if (ufo_bullets[i].life <= 0.0f) {
                ufo_bullets[i].active = 0;
            } else {
                Vec2 ext_f = calculate_external_forces(ufo_bullets[i].pos);
                ufo_bullets[i].vel.x += ext_f.x * delta_time;
                ufo_bullets[i].vel.y += ext_f.y * delta_time;
                ufo_bullets[i].pos.x += ufo_bullets[i].vel.x * delta_time;
                ufo_bullets[i].pos.y += ufo_bullets[i].vel.y * delta_time;
                
                if (player.active && distance_sq(ufo_bullets[i].pos, player.pos) > 1300.0f * 1300.0f) {
                    ufo_bullets[i].active = 0;
                }
            }
        }
    }

    // --- Update Shockwaves & Rifts ---
    if (player_rift.active) {
        player_rift.spawn_timer -= delta_time;
        if (player_rift.spawn_timer <= 0.0f) {
            player_rift.active = 0;
        } else {
            player_rift.pulse_timer += delta_time * 5.0f;
            player_rift.angle1 += 2.0f * delta_time;
            player_rift.angle2 -= 1.5f * delta_time;
        }
    }
    if (rift.active) {
        rift.pulse_timer += delta_time * 5.0f;
        rift.angle1 += 2.0f * delta_time;
        rift.angle2 -= 1.5f * delta_time;
        if (player.active && distance_sq(rift.pos, player.pos) > 1600.0f * 1600.0f) {
            float angle = ((float)rand() / RAND_MAX) * 2.0f * (float)M_PI;
            float dist = 850.0f + ((float)rand() / RAND_MAX) * 250.0f;
            rift.pos.x = player.pos.x + cosf(angle) * dist;
            rift.pos.y = player.pos.y + sinf(angle) * dist;
        }
    }
    for (int i = 0; i < 4; i++) {
        if (shockwaves[i].active) {
            shockwaves[i].life -= delta_time;
            shockwaves[i].radius += (shockwaves[i].max_radius / 0.5f) * delta_time;
            if (shockwaves[i].life <= 0.0f) {
                shockwaves[i].active = 0;
            } else {
                for (int a = 0; a < MAX_ASTEROIDS; a++) {
                    if (asteroids[a].active) {
                        float dx = shockwaves[i].pos.x - asteroids[a].pos.x;
                        float dy = shockwaves[i].pos.y - asteroids[a].pos.y;
                        float dist = sqrtf(dx*dx + dy*dy);
                        if (dist > shockwaves[i].radius - 10.0f && dist < shockwaves[i].radius + 10.0f) {
                            asteroids[a].active = 0;
                            int next_size = asteroids[a].size - 1;
                            spawn_particles(asteroids[a].pos, 15, (SDL_Color){100, 255, 255, 255});
                            if (next_size >= 1) { spawn_asteroid(asteroids[a].pos, next_size); spawn_asteroid(asteroids[a].pos, next_size); }
                            score += 50;
                        }
                    }
                }
            }
        }
    }

    // Singularity Whip (Trail Damage)
    if (player.active && player_upgrades.singularity_whip) {
        for (int t = 0; t < PHOS_TRAIL_LEN; t++) {
            Vec2 t_pos = player.trail_pos[t];
            if (t_pos.x == 0.0f && t_pos.y == 0.0f) continue;
            for (int a = 0; a < MAX_ASTEROIDS; a++) {
                if (asteroids[a].active && check_collision(t_pos, 15.0f, asteroids[a].pos, asteroids[a].radius)) {
                    asteroids[a].active = 0;
                    int next_size = asteroids[a].size - 1;
                    spawn_particles(asteroids[a].pos, 15, (SDL_Color){255, 100, 255, 255});
                    wave_asteroids_destroyed++;
                    if (next_size >= 1) {
                        spawn_asteroid(asteroids[a].pos, next_size);
                        spawn_asteroid(asteroids[a].pos, next_size);
                    }
                    score += 100;
                }
            }
        }
    }

    // --- Collision Detection ---
    
    // 1. Player Bullets vs Asteroids
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].active) continue;
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;
            
            if (check_collision(bullets[b].pos, 1.0f, asteroids[a].pos, asteroids[a].radius)) {
                // Shielded asteroid: strip shield, spawn yellow sparks, deflect bullet
                if (asteroids[a].has_shield) {
                    asteroids[a].has_shield = 0;
                    if (!player_upgrades.piercing) bullets[b].active = 0;
                    spawn_particles(asteroids[a].pos, 18, (SDL_Color){255, 220, 50, 255});
                    audio_play(SFX_EXPLOSION_SM);
                    break;
                }
                // Impact!
                if (player_upgrades.resonance_cascade) {
                    for (int s = 0; s < 4; s++) {
                        if (!shockwaves[s].active) {
                            shockwaves[s].active = 1; shockwaves[s].pos = asteroids[a].pos;
                            shockwaves[s].radius = 10.0f; shockwaves[s].max_radius = 150.0f;
                            shockwaves[s].thickness = 5.0f; shockwaves[s].life = 0.5f; break;
                        }
                    }
                }
                
                if (!player_upgrades.piercing) {
                    bullets[b].active = 0;
                }
                // Split shot: spawn 2 fragment bullets on impact
                if (player_upgrades.split_shot) {
                    float ba = atan2f(bullets[b].vel.x, -bullets[b].vel.y);
                    float bspd = sqrtf(bullets[b].vel.x*bullets[b].vel.x + bullets[b].vel.y*bullets[b].vel.y);
                    for (int ss = 0; ss < 2; ss++) {
                        for (int si = 0; si < MAX_BULLETS; si++) {
                            if (!bullets[si].active) {
                                bullets[si].active = 1;
                                bullets[si].life = BULLET_LIFE * 0.45f;
                                bullets[si].bounces = 0; bullets[si].pierces = 0;
                                float sa = ba + (ss == 0 ? 0.65f : -0.65f);
                                bullets[si].pos = asteroids[a].pos;
                                bullets[si].trail_head = 0;
                                for (int t = 0; t < PHOS_TRAIL_LEN; t++) { bullets[si].trail_pos[t] = bullets[si].pos; bullets[si].trail_ang[t] = 0.0f; }
                                bullets[si].vel.x = sinf(sa) * bspd * 0.75f;
                                bullets[si].vel.y = -cosf(sa) * bspd * 0.75f;
                                bullets[si].color = (SDL_Color){255, 200, 80, 255};
                                bullets[si].is_homing = 0;
                                break;
                            }
                        }
                    }
                }
                asteroids[a].active = 0;
                wave_asteroids_destroyed++;
                
                int next_size = asteroids[a].size - 1;
                int points_added = (asteroids[a].size == 3) ? 20 : ((asteroids[a].size == 2) ? 50 : 100);

                // Combo multiplier
                combo_count++;
                combo_timer = COMBO_WINDOW;
                int combo_mult = (combo_count > 4) ? 4 : (combo_count > 2) ? 2 : 1;
                points_added *= combo_mult;
                score += points_added;

                // Score floater
                for (int sf = 0; sf < MAX_SCORE_FLOATS; sf++) {
                    if (!score_floats[sf].active) {
                        score_floats[sf].active = 1;
                        score_floats[sf].x = asteroids[a].pos.x;
                        score_floats[sf].y = asteroids[a].pos.y;
                        score_floats[sf].vy = -60.0f;
                        score_floats[sf].value = points_added;
                        score_floats[sf].life = 1.2f;
                        break;
                    }
                }
                
                // Explode particles
                spawn_particles(asteroids[a].pos, 15, (SDL_Color){255, 255, 255, 255});
                
                // Spawn XP orbs
                int orb_val = (asteroids[a].size == 3) ? 10 : ((asteroids[a].size == 2) ? 5 : 2);
                spawn_orb(asteroids[a].pos, orb_val);

                // Split asteroid
                if (next_size >= 1) {
                    spawn_asteroid(asteroids[a].pos, next_size);
                    spawn_asteroid(asteroids[a].pos, next_size);
                    audio_play(asteroids[a].size == 3 ? SFX_EXPLOSION_MD : SFX_EXPLOSION_SM);
                } else {
                    audio_play(SFX_EXPLOSION_SM);
                }
                
                // Give extra life every 10,000 points
                static int last_extra_life_score = 0;
                if (score / 10000 > last_extra_life_score) {
                    lives++;
                    last_extra_life_score = score / 10000;
                    // Play sweet life gain sound if we want, or just let it trigger
                }
                break;
            }
        }
    }

    // 2. UFO Bullets vs Asteroids
    for (int b = 0; b < MAX_UFO_BULLETS; b++) {
        if (!ufo_bullets[b].active) continue;
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;
            
            if (check_collision(ufo_bullets[b].pos, 1.0f, asteroids[a].pos, asteroids[a].radius)) {
                ufo_bullets[b].active = 0;
                asteroids[a].active = 0;
                
                int next_size = asteroids[a].size - 1;
                spawn_particles(asteroids[a].pos, 15, (SDL_Color){255, 120, 120, 255});
                
                if (next_size >= 1) {
                    spawn_asteroid(asteroids[a].pos, next_size);
                    spawn_asteroid(asteroids[a].pos, next_size);
                    audio_play(asteroids[a].size == 3 ? SFX_EXPLOSION_MD : SFX_EXPLOSION_SM);
                } else {
                    audio_play(SFX_EXPLOSION_SM);
                }
                break;
            }
        }
    }

    // 3. Player Bullets vs UFO
    if (ufo.active) {
        for (int b = 0; b < MAX_BULLETS; b++) {
            if (!bullets[b].active) continue;
            
            if (check_collision(bullets[b].pos, 1.0f, ufo.pos, ufo.radius)) {
                bullets[b].active = 0;
                ufo.active = 0;
                audio_stop(SFX_UFO_LOOP);
                audio_play(SFX_EXPLOSION_LG);
                spawn_particles(ufo.pos, 25, (SDL_Color){255, 180, 50, 255});
                
                // Spawn several XP orbs for UFO
                int orb_count = (ufo.size == 2) ? 5 : 15;
                for (int o = 0; o < orb_count; o++) {
                    spawn_orb(ufo.pos, (ufo.size == 2) ? 10 : 20);
                }

                score += (ufo.size == 2) ? 200 : 1000; // Small UFO is 1000 pts!
                ufo_spawn_timer = 25.0f + ((float)rand() / RAND_MAX) * 15.0f;
                break;
            }
        }
    }

    // 4. Ship vs Asteroids
    if (player.active && player.invuln_timer <= 0.0f) {
        for (int a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;
            
            if (check_collision(player.pos, player.radius, asteroids[a].pos, asteroids[a].radius)) {
                if (on_collision(1, a)) continue;
                if (player_upgrades.shield_active) {
                    player_upgrades.shield_active = 0;
                    player.invuln_timer = 2.0f;
                    audio_play(SFX_EXPLOSION_MD);
                    
                    // Destroy the asteroid
                    asteroids[a].active = 0;
                    wave_asteroids_destroyed++;
                    int next_size = asteroids[a].size - 1;
                    spawn_particles(asteroids[a].pos, 15, (SDL_Color){255, 255, 255, 255});
                    if (next_size >= 1) {
                        spawn_asteroid(asteroids[a].pos, next_size);
                        spawn_asteroid(asteroids[a].pos, next_size);
                    }
                    continue;
                }
                
                // Boom!
                player.active = 0;
                audio_play(SFX_EXPLOSION_LG);
                audio_stop(SFX_THRUST);
                spawn_particles(player.pos, 35, (SDL_Color){255, 200, 100, 255});
                
                // Destroy the asteroid too
                asteroids[a].active = 0;
                wave_asteroids_destroyed++;
                int next_size = asteroids[a].size - 1;
                if (next_size >= 1) {
                    spawn_asteroid(asteroids[a].pos, next_size);
                    spawn_asteroid(asteroids[a].pos, next_size);
                }
                
                lives--;
                if (lives <= 0) {
                    game_state = STATE_GAMEOVER;
                    audio_stop(SFX_UFO_LOOP);
                }
                break;
            }
        }
    }

    // 5. Ship vs UFO / UFO Bullets
    if (player.active && player.invuln_timer <= 0.0f) {
        // Ship vs UFO
        if (ufo.active && check_collision(player.pos, player.radius, ufo.pos, ufo.radius)) {
            if (player_upgrades.shield_active) {
                player_upgrades.shield_active = 0;
                player.invuln_timer = 2.0f;
                audio_play(SFX_EXPLOSION_MD);
                
                ufo.active = 0;
                audio_stop(SFX_UFO_LOOP);
                spawn_particles(ufo.pos, 25, (SDL_Color){255, 180, 50, 255});
                ufo_spawn_timer = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
            } else {
                player.active = 0;
                ufo.active = 0;
                audio_stop(SFX_THRUST);
                audio_stop(SFX_UFO_LOOP);
                audio_play(SFX_EXPLOSION_LG);
                spawn_particles(player.pos, 35, (SDL_Color){255, 200, 100, 255});
                spawn_particles(ufo.pos, 25, (SDL_Color){255, 180, 50, 255});
                
                lives--;
                if (lives <= 0) {
                    game_state = STATE_GAMEOVER;
                }
                ufo_spawn_timer = 20.0f + ((float)rand() / RAND_MAX) * 15.0f;
            }
        }
        
        // Ship vs UFO Bullets
        if (player.active && player.invuln_timer <= 0.0f) {
            for (int b = 0; b < MAX_UFO_BULLETS; b++) {
                if (ufo_bullets[b].active && check_collision(player.pos, player.radius, ufo_bullets[b].pos, 1.0f)) {
                    if (player_upgrades.shield_active) {
                        player_upgrades.shield_active = 0;
                        player.invuln_timer = 2.0f;
                        audio_play(SFX_EXPLOSION_MD);
                        ufo_bullets[b].active = 0;
                    } else if (player_upgrades.phase_shift) {
                        player_upgrades.phase_shift = 0;
                        player.invuln_timer = 3.5f;
                        audio_play(SFX_EXPLOSION_MD);
                        ufo_bullets[b].active = 0;
                        spawn_particles(player.pos, 20, (SDL_Color){100, 200, 255, 255});
                    } else if (mirror_active_flag && distance_sq(ufo_bullets[b].pos, mirror_pos) < 30.0f * 30.0f) {
                        mirror_active_flag = 0;
                        player_upgrades.mirror_image = 0;
                        ufo_bullets[b].active = 0;
                        spawn_particles(mirror_pos, 20, (SDL_Color){100, 180, 255, 255});
                    } else {
                        ufo_bullets[b].active = 0;
                        player.active = 0;
                        audio_play(SFX_EXPLOSION_LG);
                        audio_stop(SFX_THRUST);
                        spawn_particles(player.pos, 35, (SDL_Color){255, 200, 100, 255});

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
    }
}

void game_render() {
    SDL_Color main_color = {220, 240, 255, 255}; // Glowing cool white/light cyan phosphor
    
    // Apply phosphor persistence effect
    float glow_values[] = {1.0f, 0.95f, 0.90f, 0.82f, 0.70f};
    vg_apply_persistence(glow_values[settings_glow]); // fades slightly each frame

    // Set camera offset for world rendering
    vg_set_camera(camera_pos);
    
    // --- Draw Particles ---
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life > 0.0f) {
            float fade = particles[i].life / particles[i].max_life;
            SDL_Color col = particles[i].color;
            col.a = (Uint8)(fade * 255);
            
            Vec2 p1 = {-1.0f, 0.0f};
            Vec2 p2 = {1.0f, 0.0f};
            Line pl = {p1, p2};
            Shape s = {&pl, 1, col};
            vg_draw_shape(&s, particles[i].pos, particles[i].angle, particles[i].scale * fade);
        }
    }

    // --- Draw Orbs ---
    for (int i = 0; i < MAX_ORBS; i++) {
        if (orbs[i].active) {
            SDL_Color orb_col = {150, 255, 150, 255};
            static Line orb_lines[4];
            static int orb_init = 0;
            if (!orb_init) {
                orb_init = 1;
                orb_lines[0] = (Line){{-2,-2}, {2,-2}};
                orb_lines[1] = (Line){{2,-2}, {2,2}};
                orb_lines[2] = (Line){{2,2}, {-2,2}};
                orb_lines[3] = (Line){{-2,2}, {-2,-2}};
            }
            Shape os = {orb_lines, 4, orb_col};
            vg_draw_shape_trail(&os, orbs[i].trail_pos, orbs[i].trail_ang, PHOS_TRAIL_LEN, orbs[i].trail_head, 1.0f, 0.3f, 0.6f);
            vg_draw_shape(&os, orbs[i].pos, orbs[i].life * 5.0f, 1.0f);
        }
    }

    // --- Draw Asteroids ---
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (asteroids[i].active) {
            Shape s;
            s.lines = asteroids[i].lines;
            s.line_count = asteroids[i].line_count;
            s.color = main_color;
            if (asteroids[i].has_shield) s.color = (SDL_Color){100, 255, 255, 255};
            vg_draw_shape_trail(&s, asteroids[i].trail_pos, asteroids[i].trail_ang, PHOS_TRAIL_LEN, asteroids[i].trail_head, 1.0f, 0.5f, 0.8f);
            vg_draw_shape(&s, asteroids[i].pos, asteroids[i].angle, 1.0f);
        }
    }

    // --- Draw Bullets ---
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            float vx = bullets[i].vel.x;
            float vy = bullets[i].vel.y;
            float len = sqrtf(vx*vx + vy*vy);
            Vec2 p1 = {0, 0};
            Vec2 p2 = { (vx / len) * 4.0f, (vy / len) * 4.0f };
            Line l = {p1, p2};
            Shape s = {&l, 1, bullets[i].color};
            vg_draw_shape_trail(&s, bullets[i].trail_pos, bullets[i].trail_ang, PHOS_TRAIL_LEN, bullets[i].trail_head, 1.0f, 0.7f, 0.9f);
            vg_draw_shape(&s, bullets[i].pos, 0.0f, 1.0f);
        }
    }

    for (int i = 0; i < MAX_UFO_BULLETS; i++) {
        if (ufo_bullets[i].active) {
            float vx = ufo_bullets[i].vel.x;
            float vy = ufo_bullets[i].vel.y;
            float len = sqrtf(vx*vx + vy*vy);
            Vec2 p1 = {0, 0};
            Vec2 p2 = { (vx / len) * 4.0f, (vy / len) * 4.0f };
            Line l = {p1, p2};
            Shape s = {&l, 1, ufo_bullets[i].color};
            vg_draw_shape_trail(&s, ufo_bullets[i].trail_pos, ufo_bullets[i].trail_ang, PHOS_TRAIL_LEN, ufo_bullets[i].trail_head, 1.0f, 0.7f, 0.9f);
            vg_draw_shape(&s, ufo_bullets[i].pos, 0.0f, 1.0f);
        }
    }

    if (ufo.active) {
        Shape s;
        s.lines = ufo.lines;
        s.line_count = ufo.line_count;
        s.color = (SDL_Color){255, 180, 180, 255};
        /* Scale enemies to be roughly player-ship-sized (~16-22px at scale 1.0) */
        /* Kamikaze (size 3) rotates to face its movement direction */
        float scale, draw_angle = 0.0f;
        switch (ufo.size) {
            case 1:  scale = 0.7f; break;                         /* Small saucer ~22px */
            case 2:  scale = 1.2f; break;                         /* Large saucer ~38px */
            case 3:  scale = 1.0f; draw_angle = ufo.angle; break; /* Kamikaze, rotates  */
            case 4:  scale = 0.9f; break;                         /* Bomber ~36px       */
            case 5:  scale = 1.5f; break;                         /* Eye of Void ~48px  */
            case 6:  scale = 1.1f; draw_angle = ufo.angle; break; /* Eldritch Tendril   */
            case 7:  scale = 1.2f; draw_angle = ufo.angle; break; /* Daemon Sigil       */
            default: scale = 1.0f; break;
        }
        vg_draw_shape_trail(&s, ufo.trail_pos, ufo.trail_ang, PHOS_TRAIL_LEN, ufo.trail_head, scale, 0.4f, 0.75f);
        vg_draw_shape(&s, ufo.pos, draw_angle, scale);
    }

    if (player_rift.active) {
        Shape rs;
        rs.lines = ufo_lines; 
        rs.line_count = 10;
        rs.color = (SDL_Color){255, 50, 255, 255};
        vg_draw_shape(&rs, player_rift.pos, player_rift.angle1, player_rift.radius / 16.0f);
        vg_draw_shape(&rs, player_rift.pos, player_rift.angle2, (player_rift.radius / 16.0f) * 0.8f);
    }
    if (rift.active) {
        Shape rs;
        rs.lines = ufo_lines; 
        rs.line_count = 10;
        rs.color = (SDL_Color){180, 50, 255, 180};
        vg_draw_shape(&rs, rift.pos, rift.angle1, rift.radius / 16.0f);
        vg_draw_shape(&rs, rift.pos, rift.angle2, (rift.radius / 16.0f) * 0.8f);
    }
    
    for (int i = 0; i < 4; i++) {
        if (shockwaves[i].active) {
            static Line slines[16];
            static int s_init = 0;
            if (!s_init) {
                s_init = 1;
                for (int j=0; j<16; j++) {
                    float a1 = j * (float)M_PI * 2 / 16;
                    float a2 = (j+1) * (float)M_PI * 2 / 16;
                    slines[j].p1 = (Vec2){cosf(a1), sinf(a1)};
                    slines[j].p2 = (Vec2){cosf(a2), sinf(a2)};
                }
            }
            Shape ss = {slines, 16, (SDL_Color){100, 255, 255, 150}};
            vg_draw_shape(&ss, shockwaves[i].pos, 0.0f, shockwaves[i].radius);
        }
    }

    // --- Draw Player ---
    if (player.active) {
        int visible = 1;
        if (player.invuln_timer > 0.0f) visible = ((int)(player.invuln_timer * 10) % 2 == 0);
        if (visible) {
            Shape s = {player.lines, player.line_count, main_color};
            vg_draw_shape_trail(&s, player.trail_pos, player.trail_ang, PHOS_TRAIL_LEN, player.trail_head, 1.0f, 0.4f, 0.7f);
            vg_draw_shape(&s, player.pos, player.angle, 1.0f);
            if (is_thrusting) {
                // Animated multi-segment thruster flame flicker
                static float flame_t = 0.0f; flame_t += 0.22f;
                float fl = 10.0f + 6.0f * sinf(flame_t * 3.7f) + 3.0f * sinf(flame_t * 7.3f);
                float fw = 2.5f + 1.5f * sinf(flame_t * 5.1f);
                Line flines[3] = {
                    {{-fw, 6.0f}, {0.0f, 6.0f + fl}},
                    {{0.0f, 6.0f + fl}, {fw, 6.0f}},
                    {{-fw*0.5f, 6.0f}, {fw*0.5f, 6.0f}}
                };
                float fade = 0.6f + 0.4f * sinf(flame_t * 11.0f);
                SDL_Color fc = {
                    255,
                    (Uint8)(100 + 100 * fade),
                    (Uint8)(20 * (1.0f - fade)),
                    255
                };
                Shape fs = {flines, 3, fc};
                vg_draw_shape(&fs, player.pos, player.angle, 1.0f);
            }
            if (player_upgrades.shield_active) {
                static float shield_pulse = 0.0f; shield_pulse += 0.05f;
                static Line shield_lines[8];
                static int shield_init = 0;
                if (!shield_init) { shield_init = 1; for (int i=0; i<8; i++) { float a1 = i*(float)M_PI/4.0f, a2 = (i+1)*(float)M_PI/4.0f; shield_lines[i].p1 = (Vec2){cosf(a1)*16.0f, sinf(a1)*16.0f}; shield_lines[i].p2 = (Vec2){cosf(a2)*16.0f, sinf(a2)*16.0f}; } }
                Shape ss = {shield_lines, 8, (SDL_Color){100, 180, 255, 180}};
                vg_draw_shape(&ss, player.pos, shield_pulse, 1.3f + 0.1f * sinf(shield_pulse * 2.0f));
            }
        }
    }

    // Draw mirror image (phantom twin)
    if (mirror_active_flag && player.active) {
        SDL_Color mc = {100, 180, 255, 120};
        Shape ms = {ship_lines, 5, mc};
        vg_draw_shape(&ms, mirror_pos, mirror_angle, player.radius * player_upgrades.size_mult * 0.9f);
    }

    // Score pop floaters (drawn in world coordinates)
    for (int i = 0; i < MAX_SCORE_FLOATS; i++) {
        if (score_floats[i].active) {
            float t = score_floats[i].life;
            Uint8 alpha = (t > 0.8f) ? 255 : (Uint8)(t / 0.8f * 255);
            SDL_Color fc = {255, 255, 100, alpha};
            if (score_floats[i].value >= 200) fc = (SDL_Color){255, 150, 50, alpha};
            if (score_floats[i].value >= 400) fc = (SDL_Color){255, 80, 80, alpha};
            char temp_text[32];
            sprintf(temp_text, "+%d", score_floats[i].value);
            vf_draw_string_centered(temp_text, score_floats[i].x, score_floats[i].y, 14, fc);
        }
    }


    /* ─── Draw Home Station (world-space) ─────────────────────── */
    {
        Shape hs = {home_station_lines, sizeof(home_station_lines)/sizeof(Line),
                    (SDL_Color){80,220,255,220}};
        vg_draw_shape(&hs, (Vec2){0.0f,0.0f}, home_station_angle, 1.0f);
    }

    /* ─── Draw NPCs (world-space) ──────────────────────────────── */
    for (int i = 0; i < MAX_NPC; i++) {
        if (!npcs[i].active) continue;
        SDL_Color nc = npcs[i].following ? (SDL_Color){80,255,120,255} : (SDL_Color){60,200,80,200};
        Shape ns = {npc_drone_lines, sizeof(npc_drone_lines)/sizeof(Line), nc};
        vg_draw_shape(&ns, npcs[i].pos, npcs[i].angle, 0.9f);
    }

    // Reset camera offset to zero for HUD/UI rendering
    vg_set_camera((Vec2){0.0f, 0.0f});

    // ── HUD ──────────────────────────────────────────────────────────
    char hud_text[64];
    // Score (top-left, pushed down slightly from screen edge)
    sprintf(hud_text, "%05d", score);
    vf_draw_string(hud_text, 42, 30, 20, main_color);
    // Top score (top-center, same baseline)
    sprintf(hud_text, "%05d", high_scores[0].score > score ? high_scores[0].score : score);
    vf_draw_string_centered(hud_text, SCREEN_WIDTH/2.0f, 30, 15, (SDL_Color){180, 220, 255, 180});
    // Player level (top-right)
    sprintf(hud_text, "LVL %d", player_level);
    { float tw = (strlen(hud_text) * 14 * 1.2f) - (14 * 0.2f);
      vf_draw_string(hud_text, SCREEN_WIDTH - 42 - tw, 30, 14, (SDL_Color){120, 200, 255, 200}); }

    // Combo display (center, clear of lives below)
    if (combo_count >= 2) {
        SDL_Color cc = (combo_count >= 4) ? (SDL_Color){255, 80, 80, 255} : (SDL_Color){255, 200, 50, 255};
        sprintf(hud_text, "x%d COMBO!", combo_count);
        vf_draw_string_centered(hud_text, SCREEN_WIDTH / 2.0f, 68, 18, cc);
    }
    // Lives icons — moved down so they clear the combo text (combo bottom ≈ y+18 = 86)
    for (int i = 0; i < lives - 1; i++) {
        Shape s = {ship_lines, sizeof(ship_lines)/sizeof(Line), main_color};
        Vec2 pos = { 50.0f + i * 26.0f, 108.0f };
        vg_draw_shape(&s, pos, 0.0f, 0.7f);
    }

    // ── XP bar (bottom, with breathing room from screen edge) ────────
    float xp_percent = (float)player_xp / xp_threshold;
    SDL_Color xp_col = (xp_flash_timer > 0.0f && ((int)(xp_flash_timer * 20) % 2 == 0))
        ? (SDL_Color){255, 255, 255, 255} : (SDL_Color){100, 255, 100, 255};
    float xp_bar_y = (float)(SCREEN_HEIGHT - 22);
    float xp_bar_x0 = 40.0f, xp_bar_x1 = (float)(SCREEN_WIDTH - 40);
    // Track (dim)
    { Vec2 p1 = {xp_bar_x0, xp_bar_y}, p2 = {xp_bar_x1, xp_bar_y};
      Line l = {p1, p2}; Shape s = {&l, 1, (SDL_Color){50,50,50,255}};
      vg_draw_shape(&s, (Vec2){0,0}, 0.0f, 1.0f); }
    // Fill
    { Vec2 p1 = {xp_bar_x0, xp_bar_y}, p2 = {xp_bar_x0 + (xp_bar_x1 - xp_bar_x0) * xp_percent, xp_bar_y};
      Line l = {p1, p2}; Shape s = {&l, 1, xp_col};
      vg_draw_shape(&s, (Vec2){0,0}, 0.0f, 1.0f); }
    // Labels above the bar — left: "XP"  right: current/threshold
    vf_draw_string("XP", xp_bar_x0, xp_bar_y - 16.0f, 11, (SDL_Color){80, 180, 80, 200});
    sprintf(hud_text, "%d / %d", player_xp, xp_threshold);
    { float tw = (strlen(hud_text) * 10 * 1.2f) - (10 * 0.2f);
      vf_draw_string(hud_text, xp_bar_x1 - tw, xp_bar_y - 15.0f, 10, (SDL_Color){80, 180, 80, 180}); }

    // Upgrade icon strip (right side of screen, abbreviated)
    // Starts below the player level label (y≈50) and stops well above the XP bar (y≈870)
    if (game_state == STATE_PLAYING || game_state == STATE_ATTRACT_GAMEPLAY) {
        float ix = SCREEN_WIDTH - 72.0f, iy = 140.0f;
        SDL_Color ic = {100, 220, 255, 180};
        int iw = 10;
        if (player_upgrades.triple_shot)          { vf_draw_string("3X",  ix, iy, iw, ic); iy += 20.0f; }
        if (player_upgrades.max_bounces > 0)      { vf_draw_string("BNC", ix, iy, iw, ic); iy += 20.0f; }
        if (player_upgrades.shield_active)        { vf_draw_string("SHD", ix, iy, iw, ic); iy += 20.0f; }
        if (player_upgrades.piercing)             { vf_draw_string("PRC", ix, iy, iw, ic); iy += 20.0f; }
        if (player_upgrades.homing)               { vf_draw_string("HOM", ix, iy, iw, ic); iy += 20.0f; }
        if (player_upgrades.rear_gun)             { vf_draw_string("RRG", ix, iy, iw, ic); iy += 20.0f; }
        if (player_upgrades.thermal_hull)         { vf_draw_string("RAM", ix, iy, iw, ic); iy += 20.0f; }
        if (player_upgrades.singularity_displacer){ vf_draw_string("WRP", ix, iy, iw, ic); iy += 20.0f; }
        if (player_upgrades.split_shot)           { vf_draw_string("SPL", ix, iy, iw, ic); iy += 20.0f; }
        if (player_upgrades.resonance_cascade)    { vf_draw_string("RES", ix, iy, iw, ic); iy += 20.0f; }
        if (player_upgrades.mirror_image)         { vf_draw_string("TWN", ix, iy, iw, ic); iy += 20.0f; }
        if (player_upgrades.phase_shift)          { vf_draw_string("PHS", ix, iy, iw, (SDL_Color){255,200,100,180}); iy += 20.0f; }
        if (player_upgrades.singularity_whip)     { vf_draw_string("BWP", ix, iy, iw, ic); iy += 20.0f; }
    }



    // Wave clear message removed

    if (god_mode_msg_timer > 0.0f) {
        if (god_mode) {
            vf_draw_string_centered("GOD MODE: ENABLED", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT - 50, 28, (SDL_Color){255, 50, 50, 255});
        } else {
            vf_draw_string_centered("GOD MODE: DISABLED", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT - 50, 28, (SDL_Color){100, 100, 255, 255});
        }
    }

    // Edge flash: draw bright border lines at screen edges when player is about to wrap

    /* ─── MINIMAP ──────────────────────────────────────────────── */
    if (minimap_visible && (game_state == STATE_PLAYING || game_state == STATE_PAUSED)) {
        float mmx = (float)SCREEN_WIDTH - 205.0f, mmy = 68.0f;
        float mmw = 185.0f, mmh = 148.0f;
        float range = 3500.0f;
        float scx = mmw / (range * 2.0f), scy = mmh / (range * 2.0f);
        float mcx = mmx + mmw * 0.5f, mcy = mmy + mmh * 0.5f;

        /* Background scanlines */
        for (int row = 0; row <= 6; row++) {
            float ly = mmy + row * (mmh / 6.0f);
            Line l = {{mmx, ly}, {mmx + mmw, ly}};
            Shape s = {&l, 1, (SDL_Color){0, 30, 50, 160}};
            vg_draw_shape(&s, (Vec2){0,0}, 0.0f, 1.0f);
        }
        /* Border */
        {
            Line border[4] = {
                {{mmx,mmy},{mmx+mmw,mmy}},{{mmx+mmw,mmy},{mmx+mmw,mmy+mmh}},
                {{mmx+mmw,mmy+mmh},{mmx,mmy+mmh}},{{mmx,mmy+mmh},{mmx,mmy}}
            };
            SDL_Color zone_bdr[] = {
                {80,255,255,200},{120,200,255,200},{180,80,255,200},{255,80,80,200}
            };
            for (int i = 0; i < 4; i++) {
                Shape s = {&border[i], 1, zone_bdr[player_zone]};
                vg_draw_shape(&s, (Vec2){0,0}, 0.0f, 1.0f);
            }
        }
        /* Home station (cyan cross) */
        {
            float hx = mcx + (0.0f - player.pos.x) * scx;
            float hy = mcy + (0.0f - player.pos.y) * scy;
            if (hx >= mmx && hx <= mmx+mmw && hy >= mmy && hy <= mmy+mmh) {
                Line hl[2] = {{{hx-6,hy},{hx+6,hy}},{{hx,hy-6},{hx,hy+6}}};
                for (int i=0;i<2;i++){Shape s={&hl[i],1,(SDL_Color){100,255,255,255}};vg_draw_shape(&s,(Vec2){0,0},0.0f,1.0f);}
            }
        }
        /* Asteroids (dim gray) */
        for (int i = 0; i < MAX_ASTEROIDS; i++) {
            if (!asteroids[i].active) continue;
            float ax = mcx + (asteroids[i].pos.x - player.pos.x) * scx;
            float ay = mcy + (asteroids[i].pos.y - player.pos.y) * scy;
            if (ax < mmx || ax > mmx+mmw || ay < mmy || ay > mmy+mmh) continue;
            Line l = {{ax,ay},{ax+1,ay+1}};
            Shape s = {&l,1,(SDL_Color){90,90,90,160}};
            vg_draw_shape(&s,(Vec2){0,0},0.0f,1.0f);
        }
        /* UFO (red cross) */
        if (ufo.active) {
            float ux = mcx + (ufo.pos.x - player.pos.x) * scx;
            float uy = mcy + (ufo.pos.y - player.pos.y) * scy;
            if (ux >= mmx && ux <= mmx+mmw && uy >= mmy && uy <= mmy+mmh) {
                Line ul[2] = {{{ux-4,uy},{ux+4,uy}},{{ux,uy-4},{ux,uy+4}}};
                for (int i=0;i<2;i++){Shape s={&ul[i],1,(SDL_Color){255,60,60,255}};vg_draw_shape(&s,(Vec2){0,0},0.0f,1.0f);}
            }
        }
        /* NPCs (green) */
        for (int i = 0; i < MAX_NPC; i++) {
            if (!npcs[i].active) continue;
            float nx = mcx + (npcs[i].pos.x - player.pos.x) * scx;
            float ny = mcy + (npcs[i].pos.y - player.pos.y) * scy;
            if (nx < mmx || nx > mmx+mmw || ny < mmy || ny > mmy+mmh) continue;
            Line l = {{nx-2,ny},{nx+2,ny}};
            Shape s = {&l,1,(SDL_Color){60,255,60,220}};
            vg_draw_shape(&s,(Vec2){0,0},0.0f,1.0f);
        }
        /* Player (bright white cross) */
        {
            Line pl[2] = {{{mcx-5,mcy},{mcx+5,mcy}},{{mcx,mcy-5},{mcx,mcy+5}}};
            for (int i=0;i<2;i++){Shape s={&pl[i],1,(SDL_Color){255,255,255,255}};vg_draw_shape(&s,(Vec2){0,0},0.0f,1.0f);}
        }
        /* Zone label + coordinates */
        static const char* zone_names[] = {
            "ZONE 0: HOME SPACE","ZONE 1: INNER BELT",
            "ZONE 2: DEEP VOID","ZONE 3: THE ABYSS"
        };
        static SDL_Color zone_cols[] = {
            {100,255,255,220},{160,210,255,220},{200,80,255,220},{255,80,80,220}
        };
        char coord_buf[48];
        sprintf(coord_buf, "%.0f, %.0f", player.pos.x, player.pos.y);
        vf_draw_string_centered(zone_names[player_zone], mmx+mmw*0.5f, mmy+mmh+14.0f, 9,
                                zone_cols[player_zone]);
        vf_draw_string_centered(coord_buf, mmx+mmw*0.5f, mmy+mmh+26.0f, 9,
                                (SDL_Color){140,180,200,180});
    }

    /* ─── Edge arrow pointing home ─────────────────────────────── */
    if ((game_state == STATE_PLAYING || game_state == STATE_PAUSED) && player.active) {
        float home_sx = 0.0f - camera_pos.x;
        float home_sy = 0.0f - camera_pos.y;
        int on_screen = (home_sx > 30 && home_sx < SCREEN_WIDTH-30 &&
                         home_sy > 30 && home_sy < SCREEN_HEIGHT-30);
        if (!on_screen) {
            float ddx = home_sx - SCREEN_WIDTH*0.5f;
            float ddy = home_sy - SCREEN_HEIGHT*0.5f;
            float dd  = sqrtf(ddx*ddx + ddy*ddy);
            if (dd > 1.0f) {
                ddx /= dd; ddy /= dd;
                float t = 1e9f;
                if (ddx >  0.001f) t = fminf(t, (SCREEN_WIDTH -45.0f - SCREEN_WIDTH*0.5f) / ddx);
                if (ddx < -0.001f) t = fminf(t, (45.0f - SCREEN_WIDTH*0.5f) / ddx);
                if (ddy >  0.001f) t = fminf(t, (SCREEN_HEIGHT-45.0f - SCREEN_HEIGHT*0.5f) / ddy);
                if (ddy < -0.001f) t = fminf(t, (45.0f - SCREEN_HEIGHT*0.5f) / ddy);
                float ax = SCREEN_WIDTH*0.5f + ddx*t;
                float ay = SCREEN_HEIGHT*0.5f + ddy*t;
                float px = -ddy, py = ddx;
                float as = 11.0f;
                Line arrow[3] = {
                    {{ax+ddx*as, ay+ddy*as},{ax-ddx*8+px*7, ay-ddy*8+py*7}},
                    {{ax+ddx*as, ay+ddy*as},{ax-ddx*8-px*7, ay-ddy*8-py*7}},
                    {{ax-ddx*8+px*7,ay-ddy*8+py*7},{ax-ddx*8-px*7,ay-ddy*8-py*7}}
                };
                SDL_Color ac = {100,255,220,200};
                for (int i=0;i<3;i++){Shape s={&arrow[i],1,ac};vg_draw_shape(&s,(Vec2){0,0},0.0f,1.0f);}
                vf_draw_string_centered("HOME", ax, ay - 18.0f, 9, (SDL_Color){100,255,200,160});
            }
        }
    }

    if (edge_flash_timer > 0.0f) {
        float alpha = edge_flash_timer / 0.15f;
        SDL_Color ef = {255, 255, 255, (Uint8)(alpha * 180)};
        float m = 4.0f;
        Line el[4] = {
            {{m, m}, {SCREEN_WIDTH - m, m}},
            {{SCREEN_WIDTH - m, m}, {SCREEN_WIDTH - m, SCREEN_HEIGHT - m}},
            {{SCREEN_WIDTH - m, SCREEN_HEIGHT - m}, {m, SCREEN_HEIGHT - m}},
            {{m, SCREEN_HEIGHT - m}, {m, m}}
        };
        Shape es = {el, 4, ef};
        vg_draw_shape(&es, (Vec2){0,0}, 0.0f, 1.0f);
    }

    if (game_state == STATE_TITLE) {
        static float title_timer = 0.0f; title_timer += 0.016f;
        vf_draw_string_centered("PERMADRIFT", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f - 160, 55 * (1.0f + 0.05f * sinf(title_timer * 2.0f)), main_color);
        vf_draw_string_centered("D R I F T I N G   A T   T H E   W O R L D S   E N D", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f - 70, 14, (SDL_Color){100, 180, 255, 200});
        SDL_Color c1 = (menu_selection == 0) ? (SDL_Color){255, 255, 100, 255} : main_color;
        SDL_Color c2 = (menu_selection == 1) ? (SDL_Color){255, 255, 100, 255} : main_color;
        SDL_Color c3 = (menu_selection == 2) ? (SDL_Color){255, 255, 100, 255} : main_color;
        vf_draw_string_centered("BEGIN DRIFT", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f + 50, 22, c1);
        vf_draw_string_centered("DRIFTER ANNALS", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f + 100, 22, c2);
        vf_draw_string_centered("SETTINGS", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f + 150, 22, c3);
        { float tw = (strlen("BEGIN DRIFT") * 22 * 1.2f) - (22 * 0.2f);
          if (menu_selection == 0) vf_draw_string(">", SCREEN_WIDTH/2.0f - tw/2.0f - 40.0f, SCREEN_HEIGHT/2.0f + 50, 22, c1); }
        { float tw = (strlen("DRIFTER ANNALS") * 22 * 1.2f) - (22 * 0.2f);
          if (menu_selection == 1) vf_draw_string(">", SCREEN_WIDTH/2.0f - tw/2.0f - 40.0f, SCREEN_HEIGHT/2.0f + 100, 22, c2); }
        { float tw = (strlen("SETTINGS") * 22 * 1.2f) - (22 * 0.2f);
          if (menu_selection == 2) vf_draw_string(">", SCREEN_WIDTH/2.0f - tw/2.0f - 40.0f, SCREEN_HEIGHT/2.0f + 150, 22, c3); }
    } else if (game_state == STATE_PAUSED) {
        vf_draw_string_centered("PAUSED", SCREEN_WIDTH/2.0f, 70, 38, (SDL_Color){255, 255, 255, 255});
        vf_draw_string_centered("S: SAVE   L: LOAD   Q: QUIT", SCREEN_WIDTH/2.0f, 130, 16, main_color);
        vf_draw_string_centered("ESC TO RESUME", SCREEN_WIDTH/2.0f, 165, 13, (SDL_Color){150, 150, 180, 255});

        // ── Relic Log ─────────────────────────────────────────────────────
        // Map active upgrades to name+description pairs
        typedef struct { const char* name; const char* desc; } RelicEntry;
        RelicEntry relics[30]; int relic_count = 0;
        #define ADD_RELIC(up, nm, dc) if(up){ relics[relic_count].name=(nm); relics[relic_count].desc=(dc); relic_count++; }
        ADD_RELIC(player_upgrades.triple_shot,            "TRISKELION BURST",  "Three bolts spread from prow  [ fires with FIRE key ]")
        ADD_RELIC(player_upgrades.max_bounces > 0,        "VOID RICOCHET",     "Bolts rebound off void edge   [ passive ]")
        ADD_RELIC(player_upgrades.shield_active,          "ETHER SHROUD",      "Absorbs one asteroid hit      [ passive ]")
        ADD_RELIC(player_upgrades.piercing,               "ICHOROUS ROUNDS",   "Bolts pierce through enemies  [ passive ]")
        ADD_RELIC(player_upgrades.homing,                 "SEEKER ICHORS",     "Bolts home to nearest stone   [ passive ]")
        ADD_RELIC(player_upgrades.rear_gun,               "AFT CANNON",        "Fires bolt from the aft too   [ fires with FIRE key ]")
        ADD_RELIC(player_upgrades.split_shot,             "FISSION SHOT",      "Bolts split on first impact   [ passive ]")
        ADD_RELIC(player_upgrades.mirror_image,           "WRAITH TWIN",       "Phantom twin orbits behind    [ passive ]")
        ADD_RELIC(player_upgrades.phase_shift,            "PHASE SHIFT",       "Pass through one killing blow [ passive, one-time ]")
        ADD_RELIC(player_upgrades.thermal_hull,           "THERMAL HULL",      "Ram at speed to destroy stone [ THRUST into enemy ]")
        ADD_RELIC(player_upgrades.singularity_displacer,  "RIFT DISPLACER",    "Double-tap THRUST to rift-jump [ tap THRUST twice ]")
        ADD_RELIC(player_upgrades.singularity_whip,       "BANE WHIP",         "Thrust trail scorches drift   [ hold THRUST ]")
        ADD_RELIC(player_upgrades.resonance_cascade,      "RESONANCE CASCADE", "Bolts unleash shockwaves      [ fires with FIRE key ]")
        #undef ADD_RELIC
        if (relic_count > 0) {
            vf_draw_string_centered("YOUR RELICS", SCREEN_WIDTH/2.0f, 210, 15, (SDL_Color){180, 255, 200, 180});
            float ry = 238.0f;
            float row_h = (float)(SCREEN_HEIGHT - 290) / (relic_count < 12 ? relic_count : 12);
            if (row_h > 52.0f) row_h = 52.0f;
            int shown = relic_count < 12 ? relic_count : 12;
            for (int i = 0; i < shown; i++) {
                SDL_Color nc = {100, 220, 255, 220};
                SDL_Color dc = {160, 160, 160, 200};
                char nbuf[48]; sprintf(nbuf, "%-20s", relics[i].name);
                vf_draw_string_centered(nbuf, SCREEN_WIDTH/2.0f - 60.0f, ry, 12, nc);
                vf_draw_string_centered(relics[i].desc, SCREEN_WIDTH/2.0f + 50.0f, ry, 10, dc);
                ry += row_h;
            }
        } else {
            vf_draw_string_centered("NO RELICS EQUIPPED YET", SCREEN_WIDTH/2.0f, 220, 14, (SDL_Color){120,120,120,180});
        }
    } else if (game_state == STATE_SETTINGS) {
        const char* tab_names[] = {"VIDEO", "AUDIO", "GAMEPLAY", "CONTROLS"};
        // Draw tab headers
        float tx = SCREEN_WIDTH/2.0f - 280.0f;
        for (int t = 0; t < 4; t++) {
            SDL_Color tc = (t == settings_tab) ? (SDL_Color){255,255,80,255} : (SDL_Color){100,100,160,255};
            vf_draw_string(tab_names[t], tx + t * 140.0f, 70, 16, tc);
        }
        // Underline active tab
        vf_draw_string_centered("SETTINGS", SCREEN_WIDTH/2.0f, 35, 22, (SDL_Color){150,150,255,255});
        vf_draw_string_centered("Q / E   OR   LB / RB   TO   SWITCH   TABS", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT - 70, 11, (SDL_Color){100,100,120,255});
        vf_draw_string_centered("ESC TO RETURN", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT - 48, 11, (SDL_Color){100,100,120,255});

        const char* on_off[] = {"OFF", "ON"};
        const char* dz_names[] = {"LOW", "MED", "HIGH"};
        const char* ch_names[] = {"OFF", "CROSS", "DOT"};
        const char* glow_names[] = {"OFF", "LOW", "MED", "HIGH", "MAX"};

        float base_y = 130.0f;
        float step = 55.0f;

        if (settings_tab == 0) { // VIDEO
            const char* items[4]; char bufs[4][64];
            sprintf(bufs[0], "DISPLAY:  < %s >", settings_fullscreen ? "FULLSCREEN" : "WINDOWED");
            sprintf(bufs[1], "PHOSPHOR GLOW:  < %s >", glow_names[settings_glow]);
            sprintf(bufs[2], "SHOW FPS:  < %s >", on_off[settings_show_fps]);
            sprintf(bufs[3], "SCREEN SHAKE:  < %s >", on_off[settings_screen_shake]);
            for (int i = 0; i < 4; i++) items[i] = bufs[i];
            for (int i = 0; i < 4; i++) {
                SDL_Color ic = (menu_selection == i) ? (SDL_Color){255,255,80,255} : main_color;
                vf_draw_string_centered(items[i], SCREEN_WIDTH/2.0f, base_y + i*step, 18, ic);
                if (menu_selection == i) {
                    float tw = (strlen(items[i]) * 18 * 1.2f) - (18 * 0.2f);
                    vf_draw_string(">", SCREEN_WIDTH/2.0f - tw/2.0f - 35.0f, base_y + i*step, 18, ic);
                }
            }
        } else if (settings_tab == 1) { // AUDIO
            char b0[64], b1[64], b2[64];
            sprintf(b0, "MASTER VOLUME:  < %d%% >", settings_volume);
            sprintf(b1, "SFX VOLUME:  < %d%% >", settings_sfx_vol);
            sprintf(b2, "MUSIC VOLUME:  < %d%% >", settings_music_vol);
            const char* items[] = {b0, b1, b2};
            for (int i = 0; i < 3; i++) {
                SDL_Color ic = (menu_selection == i) ? (SDL_Color){255,255,80,255} : main_color;
                vf_draw_string_centered(items[i], SCREEN_WIDTH/2.0f, base_y + i*step, 18, ic);
                if (menu_selection == i) {
                    float tw = (strlen(items[i]) * 18 * 1.2f) - (18 * 0.2f);
                    vf_draw_string(">", SCREEN_WIDTH/2.0f - tw/2.0f - 35.0f, base_y + i*step, 18, ic);
                }
            }
        } else if (settings_tab == 2) { // GAMEPLAY
            char b0[64], b1[64], b2[64];
            sprintf(b0, "DIFFICULTY:  < %s >", difficulty_names[difficulty]);
            sprintf(b1, "AUTOFIRE:  < %s >", on_off[settings_autofire]);
            sprintf(b2, "INVERT MOUSE Y:  < %s >", on_off[settings_invert_y]);
            const char* items[] = {b0, b1, b2};
            for (int i = 0; i < 3; i++) {
                SDL_Color ic = (menu_selection == i) ? (SDL_Color){255,255,80,255} : main_color;
                vf_draw_string_centered(items[i], SCREEN_WIDTH/2.0f, base_y + i*step, 18, ic);
                if (menu_selection == i) {
                    float tw = (strlen(items[i]) * 18 * 1.2f) - (18 * 0.2f);
                    vf_draw_string(">", SCREEN_WIDTH/2.0f - tw/2.0f - 35.0f, base_y + i*step, 18, ic);
                }
            }
        } else if (settings_tab == 3) { // CONTROLS
            const char* scheme_names[] = {"ARCADE", "TWIN-STICK"};
            char b0[64], b1[64], b2[64], b3[64], b4[64];
            sprintf(b0, "MOUSE AIM:  < %s >", on_off[settings_mouse_aim]);
            sprintf(b1, "CROSSHAIR:  < %s >", ch_names[settings_crosshair_style]);
            sprintf(b2, "CONTROLLER DEADZONE:  < %s >", dz_names[settings_controller_deadzone]);
            sprintf(b3, "CONTROL SCHEME:  < %s >", scheme_names[settings_control_scheme]);
            sprintf(b4, "KEYBINDS  >");
            const char* items[] = {b0, b1, b2, b3, b4};
            for (int i = 0; i < 5; i++) {
                SDL_Color ic = (menu_selection == i) ? (SDL_Color){255,255,80,255} : main_color;
                vf_draw_string_centered(items[i], SCREEN_WIDTH/2.0f, base_y + i*step, 18, ic);
                if (menu_selection == i && i < 4) {
                    float tw = (strlen(items[i]) * 18 * 1.2f) - (18 * 0.2f);
                    vf_draw_string(">", SCREEN_WIDTH/2.0f - tw/2.0f - 35.0f, base_y + i*step, 18, ic);
                }
            }
            if (g_controller) {
                vf_draw_string_centered("CONTROLLER CONNECTED", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT - 100, 12, (SDL_Color){80,200,80,255});
            }
        }
    } else if (game_state == STATE_KEYBINDS) {
        // Tab headers
        SDL_Color kb_tc = (keybind_page == 0) ? (SDL_Color){255,255,80,255} : (SDL_Color){100,100,160,255};
        SDL_Color ct_tc = (keybind_page == 1) ? (SDL_Color){255,255,80,255} : (SDL_Color){100,100,160,255};
        vf_draw_string_centered("KEYBINDS", SCREEN_WIDTH/2.0f, 35, 22, (SDL_Color){150,150,255,255});
        vf_draw_string("KEYBOARD", SCREEN_WIDTH/2.0f - 200.0f, 70, 16, kb_tc);
        vf_draw_string("CONTROLLER", SCREEN_WIDTH/2.0f + 40.0f, 70, 16, ct_tc);
        vf_draw_string_centered("Q / E   TO   SWITCH   PAGES", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT - 60, 11, (SDL_Color){100,100,120,255});
        vf_draw_string_centered("ESC TO RETURN", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT - 38, 11, (SDL_Color){100,100,120,255});

        if (rebinding_action >= 0) {
            vf_draw_string_centered("PRESS ANY KEY", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f - 20, 28, (SDL_Color){255,200,80,255});
            vf_draw_string_centered("ESC TO CANCEL", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f + 30, 16, (SDL_Color){150,150,150,255});
        } else if (ctrl_rebinding_action >= 0) {
            vf_draw_string_centered("PRESS CONTROLLER BUTTON", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f - 20, 24, (SDL_Color){255,180,50,255});
            vf_draw_string_centered("START/BACK RESERVED   ESC TO CANCEL", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f + 30, 14, (SDL_Color){150,150,150,255});
        } else if (keybind_page == 0) {
            // Keyboard binds
            vf_draw_string_centered("ENTER TO REBIND", SCREEN_WIDTH/2.0f, 100, 12, (SDL_Color){100,100,120,255});
            float kb_row_h = (float)(SCREEN_HEIGHT - 160) / KB_COUNT;
            if (kb_row_h > 44.0f) kb_row_h = 44.0f;
            for (int i = 0; i < KB_COUNT; i++) {
                SDL_Color ic = (keybind_selection == i) ? (SDL_Color){255,255,80,255} : main_color;
                const char* kname = SDL_GetScancodeName(keybinds[i]);
                char row[64]; sprintf(row, "%-12s  %s", kb_action_names[i], kname);
                float y = 120.0f + i * kb_row_h;
                vf_draw_string_centered(row, SCREEN_WIDTH/2.0f, y, 16, ic);
                if (keybind_selection == i) {
                    float tw = (strlen(row) * 16 * 1.2f) - (16*0.2f);
                    vf_draw_string(">", SCREEN_WIDTH/2.0f - tw/2.0f - 30.0f, y, 16, ic);
                }
            }
        } else {
            // Controller binds
            static const char* btn_names[] = {
                "A","B","X","Y","BACK","GUIDE","START","LSTICK","RSTICK",
                "LB","RB","DPAD_UP","DPAD_DOWN","DPAD_LEFT","DPAD_RIGHT","MISC"
            };
            vf_draw_string_centered("ENTER TO REBIND", SCREEN_WIDTH/2.0f, 100, 12, (SDL_Color){100,100,120,255});
            float ct_row_h = (float)(SCREEN_HEIGHT - 160) / CT_COUNT;
            if (ct_row_h > 44.0f) ct_row_h = 44.0f;
            for (int i = 0; i < CT_COUNT; i++) {
                SDL_Color ic = (keybind_selection == i) ? (SDL_Color){255,180,50,255} : main_color;
                int bval = (int)ctrl_binds[i];
                const char* bname = (bval >= 0 && bval < 16) ? btn_names[bval] : "---";
                char row[64]; sprintf(row, "%-12s  %s", ct_action_names[i], bname);
                float y = 120.0f + i * ct_row_h;
                vf_draw_string_centered(row, SCREEN_WIDTH/2.0f, y, 16, ic);
                if (keybind_selection == i) {
                    float tw = (strlen(row) * 16 * 1.2f) - (16*0.2f);
                    vf_draw_string(">", SCREEN_WIDTH/2.0f - tw/2.0f - 30.0f, y, 16, ic);
                }
            }
            if (!g_controller) {
                vf_draw_string_centered("NO CONTROLLER DETECTED", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT - 90, 14, (SDL_Color){255,80,80,180});
            }
        }
    } else if (game_state == STATE_HIGHSCORES) {
        vf_draw_string_centered("THE FALLEN DRIFTERS", SCREEN_WIDTH/2.0f, 100, 35, (SDL_Color){255, 220, 100, 255});
        for (int i=0; i<5; i++) { char sl[64]; sprintf(sl, "%d.  %s  %05d", i+1, high_scores[i].initials, high_scores[i].score); vf_draw_string_centered(sl, SCREEN_WIDTH/2.0f, 220 + i*50, 24, main_color); }
        vf_draw_string_centered("PRESS SPACE TO RETURN TO THE DRIFT", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT - 100, 14, (SDL_Color){180, 180, 180, 255});
    } else if (game_state == STATE_GAMEOVER) {
        vf_draw_string_centered("DRIFT ENDS", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f - 80, 45, (SDL_Color){255, 80, 80, 255});
        vf_draw_string_centered("THE AUTODYNE IS LOST TO THE VOID", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f - 10, 14, (SDL_Color){200, 180, 180, 255});
        vf_draw_string_centered("PRESS ENTER TO DRIFT AGAIN", SCREEN_WIDTH/2.0f, SCREEN_HEIGHT/2.0f + 40, 18, main_color);
    } else if (game_state == STATE_NEW_HIGHSCORE) {
        vf_draw_string_centered("NEW DRIFTER RECORD!", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f - 100, 28, (SDL_Color){255, 240, 80, 255});
        vf_draw_string_centered("INSCRIBE YOUR MARK:", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f - 30, 20, main_color);
        char di[8]; sprintf(di, "%c %c %c", temp_initials[0], temp_initials[1], temp_initials[2]); 
        vf_draw_string_centered(di, SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f + 30, 30, main_color);
    } else if (game_state == STATE_UPGRADE_SELECT) {
        static float upgrade_pulse = 0.0f; upgrade_pulse += 0.04f;
        float sz = 25.0f + 2.0f * sinf(upgrade_pulse * 3.0f);
        vf_draw_string_centered("CHRONICLE SURGE!", SCREEN_WIDTH/2.0f, 95, sz, (SDL_Color){120, 255, 160, 255});
        vf_draw_string_centered("CHOOSE YOUR RELIC", SCREEN_WIDTH/2.0f, 150, 16, (SDL_Color){200, 200, 200, 255});
        vf_draw_string_centered("WASD / ARROW KEYS + ENTER", SCREEN_WIDTH/2.0f, 175, 12, (SDL_Color){120, 120, 120, 255});
        for (int i=0; i<3; i++) {
            int is_sel = (i == selected_option);
            float pulse_s = is_sel ? (1.0f + 0.06f * sinf(upgrade_pulse * 5.0f + i)) : 1.0f;
            SDL_Color col = is_sel ? (SDL_Color){255, 255, 80, 255} : (SDL_Color){140, 140, 140, 255};
            float y_pos = 250.0f + i * 90.0f;
            // Draw bracket box around selected
            if (is_sel) {
                float bw = 300.0f, bh = 70.0f;
                float bx = SCREEN_WIDTH/2.0f - bw/2.0f - 10.0f, by = y_pos - 15.0f;
                Line bl[4] = {{{bx,by},{bx+bw,by}}, {{bx+bw,by},{bx+bw,by+bh}}, {{bx+bw,by+bh},{bx,by+bh}}, {{bx,by+bh},{bx,by}}};
                Shape bs = {bl, 4, (SDL_Color){255, 255, 80, 60}};
                vg_draw_shape(&bs, (Vec2){0,0}, 0.0f, 1.0f);
            }
            char ot[64]; sprintf(ot, "%s", upgrade_names[upgrade_options[i]]);
            vf_draw_string_centered(ot, SCREEN_WIDTH/2.0f, y_pos, 20 * pulse_s, col);
            if (is_sel) {
                vf_draw_string_centered(upgrade_descs[upgrade_options[i]], SCREEN_WIDTH/2.0f, y_pos + 28, 11, (SDL_Color){190, 190, 190, 255});
                float tw = (strlen(upgrade_names[upgrade_options[i]]) * 20 * 1.2f) - (20 * 0.2f);
                vf_draw_string(">", SCREEN_WIDTH/2.0f - tw/2.0f - 40.0f, y_pos, 20, col);
            }
        }
    }

    // Crosshair (screen-space, drawn last so it's always on top)
    if (settings_mouse_aim && settings_crosshair_style > 0 &&
        (game_state == STATE_PLAYING || game_state == STATE_ATTRACT_GAMEPLAY)) {
        int mx, my; SDL_GetMouseState(&mx, &my);
        vg_set_camera((Vec2){0,0});
        SDL_Color xhc = {100, 220, 255, 200};
        float fx = (float)mx, fy = (float)my;
        if (settings_crosshair_style == 1) { // Cross with gap
            Line cl[4] = {{{fx-14,fy},{fx-5,fy}},{{fx+5,fy},{fx+14,fy}},{{fx,fy-14},{fx,fy-5}},{{fx,fy+5},{fx,fy+14}}};
            for (int ci = 0; ci < 4; ci++) { Shape cs = {&cl[ci],1,xhc}; vg_draw_shape(&cs,(Vec2){0,0},0.0f,1.0f); }
        } else { // Dot
            Line cl[2] = {{{fx-4,fy},{fx+4,fy}},{{fx,fy-4},{fx,fy+4}}};
            for (int ci = 0; ci < 2; ci++) { Shape cs = {&cl[ci],1,xhc}; vg_draw_shape(&cs,(Vec2){0,0},0.0f,1.0f); }
        }
        vg_set_camera(camera_pos);
    }
    // FPS counter
    if (settings_show_fps) {
        char fps_buf[16]; sprintf(fps_buf, "%d FPS", fps_display_val);
        vg_set_camera((Vec2){0,0});
        vf_draw_string(fps_buf, 8, 8, 14, (SDL_Color){80,200,80,200});
        vg_set_camera(camera_pos);
    }

    vg_present();
}
