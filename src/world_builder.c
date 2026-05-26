#include "world_builder.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>

void wb_init_region(WorldRegion* region, float ast_density, float en_rate, int lives) {
    memset(region, 0, sizeof(WorldRegion));
    region->asteroid_density = ast_density;
    region->enemy_spawn_rate = en_rate;
    region->base_lives = lives;
}

void wb_generate_chondrite_gyre(WorldRegion* region, int width, int height) {
    // A simple Cellular Automata inspired generator (Caves of Qud aesthetic)
    int grid_w = width / 40;
    int grid_h = height / 40;
    if (grid_w <= 0 || grid_h <= 0) return;
    
    int* cells = (int*)calloc(grid_w * grid_h, sizeof(int));
    int* temp = (int*)calloc(grid_w * grid_h, sizeof(int));
    
    // Random initialization
    for (int y = 0; y < grid_h; y++) {
        for (int x = 0; x < grid_w; x++) {
            float r = (float)rand() / (float)RAND_MAX;
            if (r < region->asteroid_density) {
                cells[y * grid_w + x] = 1;
            }
        }
    }
    
    // 3 steps of cellular automata
    for (int step = 0; step < 3; step++) {
        for (int y = 0; y < grid_h; y++) {
            for (int x = 0; x < grid_w; x++) {
                int neighbors = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < grid_w && ny >= 0 && ny < grid_h) {
                            neighbors += cells[ny * grid_w + nx];
                        } else {
                            neighbors++; // edge is a wall
                        }
                    }
                }
                if (cells[y * grid_w + x] == 1) {
                    temp[y * grid_w + x] = (neighbors >= 3) ? 1 : 0;
                } else {
                    temp[y * grid_w + x] = (neighbors >= 5) ? 1 : 0;
                }
            }
        }
        memcpy(cells, temp, grid_w * grid_h * sizeof(int));
    }
    
    // Populate the asteroids array
    int count = 0;
    for (int y = 0; y < grid_h; y++) {
        for (int x = 0; x < grid_w; x++) {
            if (cells[y * grid_w + x] == 1 && count < MAX_CHONDRITES) {
                region->asteroids[count].position.x = x * 40.0f + 20.0f;
                region->asteroids[count].position.y = y * 40.0f + 20.0f;
                region->asteroids[count].radius = 15.0f + ((float)rand() / (float)RAND_MAX) * 10.0f;
                region->asteroids[count].active = true;
                count++;
            }
        }
    }
    
    free(cells);
    free(temp);
}

void wb_spawn_ascian_swarm(WorldRegion* region, int count, float x, float y) {
    int spawned = 0;
    for (int i = 0; i < MAX_ASCIANS && spawned < count; i++) {
        if (!region->ascians[i].active) {
            region->ascians[i].position.x = x + ((float)rand() / (float)RAND_MAX - 0.5f) * 100.0f;
            region->ascians[i].position.y = y + ((float)rand() / (float)RAND_MAX - 0.5f) * 100.0f;
            
            float angle = ((float)rand() / (float)RAND_MAX) * 3.14159f * 2.0f;
            region->ascians[i].velocity.x = cosf(angle) * 50.0f;
            region->ascians[i].velocity.y = sinf(angle) * 50.0f;
            
            region->ascians[i].health = 10.0f;
            region->ascians[i].active = true;
            spawned++;
        }
    }
}

void wb_trigger_cinnablast(WorldRegion* region, float x, float y, float intensity, float duration) {
    region->active_flare.origin.x = x;
    region->active_flare.origin.y = y;
    region->active_flare.radius = 0.0f;
    region->active_flare.intensity = intensity;
    region->active_flare.duration = duration;
    region->active_flare.active = true;
}

void wb_update_flare(WorldRegion* region, float dt) {
    if (region->active_flare.active) {
        // Expand the flare outward over time
        region->active_flare.radius += region->active_flare.intensity * dt * 2.0f;
        region->active_flare.duration -= dt;
        if (region->active_flare.duration <= 0.0f) {
            region->active_flare.active = false;
        }
    }
}
