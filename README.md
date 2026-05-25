# FULIGIN

> *"Drifting at the Edge of Urth"*

A vector-arcade space shooter set in the dying light of the last age.

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Web-lightgrey)

---

## About

FULIGIN is a retro vector-arcade space shooter where you pilot the **Autodyne** through the dying cosmos of the Latter-Day. The sun is a swollen red cinder. The void is littered with the wreckage of dead civilizations. You are a **Drifter** — salvaging chronicles, collecting reliquaries, and surviving the escalating horror of the Deep Void.

Inspired by Gene Wolfe's *Book of the New Sun*, Jack Vance's *Dying Earth*, and the pure geometric light of classic vector arcade games, FULIGIN renders a universe in phosphor and shadow — beautiful, hostile, and very nearly spent.

---

## Gameplay Features

- **Vector phosphor rendering** with configurable persistence and glow — each line lingers and fades like a real beam display
- **7 distinct enemy types** with unique AI behaviors: saucers, Vector Stalker, Boundary Weaver, Eye of the Void, Eldritch Tendril, Daemon Sigil, and more
- **30 unique relics** (run upgrades) to discover — no two runs are alike
- **Procedurally generated audio** — all sound effects and music synthesized at runtime; no external audio files required
- **Zone-based danger progression**: Home Space → Inner Belt → Deep Void → The Abyss
- **XP / Chronicle system** with per-run character progression
- **Home Station** with resource economy and warp drive between zones
- **Combo multiplier system** rewarding aggressive play
- **Post-processing effects**: chromatic aberration, screen shake, bloom, and monochrome display modes
- **Controller support** via SDL2 GameController API
- **Attract mode** with AI autopilot demonstration
- **Web build target** via Emscripten / WASM

---

## Building

```bash
# Requirements: MinGW-w64 GCC, SDL2 2.30+, SDL2_mixer 2.8+

# Clone and build
git clone https://github.com/[username]/fuligin.git
cd fuligin
make

# Or on Windows using the batch script
_build.bat

# Web build (requires Emscripten)
make wasm
```

SDL2.dll and SDL2_mixer.dll are bundled in the repository root — the game runs without any separate SDL2 installation.

---

## Controls

| Input | Action |
|---|---|
| W / A / D or Arrow Keys | Rotate / Thrust |
| Space or Left Ctrl | Fire |
| Shift | Hyperspace jump |
| Tab | Toggle minimap |
| Enter | Interact / Confirm |
| Escape | Pause |
| **Controller** | |
| Analog stick or D-pad | Rotate / Thrust |
| Face button (South) | Fire |
| Face button (East) | Hyperspace |
| Start | Pause |

---

## Project Structure

```
fuligin/
├── src/
│   ├── main.c              — Entry point, SDL2 init, and main loop
│   ├── game.c              — Core simulation: entities, physics, AI, rendering
│   ├── audio.c             — Procedural audio engine (synthesized SFX + dynamic music)
│   ├── vector_graphics.c   — Phosphor renderer with persistence and post-processing effects
│   └── vector_font.c       — Stroke font renderer
├── include/
│   ├── game.h
│   ├── audio.h
│   ├── vector_graphics.h
│   └── vector_font.h
├── thirdparty/
│   ├── SDL2-2.30.3/
│   └── SDL2_mixer-2.8.2/
├── assets/                 — (reserved for future assets)
├── SDL2.dll                — Bundled for Windows runtime
├── SDL2_mixer.dll          — Bundled for Windows runtime
├── Makefile
├── _build.bat
├── STYLE_GUIDE.md          — Coding standards
└── FULIGIN_LORE_VIBE_GUIDE.md — Design bible
```

---

## Lore

*The Latter-Day. The Autarch's realm crumbles at the edge of time.*

The Solar Rust consumes what little warmth remains. Ancient stations drift through belts of shattered iron, their chronicles — memories encoded in the wreckage of dead machines — still broadcasting into the cold.

As a Drifter piloting the **Autodyne**, you harvest those chronicles from the void and collect **reliquaries** from the ruin of older civilizations, each one whispering of powers the dying world can barely sustain. The zones grow stranger the further you venture from Home Space. The Boundary Weavers do not sleep. The Eye of the Void does not blink.

The abyss grows deeper with each pass.

---

## License

MIT License — see [LICENSE](LICENSE) for details.
