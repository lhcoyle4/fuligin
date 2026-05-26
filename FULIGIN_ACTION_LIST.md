# FULIGIN — Action List

## Style Authority Files
Every UI implementation must reference these two sources before touching a pixel:
- **`vibe_presentation.html`** / **`FULIGIN_Guide.pdf`** — canonical visual mockups. Pages 2 and 5 of the PDF show the in-game Qud/Noctis style; page 4 shows the title screen.
- **`FULIGIN_HUD_DESIGN_BRIEF.md`** — detailed spec for FF7R panel geometry, bar sizing, glow values, and color hex codes.

## The Two-Style Rule

**In-game HUD, inventory, item cards, tooltips, Cugel-9 logs, resource readouts** → **Caves of Qud / Noctis terminal aesthetic**: monospace (`Share Tech Mono`), bracket-corner rectangular panels, `[LABEL: VALUE]` notation, acid green data values, cyan structure lines, magenta category tags, all-caps.

**Main menu, pause menu, settings, reliquary selection, shop, warp screen** → **FF7 Remake menu aesthetic**: angled parallelogram panels with ~20° diagonal corner cuts, thin 1px URTH CYAN glowing borders, segmented gauge bars, chevron `>` active-state markers, dark near-black glassmorphism fill.

---

## 🔴 Core Systems

**1. dcimgui integration** ✅ [DONE — panel primitives shipped] — Replace all SDL2 hand-drawn UI with dcimgui. Implement two panel primitives as the foundation for everything else:
- `ui_panel_terminal()` ✅ — Qud/Noctis style: rectangular, bracket-style corner marks (`┌`, `┐`, `└`, `┘` drawn as 8px L-shapes), `#88ccff` 1px border, `#050505` fill, `Share Tech Mono` font. Used for all in-game HUD elements, inventory, tooltips, Cugel-9 log. Visual reference: `FULIGIN_Guide.pdf` p.5 inventory grid.
- `ui_panel_menu()` ✅ — Unified terminal style: wraps `ui_panel_terminal()` + centered `──── TITLE ────` divider. Used for all menu/settings/shop screens. (FF7R angled-cut aesthetic retired in favour of unified Qud terminal look.)

**2. HUD panel geometry** ✅ [DONE] *(terminal style — Qud/Noctis)* — Score, fuel, ammo, chronicle bar, reliquary count, zone indicator all use `ui_panel_terminal()`. No diagonal cuts on combat HUD. Monospace labels in `#88ccff` cyan; data values in `#39FF14` acid green; category/type tags in `#FF00FF` magenta using `[BRACKET]` notation. Reference: `FULIGIN_Guide.pdf` p.2 item cards and p.5 inventory UI.

**3. Segmented / block progress bars** ✅ [DONE] *(terminal style)* — `ui_bar_segmented()` for Chronicle/XP: acid green fill, discrete block segments per 10%, dark `#001100` track, 1px tight border. `ui_bar_block()` for hull/shields. Danger ramp on fuel/hull: `#39FF14` → `#FF8C00` at 30% → `#E34234` at 15%. Label format: `FUEL: 847 / 1200` (monospace, label dim, value bright). Reference: `FULIGIN_HUD_DESIGN_BRIEF.md` §1B Qud bar spec.

---

## 🟠 Visual Polish

**4. Phosphor decay / Vectrex glow** ✅ [DONE] — All vector lines (ships, bullets, UI borders) leave ~0.1s luminance trail via additive blend. Implemented via `vg_apply_persistence()` in `vector_graphics.c` — offscreen render target with exponential decay; 5-level `settings_glow` dial; called every frame at the top of `game_render()`.

**5. Scanline shimmer** ✅ [DONE] — 1px horizontal sweep, 5–10% opacity, every 3–5s. Toggleable via `graphics.scanlines`. Applies only to gameplay viewport, not menu overlays.

**6. Screen-edge vignette** ✅ [DONE] — PHOTIC RUST `#B22222` tint at screen edges, scales with zone depth. CINNABAR pulse on beat peak.

**7. Score / event floats** *(terminal style)* ✅ [DONE] — `Share Tech Mono`, all-caps, scale-up-and-fade over 1.5s. URTH CYAN for Chronicle gain, CINNABAR for damage, ACID GREEN for resource pickup. Format matches item card stat style from `FULIGIN_Guide.pdf` p.2.

**8. Zone transition banner** *(terminal style)* ✅ [DONE] — `>>> ENTERING IRON SHOALS <<<` slides in at top, monospace, GHOST WHITE with full URTH CYAN glow. Bracket-corner framing. Reference: todo.md zone transition spec.

**9. Vector stress-cracking** ✅ [DONE] — High-value asteroids display spreading wireframe fractures before shattering. URTH CYAN cracks, CINNABAR flash at break point.

**10. Warp-drive singularity exit effects** ✅ [DONE] — Three unlockable warp FX: vector expansion (default), black-hole collapse (all screen lines converge to 1px then explode), CRT power-down flash. Progression rewards. *(Default vector-expansion + CRT-bloom-flash variant shipped — fires on every warp jump; black-hole-collapse variant and the unlock-progression system deferred.)*

**11. Engine dust trail** ✅ [DONE] — Phos-green (`#39FF14`) vector particle trail proportional to ship velocity. Ambient cosmic dust sucked in at high speeds.

**12. Glitch tearing on crits** ✅ [DONE] — Crit hit or large rift: vector lines duplicate, shake, tear across horizontal scanlines + white-noise screech.

---

## 🟡 Gameplay Mechanics

**13. Zone transition system** — Five zones (HOME STATION → SCRAP FIELDS → IRON SHOALS → VOID REACHES → DEEP DRIFT), danger/density scaling outward. Warp loci as fixed nav beacons. Zone entry triggers banner (item 8).

**14. Proximity danger alert** *(terminal style)* ✅ [DONE] — Pulsing `>>> DANGER <<<` at bottom-center, CINNABAR, monospace. Matches the terminal bracket-flash style from `vibe_presentation.html`.

**15. Remnant debris / hidden hazards** — Invisible spatial hazards only revealed with PALE SIGHT Reliquary. No visual cue until equipped.

**16. Fuel clock / reactor drain** ✅ [DONE] — Passive reactor drain (0.08 fuel/s base + 0.04/s per active relic) depletes fuel even without thrusting. At 0% fuel: thrust blocked (Emergency Drift Mode), drift_penalty_timer accumulates; after 10 s adrift the ship takes a hull breach (lose a life, fuel refills to 20% emergency reserve, HULL BREACH EventFloat + screen shake). Fuel UI already uses danger ramp via ui_fuel_color().

**17. Emergency heat vent** ✅ [DONE] — Weapon overheat on rapid fire. Player-triggered coolant blowback: dense forward vector particles + instant reverse thrust, consumes fuel.

**18. Gravity slingshot boosts** ✅ [DONE] — Close orbit (150–380 u) around the void rift gives a continuous tangential velocity boost scaled by orbital depth and tangential speed. Fires only when the player is genuinely curving around the rift (|tan_v| > 30). Spawns a 3 s-cooldown "GRAVITY ASSIST" EventFloat + cyan particles on first entry.

**19. Asteroid bowling combos** ✅ [DONE] — Heavy projectile launches chain into larger rocks. `STRIKE!` float in ACID GREEN + multiplier.

**20. Cargo bay / inventory weight** *(terminal style — Qud grid)* — Grid-based cargo slots using the exact inventory grid from `FULIGIN_Guide.pdf` p.5: cyan bracket-corner slot cells, item name in cyan, category tag in magenta `[TYPE]`, stats in acid green. Heavy cargo degrades ship physics.

---

## 🟢 Enemy AI

**21. Ascians** ✅ [DONE] — Voiceless interceptor saucers patrolling regular-polygon perimeters at constant angular speed in Zone 3+. Module `src/enemy_ascian.[ch]` wired into `game.c`: `ascian_init()` in `game_init()`, `ascian_update()` after `rustweaver_update()` in the simulation tick, `ascian_render(g_renderer, camera_pos.x, camera_pos.y)` after the rust-weaver render. Spawn pacing: Zone 3+ only, 55-85s timer, 800-1100u spawn ring, cycles triangle/square/pentagon patrol shapes per spawn. Three-bolt magenta wedge volley when player crosses 600u detect radius (4.2s cooldown). Collision section 5c runs `ascian_hit_test` against player bolts (+150 score, +1 XP orb, ASCIAN DOWN float) and `ascian_check_player_hit` against the player hull (standard projectile damage — Void Stone armour soaks one hit; "INTERCEPTED" float + sad Cugel-9 quip). Voiceless per design contract — module emits no SFX of its own. Clean build, exe produced (319KB), no new warnings.

**22. Lictors** ✅ [DONE] — Elite, aggressive interceptor saucers that pursue the Autodyne at high speeds, Zone 4+ only. Module `src/enemy_lictor.[ch]` wired into `game.c`: `lictor_init()` in `game_init()`, `lictor_update()` after `ascian_update()` in the simulation tick, `lictor_render(g_renderer, camera_pos.x, camera_pos.y)` after `ascian_render()`. Spawn pacing: Zone 4+ only, 90-130s timer, 900-1200u spawn ring; foreshadow Cugel-9 quip on arrival. Pursuit AI: per-frame thrust vector toward the player (240 u/s²) with capped 260 u/s max speed and 0.18/s linear drag — smooth predator arc, not a perfect tracker. Aimed bolt every 2.4 s when player is within 900u detect radius (340 u/s bolt speed, amber streak). Collision section 5d handles player-bolt vs Lictor (+200 score, +1 XP orb, LICTOR DOWN float) and Lictor-bolt vs player (standard hit, Void Stone soak, PURSUED float + Cugel-9 quip). "Arrival accelerates beat" implemented via `lictor_alive_count()` accessor: the global beat-pulse code compresses tempo by ~35 % (clamped at 0.15 s minimum) while any Lictor is alive. Voiceless module — caller plays SFX on hit. Makefile SRC updated. Clean build, 325KB exe, zero new warnings.

**23. Rust-Weaver Drones** ✅ [DONE] — Corrosive spit bypasses shields, degrades hull plating. Module `src/enemy_rustweaver.[ch]` wired into `game.c`: `rustweaver_init()` in `game_init()`, `rustweaver_update()` after `drone_chatter_update()` in the simulation tick, `rustweaver_render(g_renderer, camera_pos.x, camera_pos.y)` after NPC shapes in `render_entities()`. Spawn pacing: Zone 2+ only, 35-55s timer with jitter, 700-1000u spawn ring. Collision section 5b runs `rustweaver_hit_test` against player bolts (+75 score, +1 XP orb, RUSTWEAVER DOWN float) and `rustweaver_check_player_hit` against the player hull (corrosion bypasses Ether Shroud and Phase Shift — direct hull-breach unless Void Stone armour soaks; "CORROSION" float + sad Cugel-9 quip). Makefile SRC line also restored to include the previously-dropped `src/state.c` and `src/game_data.c` so the build links again.

**24. Scavenger Probes** — Tractor-beam Void Steel, attempt warp escape.

**25. EMP Sentinels** — EM pulse locks steering and thrust briefly.

---

## 🔵 Lore / UI Features

**26. Cugel-9 board computer** ✅ [DONE] *(terminal style)* — HUD text log panel using `ui_panel_terminal()`. Format: `[CUGEL-9]: STRUCTURAL INTEGRITY COMPROMISED. COMPENSATING FOR PILOT INEPTITUDE.` — monospace, dim cyan label, ghost white body text. Plus TTS using sad/melancholic robot voice. Reference: `FULIGIN_Guide.pdf` p.2 lore text style (italic monospace blockquote).

**27. Pet shield drone chatter** ✅ [DONE] *(terminal style)* — Tiny `Share Tech Mono` text floats above orbital drones: `[HELP]`, `"TARGET: BIG ROCK"`, `"SHIELD AT 24%"`. Low-contrast cyan. Integrated into `src/game.c` (NPC update loop emits TARGET_UFO / TARGET_LARGE / HELP / COORDINATING events based on game state with 1.2–6 s re-arm; rendered after NPC shapes in world-camera space; `drone_chatter.c` added to Makefile SRC).

**28. The Rusty Flagon Tavern Beacon** *(menu style)* — Rare neutral station. Docking opens an FF7R-style menu overlay (angled panels) for trading contraband, buying coordinates, or playing vector roulette.

---

## ⚪ Settings / Infrastructure

**29. Settings menu** *(FF7R structure + Caves of Qud visual style)* ✅ [RETROFIT DONE] — Full-screen overlay with FF7R-style tab-based navigation and interactability: tab bar across top, `>` chevron row cursor, keyboard/controller nav between rows, segmented sliders for numeric values, toggle switches for booleans. Visual rendering uses the terminal/Qud aesthetic throughout: `ui_panel_terminal()` bracket-corner panels, `Share Tech Mono` monospace font, `#88ccff` cyan label text, `#39FF14` acid-green current values, `#FF00FF` magenta `[CATEGORY]` section headers, `#001100` dark track on slider fills. No diagonal panel cuts — rectangular terminal panels only. Reference: `FULIGIN_Settings_Spec.md` for full field list; `FULIGIN_HUD_DESIGN_BRIEF.md` §1B for bar/slider spec. **Retrofit complete: all menus, overlays (title screen, pause menu, upgrade selection, high score screen, reliquary shop, gameover, keybinds, warp menu, shop overlay, minimap panel, god-mode banner) now use `ui_panel_terminal()`. The FF7R angled-panel aesthetic is retired. Settings menu interactive implementation (tab nav, sliders, toggles) still pending.**

**30. Consolidate Settings struct** — Reconcile `GameSettings` (spec) with current `Settings` in `game.h`. One canonical struct, one `.cfg` file.

**31. Dynamic CRT glass curvature** [IN PROGRESS] — `render_crt_glass()` static function added to `game.c` (14 edge-darkening strips + 40 corner glare diagonals via plain SDL2 draw calls); `settings_crt_curve` toggle wired in; function called before `vg_present()`. Shader/GLSL barrel distortion deferred. Off by default.

**32. Chronicle chord harmonics** ✅ [DONE] — Rapid orb pickup → sequential FM-synth pentatonic notes. Combo chain = arpeggio.

---

## Suggested Implementation Order

1 (dcimgui + two panel primitives) → 2 → 3 → 29 → 6 → 5 → 8 → 14 → 13 → everything else.

The two panel primitives from item 1 unlock all other UI work — that is the critical path. Nothing else in the list should be started until `ui_panel_terminal()` and `ui_panel_menu()` are solid.
