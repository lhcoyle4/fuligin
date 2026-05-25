#ifndef WORLD_BUILDER_H
#define WORLD_BUILDER_H

#include <stdbool.h>

#define MAX_CHONDRITES 500
#define MAX_ASCIANS 50
#define MAX_PELGRANES 10
#define MAX_RELIQUARIES 20
#define MAX_BAETYLS 5

// Modular Vector
typedef struct {
    float x;
    float y;
} WB_Vec2;

// The Player's theoretical start point
typedef struct {
    WB_Vec2 position;
} Entity_Cugel;

// Standard Drones (Geometric flight paths)
typedef struct {
    WB_Vec2 position;
    WB_Vec2 velocity;
    bool active;
    float health;
} Entity_Ascian;

// Elite Interceptors (Erratic flight paths)
typedef struct {
    WB_Vec2 position;
    WB_Vec2 velocity;
    bool active;
    float health;
    float erratic_timer;
} Entity_Pelgrane;

// Bosses
typedef struct {
    WB_Vec2 position;
    bool active;
    float health;
    int phase;
} Entity_Autarch;

// Loot Crates
typedef struct {
    WB_Vec2 position;
    bool active;
    int chrome_value;
} Entity_Reliquary;

// Stationary Shops
typedef struct {
    WB_Vec2 position;
    bool active;
    int required_chrome;
    int upgrade_type;
} Entity_Baetyl;

// Procedural Asteroids
typedef struct {
    WB_Vec2 position;
    float radius;
    bool active;
} Chondrite_Asteroid;

// Environmental Hazard
typedef struct {
    WB_Vec2 origin;
    float radius;
    float intensity; // Pushing force
    bool active;
    float duration;
} Cinnablast_Flare;

// Region Settings and State
typedef struct {
    float asteroid_density; // 0.0 to 1.0
    float enemy_spawn_rate;
    int base_lives;
    
    // Arrays for generated entities
    Chondrite_Asteroid asteroids[MAX_CHONDRITES];
    Entity_Ascian ascians[MAX_ASCIANS];
    Entity_Pelgrane pelgranes[MAX_PELGRANES];
    Entity_Reliquary reliquaries[MAX_RELIQUARIES];
    Entity_Baetyl baetyls[MAX_BAETYLS];
    Cinnablast_Flare active_flare;
} WorldRegion;

// Core generation functions
void wb_init_region(WorldRegion* region, float ast_density, float en_rate, int lives);
void wb_generate_chondrite_gyre(WorldRegion* region, int width, int height);
void wb_spawn_ascian_swarm(WorldRegion* region, int count, float x, float y);
void wb_trigger_cinnablast(WorldRegion* region, float x, float y, float intensity, float duration);
void wb_update_flare(WorldRegion* region, float dt);

#endif // WORLD_BUILDER_H
