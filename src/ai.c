#include "ai.h"
#include <math.h>

// Flocking behavior for Ascian swarm
void ai_update_ascian_swarm(ai_entity_t* entities, int count, float dt) {
    if (count <= 0 || !entities) return;

    for (int i = 0; i < count; ++i) {
        float cx = 0, cy = 0;
        int neighbors = 0;
        
        // Simple cohesion
        for (int j = 0; j < count; ++j) {
            if (i == j) continue;
            float dx = entities[j].x - entities[i].x;
            float dy = entities[j].y - entities[i].y;
            float dist2 = dx * dx + dy * dy;
            if (dist2 < 10000.0f) { // Arbitrary radius
                cx += entities[j].x;
                cy += entities[j].y;
                neighbors++;
            }
        }
        
        if (neighbors > 0) {
            cx /= neighbors;
            cy /= neighbors;
            
            // Steer towards center of mass
            float dir_x = cx - entities[i].x;
            float dir_y = cy - entities[i].y;
            
            entities[i].vx += dir_x * 0.01f * dt;
            entities[i].vy += dir_y * 0.01f * dt;
        }
        
        // Update positions
        entities[i].x += entities[i].vx * dt;
        entities[i].y += entities[i].vy * dt;
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
