# Active Agent Workspace Locks

## Antigravity (Gemini)
*   **Current Branch:** `agent/gemini`
*   **Active Task:** Idle (Waiting for Claude's UI locks to release)
*   **Locked Files/Paths:**
    *   None

## Claude
*   **Current Branch:** `agent/claude`
*   **Active Task:** FF7R/CoQ HUD UI Overhaul — Phase 2 (render functions + style guide)
*   **Locked Files/Paths:**
    *   `src/game.c` — rewriting render_hud(), render_minimap(), render_overlays(), render_menus() with new HUD_* palette and angled panels
    *   `STYLE_GUIDE.md` — updating typography, color palette, and UI-direction sections

### Claude Lock Detail

Started: 2026-05-25
Phase 1 COMPLETE: include/ui.h and src/ui.c updated with HUD_* palette + 7 new primitive functions. Clean compile confirmed.

Phase 2 IN PROGRESS:
- Rewrite render_hud() in src/game.c using HUD_TL/TR/BL/BR constants, ui_panel_angled(), ui_bar_segmented(), ui_cursor_chevron(), ui_section_divider()
- Rewrite render_minimap() using HUD_ZONE_ACCENT grid cells
- Rewrite render_overlays() with HUD_CINNABAR warning chevrons, HUD_AMBER combo pop
- Rewrite render_menus() with double-border terminal style
- Update STYLE_GUIDE.md typography/color/layout sections per FULIGIN_HUD_DESIGN_BRIEF.md

Do NOT edit src/game.c or STYLE_GUIDE.md until Claude pushes the completion commit and updates this file.
