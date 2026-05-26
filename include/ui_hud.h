#ifndef UI_HUD_H
#define UI_HUD_H

#include <stdint.h>
#include "state.h"
#include "game_data.h"
#include "game.h"
#include "vector_graphics.h"

void render_hud(void);
void render_minimap(void);
void render_overlays(void);
void render_menus(void);

#endif
