# Active Agent Workspace Locks

## Antigravity (Gemini)
*   **Current Branch:** `agent/gemini`
*   **Active Task:** 20MB World Bible Generation (Procedural Text Gen)
*   **Locked Files/Paths:**
    *   `WORLD_BIBLE.md`
    *   `scripts/generate_world_bible.py`

## Claude
*   **Current Branch:** `agent/claude`
*   **Active Task:** Idle — Phase 2 complete, all locks released
*   **Locked Files/Paths:** None

### Claude — Last Completed (2026-05-25, commit 183b5a5)

Phase 2: Noctis HUD overhaul + comprehensive settings system
- `include/ui.h` + `src/ui.c`: HUD_* palette, 7 new primitives (ui_panel_angled, ui_bar_segmented, ui_bar_block, ui_cursor_chevron, ui_section_divider, ui_warning_chevrons, ui_vignette)
- `include/game.h`: Settings (9 categories), WorldGenParams, 3 new enums, 11 STATE_SETTINGS_* states
- `src/game.c`: render_hud/minimap/overlays/menus rewritten in HUD_* + FF7R/CoQ terminal aesthetic; 9-tab settings menu; settings_save/load (fuligin.cfg)
- `STYLE_GUIDE.md`: updated palette + typography + settings UI style rules
- `FULIGIN_SETTINGS_SPEC.md`: full design spec (new file)
Pushed to: lhcoyle4/fuligin (agent/claude) + lhcoyle4/asteroids_vectrex (agent/claude)

**Next for Claude:** world gen seeding in game_init(), visual QA, PR agent/claude → antigravity
