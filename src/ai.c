#include "ai.h"
#include <math.h>

// Rigid hexagon formation for Ascian swarm
void ai_update_ascian_swarm(ai_entity_t* entities, int count, float dt) {
    if (count <= 0 || !entities) return;

    int max_squad = 6;
    int dead = max_squad - count;
    if (dead < 0) dead = 0;
    float speed_mult = 1.0f + dead * 0.15f;

    // Find center of mass
    float cx = 0, cy = 0;
    for (int i = 0; i < count; ++i) {
        cx += entities[i].x;
        cy += entities[i].y;
    }
    cx /= count;
    cy /= count;

    float radius = 50.0f;
    float pi = 3.14159265358979323846f;

    for (int i = 0; i < count; ++i) {
        // Hexagon position relative to center
        float angle = i * (2.0f * pi / max_squad);
        float target_x = cx + radius * cosf(angle);
        float target_y = cy + radius * sinf(angle);
        
        // Steer towards target slot
        float dir_x = target_x - entities[i].x;
        float dir_y = target_y - entities[i].y;
        
        entities[i].vx += dir_x * 0.01f * speed_mult * dt;
        entities[i].vy += dir_y * 0.01f * speed_mult * dt;
        
        // Update positions
        entities[i].x += entities[i].vx * speed_mult * dt;
        entities[i].y += entities[i].vy * speed_mult * dt;
    }
}

// Orbiting behavior for Pelgrane
void ai_update_pelgrane(ai_entity_t* pelgrane, ai_entity_t* target, float dt) {
    if (!pelgrane || !target) return;
    
    float dx = target->x - pelgrane->x;
    float dy = target->y - pelgrane->y;
    float dist = sqrtf(dx * dx + dy * dy);
    
    if (dist > 0.001f) {
        // Normal vector
        float nx = dx / dist;
        float ny = dy / dist;
        
        // Tangent vector for orbit
        float tx = -ny;
        float ty = nx;
        
        // Orbit and slowly approach
        pelgrane->vx = (tx * 50.0f + nx * 10.0f);
        pelgrane->vy = (ty * 50.0f + ny * 10.0f);
    }
    
    pelgrane->x += pelgrane->vx * dt;
    pelgrane->y += pelgrane->vy * dt;
}
