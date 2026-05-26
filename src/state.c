#include "state.h"

/* =========== MODULE-SCOPE GLOBALS =========== */

/* --- Core state --- */
GameState  game_state  = STATE_TITLE;
Difficulty difficulty  = DIFFICULTY_NORMAL;
float      game_time   = 0.0f;  /* accumulated game time (seconds), for physics */
float      g_boot_timer = 0.0f; /* terminal boot sequence timer */
float      hyperjump_flash_timer = 0.0f;

/* --- High scores & initials entry --- */
HighScore high_scores[5];
int       new_high_score_idx = -1;
char      temp_initials[4]   = "A  ";
int       cur_initial_char   = 0;

/* --- Player entity & related --- */
ShipEntity    player;
PlayerUpgrades player_upgrades;
int   is_thrusting        = 0;
float fire_cooldown_timer  = 0.0f;
float thrust_timer         = 0.0f;

/* --- Entity pools --- */
AsteroidEntity asteroids[MAX_ASTEROIDS];
BulletEntity   bullets[MAX_BULLETS];
BulletEntity   ufo_bullets[MAX_UFO_BULLETS];
UfoEntity      ufo;
Particle       particles[MAX_PARTICLES];
OrbEntity      orbs[MAX_ORBS];
RiftEntity     rift;
RiftEntity     player_rift;
GravityWellEntity gravity_wells[MAX_GRAVITY_WELLS];
ShockwaveEntity shockwaves[4];
StructureEntity structures[MAX_STRUCTURE];
NpcEntity      npcs[MAX_NPC];

/* --- Score & progression --- */
int   score                    = 0;
int   lives                    = 3;
int   level                    = 1;
int   player_level             = 1;
int   player_xp                = 0;
int   xp_threshold             = 100;
int   total_asteroids_destroyed = 0;
int   wave_asteroids_destroyed  = 0;
int   wave_cleared_pending      = 0;
float last_extra_life_score     = 0.0f; /* score milestone for next extra life */
float xp_flash_timer            = 0.0f;

/* --- Combo system --- */
int   combo_count = 0;
float combo_timer  = 0.0f;

/* --- Bowling combo system (item 19) --- */
/* bowling_timer[i] > 0 means asteroid slot i is an active bowling fragment */
float bowling_timer[MAX_ASTEROIDS];
int   bowling_chain_count = 0;
float bowling_chain_timer = 0.0f;
int   g_bowling_next_spawn = 0; /* when 1, next spawn_asteroid() marks slot as bowling */

/* --- Score floaters --- */
ScoreFloat score_floats[MAX_SCORE_FLOATS];

/* --- Event text floaters (CRIT!, STRIKE!, HIT, VENT!) --- */
EventFloat event_floats[MAX_EVENT_FLOATS];

/* --- Upgrade selection --- */
UpgradeType upgrade_options[3];
int         selected_option = 0;

/* --- Camera & world --- */
Vec2  camera_pos        = {0.0f, 0.0f};
int   player_zone            = 0;
float player_dist_origin     = 0.0f;
float home_station_angle     = 0.0f;

/* --- Zone transition banner --- */
float zone_banner_timer      = 0.0f;   /* seconds remaining; 0 = hidden */
int   zone_banner_prev_zone  = -1;     /* -1 = not yet initialised */

/* --- Chronicle chord harmonics --- */
float orb_chord_timer        = 0.0f;   /* window for rapid-collection chain */
int   orb_chord_note         = 0;      /* next pentatonic note index [0-4] */
int   minimap_visible   = 1;

/* --- Screen shake --- */
float screen_shake_timer     = 0.0f;
float screen_shake_intensity  = 0.0f;

/* --- Menu navigation --- */
int      menu_selection    = 0;   /* 0=Start  1=High Scores  2=Settings */
GameState settings_back_state = STATE_TITLE;

/* --- Settings --- */
int  settings_volume          = 100;
int  settings_sfx_vol         = 100;
int  settings_music_vol       = 400;
int  settings_dynamic_range   = 1;
int  settings_mute_unfocused  = 1;
int settings_fullscreen       = 0;
int settings_glow             = 3;  /* 0=OFF 1=LOW 2=MED 3=HIGH 4=MAX */
int       settings_tab               = 0;  /* 0=VIDEO 1=AUDIO 2=GAMEPLAY 3=CONTROLS (now 0..8) */
int settings_screen_shake     = 1;
int settings_show_fps         = 0;
int settings_mouse_aim        = 1;
int settings_crosshair_style  = 1;  /* 0=OFF 1=CROSS 2=DOT */
int settings_autofire         = 0;
int settings_controller_deadzone = 1; /* 0=LOW 1=MED 2=HIGH */
int settings_invert_y         = 0;
int settings_control_scheme   = 1;  /* 0=ARCADE  1=TWIN_STICK */

/* --- New unified settings (FF7R/CoQ HUD overhaul) --- */
Settings  g_settings;
int       settings_row              = 0;
int       settings_keybind_pending  = -1;
uint32_t  g_world_seed              = 0;
/* Note: settings_tab already declared above as int settings_tab = 0 */

/* --- Key binds --- */
SDL_Scancode keybinds[KB_COUNT] = {
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
const char *kb_action_names[KB_COUNT] = {
    "ROTATE LEFT", "ROTATE RIGHT", "THRUST", "FIRE",
    "PAUSE", "HYPERSPACE",
    "ABILITY 1", "ABILITY 2", "ABILITY 3", "ABILITY 4",
    "MINIMAP"
};
int rebinding_action  = -1; /* -1 = not currently rebinding */
int keybind_selection = 0;
int keybind_page      = 0;  /* 0=keyboard  1=controller */

/* --- Controller binds --- */
SDL_GameController *g_controller = NULL;
SDL_GameControllerButton ctrl_binds[CT_COUNT] = {
    SDL_CONTROLLER_BUTTON_A,             /* CT_THRUST      */
    SDL_CONTROLLER_BUTTON_X,             /* CT_FIRE        */
    SDL_CONTROLLER_BUTTON_Y,             /* CT_HYPERSPACE  */
    SDL_CONTROLLER_BUTTON_B,             /* CT_ABILITY1    */
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,  /* CT_ABILITY2    */
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, /* CT_ABILITY3    */
    SDL_CONTROLLER_BUTTON_MAX,           /* CT_ABILITY4 — unbound */
    SDL_CONTROLLER_BUTTON_BACK           /* CT_MINIMAP     */
};
const char *ct_action_names[CT_COUNT] = {
    "THRUST", "FIRE", "HYPERSPACE",
    "ABILITY 1", "ABILITY 2", "ABILITY 3", "ABILITY 4",
    "MINIMAP"
};
int ctrl_rebinding_action = -1;

/* --- Twin-stick fire direction override --- */
float twin_stick_fire_angle  = 0.0f;
int   twin_stick_fire_active = 0;

/* --- Resource system --- */
int res_void_stone     = 0;
int res_void_steel     = 0;
int res_autodyne_frags = 0;
int res_hex_modules    = 0;
int res_ammo           = 0;
int res_rockets        = 0;
int res_contraband     = 0;
int res_isotopes       = 0;
int res_coolant        = 0;
int res_medicinals     = 0;
int res_biomatter      = 0;
int res_shield_caps    = 0;
int res_alien_flora    = 0;
int chrome             = 0;

/* --- Fuel system --- */
float fuel_current        = 100.0f;
float fuel_max            = 100.0f;
float drift_penalty_timer = 0.0f;   /* seconds in Emergency Drift Mode */

/* --- Player death animation --- */
Vec2  player_death_pos   = {0.0f, 0.0f};
float player_death_angle = 0.0f;
float player_death_timer = 0.0f;

/* --- Shop & warp menu --- */
int     shop_page       = 0;
int     shop_sel        = 0;
WarpLoc warp_locs[MAX_WARP_LOCS];
int     warp_loc_count  = 0;
int     warp_menu_sel   = 0;
float   warp_drive_range = 3000.0f;

/* --- Attract / idle --- */
int   is_attract_ai      = 0;
float idle_timer         = 0.0f;

/* --- Mirror image state --- */
Vec2  mirror_pos         = {0.0f, 0.0f};
float mirror_angle       = 0.0f;
int   mirror_active_flag = 0;

/* --- Cheat / debug --- */
int   god_mode           = 0;
float god_mode_msg_timer = 0.0f;

/* --- FPS counter --- */
float fps_accum       = 0.0f;
int   fps_frame_count = 0;
int   fps_display_val = 0;

/* --- Enemy wave pacing --- */
float level_start_timer = 2.0f;
float ufo_spawn_timer   = 15.0f;
float beat_timer        = 1.0f;
int   cur_beat          = 0;

/* --- Edge/XP warning flash --- */
float edge_flash_timer    = 0.0f;
float near_miss_cooldown  = 0.0f;

/* --- Warp-drive singularity exit effect ---
 * Counts down from WARP_FLASH_DUR after a warp jump. Drives an expanding
 * vector ring + CRT bloom flash rendered in screen space (see render_overlays). */
float       warp_flash_timer = 0.0f;
const float WARP_FLASH_DUR   = 0.65f;

/* --- Respawn animation state --- */
int   respawn_phase = 0;   /* 0=waiting, 1=blinking-in */
float respawn_blink = 0.0f;

/* --- Cached asteroid count (written by update_asteroids, read by update_spawning) --- */
int cached_active_asteroids = 0;

/* --- Void Stone Mechanics --- */
int time_stop_frames = 0;
#define POS_HISTORY_LEN 100
Vec2 player_pos_history[POS_HISTORY_LEN];
int pos_history_idx = 0;
int check_void_stone(void) {
    if (lives <= 1 && res_void_stone > 0) {
        res_void_stone--;
        time_stop_frames = 30;
        player.pos = player_pos_history[(pos_history_idx + POS_HISTORY_LEN - 100) % POS_HISTORY_LEN];
        player.invuln_timer = 2.0f;
        return 1;
    }
    return 0;
}

