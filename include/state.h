#ifndef STATE_H
#define STATE_H

#include "entities.h"
#include "game.h"

/* =========== MODULE-SCOPE GLOBALS =========== */

/* --- Core state --- */
extern GameState  game_state;
extern Difficulty difficulty;
extern float      game_time;
extern float      g_boot_timer;
extern float      hyperjump_flash_timer;

/* --- High scores & initials entry --- */
extern HighScore high_scores[5];
extern int       new_high_score_idx;
extern char      temp_initials[4];
extern int       cur_initial_char;

/* --- Player entity & related --- */
extern ShipEntity    player;
extern PlayerUpgrades player_upgrades;
extern int   is_thrusting;
extern float fire_cooldown_timer;
extern float thrust_timer;

/* --- Entity pools --- */
extern AsteroidEntity asteroids[MAX_ASTEROIDS];
extern BulletEntity   bullets[MAX_BULLETS];
extern BulletEntity   ufo_bullets[MAX_UFO_BULLETS];
extern UfoEntity      ufo;
extern Particle       particles[MAX_PARTICLES];
extern OrbEntity      orbs[MAX_ORBS];
extern RiftEntity     rift;
extern RiftEntity     player_rift;
extern GravityWellEntity gravity_wells[MAX_GRAVITY_WELLS];
extern ShockwaveEntity shockwaves[4];
extern StructureEntity structures[MAX_STRUCTURE];
extern NpcEntity      npcs[MAX_NPC];

/* --- Score & progression --- */
extern int   score;
extern int   lives;
extern int   level;
extern int   player_level;
extern int   player_xp;
extern int   xp_threshold;
extern int   total_asteroids_destroyed;
extern int   wave_asteroids_destroyed;
extern int   wave_cleared_pending;
extern float last_extra_life_score;
extern float xp_flash_timer;

/* --- Combo system --- */
extern int   combo_count;
extern float combo_timer;

/* --- Bowling combo system (item 19) --- */
/* bowling_timer[i] > 0 means asteroid slot i is an active bowling fragment */
extern float bowling_timer[MAX_ASTEROIDS];
extern int   bowling_chain_count;
extern float bowling_chain_timer;
extern int   g_bowling_next_spawn;

/* --- Score floaters --- */
extern ScoreFloat score_floats[MAX_SCORE_FLOATS];

/* --- Event text floaters (CRIT!, STRIKE!, HIT, VENT!) --- */
extern EventFloat event_floats[MAX_EVENT_FLOATS];

/* --- Upgrade selection --- */
extern UpgradeType upgrade_options[3];
extern int         selected_option;

/* --- Camera & world --- */
extern Vec2  camera_pos;
extern int   player_zone;
extern float player_dist_origin;
extern float home_station_angle;

/* --- Zone transition banner --- */
extern float zone_banner_timer;
extern int   zone_banner_prev_zone;

/* --- Chronicle chord harmonics --- */
extern float orb_chord_timer;
extern int   orb_chord_note;
extern int   minimap_visible;

/* --- Screen shake --- */
extern float screen_shake_timer;
extern float screen_shake_intensity;

/* --- Menu navigation --- */
extern int      menu_selection;
extern GameState settings_back_state;

/* --- Settings --- */
extern int  settings_volume;
extern int  settings_sfx_vol;
extern int  settings_music_vol;
extern int  settings_dynamic_range;
extern int  settings_mute_unfocused;
extern int settings_fullscreen;
extern int settings_glow;
extern int       settings_tab;
extern int settings_screen_shake;
extern int settings_show_fps;
extern int settings_mouse_aim;
extern int settings_crosshair_style;
extern int settings_autofire;
extern int settings_controller_deadzone;
extern int settings_invert_y;
extern int settings_control_scheme;

/* --- New unified settings (FF7R/CoQ HUD overhaul) --- */
extern Settings  g_settings;
extern int       settings_row;
extern int       settings_keybind_pending;
extern uint32_t  g_world_seed;
/* Note: settings_tab already declared above as static int settings_tab = 0 */

/* --- Key binds --- */
extern SDL_Scancode keybinds[KB_COUNT];
extern const char *kb_action_names[KB_COUNT];
extern int rebinding_action;
extern int keybind_selection;
extern int keybind_page;

/* --- Controller binds --- */
extern SDL_GameController *g_controller;
extern SDL_GameControllerButton ctrl_binds[CT_COUNT];
extern const char *ct_action_names[CT_COUNT];
extern int ctrl_rebinding_action;

/* --- Twin-stick fire direction override --- */
extern float twin_stick_fire_angle;
extern int   twin_stick_fire_active;

/* --- Resource system --- */
extern int res_void_stone;
extern int res_void_steel;
extern int res_autodyne_frags;
extern int res_hex_modules;
extern int res_ammo;
extern int res_rockets;
extern int res_contraband;
extern int res_isotopes;
extern int res_coolant;
extern int res_medicinals;
extern int res_biomatter;
extern int res_shield_caps;
extern int res_alien_flora;
extern int chrome;

/* --- Fuel system --- */
extern float fuel_current;
extern float fuel_max;
extern float drift_penalty_timer;

/* --- Player death animation --- */
extern Vec2  player_death_pos;
extern float player_death_angle;
extern float player_death_timer;

/* --- Shop & warp menu --- */
extern int     shop_page;
extern int     shop_sel;
extern WarpLoc warp_locs[MAX_WARP_LOCS];
extern int     warp_loc_count;
extern int     warp_menu_sel;
extern float   warp_drive_range;

/* --- Attract / idle --- */
extern int   is_attract_ai;
extern float idle_timer;

/* --- Mirror image state --- */
extern Vec2  mirror_pos;
extern float mirror_angle;
extern int   mirror_active_flag;

/* --- Cheat / debug --- */
extern int   god_mode;
extern float god_mode_msg_timer;

/* --- FPS counter --- */
extern float fps_accum;
extern int   fps_frame_count;
extern int   fps_display_val;

/* --- Enemy wave pacing --- */
extern float level_start_timer;
extern float ufo_spawn_timer;
extern float beat_timer;
extern int   cur_beat;

/* --- Edge/XP warning flash --- */
extern float edge_flash_timer;
extern float near_miss_cooldown;

/* --- Warp-drive singularity exit effect ---
extern * Counts down from WARP_FLASH_DUR after a warp jump. Drives an expanding;
extern * vector ring + CRT bloom flash rendered in screen space (see render_overlays). */;
extern float       warp_flash_timer;
extern const float WARP_FLASH_DUR;

/* --- Respawn animation state --- */
extern int   respawn_phase;
extern float respawn_blink;

/* --- Cached asteroid count (written by update_asteroids, read by update_spawning) --- */
extern int cached_active_asteroids;

/* --- Void Stone Mechanics --- */
extern int time_stop_frames;
#define POS_HISTORY_LEN 100
extern Vec2 player_pos_history[POS_HISTORY_LEN];
extern int pos_history_idx;
extern int check_void_stone(void);
extern return 0;
extern };



#endif
