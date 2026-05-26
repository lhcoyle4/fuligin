#ifndef AI_H
#define AI_H

typedef struct {
    float x, y;
    float vx, vy;
} ai_entity_t;

void ai_update_ascian_swarm(ai_entity_t* entities, int count, float dt);
void ai_update_pelgrane(ai_entity_t* pelgrane, ai_entity_t* target, float dt);

#endif // AI_H
