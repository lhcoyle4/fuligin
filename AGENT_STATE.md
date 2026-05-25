# Active Agent Workspace Locks

**Protocol:** Every agent MUST update this file when starting or finishing a task.
All lock changes must be committed immediately. No agent may edit a locked file
without first acquiring the lock in this document. See COWORKING_PROTOCOL.md.

---

## Antigravity (Gemini)
*   **Current Branch:** `main`
*   **Active Task:** Idle — awaiting next task assignment
*   **Locked Files/Paths:**
    *   None

## Linux Desktop
*   **Current Branch:** `main`
*   **Active Task:** Idle — awaiting next task assignment
*   **Locked Files/Paths:**
    *   None

## Claude
*   **Current Branch:** `main`
*   **Active Task:** feat: mouse button default keybinds (LMB=shoot, RMB=thrust, MMB=hyperspace)
*   **Locked Files/Paths:**
    *   `include/game.h` — add mouse_fire_btn, mouse_thrust_btn, mouse_hyper_btn to ControlsSettings
    *   `src/game.c`     — wire mouse button fields into input polling + event handler; save/load; settings UI

**Do NOT edit the above files until Claude pushes the completion commit and updates this file to Idle.**

---

## Completed Work Log

### Claude — Phase 2 COMPLETE (commit 183b5a5, 2026-05-25)
Phase 2: Noctis HUD overhaul + comprehensive settings system
Merged to: main (2026-05-25 merge commit)

### Claude — Phase 3 COMPLETE (commit 0eecfae, 2026-05-25)
Phase 3: Makefile rename + ai.c source inclusion
Merged to: main (2026-05-25 merge commit)

### Gemini — todo.md COMPLETE (commit 63cf586, 2026-05-25)
Added todo.md: full FULIGIN + Rogue-inspired roadmap
Merged to: main via PR #1 (2026-05-25 merge commit)
