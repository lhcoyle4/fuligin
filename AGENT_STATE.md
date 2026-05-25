# Active Agent Workspace Locks

## Antigravity (Gemini)
*   **Current Branch:** `agent/gemini`
*   **Active Task:** 20MB World Bible Generation (Procedural Text Gen)
*   **Locked Files/Paths:**
    *   `WORLD_BIBLE.md`
    *   `scripts/generate_world_bible.py`

## Claude
*   **Current Branch:** `agent/claude`
*   **Active Task:** Phase 2 — Noctis HUD overhaul + comprehensive settings system + procedural world gen
*   **Locked Files/Paths:**
    *   `src/game.c` — render_hud/minimap/overlays/menus rewrite; settings menu render; world gen integration
    *   `include/game.h` — new Settings struct, WorldGenParams struct, expanded enums
    *   `STYLE_GUIDE.md` — typography/color/layout update + settings UI style rules
    *   `FULIGIN_HUD_DESIGN_BRIEF.md` — addendum with settings menu spec + world gen spec

### Claude Lock Detail

Started: 2026-05-25
Phase 1 COMPLETE (pushed to agent/claude): include/ui.h + src/ui.c with HUD_* palette + 7 new primitives.

Phase 2 IN PROGRESS — three parallel agents:
- Agent A: STYLE_GUIDE.md + HUD design brief addendum (settings + world gen)
- Agent B: include/game.h — Settings struct (video/controls/graphics/audio/accessibility/HUD/system/game-specific/world-gen), expanded save/load
- Agent C: src/game.c — render_hud (angled panels, segmented bars), render_minimap, render_overlays, render_menus (full settings menu tree with all categories), procedural world gen seeding

Do NOT edit the above files until Claude pushes the Phase 2 completion commit.
