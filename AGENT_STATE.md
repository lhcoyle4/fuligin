# Active Agent Workspace Locks

## Antigravity (Gemini)
*   **Current Branch:** `agent/gemini`
*   **Active Task:** Idle — Phase 3 feature branches merged (music-tempo, radar-blips, combo-ui)
*   **Locked Files/Paths:**
    *   None

## Claude
*   **Current Branch:** `agent/claude`
*   **Active Task:** Phase 3 — Makefile rename + ai.c source inclusion + settings persistence QA
*   **Locked Files/Paths:**
    *   `Makefile` — rename TARGET fuligin.exe → launch_fuligin.exe; add src/ai.c to SRC
    *   `FULIGIN_SETTINGS_SPEC.md` — settings system documentation update
    *   `src/ui.c` — Phase 3 UI polish (scanline density, vignette tuning, bar gap fixes)
    *   `include/ui.h` — any Phase 3 ui primitive additions

### Claude — Phase 2 COMPLETE (commit 183b5a5, 2026-05-25)

Phase 2: Noctis HUD overhaul + comprehensive settings system
- `include/ui.h` + `src/ui.c`: HUD_* palette (28 constants), 7 new primitives
  (ui_panel_angled, ui_bar_segmented, ui_bar_block, ui_cursor_chevron,
   ui_section_divider, ui_warning_chevrons, ui_vignette)
- `include/game.h`: Settings (9 categories), WorldGenParams, 3 new enums,
  11 STATE_SETTINGS_* states
- `src/game.c`: render_hud/minimap/overlays/menus rewritten in HUD_* palette;
  9-tab settings menu; settings_save/load (fuligin.cfg)
- `STYLE_GUIDE.md`: HUD_* palette + typography + settings UI style rules
- `FULIGIN_SETTINGS_SPEC.md`: full design spec
Pushed to: lhcoyle4/fuligin (agent/claude) + lhcoyle4/asteroids_vectrex (agent/claude)

### Claude — Phase 3 Plan

1. Commit Makefile rename (launch_fuligin.exe) + src/ai.c inclusion — in progress
2. UI polish pass: ui.c scanline/vignette/bar tuning
3. Verify settings_save/load against fuligin.cfg on live game
4. Once Gemini releases game.c: wire WorldGenParams seed into game_init(),
   add SDL_SetWindowFullscreen for fullscreen toggle
5. Visual QA: launch game, screenshot HUD panels and settings menu
6. PR agent/claude → antigravity (human action)

**Do NOT edit `Makefile`, `src/ui.c`, `include/ui.h`, or `FULIGIN_SETTINGS_SPEC.md`
until Claude pushes the Phase 3 completion commit.**
