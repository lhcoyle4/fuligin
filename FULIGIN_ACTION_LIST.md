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

**1. dcimgui integration** — Replace all SDL2 hand-drawn UI with dcimgui. Implement two panel primitives as the foundation for everything else:
- `ui_panel_terminal()` — Qud/Noctis style: rectangular, bracket-style corner marks (`┌`, `┐`, `└`, `┘` drawn as 8px L-shapes), `#88ccff` 1px border, `#050505` fill, `Share Tech Mono` font. Used for all in-game HUD elements, inventory, tooltips, Cugel-9 log. Visual reference: `FULIGIN_Guide.pdf` p.5 inventory grid.
- `ui_panel_menu()` — FF7R style: parallelogram with top-left ~20px diagonal clip, `#00FFFF` 1px glowing border, CHONDRITE GREY at 35% opacity fill, `Orbitron` or equivalent geometric sans for headers. Used for all menu/settings/shop screens. Visual reference: `FULIGIN_HUD_DESIGN_BRIEF.md` §1A + `FULIGIN_Guide.pdf` p.4 title screen.

**2. HUD panel geometry** *(terminal style — Qud/Noctis)* — Score, fuel, ammo, chronicle bar, reliquary count, zone indicator all use `ui_panel_terminal()`. No diagonal cuts on combat HUD. Monospace labels in `#88ccff` cyan; data values in `#39FF14` acid green; category/type tags in `#FF00FF` magenta using `[BRACKET]` notation. Reference: `FULIGIN_Guide.pdf` p.2 item cards and p.5 inventory UI.

**3. Segmented / block progress bars** *(terminal style)* — `ui_bar_segmented()` for Chronicle/XP: acid green fill, discrete block segments per 10%, dark `#001100` track, 1px tight border. `ui_bar_block()` for hull/shields. Danger ramp on fuel/hull: `#39FF14` → `#FF8C00` at 30% → `#E34234` at 15%. Label format: `FUEL: 847 / 1200` (monospace, label dim, value bright). Reference: `FULIGIN_HUD_DESIGN_BRIEF.md` §1B Qud bar spec.

---

## 🟠 Visual Polish

**4. Phosphor decay / Vectrex glow** — All vector lines (ships, bullets, UI borders) leave ~0.1s luminance trail via additive blend. Applies equally to `ui_panel_terminal()` border lines and gameplay geometry.

**5. Scanline shimmer** — 1px horizontal sweep, 5–10% opacity, every 3–5s. Toggleable via `graphics.scanlines`. Applies only to gameplay viewport, not menu overlays.

**6. Screen-edge vignette** — PHOTIC RUST `#B22222` tint at screen edges, scales with zone depth. CINNABAR pulse on beat peak.

**7. Score / event floats** *(terminal style)* — `Share Tech Mono`, all-caps, scale-up-and-fade over 1.5s. URTH CYAN for Chronicle gain, CINNABAR for damage, ACID GREEN for resource pickup. Format matches item card stat style from `FULIGIN_Guide.pdf` p.2.

**8. Zone transition banner** *(terminal style)* ✅ [DONE] — `>>> ENTERING IRON SHOALS <<<` slides in at top, monospace, GHOST WHITE with full URTH CYAN glow. Bracket-corner framing. Reference: todo.md zone transition spec.

**9. Vector stress-cracking** ✅ [DONE] — High-value asteroids display spreading wireframe fractures before shattering. URTH CYAN cracks, CINNABAR flash at break point.

**10. Warp-drive singularity exit effects** — Three unlockable warp FX: vector expansion (default), black-hole collapse (all screen lines converge to 1px then explode), CRT power-down flash. Progression rewards.

**11. Engine dust trail** — Phos-green (`#39FF14`) vector particle trail proportional to ship velocity. Ambient cosmic dust sucked in at high speeds.

**12. Glitch tearing on crits** — Crit hit or large rift: vector lines duplicate, shake, tear across horizontal scanlines + white-noise screech.

---

## 🟡 Gameplay Mechanics

**13. Zone transition system** — Five zones (HOME STATION → SCRAP FIELDS → IRON SHOALS → VOID REACHES → DEEP DRIFT), danger/density scaling outward. Warp loci as fixed nav beacons. Zone entry triggers banner (item 8).

**14. Proximity danger alert** *(terminal style)* ✅ [DONE] — Pulsing `>>> DANGER <<<` at bottom-center, CINNABAR, monospace. Matches the terminal bracket-flash style from `vibe_presentation.html`.

**15. Remnant debris / hidden hazards** — Invisible spatial hazards only revealed with PALE SIGHT Reliquary. No visual cue until equipped.

**16. Fuel clock / reactor drain** — Background drain; rate scales with equipped Reliquaries/weapons. Emergency Drift Mode at 0%: drift, shields fail, hull depletes. Fuel UI uses danger ramp from item 3.

**17. Emergency heat vent** — Weapon overheat on rapid fire. Player-triggered coolant blowback: dense forward vector particles + instant reverse thrust, consumes fuel.

**18. Gravity slingshot boosts** — Close orbit around heavy rifts gives free velocity boost. Tactical physics use.

**19. Asteroid bowling combos** — Heavy projectile launches chain into larger rocks. `STRIKE!` float in ACID GREEN + multiplier.

**20. Cargo bay / inventory weight** *(terminal style — Qud grid)* — Grid-based cargo slots using the exact inventory grid from `FULIGIN_Guide.pdf` p.5: cyan bracket-corner slot cells, item name in cyan, category tag in magenta `[TYPE]`, stats in acid green. Heavy cargo degrades ship physics.

---

## 🟢 Enemy AI

**21. Ascians** — Rigid geometric patrol trajectories. No randomness. High-zone only.

**22. Lictors** — Elite fast interceptors, player-targeted. Arrival accelerates beat.

**23. Rust-Weaver Drones** — Corrosive spit bypasses shields, degrades hull plating.

**24. Scavenger Probes** — Tractor-beam Void Steel, attempt warp escape.

**25. EMP Sentinels** — EM pulse locks steering and thrust briefly.

---

## 🔵 Lore / UI Features

**26. Cugel-9 board computer** *(terminal style)* — HUD text log panel using `ui_panel_terminal()`. Format: `[CUGEL-9]: STRUCTURAL INTEGRITY COMPROMISED. COMPENSATING FOR PILOT INEPTITUDE.` — monospace, dim cyan label, ghost white body text. Plus TTS using sad/melancholic robot voice. Reference: `FULIGIN_Guide.pdf` p.2 lore text style (italic monospace blockquote).

**27. Pet shield drone chatter** *(terminal style)* — Tiny `Share Tech Mono` text floats above orbital drones: `[HELP]`, `"TARGET: BIG ROCK"`, `"SHIELD AT 24%"`. Low-contrast cyan.

**28. The Rusty Flagon Tavern Beacon** *(menu style)* — Rare neutral station. Docking opens an FF7R-style menu overlay (angled panels) for trading contraband, buying coordinates, or playing vector roulette.

---

## ⚪ Settings / Infrastructure

**29. Settings menu** *(FF7R menu style)* — Full-screen overlay using `ui_panel_menu()` throughout. Tab bar across top with diagonal-cut active tab indicator. Row navigation with `>` chevron. Sliders and toggles use the ATB-style segmented bar from `FULIGIN_HUD_DESIGN_BRIEF.md` §1A. Reference: `FULIGIN_Settings_Spec.md` for full field list.

**30. Consolidate Settings struct** — Reconcile `GameSettings` (spec) with current `Settings` in `game.h`. One canonical struct, one `.cfg` file.

**31. Dynamic CRT glass curvature** — Toggleable shader: 1970s CRT curved faceplate + subtle glass corner reflections. Off by default.

**32. Chronicle chord harmonics** ✅ [DONE] — Rapid orb pickup → sequential FM-synth pentatonic notes. Combo chain = arpeggio.

---

## Suggested Implementation Order

1 (dcimgui + two panel primitives) → 2 → 3 → 29 → 6 → 5 → 8 → 14 → 13 → everything else.

The two panel primitives from item 1 unlock all other UI work — that is the critical path. Nothing else in the list should be started until `ui_panel_terminal()` and `ui_panel_menu()` are solid.
