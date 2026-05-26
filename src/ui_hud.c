#include "ui_hud.h"
#include "ui.h"
#include "vector_font.h"
#include "audio.h"
#include "game.h"
#include "vector_graphics.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

extern SDL_Renderer *g_renderer;

#define HUD_SCORE_X          42.0f
#define HUD_SCORE_Y          30.0f
#define HUD_TOPSCORE_Y       30.0f
#define HUD_LEVEL_Y          30.0f
#define HUD_COMBO_Y          68.0f
#define HUD_LIVES_Y         108.0f
#define HUD_LIVES_X_BASE     50.0f
#define HUD_LIVES_STEP       26.0f
#define HUD_UPGRADE_X    (SCREEN_WIDTH - 72.0f)
#define HUD_UPGRADE_Y0      140.0f
#define HUD_UPGRADE_STEP     20.0f

/**
 * @brief Returns the screen-space position of a gamepad button on the
 *        vector controller diagram centred at (cx, cy).
 */
static Vec2 get_controller_button_pos(SDL_GameControllerButton btn,
                                      float cx, float cy)
{
    float f_cx = cx + 70.0f;
    float f_cy = cy - 20.0f;
    switch (btn) {
        case SDL_CONTROLLER_BUTTON_A:
            return (Vec2){f_cx, f_cy + 25.0f};
        case SDL_CONTROLLER_BUTTON_B:
            return (Vec2){f_cx + 25.0f, f_cy};
        case SDL_CONTROLLER_BUTTON_X:
            return (Vec2){f_cx - 25.0f, f_cy};
        case SDL_CONTROLLER_BUTTON_Y:
            return (Vec2){f_cx, f_cy - 25.0f};
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
            return (Vec2){cx - 82.0f, cy - 63.0f};
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
            return (Vec2){cx + 82.0f, cy - 63.0f};
        case SDL_CONTROLLER_BUTTON_BACK:
            return (Vec2){cx - 20.0f, cy - 20.0f};
        case SDL_CONTROLLER_BUTTON_START:
            return (Vec2){cx + 20.0f, cy - 20.0f};
        case SDL_CONTROLLER_BUTTON_LEFTSTICK:
            return (Vec2){cx - 70.0f, cy - 20.0f};
        case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
            return (Vec2){cx + 30.0f, cy + 30.0f};
        case SDL_CONTROLLER_BUTTON_DPAD_UP:
            return (Vec2){cx - 30.0f, cy + 15.0f};
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            return (Vec2){cx - 30.0f, cy + 45.0f};
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            return (Vec2){cx - 45.0f, cy + 30.0f};
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            return (Vec2){cx - 15.0f, cy + 30.0f};
        case SDL_CONTROLLER_BUTTON_GUIDE:
            return (Vec2){cx, cy - 20.0f};
        default:
            return (Vec2){cx, cy};
    }
}

/**
 * @brief Draws a vector-art gamepad shell centred at (cx, cy).
 *        highlighted_btn is an SDL_GameControllerButton value; the
 *        matching button is drawn in the highlight colour.
 */
static void draw_gamepad(float cx, float cy, int highlighted_btn)
{
    SDL_Color base_color      = (SDL_Color){100, 255, 255, 120};
    SDL_Color highlight_color = (SDL_Color){255, 220,  80, 255};

    /* 1. Controller outer shell */
    Line shell[] = {
        {{cx - 130, cy - 60}, {cx + 130, cy - 60}},
        {{cx - 130, cy - 60}, {cx - 170, cy - 30}},
        {{cx + 130, cy - 60}, {cx + 170, cy - 30}},
        {{cx - 170, cy - 30}, {cx - 145, cy + 95}},
        {{cx + 170, cy - 30}, {cx + 145, cy + 95}},
        {{cx - 145, cy + 95}, {cx -  95, cy + 95}},
        {{cx + 145, cy + 95}, {cx +  95, cy + 95}},
        {{cx -  95, cy + 95}, {cx -  60, cy + 25}},
        {{cx +  95, cy + 95}, {cx +  60, cy + 25}},
        {{cx -  60, cy + 25}, {cx,        cy + 40}},
        {{cx,        cy + 40}, {cx +  60, cy + 25}}
    };
    Shape shell_shape = {shell, sizeof(shell) / sizeof(Line), base_color};
    vg_draw_shape(&shell_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 2. Bumpers */
    SDL_Color lb_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_LEFTSHOULDER)
                         ? highlight_color : base_color;
    Line lb[] = {
        {{cx - 125, cy - 68}, {cx -  40, cy - 64}},
        {{cx -  40, cy - 64}, {cx -  43, cy - 58}},
        {{cx -  43, cy - 58}, {cx - 120, cy - 62}},
        {{cx - 120, cy - 62}, {cx - 125, cy - 68}}
    };
    Shape lb_shape = {lb, sizeof(lb) / sizeof(Line), lb_color};
    vg_draw_shape(&lb_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    SDL_Color rb_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)
                         ? highlight_color : base_color;
    Line rb[] = {
        {{cx + 125, cy - 68}, {cx +  40, cy - 64}},
        {{cx +  40, cy - 64}, {cx +  43, cy - 58}},
        {{cx +  43, cy - 58}, {cx + 120, cy - 62}},
        {{cx + 120, cy - 62}, {cx + 125, cy - 68}}
    };
    Shape rb_shape = {rb, sizeof(rb) / sizeof(Line), rb_color};
    vg_draw_shape(&rb_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 3. Triggers (LT / RT) */
    Line lt[] = {
        {{cx - 120, cy - 83}, {cx -  70, cy - 80}},
        {{cx -  70, cy - 80}, {cx -  68, cy - 70}},
        {{cx -  68, cy - 70}, {cx - 115, cy - 73}},
        {{cx - 115, cy - 73}, {cx - 120, cy - 83}}
    };
    Shape lt_shape = {lt, sizeof(lt) / sizeof(Line), base_color};
    vg_draw_shape(&lt_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line rt[] = {
        {{cx + 120, cy - 83}, {cx +  70, cy - 80}},
        {{cx +  70, cy - 80}, {cx +  68, cy - 70}},
        {{cx +  68, cy - 70}, {cx + 115, cy - 73}},
        {{cx + 115, cy - 73}, {cx + 120, cy - 83}}
    };
    Shape rt_shape = {rt, sizeof(rt) / sizeof(Line), base_color};
    vg_draw_shape(&rt_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 4. Sticks (LSTICK & RSTICK) */
    float ls_x = cx - 70.0f, ls_y = cy - 20.0f;
    SDL_Color ls_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_LEFTSTICK)
                         ? highlight_color : base_color;
    Line ls_circle[12];
    for (int i = 0; i < 12; i++) {
        float a1 = i       * 2.0f * (float)M_PI / 12.0f;
        float a2 = (i + 1) * 2.0f * (float)M_PI / 12.0f;
        ls_circle[i].p1 = (Vec2){ls_x + cosf(a1) * 20.0f, ls_y + sinf(a1) * 20.0f};
        ls_circle[i].p2 = (Vec2){ls_x + cosf(a2) * 20.0f, ls_y + sinf(a2) * 20.0f};
    }
    Shape ls_shape = {ls_circle, 12, ls_color};
    vg_draw_shape(&ls_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line ls_cross[] = {
        {{ls_x - 12.0f, ls_y}, {ls_x + 12.0f, ls_y}},
        {{ls_x, ls_y - 12.0f}, {ls_x, ls_y + 12.0f}}
    };
    Shape ls_cross_shape = {ls_cross, 2, ls_color};
    vg_draw_shape(&ls_cross_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    float rs_x = cx + 30.0f, rs_y = cy + 30.0f;
    SDL_Color rs_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_RIGHTSTICK)
                         ? highlight_color : base_color;
    Line rs_circle[12];
    for (int i = 0; i < 12; i++) {
        float a1 = i       * 2.0f * (float)M_PI / 12.0f;
        float a2 = (i + 1) * 2.0f * (float)M_PI / 12.0f;
        rs_circle[i].p1 = (Vec2){rs_x + cosf(a1) * 20.0f, rs_y + sinf(a1) * 20.0f};
        rs_circle[i].p2 = (Vec2){rs_x + cosf(a2) * 20.0f, rs_y + sinf(a2) * 20.0f};
    }
    Shape rs_shape = {rs_circle, 12, rs_color};
    vg_draw_shape(&rs_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line rs_cross[] = {
        {{rs_x - 12.0f, rs_y}, {rs_x + 12.0f, rs_y}},
        {{rs_x, rs_y - 12.0f}, {rs_x, rs_y + 12.0f}}
    };
    Shape rs_cross_shape = {rs_cross, 2, rs_color};
    vg_draw_shape(&rs_cross_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 5. D-pad */
    float dp_x = cx - 30.0f, dp_y = cy + 30.0f;
    SDL_Color dp_u_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_DPAD_UP)
                           ? highlight_color : base_color;
    SDL_Color dp_d_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN)
                           ? highlight_color : base_color;
    SDL_Color dp_l_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT)
                           ? highlight_color : base_color;
    SDL_Color dp_r_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
                           ? highlight_color : base_color;

    Line dp_top[] = {
        {{dp_x - 6.0f, dp_y - 20.0f}, {dp_x + 6.0f, dp_y - 20.0f}},
        {{dp_x - 6.0f, dp_y - 20.0f}, {dp_x - 6.0f, dp_y -  6.0f}},
        {{dp_x + 6.0f, dp_y - 20.0f}, {dp_x + 6.0f, dp_y -  6.0f}}
    };
    Shape dp_top_shape = {dp_top, 3, dp_u_color};
    vg_draw_shape(&dp_top_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line dp_bot[] = {
        {{dp_x - 6.0f, dp_y + 20.0f}, {dp_x + 6.0f, dp_y + 20.0f}},
        {{dp_x - 6.0f, dp_y + 20.0f}, {dp_x - 6.0f, dp_y +  6.0f}},
        {{dp_x + 6.0f, dp_y + 20.0f}, {dp_x + 6.0f, dp_y +  6.0f}}
    };
    Shape dp_bot_shape = {dp_bot, 3, dp_d_color};
    vg_draw_shape(&dp_bot_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line dp_left[] = {
        {{dp_x - 20.0f, dp_y - 6.0f}, {dp_x - 20.0f, dp_y + 6.0f}},
        {{dp_x - 20.0f, dp_y - 6.0f}, {dp_x -  6.0f, dp_y - 6.0f}},
        {{dp_x - 20.0f, dp_y + 6.0f}, {dp_x -  6.0f, dp_y + 6.0f}}
    };
    Shape dp_left_shape = {dp_left, 3, dp_l_color};
    vg_draw_shape(&dp_left_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line dp_right[] = {
        {{dp_x + 20.0f, dp_y - 6.0f}, {dp_x + 20.0f, dp_y + 6.0f}},
        {{dp_x + 20.0f, dp_y - 6.0f}, {dp_x +  6.0f, dp_y - 6.0f}},
        {{dp_x + 20.0f, dp_y + 6.0f}, {dp_x +  6.0f, dp_y + 6.0f}}
    };
    Shape dp_right_shape = {dp_right, 3, dp_r_color};
    vg_draw_shape(&dp_right_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line dp_mid[] = {
        {{dp_x - 6.0f, dp_y - 6.0f}, {dp_x + 6.0f, dp_y - 6.0f}},
        {{dp_x + 6.0f, dp_y - 6.0f}, {dp_x + 6.0f, dp_y + 6.0f}},
        {{dp_x + 6.0f, dp_y + 6.0f}, {dp_x - 6.0f, dp_y + 6.0f}},
        {{dp_x - 6.0f, dp_y + 6.0f}, {dp_x - 6.0f, dp_y - 6.0f}}
    };
    Shape dp_mid_shape = {dp_mid, 4, base_color};
    vg_draw_shape(&dp_mid_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 6. Face buttons (A, B, X, Y) */
    float f_cx = cx + 70.0f, f_cy = cy - 20.0f;
    SDL_Color a_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_A) ? highlight_color : base_color;
    SDL_Color b_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_B) ? highlight_color : base_color;
    SDL_Color x_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_X) ? highlight_color : base_color;
    SDL_Color y_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_Y) ? highlight_color : base_color;
    float btn_r = 7.0f;
    float btn_positions[][2] = {
        {f_cx,        f_cy + 25.0f},   /* A */
        {f_cx + 25.0f, f_cy},           /* B */
        {f_cx - 25.0f, f_cy},           /* X */
        {f_cx,        f_cy - 25.0f}    /* Y */
    };
    SDL_Color btn_colors[] = {a_color, b_color, x_color, y_color};
    const char *btn_lbls[] = {"A", "B", "X", "Y"};
    for (int b = 0; b < 4; b++) {
        float bx = btn_positions[b][0];
        float by = btn_positions[b][1];
        Line b_circle[8];
        for (int i = 0; i < 8; i++) {
            float a1 = i       * 2.0f * (float)M_PI / 8.0f;
            float a2 = (i + 1) * 2.0f * (float)M_PI / 8.0f;
            b_circle[i].p1 = (Vec2){bx + cosf(a1) * btn_r, by + sinf(a1) * btn_r};
            b_circle[i].p2 = (Vec2){bx + cosf(a2) * btn_r, by + sinf(a2) * btn_r};
        }
        Shape b_shape = {b_circle, 8, btn_colors[b]};
        vg_draw_shape(&b_shape, (Vec2){0, 0}, 0.0f, 1.0f);
        vf_draw_string(btn_lbls[b], bx - 3.5f, by - 5.0f, 10.0f, btn_colors[b]);
    }

    /* 7. Back & Start buttons */
    SDL_Color back_color  = (highlighted_btn == SDL_CONTROLLER_BUTTON_BACK)
                            ? highlight_color : base_color;
    SDL_Color start_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_START)
                            ? highlight_color : base_color;
    Line back_btn[4] = {
        {{cx - 25.0f, cy - 23.0f}, {cx - 15.0f, cy - 23.0f}},
        {{cx - 15.0f, cy - 23.0f}, {cx - 15.0f, cy - 17.0f}},
        {{cx - 15.0f, cy - 17.0f}, {cx - 25.0f, cy - 17.0f}},
        {{cx - 25.0f, cy - 17.0f}, {cx - 25.0f, cy - 23.0f}}
    };
    Shape back_shape = {back_btn, 4, back_color};
    vg_draw_shape(&back_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line start_btn[4] = {
        {{cx + 15.0f, cy - 23.0f}, {cx + 25.0f, cy - 23.0f}},
        {{cx + 25.0f, cy - 23.0f}, {cx + 25.0f, cy - 17.0f}},
        {{cx + 25.0f, cy - 17.0f}, {cx + 15.0f, cy - 17.0f}},
        {{cx + 15.0f, cy - 17.0f}, {cx + 15.0f, cy - 23.0f}}
    };
    Shape start_shape = {start_btn, 4, start_color};
    vg_draw_shape(&start_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    /* 8. Guide button (centre circle with X mark) */
    SDL_Color guide_color = (highlighted_btn == SDL_CONTROLLER_BUTTON_GUIDE)
                            ? highlight_color : base_color;
    Line guide_circle[10];
    for (int i = 0; i < 10; i++) {
        float a1 = i       * 2.0f * (float)M_PI / 10.0f;
        float a2 = (i + 1) * 2.0f * (float)M_PI / 10.0f;
        guide_circle[i].p1 = (Vec2){cx + cosf(a1) * 11.0f, cy - 20.0f + sinf(a1) * 11.0f};
        guide_circle[i].p2 = (Vec2){cx + cosf(a2) * 11.0f, cy - 20.0f + sinf(a2) * 11.0f};
    }
    Shape guide_shape = {guide_circle, 10, guide_color};
    vg_draw_shape(&guide_shape, (Vec2){0, 0}, 0.0f, 1.0f);

    Line guide_x[] = {
        {{cx - 4.0f, cy - 24.0f}, {cx + 4.0f, cy - 16.0f}},
        {{cx - 4.0f, cy - 16.0f}, {cx + 4.0f, cy - 24.0f}}
    };
    Shape guide_x_shape = {guide_x, 2, guide_color};
    vg_draw_shape(&guide_x_shape, (Vec2){0, 0}, 0.0f, 1.0f);
}

/* =========== CONTROLLER BINDS PAGE =========== */

/**
 * @brief Renders the controller-keybind page: the gamepad diagram plus
 *        labelled connector lines from each action to its bound button.
 */
static void render_controller_binds_page(float cx, float cy,
                                         int keybind_selection,
                                         SDL_GameControllerButton *ctrl_binds,
                                         const char **ct_action_names)
{
    static const char *btn_names[] = {
        "A", "B", "X", "Y", "BACK", "GUIDE", "START",
        "LSTICK", "RSTICK", "LB", "RB",
        "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT", "MISC"
    };

    vf_draw_string_centered("ENTER TO REBIND   Q/E TO SWITCH",
                            SCREEN_WIDTH / 2.0f, 100, 12.0f,
                            (SDL_Color){100, 100, 120, 255});

    int highlighted_btn = (int)ctrl_binds[keybind_selection];
    draw_gamepad(cx, cy, highlighted_btn);

    for (int i = 0; i < CT_COUNT; i++) {
        int is_selected = (keybind_selection == i);
        SDL_Color color      = is_selected
                               ? (SDL_Color){255, 220,  80, 255}
                               : (SDL_Color){100, 255, 255, 255};
        SDL_Color line_color = is_selected
                               ? (SDL_Color){255, 220,  80, 255}
                               : (SDL_Color){100, 255, 255,  60};

        int bval = (int)ctrl_binds[i];
        const char *bname = (bval >= 0 && bval < 16) ? btn_names[bval] : "UNBOUND";

        char row[64];
        sprintf(row, "%s: %s", ct_action_names[i], bname);

        float ax, ay, ax_mid;
        if (i < 4) {
            /* Left column */
            ax     = 390.0f;
            ay     = 220.0f + i * 110.0f;
            ax_mid = ax + 40.0f;
            vf_draw_string(row, 120.0f, ay - 6.0f, 15.0f, color);
            if (is_selected)
                vf_draw_string(">", 95.0f, ay - 6.0f, 15.0f, color);
        } else {
            /* Right column */
            ax     = 890.0f;
            ay     = 220.0f + (i - 4) * 110.0f;
            ax_mid = ax - 40.0f;
            vf_draw_string(row, 920.0f, ay - 6.0f, 15.0f, color);
            if (is_selected)
                vf_draw_string(">", 895.0f, ay - 6.0f, 15.0f, color);
        }

        if (bval >= 0 && bval < 16) {
            Vec2 bpos = get_controller_button_pos(ctrl_binds[i], cx, cy);

            Line conn[2] = {
                {{ax,     ay}, {ax_mid, ay}},
                {{ax_mid, ay}, {bpos.x, bpos.y}}
            };
            Shape conn_shape = {conn, 2, line_color};
            vg_draw_shape(&conn_shape, (Vec2){0, 0}, 0.0f, 1.0f);

            Line dot_anchor[] = {
                {{ax - 1.0f, ay},       {ax + 1.0f, ay}},
                {{ax,        ay - 1.0f},{ax,         ay + 1.0f}}
            };
            Shape dot_a_shape = {dot_anchor, 2, line_color};
            vg_draw_shape(&dot_a_shape, (Vec2){0, 0}, 0.0f, 1.0f);

            Line dot_btn[] = {
                {{bpos.x - 1.0f, bpos.y},       {bpos.x + 1.0f, bpos.y}},
                {{bpos.x,        bpos.y - 1.0f}, {bpos.x,        bpos.y + 1.0f}}
            };
            Shape dot_b_shape = {dot_btn, 2, line_color};
            vg_draw_shape(&dot_b_shape, (Vec2){0, 0}, 0.0f, 1.0f);
        }
    }
}

/* ────────────────────────────────────────────────────────────────────── */

/**
 * @brief Renders the HUD: score, lives, fuel bar, combo indicator,
 *        XP bar, resource readout, and active-upgrade icon strip.
 *
 * Camera must be reset to (0,0) before this call.
 */
void render_hud(void)
{
    char hud_text[64];

    /* Resolve zone color for border accents */
    SDL_Color zone_color = ui_zone_color(player_zone);

    /* ── Atmosphere passes — drawn before all panels ── */
    if (g_settings.graphics.vignette)
        ui_vignette(g_renderer);
    if (g_settings.graphics.particle_count > 0)
        ui_particle_drift(g_renderer, game_time, g_settings.graphics.particle_count);

    /* PLACEHOLDER: old panel rendering follows — will be replaced by angled panels */
    ui_particle_drift(g_renderer, game_time, 0); /* no-op, kept for branch parity */

    /* ================================================================
     * TOP-LEFT PANEL — [SCORE] / ZONE / COMBO  (FF7R angled cut)
     * ================================================================ */
    {
        float px = HUD_TL_X, py = HUD_TL_Y, pw = HUD_TL_W, ph = HUD_TL_H;
        float pad = HUD_PAD_INNER;

        ui_panel_angled(g_renderer, px, py, pw, ph, HUD_TL_CUT, zone_color);

        /* Row 1: [SCORE] label + score value */
        float row1_y = py + pad;
        vf_draw_string("[SCORE]", px + pad + HUD_TL_CUT, row1_y, 9, HUD_TEXT_DIM);
        sprintf(hud_text, "%08d", score);
        {
            float tw = (float)strlen(hud_text) * 9.0f * 1.1f;
            vf_draw_string(hud_text, px + pw - pad - tw, row1_y, 16, HUD_TEXT_GOLD);
        }

        /* Separator */
        float sep_y = row1_y + HUD_ROW_H + 4.0f;
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer,
            HUD_BORDER_MID.r, HUD_BORDER_MID.g, HUD_BORDER_MID.b, 80);
        SDL_RenderDrawLineF(g_renderer, px + pad, sep_y, px + pw - pad, sep_y);

        /* Row 2: ZONE: name | LVL: N */
        static const char *tl_zone_names[] = {
            "HOME SPACE", "INNER BELT", "DEEP VOID", "THE ABYSS", "DEEP DRIFT"
        };
        float row2_y = sep_y + 5.0f;
        vf_draw_string("ZONE:", px + pad + HUD_TL_CUT, row2_y, 9, HUD_TEXT_DIM);
        vf_draw_string(tl_zone_names[player_zone],
                       px + pad + HUD_TL_CUT + 42.0f, row2_y, 9, zone_color);
        sprintf(hud_text, "LVL:%d", player_level);
        vf_draw_string(hud_text, px + pw - pad - 42.0f, row2_y, 9, HUD_TEXT_DIM);

        /* Row 3: COMBO */
        float row3_y = row2_y + HUD_ROW_H + 2.0f;
        ui_combo_multiplier(g_renderer, px + pad + HUD_TL_CUT, row3_y, combo_count, combo_timer);
    }

    /* ================================================================
     * BOTTOM-LEFT PANEL — [HULL] / [CHRON] / [LIVES]  (FF7R angled)
     * ================================================================ */
    {
        float px = HUD_BL_X, py = HUD_BL_Y, pw = HUD_BL_W, ph = HUD_BL_H;
        float pad = HUD_PAD_INNER;

        ui_panel_angled(g_renderer, px, py, pw, ph, HUD_BL_CUT, zone_color);

        float bar_x = px + pad + HUD_BL_CUT + 48.0f;
        float bar_w = pw - (bar_x - px) - pad;

        /* Row 1: [HULL] — placeholder solid bar (no HP struct yet) */
        float row1_y = py + pad;
        vf_draw_string("[HULL]", px + pad + HUD_BL_CUT, row1_y, 9, HUD_TEXT_DIM);
        ui_bar(g_renderer, bar_x, row1_y, bar_w, 8.0f, 1.0f, 1.0f, HUD_TEAL);

        /* Row 2: [CHRON] segmented XP bar */
        float row2_y = row1_y + HUD_ROW_H + 4.0f;
        vf_draw_string("[CHRON]", px + pad + HUD_BL_CUT, row2_y, 9, HUD_TEXT_DIM);
        SDL_Color xp_fill = HUD_GOLD_BAR;
        if (xp_flash_timer > 0.0f && ((int)(xp_flash_timer * 20) % 2 == 0))
            xp_fill = HUD_TEXT_PRIMARY;
        ui_bar_segmented(g_renderer, bar_x, row2_y, bar_w, 8.0f,
                         (float)player_xp, (float)xp_threshold, 2, xp_fill);
        sprintf(hud_text, "%dXP", player_xp);
        {
            float tw = (float)strlen(hud_text) * 7.0f * 1.1f;
            vf_draw_string(hud_text, px + pw - pad - tw, row2_y, 7, HUD_TEXT_DIM);
        }

        /* Row 3: [LIVES] — ship glyphs */
        float row3_y = row2_y + HUD_ROW_H + 4.0f;
        vf_draw_string("[LIVES]", px + pad + HUD_BL_CUT, row3_y, 9, HUD_TEXT_DIM);
        {
            SDL_Color ship_col = HUD_TEXT_CYAN;
            Shape s = {ship_lines, sizeof(ship_lines) / sizeof(Line), ship_col};
            int icon_count = lives - 1;
            if (icon_count < 0) icon_count = 0;
            if (icon_count > 6) icon_count = 6;
            for (int i = 0; i < icon_count; i++) {
                Vec2 pos = {bar_x + i * 14.0f, row3_y + 4.0f};
                vg_draw_shape(&s, pos, 0.0f, 0.45f);
            }
        }
    }

    /* ================================================================
     * BOTTOM-RIGHT PANEL — [FUEL] / [ZONE] / [AMMO]  (FF7R angled)
     * ================================================================ */
    {
        float px = HUD_BR_X, py = HUD_BR_Y, pw = HUD_BR_W, ph = HUD_BR_H;
        float pad = HUD_PAD_INNER;

        ui_panel_angled(g_renderer, px, py, pw, ph, HUD_BR_CUT, zone_color);

        float bar_x = px + pad + HUD_BR_CUT + 48.0f;
        float bar_w = pw - (bar_x - px) - pad;

        /* Row 1: [FUEL] bar + percent */
        float row1_y = py + pad;
        float fpct = (fuel_max > 0.0f) ? (fuel_current / fuel_max) : 0.0f;
        SDL_Color fc = ui_fuel_color(fpct);
        vf_draw_string("[FUEL]", px + pad + HUD_BR_CUT, row1_y, 9, HUD_TEXT_DIM);
        ui_bar(g_renderer, bar_x, row1_y, bar_w, 8.0f, fuel_current, fuel_max, fc);
        sprintf(hud_text, "%d%%", (int)(fpct * 100.0f));
        {
            float tw = (float)strlen(hud_text) * 7.0f * 1.1f;
            vf_draw_string(hud_text, px + pw - pad - tw, row1_y, 7, fc);
        }

        /* Row 2: [ZONE] — zone name in zone accent color */
        static const char *br_zone_names[] = {
            "HOME SPACE", "INNER BELT", "DEEP VOID", "THE ABYSS", "DEEP DRIFT"
        };
        float row2_y = row1_y + HUD_ROW_H + 4.0f;
        vf_draw_string("[ZONE]", px + pad + HUD_BR_CUT, row2_y, 9, HUD_TEXT_DIM);
        vf_draw_string(br_zone_names[player_zone], bar_x, row2_y, 9, zone_color);

        /* Row 3: [AMMO] block bar */
        float row3_y = row2_y + HUD_ROW_H + 4.0f;
        vf_draw_string("[AMMO]", px + pad + HUD_BR_CUT, row3_y, 9, HUD_TEXT_DIM);
        ui_bar_block(g_renderer, bar_x, row3_y,
                     (float)res_ammo, 12.0f, 12, HUD_AMBER);

        /* Contraband warning */
        if (res_contraband > 0) {
            SDL_Color cb_warn = ui_pulse(HUD_CINNABAR, game_time, 2.0f, 0.4f);
            vf_draw_string("CONTRABAND", px + pad + HUD_BR_CUT, row3_y, 7, cb_warn);
        }
    }

    /* ================================================================
     * RELIC STRIP — upgrade badges along right edge (preserved)
     * ================================================================ */
    if (game_state == STATE_PLAYING || game_state == STATE_ATTRACT_GAMEPLAY) {
        float ix = (float)(SCREEN_WIDTH - 45);
        float iy = HUD_UPGRADE_Y0;
        SDL_Color ic = HUD_TEXT_CYAN;

        if (player_upgrades.triple_shot)
            { vf_draw_string("3X",  ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.max_bounces > 0)
            { vf_draw_string("BNC", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.shield_active)
            { vf_draw_string("SHD", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.piercing)
            { vf_draw_string("PRC", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.homing)
            { vf_draw_string("HOM", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.rear_gun)
            { vf_draw_string("RRG", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.thermal_hull)
            { vf_draw_string("RAM", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.singularity_displacer)
            { vf_draw_string("WRP", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.split_shot)
            { vf_draw_string("SPL", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.resonance_cascade)
            { vf_draw_string("RES", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.mirror_image)
            { vf_draw_string("TWN", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.phase_shift)
            { vf_draw_string("PHS", ix, iy, 9, HUD_AMBER); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.singularity_whip)
            { vf_draw_string("BWP", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        if (player_upgrades.nova_explosion)
            { vf_draw_string("NOV", ix, iy, 9, ic); iy += HUD_UPGRADE_STEP; }
        (void)iy;
    }

    /* FPS counter */
    if (settings_show_fps) {
        sprintf(hud_text, "%d FPS", fps_display_val);
        vf_draw_string(hud_text, 8, 8, 12, HUD_GREEN);
    }
}

/* ────────────────────────────────────────────────────────────────────── */

/**
 * @brief Renders the minimap in the upper-right corner showing relative
 *        entity positions: player (white), asteroids (grey), enemies
 *        (red), NPCs (green), home station (cyan), enemy bullets (red).
 *        Also draws the off-screen home arrow when the station is not
 *        visible in the viewport.
 */
void render_minimap(void)
{
    if (!minimap_visible) return;

    /* Panel frame — rectangular (minimap uses CoQ rectangle style) */
    float panel_x = (float)HUD_TR_X;
    float panel_y = (float)HUD_TR_Y;
    float panel_w = (float)HUD_TR_W;
    float panel_h = (float)HUD_TR_H;
    SDL_Color mm_zone_col = ui_zone_color(player_zone);

    /* Use ui_panel_terminal + ACTIVE border for stronger minimap visibility
     * — matches the unified terminal style and stands out against the void. */
    ui_panel_terminal(g_renderer, panel_x, panel_y, panel_w, panel_h, HUD_BORDER_ACTIVE);
    ui_scanlines(g_renderer, panel_x, panel_y, panel_w, panel_h);

    /* Header */
    vf_draw_string("[MINIMAP]", panel_x + HUD_PAD_INNER,
                   panel_y + HUD_PAD_INNER, 9, HUD_TEXT_DIM);

    /* Separator */
    float mm_sep_y = panel_y + HUD_PAD_INNER + HUD_ROW_H + 2.0f;
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer,
        HUD_BORDER_MID.r, HUD_BORDER_MID.g, HUD_BORDER_MID.b, 80);
    SDL_RenderDrawLineF(g_renderer,
        panel_x + 4.0f, mm_sep_y, panel_x + panel_w - 4.0f, mm_sep_y);

    /* Map area coordinates */
    float mmx  = panel_x + 4.0f;
    float mmy  = mm_sep_y + 4.0f;
    float mmw  = panel_w - 8.0f;
    float mmh  = panel_y + panel_h - mmy - 4.0f;
    float range = 3500.0f;
    float scx  = mmw / (range * 2.0f);
    float scy  = mmh / (range * 2.0f);
    float mcx  = mmx + mmw * 0.5f;
    float mcy  = mmy + mmh * 0.5f;

    /* Draw grid cell background */
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g_renderer,
        HUD_PANEL_DEEP.r, HUD_PANEL_DEEP.g, HUD_PANEL_DEEP.b, HUD_PANEL_DEEP.a);
    {
        int cell_sz = 5;
        int cols = (int)(mmw / cell_sz);
        int rows_mm = (int)(mmh / cell_sz);
        for (int rr = 0; rr < rows_mm; rr++) {
            for (int cc = 0; cc < cols; cc++) {
                SDL_FRect cell = { mmx + cc * cell_sz, mmy + rr * cell_sz, 4.0f, 4.0f };
                SDL_RenderFillRectF(g_renderer, &cell);
            }
        }
    }
    (void)mm_zone_col;

    /* Home station — cyan cross */
    {
        float hx = mcx + (0.0f - player.pos.x) * scx;
        float hy = mcy + (0.0f - player.pos.y) * scy;
        if (hx >= mmx && hx <= mmx + mmw && hy >= mmy && hy <= mmy + mmh) {
            Line hl[2] = {{{hx - 6, hy}, {hx + 6, hy}},
                          {{hx, hy - 6}, {hx, hy + 6}}};
            for (int i = 0; i < 2; i++) {
                Shape s = {&hl[i], 1, (SDL_Color){100, 255, 255, 255}};
                vg_draw_shape(&s, (Vec2){0, 0}, 0.0f, 1.0f);
            }
        }
    }

    /* Asteroids - White radar blips */
    for (int i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) continue;
        float ax = mcx + (asteroids[i].pos.x - player.pos.x) * scx;
        float ay = mcy + (asteroids[i].pos.y - player.pos.y) * scy;
        if (ax < mmx || ax > mmx + mmw || ay < mmy || ay > mmy + mmh) continue;
        ui_minimap_blip(g_renderer, ax, ay, (SDL_Color){255, 255, 255, 255}, 1.0f);
    }

    /* Enemy (UFO) - Red radar blips */
    if (ufo.active) {
        float ux = mcx + (ufo.pos.x - player.pos.x) * scx;
        float uy = mcy + (ufo.pos.y - player.pos.y) * scy;
        if (ux >= mmx && ux <= mmx + mmw && uy >= mmy && uy <= mmy + mmh) {
            ui_minimap_blip(g_renderer, ux, uy, (SDL_Color){255, 0, 0, 255}, 2.0f);
        }
    }

    /* Anomalies/Rifts - Purple radar blips */
    if (rift.active) {
        float rx = mcx + (rift.pos.x - player.pos.x) * scx;
        float ry = mcy + (rift.pos.y - player.pos.y) * scy;
        if (rx >= mmx && rx <= mmx + mmw && ry >= mmy && ry <= mmy + mmh) {
            ui_minimap_blip(g_renderer, rx, ry, (SDL_Color){160, 32, 240, 255}, 3.0f);
        }
    }

    /* NPCs - Green radar blips */
    for (int i = 0; i < MAX_NPC; i++) {
        if (!npcs[i].active) continue;
        float nx = mcx + (npcs[i].pos.x - player.pos.x) * scx;
        float ny = mcy + (npcs[i].pos.y - player.pos.y) * scy;
        if (nx < mmx || nx > mmx + mmw || ny < mmy || ny > mmy + mmh) continue;
        ui_minimap_blip(g_renderer, nx, ny, (SDL_Color){20, 180, 20, 255}, 2.0f);
    }

    /* Player — bright cyan 3×3 rect at map centre */
    SDL_SetRenderDrawColor(g_renderer,
        HUD_TEXT_CYAN.r, HUD_TEXT_CYAN.g, HUD_TEXT_CYAN.b, 255);
    {
        SDL_FRect pdot = {mcx - 1.5f, mcy - 1.5f, 3.0f, 3.0f};
        SDL_RenderFillRectF(g_renderer, &pdot);
    }

    /* Zone label beneath map */
    static const char *mm_zn[] = {
        "HOME SPACE", "INNER BELT", "DEEP VOID", "THE ABYSS", "DEEP DRIFT"
    };
    vf_draw_string_centered(mm_zn[player_zone],
                            panel_x + panel_w * 0.5f,
                            mmy + mmh + 6.0f, 7,
                            ui_zone_color(player_zone));

    /* Off-screen home arrow (preserved) */
    if (player.active) {
        float home_sx = 0.0f - camera_pos.x;
        float home_sy = 0.0f - camera_pos.y;
        int on_screen = (home_sx > 30 && home_sx < SCREEN_WIDTH  - 30 &&
                         home_sy > 30 && home_sy < SCREEN_HEIGHT - 30);
        if (!on_screen) {
            float ddx = home_sx - SCREEN_WIDTH  * 0.5f;
            float ddy = home_sy - SCREEN_HEIGHT * 0.5f;
            float dd  = sqrtf(ddx * ddx + ddy * ddy);
            if (dd > 1.0f) {
                ddx /= dd; ddy /= dd;
                float t = 1e9f;
                if (ddx >  0.001f) t = fminf(t, (SCREEN_WIDTH  - 45.0f - SCREEN_WIDTH  * 0.5f) / ddx);
                if (ddx < -0.001f) t = fminf(t, (45.0f         - SCREEN_WIDTH  * 0.5f) / ddx);
                if (ddy >  0.001f) t = fminf(t, (SCREEN_HEIGHT - 45.0f - SCREEN_HEIGHT * 0.5f) / ddy);
                if (ddy < -0.001f) t = fminf(t, (45.0f         - SCREEN_HEIGHT * 0.5f) / ddy);
                float arx = SCREEN_WIDTH  * 0.5f + ddx * t;
                float ary = SCREEN_HEIGHT * 0.5f + ddy * t;
                float arpx = -ddy, arpy = ddx;
                float as = 11.0f;
                Line arrow[3] = {
                    {{arx + ddx * as,           ary + ddy * as},
                     {arx - ddx * 8 + arpx * 7, ary - ddy * 8 + arpy * 7}},
                    {{arx + ddx * as,           ary + ddy * as},
                     {arx - ddx * 8 - arpx * 7, ary - ddy * 8 - arpy * 7}},
                    {{arx - ddx * 8 + arpx * 7, ary - ddy * 8 + arpy * 7},
                     {arx - ddx * 8 - arpx * 7, ary - ddy * 8 - arpy * 7}}
                };
                SDL_Color ac = HUD_TEXT_CYAN;
                ac.a = 200;
                for (int i = 0; i < 3; i++) {
                    Shape s = {&arrow[i], 1, ac};
                    vg_draw_shape(&s, (Vec2){0, 0}, 0.0f, 1.0f);
                }
                vf_draw_string_centered("HOME", arx, ary - 18.0f, 9,
                                        (SDL_Color){100, 255, 200, 160});
            }
        }
    }
}

/* ────────────────────────────────────────────────────────────────────── */

/**
 * @brief Renders screen-space overlays: score pop-floats, edge flash,
 *        shop overlay, and warp-menu overlay.
 *
 * Camera is in HUD (0,0) space when this returns; caller restores it.
 */
void render_overlays(void)
{
    /* Score pop-floats (world coordinates — rendered while world camera is set) */
    SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < MAX_SCORE_FLOATS; i++) {
        if (!score_floats[i].active) continue;
        float t     = score_floats[i].life;
        Uint8 alpha = (t > 0.8f) ? 255 : (Uint8)(t / 0.8f * 255);
        SDL_Color fc = HUD_TEXT_GOLD;  fc.a = alpha;
        if (score_floats[i].value >= 200) { fc = HUD_AMBER;    fc.a = alpha; }
        if (score_floats[i].value >= 400) { fc = HUD_CINNABAR; fc.a = alpha; }
        char temp_text[32];
        sprintf(temp_text, "+%d", score_floats[i].value);
        vf_draw_string_centered(temp_text,
                                score_floats[i].x, score_floats[i].y, 14, fc);
    }

    /* Switch to screen space for all remaining overlays */
    vg_set_camera((Vec2){0.0f, 0.0f});

    /* Edge-wrap flash — bright border momentarily when ship wraps */
    if (edge_flash_timer > 0.0f) {
        float alpha = edge_flash_timer / 0.15f;
        SDL_Color ef = HUD_BORDER_ACTIVE; ef.a = (Uint8)(alpha * 180);
        float m = 4.0f;
        Line el[4] = {
            {{m,                m},               {SCREEN_WIDTH - m, m}},
            {{SCREEN_WIDTH - m, m},               {SCREEN_WIDTH - m, SCREEN_HEIGHT - m}},
            {{SCREEN_WIDTH - m, SCREEN_HEIGHT - m},{m,               SCREEN_HEIGHT - m}},
            {{m,                SCREEN_HEIGHT - m},{m,               m}}
        };
        Shape es = {el, 4, ef};
        vg_draw_shape(&es, (Vec2){0, 0}, 0.0f, 1.0f);
    }

    /* Autarch (god) mode banner */
    if (god_mode) {
        SDL_Color gm_col = ui_pulse(HUD_GOLD_BAR, game_time, 2.0f, 0.4f);
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        ui_panel(g_renderer, SCREEN_WIDTH / 2.0f - 200.0f, 4.0f, 400.0f, 28.0f,
                 gm_col);
        vf_draw_string_centered("AUTARCH MODE", SCREEN_WIDTH / 2.0f, 10.0f,
                                14, gm_col);
    }

    /* Critical hull warning — show when on last life */
    if (game_state == STATE_PLAYING && lives <= 1) {
        SDL_Color warn = ui_pulse(HUD_CINNABAR, game_time, 3.0f, 0.5f);
        ui_warning_chevrons(g_renderer, SCREEN_WIDTH / 2.0f - 28.0f,
                            SCREEN_HEIGHT - 30.0f, warn);
    }

    /* Combo pop text */
    if (combo_timer > 0.0f && combo_count >= 4 && g_settings.hud.show_combo) {
        float ct = combo_timer;
        Uint8 ca  = (ct > 0.3f) ? 255 : (Uint8)(ct / 0.3f * 255);
        SDL_Color cc = HUD_AMBER; cc.a = ca;
        char combuf[32];
        sprintf(combuf, "x%d", combo_count);
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        vf_draw_string_centered(combuf, SCREEN_WIDTH / 2.0f,
                                SCREEN_HEIGHT / 2.0f - 60.0f, 28, cc);
    }

    /* Shop overlay */
    if (game_state == STATE_SHOP) {
        ui_panel(g_renderer, 50, 30, SCREEN_WIDTH - 100, SCREEN_HEIGHT - 60,
                 HUD_BORDER_MAIN);
        vf_draw_string_centered("HOME STATION EXCHANGE",
                                SCREEN_WIDTH / 2.0f, 60, 20, HUD_TEXT_CYAN);
        vf_draw_string_centered("[ ESC ] CLOSE",
                                SCREEN_WIDTH / 2.0f, 88, 11,
                                (SDL_Color){120, 120, 140, 200});
        static const char *shop_names[] = {
            "FUEL CELLS (25u)",   "FULL REFUEL",        "THRUSTER UPGRADE",
            "CANNON UPGRADE",     "HULL PLATING",        "EMERGENCY LIFE",
            "ROCKET PACK (+5)",   "FIGHTER DRONE",       "REPAIR DRONE",
            "WARP RANGE +1000u",  "FUEL TANK UPGRADE",  "AMMO RESUPPLY",
            "VOID STEEL SMELTER", "AUTODYNE SCANNER",    "CHROME: TRISKELION",
            "CHROME: ETHER SHROUD"
        };
        #define SHOP_TOTAL 16
        for (int si = 0; si < SHOP_TOTAL && si < SHOP_ITEMS_PER_PAGE; si++) {
            float iy = 115.0f + si * 42.0f;
            SDL_Color ic = (si == shop_sel) ? HUD_TEXT_GOLD : HUD_TEXT_PRIMARY;
            if (si == shop_sel) {
                ui_corner_brackets(g_renderer,
                                   80, iy - 4, SCREEN_WIDTH - 160, 36,
                                   HUD_TEXT_GOLD, 10.0f);
                ui_cursor_chevron(g_renderer, 70.0f, iy, HUD_TEXT_CYAN);
            }
            vf_draw_string(shop_names[si], 110.0f, iy, 13, ic);
        }
        #undef SHOP_TOTAL
        char inv[128];
        sprintf(inv, "INV: VS:%d AF:%d HX:%d IS:%d AM:%d RK:%d CHR:%d",
                res_void_steel, res_autodyne_frags, res_hex_modules,
                res_isotopes, res_ammo, res_rockets, chrome);
        vf_draw_string_centered(inv, SCREEN_WIDTH / 2.0f,
                                SCREEN_HEIGHT - 70, 10, HUD_TEXT_DIM);
    }

    /* Warp-menu overlay */
    if (game_state == STATE_WARP_MENU) {
        ui_panel(g_renderer, 190, 140, SCREEN_WIDTH - 380, SCREEN_HEIGHT - 280,
                 HUD_BORDER_MAIN);
        vf_draw_string_centered("WARP DRIVE \xe2\x80\x94 SAVED LOCI",
                                SCREEN_WIDTH / 2.0f, 165, 16, HUD_TEXT_CYAN);
        if (warp_loc_count == 0) {
            vf_draw_string_centered("NO LOCI SAVED",
                                    SCREEN_WIDTH / 2.0f, 230, 11,
                                    HUD_TEXT_DIM);
        } else {
            for (int wi = 0; wi < warp_loc_count; wi++) {
                float wy   = 200.0f + wi * 40.0f;
                float wdx  = warp_locs[wi].pos.x - player.pos.x;
                float wdy  = warp_locs[wi].pos.y - player.pos.y;
                float wdist = sqrtf(wdx * wdx + wdy * wdy);
                int in_range = (wdist <= warp_drive_range) &&
                               (fuel_current >= 20.0f);
                SDL_Color wc = (wi == warp_menu_sel)
                               ? HUD_TEXT_GOLD
                               : in_range
                                 ? HUD_GREEN
                                 : HUD_CINNABAR;
                char wbuf[80];
                sprintf(wbuf, "%s  [%.0fu]%s",
                        warp_locs[wi].label, wdist,
                        in_range ? "" : " OUT OF RANGE");
                if (wi == warp_menu_sel)
                    ui_cursor_chevron(g_renderer, 200.0f, wy, HUD_TEXT_CYAN);
                vf_draw_string_centered(wbuf, SCREEN_WIDTH / 2.0f, wy, 13, wc);
            }
        }
        vf_draw_string_centered("ENTER=WARP  ESC=CANCEL  (20 FUEL)",
                                SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 165, 11,
                                HUD_TEXT_DIM);
    }

    /* Crosshair — drawn last so it sits on top of everything */
    if (settings_mouse_aim && settings_crosshair_style > 0 &&
        (game_state == STATE_PLAYING ||
         game_state == STATE_ATTRACT_GAMEPLAY)) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        SDL_Color xhc = HUD_TEXT_CYAN; xhc.a = 200;
        float fx = (float)mx, fy = (float)my;
        if (settings_crosshair_style == 1) {   /* Cross with centre gap */
            Line cl[4] = {
                {{fx - 14, fy}, {fx -  5, fy}},
                {{fx +  5, fy}, {fx + 14, fy}},
                {{fx, fy - 14}, {fx, fy -  5}},
                {{fx, fy +  5}, {fx, fy + 14}}
            };
            for (int ci = 0; ci < 4; ci++) {
                Shape cs = {&cl[ci], 1, xhc};
                vg_draw_shape(&cs, (Vec2){0, 0}, 0.0f, 1.0f);
            }
        } else {                                /* Dot */
            Line cl[2] = {
                {{fx - 4, fy}, {fx + 4, fy}},
                {{fx, fy - 4}, {fx, fy + 4}}
            };
            for (int ci = 0; ci < 2; ci++) {
                Shape cs = {&cl[ci], 1, xhc};
                vg_draw_shape(&cs, (Vec2){0, 0}, 0.0f, 1.0f);
            }
        }
    }

}

/* ────────────────────────────────────────────────────────────────────── */

/**
 * @brief Renders the currently active menu or UI screen based on
 *        game_state.  World-rendering helpers have already run; camera
 *        is in screen space (0,0) at entry.
 */
void render_menus(void)
{
    /* =========== STATE: TITLE =========== */
    if (game_state == STATE_TITLE) {
        static float title_timer = 0.0f;
        title_timer += 0.016f;  /* visual-only timer, frame-rate dependence acceptable */

        if (g_boot_timer < 3.0f) {
            /* Centered boot splash — 5 lines × 30px = 150px stack, centered
             * vertically in 960px screen → start_y = 405.  Center-x = 640. */
            SDL_Color term_green = {50, 255, 50, 255};
            const float cx      = (float)SCREEN_WIDTH / 2.0f;   /* 640 */
            float       start_y = ((float)SCREEN_HEIGHT - 150.0f) / 2.0f; /* 405 */

            if (g_boot_timer > 0.1f) {
                vf_draw_string_centered("NOCTIS OS V4.0 LOADED...", cx, start_y, 16.0f, term_green);
                start_y += 30.0f;
            }
            if (g_boot_timer > 0.6f) {
                vf_draw_string_centered("MEM CHECK OK...", cx, start_y, 16.0f, term_green);
                start_y += 30.0f;
            }
            if (g_boot_timer > 1.2f) {
                vf_draw_string_centered("INITIALIZING SENSORS...", cx, start_y, 16.0f, term_green);
                start_y += 30.0f;
            }
            if (g_boot_timer > 1.8f) {
                vf_draw_string_centered("LOADING FULIGIN PROTOCOL...", cx, start_y, 16.0f, term_green);
                start_y += 30.0f;
            }
            if (g_boot_timer > 2.5f) {
                vf_draw_string_centered("SYSTEM READY.", cx, start_y, 16.0f, term_green);
            }
            return;
        }

        /* Background atmosphere */
        ui_particle_drift(g_renderer, game_time, 40);

        /* Central hero panel */
        ui_panel(g_renderer, 340, 200, 600, 520, HUD_BORDER_MAIN);

        float panel_cx = 340.0f + 600.0f / 2.0f;  /* 640 */

        /* Pulsing title */
        float title_sz = 52.0f * (1.0f + 0.04f * sinf(title_timer * 2.0f));
        SDL_Color title_col = ui_pulse(HUD_TEXT_CYAN, game_time, 0.8f, 0.15f);
        vf_draw_string_centered("FULIGIN", panel_cx, 260, title_sz, title_col);

        /* Subtitle — dim cyan */
        SDL_Color dim_cyan = {0, 180, 180, 180};
        vf_draw_string_centered(
            "D R I F T I N G   A T   T H E   E D G E   O F   U R T H",
            panel_cx, 340, 11, dim_cyan);

        /* Thin separator at y=370 */
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 26, 26, 46, 140);
        SDL_RenderDrawLine(g_renderer, 350, 370, 930, 370);

        /* Menu items */
        const char *menu_labels[] = {"BEGIN DRIFT", "DRIFTER ANNALS", "SETTINGS"};
        float item_y_start = 400.0f;
        float item_step    = 60.0f;
        for (int i = 0; i < 3; i++) {
            int   is_sel = (menu_selection == i);
            SDL_Color ic = is_sel ? HUD_TEXT_GOLD : HUD_TEXT_PRIMARY;
            float iy     = item_y_start + i * item_step;
            vf_draw_string_centered(menu_labels[i], panel_cx, iy, 20, ic);
            if (is_sel) {
                ui_cursor_chevron(g_renderer, panel_cx - 130.0f, iy, HUD_TEXT_CYAN);
                ui_corner_brackets(g_renderer,
                                   panel_cx - 120.0f, iy - 6.0f,
                                   240.0f, 30.0f,
                                   HUD_TEXT_GOLD, 12.0f);
            }
        }

    /* =========== STATE: PAUSED =========== */
    } else if (game_state == STATE_PAUSED) {
        /* Dark semi-transparent overlay */
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 160);
        SDL_Rect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        SDL_RenderFillRect(g_renderer, &overlay);

        /* Center panel */
        ui_panel(g_renderer, 200, 40, 880, SCREEN_HEIGHT - 80, HUD_BORDER_MAIN);

        float panel_cx = 200.0f + 880.0f / 2.0f;  /* 640 */

        /* Header */
        vf_draw_string_centered("STASIS", panel_cx, 65, 32, HUD_TEXT_PRIMARY);

        /* Pause options section */
        ui_panel_header(g_renderer, 200, 105, 880, "PAUSE OPTIONS", HUD_TEXT_CYAN);

        const char *pause_opts[] = {
            "S: SAVE GAME",
            "L: LOAD GAME",
            "Q: QUIT TO TITLE",
            "ESC/SPACE: RESUME"
        };
        float opt_y = 125.0f;
        for (int i = 0; i < 4; i++) {
            vf_draw_string_centered(pause_opts[i], panel_cx, opt_y, 17, HUD_TEXT_PRIMARY);
            opt_y += 32.0f;
        }

        /* Separator at y=255 */
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g_renderer, 26, 26, 46, 160);
        SDL_RenderDrawLine(g_renderer, 210, 255, 1070, 255);

        /* Relics section */
        ui_panel_header(g_renderer, 200, 265, 880, "RELIQUARIES CARRIED", HUD_TEXT_CYAN);

        /* ── Relic log: map active upgrades to name+description rows ── */
        typedef struct { const char *name; const char *desc; } RelicEntry;
        RelicEntry relics[30];
        int relic_count = 0;
        #define ADD_RELIC(up, nm, dc) \
            if (up) { relics[relic_count].name = (nm); \
                      relics[relic_count].desc = (dc); \
                      relic_count++; }
        ADD_RELIC(player_upgrades.triple_shot,
                  "TRISKELION BURST",   "Three bolts spread from prow  [ fires with FIRE key ]")
        ADD_RELIC(player_upgrades.max_bounces > 0,
                  "VOID RICOCHET",      "Bolts rebound off void edge   [ passive ]")
        ADD_RELIC(player_upgrades.shield_active,
                  "ETHER SHROUD",       "Absorbs one asteroid hit      [ passive ]")
        ADD_RELIC(player_upgrades.piercing,
                  "ICHOROUS ROUNDS",    "Bolts pierce through enemies  [ passive ]")
        ADD_RELIC(player_upgrades.homing,
                  "SEEKER ICHORS",      "Bolts home to nearest stone   [ passive ]")
        ADD_RELIC(player_upgrades.rear_gun,
                  "AFT CANNON",         "Fires bolt from the aft too   [ fires with FIRE key ]")
        ADD_RELIC(player_upgrades.split_shot,
                  "FISSION SHOT",       "Bolts split on first impact   [ passive ]")
        ADD_RELIC(player_upgrades.mirror_image,
                  "WRAITH TWIN",        "Phantom twin orbits behind    [ passive ]")
        ADD_RELIC(player_upgrades.phase_shift,
                  "PHASE SHIFT",        "Pass through one killing blow [ passive, one-time ]")
        ADD_RELIC(player_upgrades.thermal_hull,
                  "THERMAL HULL",       "Ram at speed to destroy stone [ THRUST into enemy ]")
        ADD_RELIC(player_upgrades.singularity_displacer,
                  "RIFT DISPLACER",     "Double-tap THRUST to rift-jump [ tap THRUST twice ]")
        ADD_RELIC(player_upgrades.singularity_whip,
                  "BANE WHIP",          "Thrust trail scorches drift   [ hold THRUST ]")
        ADD_RELIC(player_upgrades.resonance_cascade,
                  "RESONANCE CASCADE",  "Bolts unleash shockwaves      [ fires with FIRE key ]")
        #undef ADD_RELIC

        if (relic_count > 0) {
            float ry    = 285.0f;
            int   shown = relic_count < 12 ? relic_count : 12;
            float row_h = (float)(SCREEN_HEIGHT - 310) / shown;
            if (row_h > 52.0f) row_h = 52.0f;
            /* Icon col x=220, name col x=285, desc col x=560 */
            for (int i = 0; i < shown; i++) {
                char icon[4] = {relics[i].name[0], relics[i].name[1],
                                relics[i].name[2], '\0'};
                vf_draw_string(icon,            220, ry, 11, HUD_TEXT_CYAN);
                vf_draw_string(relics[i].name,  285, ry, 11, HUD_TEXT_CYAN);
                vf_draw_string(relics[i].desc,  560, ry,  9, HUD_TEXT_DIM);
                ry += row_h;
            }
        } else {
            vf_draw_string_centered("NO RELIQUARIES CARRIED",
                                    panel_cx, 310, 14, HUD_TEXT_DIM);
        }

    /* =========== STATE: SETTINGS =========== */
    } else if (game_state == STATE_SETTINGS) {
        /* Tab names for the 9-category settings menu */
        const char *tab_names[] = {
            "VIDEO", "GRAPHICS", "AUDIO", "CONTROLS",
            "HUD", "ACCESS", "GAMEPLAY", "WORLD", "SYSTEM"
        };
        const int tab_count = 9;

        /* Outer 800×640 center panel */
        float sp_x = (SCREEN_WIDTH  - 800) / 2.0f;   /* 240 */
        float sp_y = (SCREEN_HEIGHT - 640) / 2.0f;   /* 160 */
        float sp_w = 800.0f, sp_h = 640.0f;
        SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        ui_panel(g_renderer, sp_x, sp_y, sp_w, sp_h, HUD_BORDER_MAIN);

        float panel_cx = sp_x + sp_w / 2.0f;  /* 640 */

        /* Header */
        vf_draw_string_centered("AUTODYNE CONFIGURATION", panel_cx,
                                sp_y + 22.0f, 18, HUD_TEXT_CYAN);

        /* Tab strip — two rows of angled mini-tabs, 5 on top row, 4 on second */
        float tab_w = 140.0f, tab_h = 26.0f;
        float tab_row1_y = sp_y + 48.0f;
        float tab_row2_y = tab_row1_y + tab_h + 4.0f;
        float tab_row1_x = sp_x + 10.0f;
        float tab_row2_x = sp_x + 10.0f + (tab_w + 6.0f) * 0.5f;  /* offset 2nd row */
        for (int t = 0; t < tab_count; t++) {
            int   row   = (t < 5) ? 0 : 1;
            int   col   = (t < 5) ? t : t - 5;
            float tx    = (row == 0 ? tab_row1_x : tab_row2_x) + col * (tab_w + 6.0f);
            float ty    = (row == 0) ? tab_row1_y : tab_row2_y;
            int   is_sel = (t == settings_tab);
            SDL_Color border = is_sel ? HUD_BORDER_ACTIVE : HUD_BORDER_DIM;
            ui_panel_angled(g_renderer, tx, ty, tab_w, tab_h, 6.0f, border);
            SDL_Color tc = is_sel ? HUD_TEXT_GOLD : HUD_TEXT_DIM;
            vf_draw_string_centered(tab_names[t], tx + tab_w / 2.0f,
                                    ty + 7.0f, 11, tc);
        }

        /* Content area starts below tabs */
        float base_y = tab_row2_y + tab_h + 16.0f;
        float step   = 44.0f;
        float row_x  = sp_x + 40.0f;

        /* Helper macro: draw one settings row at index i with label and value */
        #define DRAW_SETTINGS_ROW(idx, label, valstr) \
        do { \
            int   _sel = (settings_row == (idx)); \
            float _y   = base_y + (idx) * step; \
            SDL_Color _lc = _sel ? HUD_TEXT_CYAN  : HUD_TEXT_DIM; \
            SDL_Color _vc = _sel ? HUD_TEXT_GOLD  : HUD_TEXT_PRIMARY; \
            if (_sel) ui_cursor_chevron(g_renderer, row_x - 16.0f, _y, HUD_TEXT_CYAN); \
            vf_draw_string((label),   row_x,         _y, 13, _lc); \
            vf_draw_string((valstr),  row_x + 340.0f, _y, 13, _vc); \
            if (_sel) { \
                float _bw = sp_w - 80.0f; \
                ui_corner_brackets(g_renderer, row_x - 4.0f, _y - 6.0f, \
                                   _bw, 22.0f, HUD_BORDER_ACTIVE, 8.0f); \
            } \
        } while (0)

        const char *on_off[]     = {"OFF", "ON"};
        const char *glow_names[] = {"OFF", "LOW", "MED", "HIGH", "MAX"};
        const char *dz_names[]   = {"LOW", "MED", "HIGH"};
        const char *ch_names[]   = {"NONE", "DOT", "CROSS", "CHEVRON"};
        const char *cb_names[]   = {"NONE", "DEUTERANOPIA", "PROTANOPIA", "TRITANOPIA"};
        const char *diff_names[] = {"AUTODYNE", "STANDARD", "CACOGEN"};
        char _vbuf[64];

        /* ── VIDEO tab ── */
        if (settings_tab == 0) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "DISPLAY MODE",
                g_settings.video.fullscreen ? "FULLSCREEN" : "WINDOWED");
            sprintf(_vbuf, "%d", g_settings.video.refresh_rate);
            DRAW_SETTINGS_ROW(r++, "REFRESH RATE", _vbuf);
            DRAW_SETTINGS_ROW(r++, "VSYNC",
                on_off[g_settings.video.vsync]);

        /* ── GRAPHICS tab ── */
        } else if (settings_tab == 1) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "PHOSPHOR GLOW",
                glow_names[g_settings.graphics.glow_intensity]);
            sprintf(_vbuf, "%d", g_settings.graphics.particle_count);
            DRAW_SETTINGS_ROW(r++, "PARTICLE DRIFT", _vbuf);
            DRAW_SETTINGS_ROW(r++, "CRT SCANLINES",
                on_off[g_settings.graphics.scanlines]);
            DRAW_SETTINGS_ROW(r++, "VIGNETTE",
                on_off[g_settings.graphics.vignette]);
            DRAW_SETTINGS_ROW(r++, "MOTION BLUR",
                on_off[g_settings.graphics.motion_blur]);
            DRAW_SETTINGS_ROW(r++, "SCREEN SHAKE",
                on_off[g_settings.graphics.screen_shake]);

        /* ── AUDIO tab ── */
        } else if (settings_tab == 2) {
            int r = 0;
            sprintf(_vbuf, "%d%%", g_settings.audio.master_vol);
            DRAW_SETTINGS_ROW(r++, "MASTER VOLUME", _vbuf);
            sprintf(_vbuf, "%d%%", g_settings.audio.music_vol);
            DRAW_SETTINGS_ROW(r++, "MUSIC VOLUME",  _vbuf);
            sprintf(_vbuf, "%d%%", g_settings.audio.sfx_vol);
            DRAW_SETTINGS_ROW(r++, "SFX VOLUME",    _vbuf);
            sprintf(_vbuf, "%d%%", g_settings.audio.ui_vol);
            DRAW_SETTINGS_ROW(r++, "UI VOLUME",     _vbuf);
            DRAW_SETTINGS_ROW(r++, "STREAMER MODE",
                on_off[g_settings.audio.streamer_mode]);

        /* ── CONTROLS tab ── */
        } else if (settings_tab == 3) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "MOUSE AIM",
                on_off[g_settings.controls.mouse_aim]);
            sprintf(_vbuf, "%d", g_settings.controls.mouse_sensitivity);
            DRAW_SETTINGS_ROW(r++, "MOUSE SENSITIVITY", _vbuf);
            DRAW_SETTINGS_ROW(r++, "AUTOFIRE",
                on_off[g_settings.controls.autofire]);
            DRAW_SETTINGS_ROW(r++, "AIM ASSIST",
                on_off[g_settings.controls.aim_assist]);
            DRAW_SETTINGS_ROW(r++, "CTRL DEADZONE",
                dz_names[g_settings.controls.ctrl_deadzone]);
            DRAW_SETTINGS_ROW(r++, "INVERT Y",
                on_off[g_settings.controls.invert_y]);
            DRAW_SETTINGS_ROW(r++, "KEYBINDS", ">");
            if (g_controller)
                vf_draw_string_centered("CONTROLLER CONNECTED",
                                        panel_cx, sp_y + sp_h - 40.0f,
                                        11, HUD_GREEN);

        /* ── HUD tab ── */
        } else if (settings_tab == 4) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "SHOW FPS",
                on_off[g_settings.hud.show_fps]);
            DRAW_SETTINGS_ROW(r++, "SHOW MINIMAP",
                on_off[g_settings.hud.show_minimap]);
            DRAW_SETTINGS_ROW(r++, "CROSSHAIR",
                ch_names[g_settings.hud.crosshair]);
            sprintf(_vbuf, "%d%%", g_settings.hud.hud_scale);
            DRAW_SETTINGS_ROW(r++, "HUD SCALE",   _vbuf);
            DRAW_SETTINGS_ROW(r++, "SHOW COMBO",
                on_off[g_settings.hud.show_combo]);
            DRAW_SETTINGS_ROW(r++, "SHOW ZONE NAME",
                on_off[g_settings.hud.show_zone_name]);

        /* ── ACCESSIBILITY tab ── */
        } else if (settings_tab == 5) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "COLORBLIND MODE",
                cb_names[g_settings.accessibility.colorblind]);
            DRAW_SETTINGS_ROW(r++, "HIGH CONTRAST",
                on_off[g_settings.accessibility.high_contrast]);
            sprintf(_vbuf, "%+d", g_settings.accessibility.font_size_delta);
            DRAW_SETTINGS_ROW(r++, "FONT SIZE DELTA", _vbuf);
            DRAW_SETTINGS_ROW(r++, "REDUCE MOTION",
                on_off[g_settings.accessibility.reduce_motion]);

        /* ── GAMEPLAY tab ── */
        } else if (settings_tab == 6) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "DIFFICULTY",
                diff_names[g_settings.gameplay.difficulty]);
            sprintf(_vbuf, "%d", g_settings.gameplay.starting_lives);
            DRAW_SETTINGS_ROW(r++, "STARTING LIVES", _vbuf);

        /* ── WORLD tab ── */
        } else if (settings_tab == 7) {
            int r = 0;
            sprintf(_vbuf, "%u", (unsigned)g_settings.world.seed);
            DRAW_SETTINGS_ROW(r++, "WORLD SEED",        _vbuf);
            sprintf(_vbuf, "%.2f", g_settings.world.asteroid_density);
            DRAW_SETTINGS_ROW(r++, "ASTEROID DENSITY",  _vbuf);
            sprintf(_vbuf, "%.2f", g_settings.world.enemy_density);
            DRAW_SETTINGS_ROW(r++, "ENEMY DENSITY",     _vbuf);
            sprintf(_vbuf, "%.2f", g_settings.world.loot_multiplier);
            DRAW_SETTINGS_ROW(r++, "LOOT MULTIPLIER",   _vbuf);
            sprintf(_vbuf, "%d",   g_settings.world.zone_sharpness);
            DRAW_SETTINGS_ROW(r++, "ZONE SHARPNESS",    _vbuf);
            sprintf(_vbuf, "%d",   g_settings.world.starting_zone);
            DRAW_SETTINGS_ROW(r++, "STARTING ZONE",     _vbuf);

        /* ── SYSTEM tab ── */
        } else if (settings_tab == 8) {
            int r = 0;
            DRAW_SETTINGS_ROW(r++, "SHOW INTRO",
                on_off[g_settings.show_intro]);
            DRAW_SETTINGS_ROW(r++, "RESET DEFAULTS", "ENTER");
            DRAW_SETTINGS_ROW(r++, "SAVE & QUIT",    "ENTER");
        }

        #undef DRAW_SETTINGS_ROW

        /* Bottom instructions */
        vf_draw_string_centered(
            "[ Q / E ]  PREV/NEXT TAB    [ UP / DN ]  SELECT ROW    "
            "[ LT / RT ]  CHANGE VALUE    [ ESC ]  BACK",
            panel_cx, sp_y + sp_h - 20.0f, 9, HUD_TEXT_DARK);

    /* =========== STATE: KEYBINDS =========== */
    } else if (game_state == STATE_KEYBINDS) {
        ui_panel(g_renderer, 80, 20, SCREEN_WIDTH - 160, SCREEN_HEIGHT - 40,
                 HUD_BORDER_MAIN);

        float panel_cx = 80.0f + (SCREEN_WIDTH - 160) / 2.0f;  /* 640 */

        SDL_Color kb_tc = (keybind_page == 0) ? HUD_TEXT_GOLD : HUD_TEXT_DIM;
        SDL_Color ct_tc = (keybind_page == 1) ? HUD_TEXT_GOLD : HUD_TEXT_DIM;

        vf_draw_string_centered("KEYBINDS", panel_cx, 35, 22, HUD_TEXT_CYAN);
        vf_draw_string("KEYBOARD",   panel_cx - 200.0f, 70, 16, kb_tc);
        vf_draw_string("CONTROLLER", panel_cx +  40.0f, 70, 16, ct_tc);

        vf_draw_string_centered("Q / E   TO   SWITCH   PAGES",
                                panel_cx, SCREEN_HEIGHT - 60, 11, HUD_TEXT_DARK);
        vf_draw_string_centered("ESC TO RETURN",
                                panel_cx, SCREEN_HEIGHT - 38, 11, HUD_TEXT_DARK);

        if (rebinding_action >= 0) {
            vf_draw_string_centered("PRESS ANY KEY",
                                    panel_cx,
                                    SCREEN_HEIGHT / 2.0f - 20, 28, HUD_AMBER);
            vf_draw_string_centered("ESC TO CANCEL",
                                    panel_cx,
                                    SCREEN_HEIGHT / 2.0f + 30, 16, HUD_TEXT_DIM);
        } else if (ctrl_rebinding_action >= 0) {
            vf_draw_string_centered("PRESS CONTROLLER BUTTON",
                                    panel_cx,
                                    SCREEN_HEIGHT / 2.0f - 20, 24, HUD_AMBER);
            vf_draw_string_centered("START/BACK RESERVED   ESC TO CANCEL",
                                    panel_cx,
                                    SCREEN_HEIGHT / 2.0f + 30, 14, HUD_TEXT_DIM);
        } else if (keybind_page == 0) {
            /* Keyboard binds list */
            vf_draw_string_centered("ENTER TO REBIND",
                                    panel_cx, 100, 12, HUD_TEXT_DARK);
            float kb_row_h = (float)(SCREEN_HEIGHT - 160) / KB_COUNT;
            if (kb_row_h > 44.0f) kb_row_h = 44.0f;
            for (int i = 0; i < KB_COUNT; i++) {
                int   is_sel = (keybind_selection == i);
                SDL_Color ic = is_sel ? HUD_TEXT_GOLD : HUD_TEXT_PRIMARY;
                const char *kname = SDL_GetScancodeName(keybinds[i]);
                char row[64];
                sprintf(row, "%-12s  %s", kb_action_names[i], kname);
                float y = 120.0f + i * kb_row_h;
                if (is_sel)
                    ui_cursor_chevron(g_renderer, panel_cx - 200.0f, y,
                                      HUD_TEXT_CYAN);
                vf_draw_string_centered(row, panel_cx, y, 16, ic);
                if (is_sel) {
                    float bw = (float)strlen(row) * 16 * 0.7f + 20.0f;
                    ui_corner_brackets(g_renderer,
                                       panel_cx - bw / 2.0f, y - 8.0f,
                                       bw, 24.0f, HUD_TEXT_GOLD, 10.0f);
                }
            }
        } else {
            /* Controller binds diagram page */
            float cx = SCREEN_WIDTH  / 2.0f;
            float cy = SCREEN_HEIGHT / 2.0f;
            render_controller_binds_page(cx, cy, keybind_selection,
                                         ctrl_binds, ct_action_names);
            if (!g_controller) {
                vf_draw_string_centered("NO CONTROLLER DETECTED",
                                        panel_cx,
                                        SCREEN_HEIGHT - 90, 14,
                                        HUD_CINNABAR);
            }
        }

    /* =========== STATE: HIGH SCORES (DRIFTER ANNALS) =========== */
    } else if (game_state == STATE_HIGHSCORES) {
        /* Background particles */
        ui_particle_drift(g_renderer, game_time, 25);

        /* Panel */
        ui_panel(g_renderer, 280, 80, 720, 750, HUD_GOLD_BAR);

        float panel_cx = 280.0f + 720.0f / 2.0f;  /* 640 */

        /* Header */
        vf_draw_string_centered("THE FALLEN DRIFTERS", panel_cx, 115, 28,
                                HUD_TEXT_GOLD);

        /* Score entries */
        for (int i = 0; i < 5; i++) {
            float ey = 210.0f + i * 90.0f;
            char rank_buf[4];
            sprintf(rank_buf, "%d.", i + 1);
            vf_draw_string_centered(rank_buf,                  panel_cx - 160.0f, ey, 20, HUD_TEXT_DIM);
            vf_draw_string_centered(high_scores[i].initials,   panel_cx -  40.0f, ey, 20, HUD_TEXT_CYAN);
            char score_buf[16];
            sprintf(score_buf, "%05d", high_scores[i].score);
            vf_draw_string_centered(score_buf,                 panel_cx + 120.0f, ey, 20, HUD_TEXT_PRIMARY);
            /* AUTARCH label above top entry */
            if (i == 0) {
                vf_draw_string_centered("AUTARCH", panel_cx, ey - 20.0f, 9,
                                        HUD_TEXT_GOLD);
            }
        }

        /* Instruction */
        vf_draw_string_centered("PRESS SPACE TO RETURN TO THE DRIFT",
                                panel_cx, SCREEN_HEIGHT - 80, 12, HUD_TEXT_DIM);

    /* =========== STATE: GAME OVER =========== */
    } else if (game_state == STATE_GAMEOVER) {
        /* Background particles with cinnabar tint */
        ui_particle_drift(g_renderer, game_time, 50);

        /* Panel */
        ui_panel(g_renderer, 290, 280, 700, 340, HUD_CINNABAR);

        float panel_cx = 290.0f + 700.0f / 2.0f;  /* 640 */

        SDL_Color go_col = ui_pulse(HUD_CINNABAR, game_time, 1.5f, 0.3f);
        vf_draw_string_centered("DRIFT ENDS", panel_cx, 315, 42, go_col);

        vf_draw_string_centered("THE AUTODYNE IS LOST TO THE VOID",
                                panel_cx, 380, 13, HUD_TEXT_DIM);

        char final_score[48];
        sprintf(final_score, "FINAL CHRONICLE: %d", score);
        vf_draw_string_centered(final_score, panel_cx, 420, 18, HUD_TEXT_PRIMARY);

        vf_draw_string_centered("PRESS ENTER TO DRIFT AGAIN",
                                panel_cx, 475, 16, HUD_AMBER);

    /* =========== STATE: NEW HIGH SCORE =========== */
    } else if (game_state == STATE_NEW_HIGHSCORE) {
        /* Panel */
        ui_panel(g_renderer, 340, 280, 600, 340, HUD_GOLD_BAR);

        float panel_cx = 340.0f + 600.0f / 2.0f;  /* 640 */

        vf_draw_string_centered("NEW DRIFTER RECORD!", panel_cx, 310, 24,
                                HUD_TEXT_GOLD);

        char score_buf[32];
        sprintf(score_buf, "%d", score);
        vf_draw_string_centered(score_buf, panel_cx, 350, 16, HUD_TEXT_PRIMARY);

        vf_draw_string_centered("INSCRIBE YOUR MARK:", panel_cx, 390, 18,
                                HUD_TEXT_PRIMARY);

        char di[8];
        sprintf(di, "%c %c %c",
                temp_initials[0], temp_initials[1], temp_initials[2]);
        vf_draw_string_centered(di, panel_cx, 440, 28, HUD_TEXT_CYAN);

    /* =========== STATE: UPGRADE SELECT (RELIC CHOICE) =========== */
    } else if (game_state == STATE_UPGRADE_SELECT) {
        static float upgrade_pulse = 0.0f;
        upgrade_pulse += 0.04f; /* visual-only timer */

        /* Background particles */
        ui_particle_drift(g_renderer, game_time, 30);

        /* Pulsing header */
        float sz = 24.0f + 2.0f * sinf(upgrade_pulse * 3.0f);
        SDL_Color surge_col = ui_pulse(HUD_GREEN, game_time, 2.0f, 0.3f);
        vf_draw_string_centered("CHRONICLE SURGE!",
                                SCREEN_WIDTH / 2.0f, 80, sz, surge_col);
        vf_draw_string_centered("CHOOSE YOUR RELIC",
                                SCREEN_WIDTH / 2.0f, 130, 14, HUD_TEXT_DIM);
        vf_draw_string_centered("WASD / ARROWS + ENTER",
                                SCREEN_WIDTH / 2.0f, 155, 11, HUD_TEXT_DARK);

        /* 3 relic cards in a row:
         * Card w=340, h=180, spacing=20
         * Total = 340*3 + 20*2 = 1060, start_x = (1280-1060)/2 = 110 */
        float card_w   = 340.0f;
        float card_h   = 180.0f;
        float card_gap = 20.0f;
        float card_sx  = 110.0f;
        float card_y   = 200.0f;

        for (int i = 0; i < 3; i++) {
            int   is_sel     = (i == selected_option);
            float cx         = card_sx + i * (card_w + card_gap);
            SDL_Color border = is_sel ? HUD_BORDER_ACTIVE : HUD_PANEL_DEEP;

            ui_panel(g_renderer, cx, card_y, card_w, card_h, border);

            if (is_sel) {
                ui_corner_brackets(g_renderer, cx, card_y, card_w, card_h,
                                   HUD_TEXT_GOLD, 16.0f);
            }

            float card_cx = cx + card_w / 2.0f;
            float name_sz = is_sel
                            ? (18.0f * (1.0f + 0.05f * sinf(upgrade_pulse * 5.0f + i)))
                            : 14.0f;
            SDL_Color nc  = is_sel ? HUD_TEXT_GOLD : HUD_TEXT_DIM;

            vf_draw_string_centered(upgrade_names[upgrade_options[i]],
                                    card_cx, 250, name_sz, nc);

            /* Separator line at y=280 */
            SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(g_renderer, 26, 26, 46, 120);
            SDL_RenderDrawLine(g_renderer, (int)(cx + 4), 280,
                               (int)(cx + card_w - 4), 280);

            /* Description */
            vf_draw_string_centered(upgrade_descs[upgrade_options[i]],
                                    card_cx, 295, 10, HUD_TEXT_DARK);

            if (is_sel) {
                SDL_Color dim_amber = HUD_AMBER; dim_amber.a = 160;
                vf_draw_string_centered("PRESS ENTER", card_cx, 355, 9, dim_amber);
            }
        }
    }
}
